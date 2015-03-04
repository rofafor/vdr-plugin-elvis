/*
 * widget.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <string.h>

#include "common.h"
#include "log.h"
#include "widget.h"

#undef USE_COOKIE_JAR

// --- cElvisWidgetEventInfo ------------------------------------------------

cElvisWidgetEventInfo::cElvisWidgetEventInfo(int idP, const char *nameP, const char *channelP, const char *shortTextP, const char *descriptionP, int lengthP, const char *fLengthP,
                                     const char *thumbnailP, const char *startTimeP, const char *endTimeP, const char *urlP, int programViewIdP, int recordingIdP,
                                     bool hasStartedP, bool hasEndedP, bool isRecordedP, bool isReadyP, bool isWildcardP, bool isEncryptedP)
: idM(idP),
  lengthM(lengthP),
  programViewIdM(programViewIdP),
  recordingIdM(recordingIdP),
  hasStartedM(hasStartedP),
  hasEndedM(hasEndedP),
  isRecordedM(isRecordedP),
  isReadyM(isReadyP),
  isWildcardM(isWildcardP),
  isEncryptedM(isEncryptedP),
  nameM(nameP),
  channelM(channelP),
  shortTextM(shortTextP),
  descriptionM(descriptionP),
  fLengthM(fLengthP),
  thumbnailM(thumbnailP),
  startTimeM(startTimeP),
  endTimeM(endTimeP),
  urlM(urlP),
  startTimeValueM(strtotime(startTimeP)),
  endTimeValueM(strtotime(endTimeP))
{
}

cElvisWidgetEventInfo::~cElvisWidgetEventInfo()
{
}

// --- cElvisWidgetVODInfo --------------------------------------------------

cElvisWidgetVODInfo::cElvisWidgetVODInfo(int idP, int lengthP, int ageLimitP, int yearP, int priceP, const char *titleP, const char *originalTitleP, const char *currencyP,
                                         const char *shortDescriptionP, const char *infoP, const char *info2P, const char *trailerUrlP, const char *categoriesP)
: idM(idP),
  lengthM(lengthP),
  ageLimitM(ageLimitP),
  yearM(yearP),
  priceM(priceP),
  titleM(titleP),
  originalTitleM(originalTitleP),
  currencyM(currencyP),
  shortDescriptionM(shortDescriptionP),
  infoM(infoP),
  info2M(info2P),
  trailerUrlM(trailerUrlP),
  categoriesM(categoriesP)
{
}

cElvisWidgetVODInfo::~cElvisWidgetVODInfo()
{
}

// --- cElvisWidget ----------------------------------------------------

const char *cElvisWidget::baseCookieNameS = "cookie.conf";
const char* cElvisWidget::baseUrlViihdeS = "http://api.elisaviihde.fi/etvrecorder";

cElvisWidget *cElvisWidget::instanceS = NULL;

cElvisWidget *cElvisWidget::GetInstance()
{
  if (!instanceS)
     instanceS = new cElvisWidget();

  return instanceS;
}

void cElvisWidget::Destroy()
{
  DELETE_POINTER(instanceS);
}

cElvisWidget::cElvisWidget()
:  dataM(""),
   handleM(NULL),
   headerListM(NULL)
{
}

cElvisWidget::~cElvisWidget()
{
  cMutexLock(mutexM);

  if (handleM) {
     // cleanup curl stuff
     if (headerListM) {
        curl_slist_free_all(headerListM);
        headerListM = NULL;
        }
     curl_easy_cleanup(handleM);
     handleM = NULL;
     }
}

int cElvisWidget::DebugCallback(CURL *handleP, curl_infotype typeP, char *dataP, size_t sizeP, void *userPtrP)
{
  cElvisWidget *obj = reinterpret_cast<cElvisWidget *>(userPtrP);

  if (obj) {
     switch (typeP) {
       case CURLINFO_TEXT:
            debug8("%s INFO %.*s", __PRETTY_FUNCTION__, (int)sizeP, dataP);
            break;
       case CURLINFO_HEADER_IN:
            debug8("%s HEAD <<< %.*s", __PRETTY_FUNCTION__,  (int)sizeP, dataP);
            break;
       case CURLINFO_HEADER_OUT:
            debug8("%s HEAD >>>\n%.*s", __PRETTY_FUNCTION__, (int)sizeP, dataP);
            break;
       case CURLINFO_DATA_IN:
            debug8("%s DATA <<< %.*s", __PRETTY_FUNCTION__,  (int)sizeP, dataP);
            break;
       case CURLINFO_DATA_OUT:
            debug8("%s DATA >>>\n%.*s", __PRETTY_FUNCTION__, (int)sizeP, dataP);
            break;
       default:
            break;
       }
     }

  return 0;
}

size_t cElvisWidget::WriteCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cElvisWidget *obj = reinterpret_cast<cElvisWidget *>(dataP);
  size_t len = sizeP * nmembP;

  if (obj)
     obj->PutData((const char *)ptrP, (int)len);

  return len;
}

cString cElvisWidget::Unescape(const char *s)
{
  cString res; // = strunescape(s);
  if (handleM) {
     char *p = curl_easy_unescape(handleM, s, 0, NULL);
     res = p;
     curl_free(p);
     }

  return res;
}

cString cElvisWidget::Escape(const char *s)
{
  cString res; // = strescape(s);
  if (handleM) {
     char *p = curl_easy_escape(handleM, s, 0);
     res = p;
     curl_free(p);
     }

  return res;
}

void cElvisWidget::PutData(const char *dataP, unsigned int lenP)
{
  cString data(dataP);
  data.Truncate(lenP);
  dataM = cString::sprintf("%s%s", *dataM, *data);
}

bool cElvisWidget::Perform(const char *urlP, const char *msgP)
{
  debug16("%s (%s, %s)", __PRETTY_FUNCTION__, urlP,  msgP);

  if (handleM) {
     CURLcode err;
     long http_code = 0;

     // reset data
     dataM = "";

     curl_easy_setopt(handleM, CURLOPT_URL, urlP);
     err = curl_easy_perform(handleM);
     if (err != CURLE_OK) {
        error("%s (%s, %s) Perform (%s)", __PRETTY_FUNCTION__, urlP, msgP, curl_easy_strerror(err));
        return false;
        }

     err = curl_easy_getinfo(handleM, CURLINFO_RESPONSE_CODE, &http_code);
     if (err != CURLE_OK) {
        error("%s (%s, %s) Getinfo (%s)", __PRETTY_FUNCTION__, urlP, msgP, curl_easy_strerror(err));
        return false;
        }

     if (http_code != 200) {
        error("%s (%s, %s) Invalid HTTP code (%ld)", __PRETTY_FUNCTION__, urlP, msgP, http_code);
        return false;
        }

     if (isempty(*dataM)) {
        debug2("%s (%s, %s) Empty data", __PRETTY_FUNCTION__, urlP,  msgP);
        return false;
        }
     }

  return true;
}

bool cElvisWidget::Login()
{
  if (isempty(ElvisConfig.Username) || isempty(ElvisConfig.Password)) {
     error("Invalid credentials");
     return false;
     }

  if (!IsLogged() && handleM) {
#ifdef USE_COOKIE_JAR
     cString url = cString::sprintf("%s/login.sl?username=%s&password=%s&savelogin=true&ajax=true", baseUrlViihdeS, ElvisConfig.Username, ElvisConfig.Password);
#else
     cString url = cString::sprintf("%s/login.sl?username=%s&password=%s&ajax=true", baseUrlViihdeS, ElvisConfig.Username, ElvisConfig.Password);
#endif

     if (Perform(*url, "Login"))
        return strstr(*dataM, "TRUE");
     }

  return false;
}

bool cElvisWidget::Logout()
{
  if (IsLogged() && handleM) {
     cString url = cString::sprintf("%s/logout.sl?ajax=true", baseUrlViihdeS);

     return Perform(*url, "Logout");
     }

  return false;
}

bool cElvisWidget::IsLogged()
{
  if (handleM) {
     cString url = cString::sprintf("%s/login.sl?islogged", baseUrlViihdeS);

     if (Perform(*url, "IsLogged"))
        return strstr(*dataM, "TRUE");
     }

  return false;
}

bool cElvisWidget::Invalidate()
{
  if (handleM) {
#ifdef USE_COOKIE_JAR
     // erase all cookies
     curl_easy_setopt(handleM, CURLOPT_COOKIELIST, "ALL");
#endif

     // start a new session
     curl_easy_setopt(handleM, CURLOPT_COOKIESESSION, 1L);

     return true;
     }

  return false;
}

bool cElvisWidget::IsLoginRequired(const char *stringP)
{
  if (strstr(stringP, "</html>"))
     return true;

  return false;
}

bool cElvisWidget::Load(const char *directoryP)
{
  // initialize the curl session
  if (!handleM)
     handleM = curl_easy_init();

  if (handleM) {
     // verbose output
     curl_easy_setopt(handleM, CURLOPT_VERBOSE, 1L);
     curl_easy_setopt(handleM, CURLOPT_DEBUGFUNCTION, cElvisWidget::DebugCallback);
     curl_easy_setopt(handleM, CURLOPT_DEBUGDATA, this);

     // set callback
     curl_easy_setopt(handleM, CURLOPT_WRITEFUNCTION, cElvisWidget::WriteCallback);
     curl_easy_setopt(handleM, CURLOPT_WRITEDATA, this);

     // no progress meter and no signaling
     curl_easy_setopt(handleM, CURLOPT_NOPROGRESS, 1L);
     curl_easy_setopt(handleM, CURLOPT_NOSIGNAL, 1L);

     // set timeout
     curl_easy_setopt(handleM, CURLOPT_CONNECTTIMEOUT, 5L);
     curl_easy_setopt(handleM, CURLOPT_TIMEOUT, 10L);

     // set user-agent
     curl_easy_setopt(handleM, CURLOPT_USERAGENT, *cString::sprintf("vdr-%s/%s", PLUGIN_NAME_I18N, VERSION));

     // follow location
     curl_easy_setopt(handleM, CURLOPT_FOLLOWLOCATION, 1L);

     // enable cookies
#ifdef USE_COOKIE_JAR
     curl_easy_setopt(handleM, CURLOPT_COOKIEJAR, directoryP ? *cString::sprintf("%s/%s", directoryP, baseCookieNameS) : "-");
#else
     curl_easy_setopt(handleM, CURLOPT_COOKIEFILE, "");
#endif

     // set additional headers to prevent caching
     headerListM = curl_slist_append(headerListM, "Cache-Control: no-store, no-cache, must-revalidate");
     headerListM = curl_slist_append(headerListM, "Cache-Control: post-check=0, pre-check=0");
     headerListM = curl_slist_append(headerListM, "Pragma: no-cache");
     headerListM = curl_slist_append(headerListM, "Expires: Mon, 26 Jul 1997 05:00:00 GMT");
     curl_easy_setopt(handleM, CURLOPT_HTTPHEADER, headerListM);

     //Login();

     return true;
     }

  return false;
}

void cElvisWidget::ParseFolders(cElvisWidgetFolderCallbackIf &callbackP, json_t *objP, int folderIdP)
{
    void *iter = json_object_iter(objP);
    while (iter) {
        const char *key = json_object_iter_key(iter);
        json_t *value = json_object_iter_value(iter);
        if ((!strcmp(key, "folders") || !strcmp(key, "subfolders")) && json_is_array(value)) {
            for (unsigned int i = 0; i < json_array_size(value); i++) {
                cString name = "";
                int id = 0, count = 0;
                bool has_pin = false;
                json_t *obj3 = json_array_get(value, i);
                json_t *obj4 = json_object_get(obj3, "id");
                if (json_is_string(obj4))
                    id = (int)strtol(json_string_value(obj4), NULL, 10);
                obj4 = json_object_get(obj3, "count");
                if (json_is_string(obj4))
                    count = (int)strtol(json_string_value(obj4), NULL, 10);
                obj4 = json_object_get(obj3, "name");
                if (json_is_string(obj4))
                    name = Unescape(json_string_value(obj4));
                obj4 = json_object_get(obj3, "has_pin");
                if (json_is_string(obj4) && !strcasecmp(json_string_value(obj4), "true"))
                    has_pin = true;
                obj4 = json_object_get(obj3, "subfolders");
                if (json_array_size(obj4)) {
                    ParseFolders(callbackP, json_deep_copy(obj3), id);
                }
                debug2("%s (, , %d) id=%d name='%s' count=%d protected=%d", __PRETTY_FUNCTION__, folderIdP, id, *name, count, has_pin);
                callbackP.AddFolder(id, *name, count, has_pin);
            }
        }
        iter = json_object_iter_next(objP, iter);
    }
    json_decref(objP);

}

bool cElvisWidget::GetFolders(cElvisWidgetFolderCallbackIf &callbackP)
{
  cMutexLock(mutexM);

  if (handleM) {
     cString url = cString::sprintf("%s/ready.sl?folderlist&ajax=true", baseUrlViihdeS);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetFolders")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               json_error_t error;
               json_t *obj = json_loads(*dataM, 0, &error);
               if (obj)
                  ParseFolders(callbackP, obj);
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::GetRecordings(cElvisWidgetRecordingCallbackIf &callbackP, int folderIdP)
{
  cMutexLock(mutexM);

  if (handleM) {
     cString url = (folderIdP < 0) ? cString::sprintf("%s/ready.sl?ajax=true&clear=true", baseUrlViihdeS) :
                                     cString::sprintf("%s/ready.sl?folderid=%d&ajax=true&clear=true", baseUrlViihdeS, folderIdP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetRecordings")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               json_error_t error;
               json_t *obj = json_loads(*dataM, 0, &error);
               if (obj) {
                  void *iter = json_object_iter(obj);
                  while (iter) {
                        const char *key = json_object_iter_key(iter);
                        json_t *value = json_object_iter_value(iter);
                        if (!strcmp(key, "ready_data") && json_is_array(value)) {
                           for (unsigned int i = 0; i < json_array_size(value); i++) {
                               json_t *obj2 = json_array_get(value, i);
                               void *iter2 = json_object_iter(obj2);
                               while (iter2) {
                                     const char *key2 = json_object_iter_key(iter2);
                                     json_t *value2 = json_object_iter_value(iter2);
                                     if (!strcmp(key2, "folders") && json_is_array(value2)) {
                                        for (unsigned int j = 0; j < json_array_size(value2); j++) {
                                            cString name = "", size = "";
                                            int id = 0, count = 0;
                                            json_t *obj3 = json_array_get(value2, j);
                                            json_t *obj4 = json_object_get(obj3, "id");
                                            if (json_is_string(obj4))
                                               id = (int)strtol(json_string_value(obj4), NULL, 10);
                                            obj4 = json_object_get(obj3, "name");
                                            if (json_is_string(obj4))
                                               name = Unescape(json_string_value(obj4));
                                            obj4 = json_object_get(obj3, "size");
                                            if (json_is_string(obj4))
                                               size = Unescape(json_string_value(obj4));
                                            obj4 = json_object_get(obj3, "recordings_count");
                                            if (json_is_string(obj4))
                                               count = (int)strtol(json_string_value(obj4), NULL, 10);
                                            debug2("%s (, %d): id=%d count=%d name='%s' size='%s'", __PRETTY_FUNCTION__, folderIdP, id, count, *name, *size);
                                            callbackP.AddFolder(id, count, *name, *size);
                                            }
                                        }
                                     else if (!strcmp(key2, "recordings") && json_is_array(value2)) {
                                        for (unsigned int j = 0; j < json_array_size(value2); j++) {
                                            cString name = "", channel = "", start_time = "", timestamp = "";
                                            int id = 0, program_id = 0, folder_id = 0, count = 0, length = 0;
                                            json_t *obj3 = json_array_get(value2, j);
                                            json_t *obj4 = json_object_get(obj3, "id");
                                            if (json_is_string(obj4))
                                               id = (int)strtol(json_string_value(obj4), NULL, 10);
                                            obj4 = json_object_get(obj3, "program_id");
                                            if (json_is_string(obj4))
                                               program_id = (int)strtol(json_string_value(obj4), NULL, 10);
                                            obj4 = json_object_get(obj3, "folder_id");
                                            if (json_is_string(obj4))
                                               folder_id = (int)strtol(json_string_value(obj4), NULL, 10);
                                            obj4 = json_object_get(obj3, "name");
                                            if (json_is_string(obj4))
                                               name = Unescape(json_string_value(obj4));
                                            obj4 = json_object_get(obj3, "channel");
                                            if (json_is_string(obj4))
                                               channel = Unescape(json_string_value(obj4));
                                            obj4 = json_object_get(obj3, "start_time");
                                            if (json_is_string(obj4))
                                               start_time = Unescape(json_string_value(obj4));
                                            obj4 = json_object_get(obj3, "timestamp");
                                            if (json_is_string(obj4))
                                               timestamp = Unescape(json_string_value(obj4));
                                            obj4 = json_object_get(obj3, "viewcount");
                                            if (json_is_string(obj4))
                                               count = (int)strtol(json_string_value(obj4), NULL, 10);
                                            obj4 = json_object_get(obj3, "length");
                                            if (json_is_string(obj4))
                                               length = (int)strtol(json_string_value(obj4), NULL, 10);
                                            debug2("%s (, %d): id=%d program_id=%d folder_id=%d count=%d length=%d name='%s' channel='%s' start_time='%s'", __PRETTY_FUNCTION__, folderIdP, id, program_id, folder_id, count, length, *name, *channel, *start_time);
                                            callbackP.AddRecording(id, program_id, folder_id, count, length, *name, *channel, *start_time);
                                            }
                                        }
                                     iter2 = json_object_iter_next(obj2, iter2);
                                     }
                               }
                           }
                        iter = json_object_iter_next(obj, iter);
                        }
                  json_decref(obj);
                  }
               return true;
               }
            }
         }
     }
  return false;
}

bool cElvisWidget::RemoveRecording(int idP)
{
  cMutexLock(mutexM);

  if (handleM && (idP > 0)) {
     cString url = cString::sprintf("%s/program.sl?remove=true&removep=%d&ajax=true", baseUrlViihdeS, idP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "RemoveRecording")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               debug2("%s (%d) Removed recording", __PRETTY_FUNCTION__, idP);
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::RemoveFolder(int idP)
{
  cMutexLock(mutexM);

  if (handleM && (idP > 0)) {
     cString url = cString::sprintf("%s/ready.sl?delete_folder=%d&ajax=true", baseUrlViihdeS, idP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "RemoveFolder")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               debug2("%s (%d) Removed folder", __PRETTY_FUNCTION__, idP);
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::RenameFolder(int idP, const char *nameP)
{
  cMutexLock(mutexM);

  if (handleM && (idP > 0)) {
     cString url = cString::sprintf("%s/ready.sl?rename_folder=%d&name=%s&ajax=true", baseUrlViihdeS, idP, *Escape(nameP));
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "RenameFolder")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               debug2("%s (%d, %s) Renamed folder", __PRETTY_FUNCTION__, idP, nameP);
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::CreateFolder(const char *nameP, int parentFolderIdP)
{
  cMutexLock(mutexM);

  if (handleM && nameP && !isempty(nameP)) {
     cString url = cString::sprintf("%s/ready.sl?create_subfolder=true&folder=%s%s&ajax=true", baseUrlViihdeS, *Escape(nameP), (parentFolderIdP > 0) ? *cString::sprintf("&parent=%d", parentFolderIdP) : "");
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "CreateFolder")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               debug2("%s (%s, %d) Created new folder", __PRETTY_FUNCTION__, nameP, parentFolderIdP);
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::GetTimers(cElvisWidgetTimerCallbackIf &callbackP)
{
  cMutexLock(mutexM);

  if (handleM) {
     cString url = cString::sprintf("%s/recordings.sl?ajax=true", baseUrlViihdeS);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetTimers")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               json_error_t error;
               json_t *obj = json_loads(*dataM, 0, &error);
               if (obj) {
                  void *iter = json_object_iter(obj);
                  while (iter) {
                        const char *key = json_object_iter_key(iter);
                        json_t *value = json_object_iter_value(iter);
                        if (!strcmp(key, "recordings") && json_is_array(value)) {
                           for (unsigned int i = 0; i < json_array_size(value); i++) {
                               json_t *obj2 = json_array_get(value, i);
                               if (json_is_object(obj2)) {
                                  int program_id = 0, length = 0;
                                  cString name = "", channel = "", start_time = "", wild_card = "";
                                  json_t *obj3 = json_object_get(obj2, "program_id");
                                  if (json_is_string(obj3))
                                     program_id = (int)strtol(json_string_value(obj3), NULL, 10);
                                  obj3 = json_object_get(obj2, "length");
                                  if (json_is_string(obj3))
                                     length = (int)strtol(json_string_value(obj3), NULL, 10);
                                  obj3 = json_object_get(obj2, "name");
                                  if (json_is_string(obj3))
                                     name = Unescape(json_string_value(obj3));
                                  obj3 = json_object_get(obj2, "channel");
                                  if (json_is_string(obj3))
                                     channel = Unescape(json_string_value(obj3));
                                  obj3 = json_object_get(obj2, "start_time");
                                  if (json_is_string(obj3))
                                     start_time = Unescape(json_string_value(obj3));
                                  obj3 = json_object_get(obj2, "wild_card");
                                  if (json_is_string(obj3))
                                     wild_card = Unescape(json_string_value(obj3));
                                  debug2("%s program_id=%d length=%d name='%s' channel='%s' start_time='%s' wild_card='%s'", __PRETTY_FUNCTION__, program_id, length, *name, *channel, *start_time, *wild_card);
                                  callbackP.AddTimer(program_id, length, *name, *channel, *start_time, *wild_card);
                                  }
                               }
                           }
                        iter = json_object_iter_next(obj, iter);
                        }
                  json_decref(obj);
                  }
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::AddTimer(int programIdP, int folderIdP)
{
  cMutexLock(mutexM);

  if (handleM && (programIdP > 0)) {
     cString url = (folderIdP < 0) ? cString::sprintf("%s/program.sl?programid=%d&record=%d&ajax=true", baseUrlViihdeS, programIdP, programIdP) :
                                     cString::sprintf("%s/program.sl?programid=%d&record=%d&folderid=%d&ajax=true", baseUrlViihdeS, programIdP, programIdP, folderIdP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         // always login
         Login();
         if (Perform(*url, "AddTimer")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               continue;
               }
            else if (strstr(*dataM, "TRUE")) {
               debug2("%s (%d, %d) Added timer", __PRETTY_FUNCTION__, programIdP, folderIdP);
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::RemoveTimer(int idP)
{
  cMutexLock(mutexM);

  if (handleM && (idP > 0)) {
     cString url = cString::sprintf("%s/program.sl?remover=%d&ajax=true", baseUrlViihdeS, idP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         // always login
         Login();
         if (Perform(*url, "RemoveTimer")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               continue;
               }
            else {
               debug2("%s (%d) Removed timer", __PRETTY_FUNCTION__, idP);
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::GetSearchTimers(cElvisWidgetSearchTimerCallbackIf &callbackP)
{
  cMutexLock(mutexM);

  if (handleM) {
     cString url = cString::sprintf("%s/wildcards.sl?ajax=true", baseUrlViihdeS);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetSearchTimers")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               json_error_t error;
               json_t *obj = json_loads(*dataM, 0, &error);
               if (obj) {
                  void *iter = json_object_iter(obj);
                  while (iter) {
                        const char *key = json_object_iter_key(iter);
                        json_t *value = json_object_iter_value(iter);
                        if (!strcmp(key, "wildcardrecordings") && json_is_array(value)) {
                           for (unsigned int i = 0; i < json_array_size(value); i++) {
                               int recording_id = 0;
                               cString folder = "", added = "", wild_card_channel = "", wild_card = "";
                               json_t *obj2 = json_array_get(value, i);
                               json_t *obj3 = json_object_get(obj2, "recording_id");
                               if (json_is_string(obj3))
                                  recording_id = (int)strtol(json_string_value(obj3), NULL, 10);
                               obj3 = json_object_get(obj2, "folder");
                               if (json_is_string(obj3))
                                  folder = Unescape(json_string_value(obj3));
                               obj3 = json_object_get(obj2, "added");
                               if (json_is_string(obj3))
                                  added = Unescape(json_string_value(obj3));
                               obj3 = json_object_get(obj2, "wild_card_channel");
                               if (json_is_string(obj3))
                                  wild_card_channel = Unescape(json_string_value(obj3));
                               obj3 = json_object_get(obj2, "wild_card");
                               if (json_is_string(obj3))
                                  wild_card = Unescape(json_string_value(obj3));
                               debug2("%s recording_id=%d folder='%s' added='%s' wild_card_channel='%s' wild_card='%s'", __PRETTY_FUNCTION__, recording_id, *folder, *added, *wild_card_channel, *wild_card);
                               callbackP.AddSearchTimer(recording_id, *folder, *added, *wild_card_channel, *wild_card);
                               }
                           }
                        iter = json_object_iter_next(obj, iter);
                        }
                  json_decref(obj);
                  }
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::AddSearchTimer(const char *channelP, const char *wildcardP, int folderIdP, int wildcardIdP)
{
  cMutexLock(mutexM);

  if (handleM && channelP && wildcardP) {
     cString url = (wildcardIdP < 0) ? cString::sprintf("%s/wildcards.sl?channel=%s&folderid=%s&wildcard=%s&record=true&ajax=true", baseUrlViihdeS, *Escape(channelP),
                                                        (folderIdP < 0) ? "" : *cString::sprintf("%d", folderIdP), *Escape(wildcardP)) :
                                       cString::sprintf("%s/wildcards.sl?edit_wildcard=%d&channel=%s&folderid=%s&wildcard=%s&record=true&ajax=true", baseUrlViihdeS,
                                                        wildcardIdP, *Escape(channelP), (folderIdP < 0) ? "" : *cString::sprintf("%d", folderIdP), *Escape(wildcardP));
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "AddSearchTimer")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else if (strstr(*dataM, "TRUE")) {
               if (wildcardIdP < 0) {
                  debug2("%s (%s, %s, %d, %d) Added search timer", __PRETTY_FUNCTION__, channelP, wildcardP, folderIdP, wildcardIdP);
                  }
               else {
                  debug2("%s (%s, %s, %d, %d) Edited search timer", __PRETTY_FUNCTION__, channelP, wildcardP, folderIdP, wildcardIdP);
                  }
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::RemoveSearchTimer(int idP)
{
  cMutexLock(mutexM);

  if (handleM && (idP > 0)) {
     cString url = cString::sprintf("%s/wildcards.sl?remover=%d&ajax=true", baseUrlViihdeS, idP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "RemoveSearchTimer")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               debug2("%s (%d) Removed search timer", __PRETTY_FUNCTION__, idP);
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::GetChannels(cElvisWidgetChannelCallbackIf &callbackP)
{
  cMutexLock(mutexM);

  if (handleM) {
     cString url = cString::sprintf("%s/ajaxprograminfo.sl?channellist&ajax=true", baseUrlViihdeS);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetChannels")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               json_error_t error;
               json_t *obj = json_loads(*dataM, 0, &error);
               if (obj) {
                  void *iter = json_object_iter(obj);
                  while (iter) {
                        const char *key = json_object_iter_key(iter);
                        json_t *value = json_object_iter_value(iter);
                        if (!strcmp(key, "channels") && json_is_array(value)) {
                           for (unsigned int i = 0; i < json_array_size(value); i++) {
                               json_t *obj2 = json_array_get(value, i);
                               if (json_is_string(obj2)) {
                                  cString name = "", logo = "";
                                  json_t *obj3 = json_object_get(obj2, "name");
                                  if (json_is_string(obj3))
                                     name = Unescape(json_string_value(obj3));
                                  obj3 = json_object_get(obj2, "logo");
                                  if (json_is_string(obj3))
                                     logo = Unescape(json_string_value(obj3));
                                  debug2("%s channel='%s' logo='%s'", __PRETTY_FUNCTION__, *name, *logo);
                                  callbackP.AddChannel(*name, *logo);
                                  }
                               }
                           }
                        iter = json_object_iter_next(obj, iter);
                        }
                  json_decref(obj);
                  }
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::GetEvents(cElvisWidgetEventCallbackIf &callbackP, const char *channelP)
{
  cMutexLock(mutexM);

  if (handleM && channelP && !isempty(channelP)) {
     cString url = cString::sprintf("%s/ajaxprograminfo.sl?channel=%s&ajax=true", baseUrlViihdeS, *Escape(channelP));
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetEvents")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               json_error_t error;
               json_t *obj = json_loads(*dataM, 0, &error);
               if (obj) {
                  void *iter = json_object_iter(obj);
                  while (iter) {
                        const char *key = json_object_iter_key(iter);
                        json_t *value = json_object_iter_value(iter);
                        if (!strcmp(key, "channelname") && json_is_string(value)) {
                           cString channelname = Unescape(json_string_value(value));
                           if (strcmp(channelP, *channelname))
                              error("%s (, %s) Invalid channelname '%s'", __PRETTY_FUNCTION__, channelP, *channelname);
                           }
                        else if (!strcmp(key, "programs") && json_is_array(value)) {
                           for (unsigned int i = 0; i < json_array_size(value); i++) {
                               int id = 0;
                               cString name = "", simple_start_time = "", simple_end_time = "", start_time = "", end_time = "";
                               json_t *obj2 = json_array_get(value, i);
                               json_t *obj3 = json_object_get(obj2, "id");
                               if (json_is_string(obj3))
                                  id = (int)strtol(json_string_value(obj3), NULL, 10);
                               obj3 = json_object_get(obj2, "name");
                               if (json_is_string(obj3))
                                  name = Unescape(json_string_value(obj3));
                               obj3 = json_object_get(obj2, "simple_start_time");
                               if (json_is_string(obj3))
                                  simple_start_time = Unescape(json_string_value(obj3));
                               obj3 = json_object_get(obj2, "simple_end_time");
                               if (json_is_string(obj3))
                                  simple_end_time = Unescape(json_string_value(obj3));
                               obj3 = json_object_get(obj2, "start_time");
                               if (json_is_string(obj3))
                                  start_time = Unescape(json_string_value(obj3));
                               obj3 = json_object_get(obj2, "end_time");
                               if (json_is_string(obj3))
                                  end_time = Unescape(json_string_value(obj3));
                               debug2("%s (%s) id=%d name='%s' simple_start_time='%s' simple_end_time='%s' start_time='%s' end_time='%s'", __PRETTY_FUNCTION__, channelP, id, *name, *simple_start_time, *simple_end_time, *start_time, *end_time);
                               callbackP.AddEvent(id, *name, *simple_start_time, *simple_end_time, *start_time, *end_time);
                               }
                           }
                        iter = json_object_iter_next(obj, iter);
                        }
                  json_decref(obj);
                  }
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::GetEPG(cElvisWidgetEPGCallbackIf &callbackP)
{
  cMutexLock(mutexM);

  if (handleM) {
     cString url = cString::sprintf("%s/ajaxprograminfo.sl?ajax=true", baseUrlViihdeS);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetEPG")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               json_error_t error;
               json_t *obj = json_loads(*dataM, 0, &error);
               if (obj) {
                  void *iter = json_object_iter(obj);
                  while (iter) {
                        const char *key = json_object_iter_key(iter);
                        json_t *value = json_object_iter_value(iter);
                        if (!strcmp(key, "channels") && json_is_array(value)) {
                           for (unsigned int i = 0; i < json_array_size(value); i++) {
                               json_t *obj2 = json_array_get(value, i);
                               if (json_is_object(obj2)) {
                                  void *iter2 = json_object_iter(obj2);
                                  while (iter2) {
                                        cString channel = Unescape(json_object_iter_key(iter2));
                                        json_t *value2 = json_object_iter_value(iter2);
                                        if (json_is_array(value2)) {
                                           for (unsigned int j = 0; j < json_array_size(value2); j++) {
                                               int id = 0;
                                               cString name = "", simple_start_time = "", simple_end_time = "", start_time = "", end_time = "", short_text = "";
                                               json_t *obj3 = json_array_get(value2, j);
                                               json_t *obj4 = json_object_get(obj3, "id");
                                               if (json_is_string(obj4))
                                                  id = (int)strtol(json_string_value(obj4), NULL, 10);
                                               obj4 = json_object_get(obj3, "name");
                                               if (json_is_string(obj4))
                                                  name = Unescape(json_string_value(obj4));
                                               obj4 = json_object_get(obj3, "simple_start_time");
                                               if (json_is_string(obj4))
                                                  simple_start_time = Unescape(json_string_value(obj4));
                                               obj4 = json_object_get(obj3, "simple_end_time");
                                               if (json_is_string(obj4))
                                                  simple_end_time = Unescape(json_string_value(obj4));
                                               obj4 = json_object_get(obj3, "start_time");
                                               if (json_is_string(obj4))
                                                  start_time = Unescape(json_string_value(obj4));
                                               obj4 = json_object_get(obj3, "end_time");
                                               if (json_is_string(obj4))
                                                  end_time = Unescape(json_string_value(obj4));
                                               obj4 = json_object_get(obj3, "short_text");
                                               if (json_is_string(obj4))
                                                  short_text = Unescape(json_string_value(obj4));
                                               debug2("%s channel='%s' id=%d name='%s' simple_start_time='%s' simple_end_time='%s' start_time='%s' end_time='%s' short_text='%s'", __PRETTY_FUNCTION__, *channel, id, *name, *simple_start_time, *simple_end_time, *start_time, *end_time, *short_text);
                                               callbackP.AddEvent(*channel, id, *name, *simple_start_time, *simple_end_time, *start_time, *end_time, *short_text);
                                               }
                                           }
                                        iter2 = json_object_iter_next(obj2, iter2);
                                        }
                                  }
                               }
                           }
                        iter = json_object_iter_next(obj, iter);
                        }
                  json_decref(obj);
                  }
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::GetTopEvents(cElvisWidgetTopEventCallbackIf &callbackP)
{
  cMutexLock(mutexM);

  if (handleM) {
     cString url = cString::sprintf("%s/channels.sl?ajax=true", baseUrlViihdeS);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetEvents")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               json_error_t error;
               json_t *obj = json_loads(*dataM, 0, &error);
               if (obj) {
                  void *iter = json_object_iter(obj);
                  while (iter) {
                        const char *key = json_object_iter_key(iter);
                        json_t *value = json_object_iter_value(iter);
                        if (!strcmp(key, "programs") && json_is_array(value)) {
                           for (unsigned int i = 0; i < json_array_size(value); i++) {
                               json_t *obj2 = json_array_get(value, i);
                               if (json_is_object(obj2)) {
                                  int id = 0;
                                  cString name = "", channel = "", start_time = "", end_time = "";
                                  json_t *obj3 = json_object_get(obj2, "program_id");
                                  if (json_is_string(obj3))
                                     id = (int)strtol(json_string_value(obj3), NULL, 10);
                                  obj3 = json_object_get(obj2, "name");
                                  if (json_is_string(obj3))
                                     name = Unescape(json_string_value(obj3));
                                  obj3 = json_object_get(obj2, "channel");
                                  if (json_is_string(obj3))
                                     channel = Unescape(json_string_value(obj3));
                                  obj3 = json_object_get(obj2, "start_time");
                                  if (json_is_string(obj3))
                                     start_time = Unescape(json_string_value(obj3));
                                  obj3 = json_object_get(obj2, "end_time");
                                  if (json_is_string(obj3))
                                     end_time = Unescape(json_string_value(obj3));
                                  debug2("%s id=%d name='%s' channel='%s' start_time='%s' end_time='%s'", __PRETTY_FUNCTION__, id, *name, *channel, *start_time, *end_time);
                                  callbackP.AddEvent(id, *name, *channel, *start_time, *end_time);
                                  }
                               }
                           }
                        iter = json_object_iter_next(obj, iter);
                        }
                  json_decref(obj);
                  }
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::GetVOD(cElvisWidgetVODCallbackIf &callbackP, const char *categoryP, unsigned int countP)
{
  cMutexLock(mutexM);

  if (handleM) {
     cString url = !strcmp(categoryP, "favorites") ? cString::sprintf("%s/vod.sl?data=true&favorites=true&loadfavorites&ajax=true", baseUrlViihdeS) :
                                                     cString::sprintf("%s/vod.sl?data=true&category=%s&count=%d&ajax=true", baseUrlViihdeS, *Escape(categoryP), countP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetVOD")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               json_error_t error;
               json_t *obj = json_loads(*dataM, 0, &error);
               if (obj) {
                  void *iter = json_object_iter(obj);
                  while (iter) {
                        const char *key = json_object_iter_key(iter);
                        json_t *value = json_object_iter_value(iter);
                        if (!strcmp(key, "vods") && json_is_array(value)) {
                           for (unsigned int i = 0; i < json_array_size(value); i++) {
                               int id = 0, length = 0, agelimit = 0, year = 0, price = 0;
                               cString title = "", currency = "", cover = "", trailer = "";
                               json_t *obj2 = json_array_get(value, i);
                               json_t *obj3 = json_object_get(obj2, "id");
                               if (json_is_string(obj3))
                                  id = (int)strtol(json_string_value(obj3), NULL, 10);
                               obj3 = json_object_get(obj2, "length");
                               if (json_is_string(obj3))
                                  length = (int)strtol(json_string_value(obj3), NULL, 10);
                               obj3 = json_object_get(obj2, "agelimit");
                               if (json_is_string(obj3))
                                  agelimit = (int)strtol(json_string_value(obj3), NULL, 10);
                               obj3 = json_object_get(obj2, "year");
                               if (json_is_string(obj3))
                                  year = (int)strtol(json_string_value(obj3), NULL, 10);
                               obj3 = json_object_get(obj2, "price");
                               if (json_is_string(obj3))
                                  price = (int)strtol(json_string_value(obj3), NULL, 10);
                               obj3 = json_object_get(obj2, "title");
                               if (json_is_string(obj3))
                                  title = Unescape(json_string_value(obj3));
                               obj3 = json_object_get(obj2, "currency");
                               if (json_is_string(obj3))
                                  currency = Unescape(json_string_value(obj3));
                               obj3 = json_object_get(obj2, "cover");
                               if (json_is_string(obj3))
                                  cover = Unescape(json_string_value(obj3));
                               obj3 = json_object_get(obj2, "trailer");
                               if (json_is_string(obj3))
                                  trailer = Unescape(json_string_value(obj3));
                               debug2("%s (%s, %u): id=%d length=%d agelimit=%d year=%d price=%d title='%s' currency='%s' cover='%s' trailer='%s'", __PRETTY_FUNCTION__, categoryP, countP, id, length, agelimit, year, price, *title, *currency, *cover, *trailer);
                               callbackP.AddVOD(id, length, agelimit, year, price, *title, *currency, *cover, *trailer);
                               }
                           }
                        iter = json_object_iter_next(obj, iter);
                        }
                  json_decref(obj);
                  }
               return true;
               }
            }
         }
     }

  return false;
}

cElvisWidgetEventInfo *cElvisWidget::GetEventInfo(int idP)
{
  cMutexLock(mutexM);
  if (handleM && (idP > 0)) {
     cString url = cString::sprintf("%s/program.sl?programid=%d&ajax=true", baseUrlViihdeS, idP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetEventInfo")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               json_error_t error;
               json_t *obj = json_loads(*dataM, 0, &error);
               if (obj) {
                  bool has_started = false, has_ended = false, recorded = false, ready = false, is_wildcard = false, scrambled_channel = false;
                  int id = 0, length = 0, programviewid = 0, recordingid = 0;
                  cString name = "", channel = "", short_text = "", description = "", flength = "", tn = "", start_time = "", end_time = "", url = "";
                  json_t *obj2 = json_object_get(obj, "id");
                  if (json_is_string(obj2))
                     id = (int)strtol(json_string_value(obj2), NULL, 10);
                  obj2 = json_object_get(obj, "length");
                  if (json_is_string(obj2))
                     length = (int)strtol(json_string_value(obj2), NULL, 10);
                  obj2 = json_object_get(obj, "programviewid");
                  if (json_is_string(obj2))
                     programviewid = (int)strtol(json_string_value(obj2), NULL, 10);
                  obj2 = json_object_get(obj, "recordingid");
                  if (json_is_string(obj2))
                     recordingid = (int)strtol(json_string_value(obj2), NULL, 10);
                  obj2 = json_object_get(obj, "name");
                  if (json_is_string(obj2))
                     name = Unescape(json_string_value(obj2));
                  obj2 = json_object_get(obj, "channel");
                  if (json_is_string(obj2))
                     channel = Unescape(json_string_value(obj2));
                  obj2 = json_object_get(obj, "short_text");
                  if (json_is_string(obj2))
                     short_text = Unescape(json_string_value(obj2));
                  obj2 = json_object_get(obj, "description");
                  if (json_is_string(obj2))
                     description = Unescape(json_string_value(obj2));
                  obj2 = json_object_get(obj, "flength");
                  if (json_is_string(obj2))
                     flength = Unescape(json_string_value(obj2));
                  obj2 = json_object_get(obj, "tn");
                  if (json_is_string(obj2))
                     tn = Unescape(json_string_value(obj2));
                  obj2 = json_object_get(obj, "start_time");
                  if (json_is_string(obj2))
                     start_time = Unescape(json_string_value(obj2));
                  obj2 = json_object_get(obj, "end_time");
                  if (json_is_string(obj2))
                     end_time = Unescape(json_string_value(obj2));
                  obj2 = json_object_get(obj, "url");
                  if (json_is_string(obj2))
                     url = Unescape(json_string_value(obj2));
                  obj2 = json_object_get(obj, "has_started");
                  if (json_is_string(obj2) && !strcasecmp(json_string_value(obj2), "true"))
                     has_started = true;
                  obj2 = json_object_get(obj, "has_ended");
                  if (json_is_string(obj2) && !strcasecmp(json_string_value(obj2), "true"))
                     has_ended = true;
                  obj2 = json_object_get(obj, "recorded");
                  if (json_is_string(obj2) && !strcasecmp(json_string_value(obj2), "true"))
                     recorded = true;
                  obj2 = json_object_get(obj, "ready");
                  if (json_is_string(obj2) && !strcasecmp(json_string_value(obj2), "true"))
                     ready = true;
                  obj2 = json_object_get(obj, "is_wildcard");
                  if (json_is_string(obj2) && !strcasecmp(json_string_value(obj2), "true"))
                     is_wildcard = true;
                  obj2 = json_object_get(obj, "scrambled_channel");
                  if (json_is_string(obj2) && !strcasecmp(json_string_value(obj2), "true"))
                     scrambled_channel = true;
                  debug2("%s (%d) id=%d name='%s' channel='%s' short_text='%s' description='%s' length=%d flength='%s' tn='%s' start_time='%s' end_time='%s' url='%s' "
                        "programviewid=%d recordingid=%d has_started=%d has_ended=%d recorded=%d ready=%d is_wildcard=%d scrambled_channel=%d", __PRETTY_FUNCTION__, idP,
                        id, *name, *channel, *short_text, *description, length, *flength, *tn, *start_time, *end_time, *url, programviewid, recordingid, has_started,
                        has_ended, recorded, ready, is_wildcard, scrambled_channel);
                  json_decref(obj);
                  return new cElvisWidgetEventInfo(id, *name, *channel, *short_text, *description, length, *flength, *tn, *start_time, *end_time, *url, programviewid,
                                               recordingid, has_started, has_ended, recorded, ready, is_wildcard, scrambled_channel);
                  }
               }
            }
         }
     }

  return NULL;
}

cElvisWidgetVODInfo *cElvisWidget::GetVODInfo(int idP)
{
  cMutexLock(mutexM);
  if (handleM && (idP > 0)) {
     cString url = cString::sprintf("%s/vod.sl?data=true&vod=%d&ajax=true", baseUrlViihdeS, idP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetVODInfo")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               json_error_t error;
               json_t *obj = json_loads(*dataM, 0, &error);
               if (obj) {
                  int id = 0, length = 0, agelimit = 0, year = 0, price = 0;
                  cString title = "", original_title = "", currency = "", short_desc = "", info = "", info2 = "", trailer_url = "", categories = "";
                  json_t *obj2 = json_object_get(obj, "id");
                  if (json_is_string(obj2))
                     id = (int)strtol(json_string_value(obj2), NULL, 10);
                  obj2 = json_object_get(obj, "length");
                  if (json_is_string(obj2))
                     length = (int)strtol(json_string_value(obj2), NULL, 10);
                  obj2 = json_object_get(obj, "agelimit");
                  if (json_is_string(obj2))
                     agelimit = (int)strtol(json_string_value(obj2), NULL, 10);
                  obj2 = json_object_get(obj, "year");
                  if (json_is_string(obj2))
                     year = (int)strtol(json_string_value(obj2), NULL, 10);
                  obj2 = json_object_get(obj, "price");
                  if (json_is_string(obj2))
                     price = (int)strtol(json_string_value(obj2), NULL, 10);
                  obj2 = json_object_get(obj, "title");
                  if (json_is_string(obj2))
                     title = Unescape(json_string_value(obj2));
                  obj2 = json_object_get(obj, "original_title");
                  if (json_is_string(obj2))
                     original_title = Unescape(json_string_value(obj2));
                  obj2 = json_object_get(obj, "currency");
                  if (json_is_string(obj2))
                     currency = Unescape(json_string_value(obj2));
                  obj2 = json_object_get(obj, "short_desc");
                  if (json_is_string(obj2))
                     short_desc = Unescape(json_string_value(obj2));
                  obj2 = json_object_get(obj, "info");
                  if (json_is_string(obj2))
                     info = Unescape(strstrip(json_string_value(obj2), "\r"));
                  obj2 = json_object_get(obj, "info2");
                  if (json_is_string(obj2))
                     info2 = Unescape(json_string_value(obj2));
                  obj2 = json_object_get(obj, "trailer_url");
                  if (json_is_string(obj2))
                     trailer_url = Unescape(json_string_value(obj2));
                  obj2 = json_object_get(obj, "categories");
                  if (json_is_array(obj2)) {
                     for (unsigned int i = 0; i < json_array_size(obj2); i++) {
                         json_t *obj3 = json_array_get(obj2, i);
                         if (json_is_object(obj3)) {
                            void *iter = json_object_iter(obj3);
                            while (iter) {
                                  const char *key = json_object_iter_key(iter);
                                  json_t *value = json_object_iter_value(iter);
                                  if (!strcmp(key, "cat") && json_is_string(value))
                                     categories = cString::sprintf("%s%s%s", *categories, isempty(*categories) ? "" : ", ", *Unescape(json_string_value(value)));
                                  iter = json_object_iter_next(obj3, iter);
                                  }
                            }
                         }
                     }
                  debug2("%s (%d) id=%d length=%d agelimit=%d year=%d price=%d title='%s' original_title='%s' currency='%s' short_desc='%s' info='%s' info2='%s' "
                        "trailer_url='%s' categories='%s'", __PRETTY_FUNCTION__, idP, id, length, agelimit, year, price, *title, *original_title, *currency,
                        *short_desc, *info, *info2, *trailer_url, *categories);
                  json_decref(obj);
                  return new cElvisWidgetVODInfo(id, length, agelimit, year, price, *title, *original_title, *currency, *short_desc, *info, *info2, *trailer_url, *categories);
                  }
               }
            }
         }
     }

  return NULL;
}

bool cElvisWidget::SearchVOD(cElvisWidgetVODCallbackIf &callbackP, const char *titleP, const char *descP, bool hdP)
{
  cMutexLock(mutexM);
  cString term = cString::sprintf("&format=%s", hdP ? "HD" : "NONHD");

  if (titleP && !isempty(titleP))
     term = cString::sprintf("%s&title=%s", *term, *Escape(titleP));
  if (descP && !isempty(descP))
     term = cString::sprintf("%s&description=%s", *term, *Escape(descP));
  if (handleM && !isempty(*term)) {
     cString url = cString::sprintf("%s/vod.sl?data=true&category=all&advancedsearch=true&ajax=true%s", baseUrlViihdeS, *term);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "SearchVOD")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else {
               json_error_t error;
               json_t *obj = json_loads(*dataM, 0, &error);
               if (obj) {
                  void *iter = json_object_iter(obj);
                  while (iter) {
                        const char *key = json_object_iter_key(iter);
                        json_t *value = json_object_iter_value(iter);
                        if (!strcmp(key, "vods") && json_is_array(value)) {
                           for (unsigned int i = 0; i < json_array_size(value); i++) {
                               int id = 0, length = 0, agelimit = 0, year = 0, price = 0;
                               cString title = "", currency = "", cover = "", trailer = "";
                               json_t *obj2 = json_array_get(value, i);
                               json_t *obj3 = json_object_get(obj2, "id");
                               if (json_is_string(obj3))
                                  id = (int)strtol(json_string_value(obj3), NULL, 10);
                               obj3 = json_object_get(obj2, "length");
                               if (json_is_string(obj3))
                                  length = (int)strtol(json_string_value(obj3), NULL, 10);
                               obj3 = json_object_get(obj2, "agelimit");
                               if (json_is_string(obj3))
                                  agelimit = (int)strtol(json_string_value(obj3), NULL, 10);
                               obj3 = json_object_get(obj2, "year");
                               if (json_is_string(obj3))
                                  year = (int)strtol(json_string_value(obj3), NULL, 10);
                               obj3 = json_object_get(obj2, "price");
                               if (json_is_string(obj3))
                                  price = (int)strtol(json_string_value(obj3), NULL, 10);
                               obj3 = json_object_get(obj2, "title");
                               if (json_is_string(obj3))
                                  title = Unescape(json_string_value(obj3));
                               obj3 = json_object_get(obj2, "currency");
                               if (json_is_string(obj3))
                                  currency = Unescape(json_string_value(obj3));
                               obj3 = json_object_get(obj2, "cover");
                               if (json_is_string(obj3))
                                  cover = Unescape(json_string_value(obj3));
                               obj3 = json_object_get(obj2, "trailer");
                               if (json_is_string(obj3))
                                  trailer = Unescape(json_string_value(obj3));
                               debug2("%s (, %s, %s, %d) id=%d length=%d agelimit=%d year=%d price=%d title='%s' currency='%s' cover='%s' trailer='%s'",
                                      __PRETTY_FUNCTION__, titleP, descP, hdP,
                                      id, length, agelimit, year, price, *title, *currency, *cover, *trailer);
                               callbackP.AddVOD(id, length, agelimit, year, price, *title, *currency, *cover, *trailer);
                               }
                           }
                        iter = json_object_iter_next(obj, iter);
                        }
                  json_decref(obj);
                  }
               return true;
               }
            }
         }
     }

  return false;
}

bool cElvisWidget::SetVODFavorite(int idP, bool onOffP)
{
  cMutexLock(mutexM);

  if (handleM && (idP > 0)) {
     cString url = cString::sprintf("%s/vod.sl?action=true&%sfavorite=%d&ajax=true", baseUrlViihdeS, onOffP ? "add" : "remove", idP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "SetVODFavorite")) {
            if (IsLoginRequired(*dataM)) {
               info("%s Relogin...", __PRETTY_FUNCTION__);
               Login();
               continue;
               }
            else if (strstr(*dataM, "OK")) {
               return true;
               }
            }
         }
     }

  return false;
}
