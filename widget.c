/*
 * widget.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <string.h>

#include "common.h"
#include "widget.h"

#undef USE_COOKIE_JAR

// --- cElvisWidgetEventInfo ------------------------------------------------

cElvisWidgetEventInfo::cElvisWidgetEventInfo(int idP, const char *nameP, const char *channelP, const char *shortTextP, const char *descriptionP, int lengthP, const char *fLengthP,
                                     const char *thumbnailP, const char *startTimeP, const char *endTimeP, const char *urlP, int programViewIdP, int recordingIdP,
                                     bool hasStartedP, bool hasEndedP, bool isRecordedP, bool isReadyP, bool isWildcardP)
: idM(idP),
  lengthM(lengthP),
  programViewIdM(programViewIdP),
  recordingIdM(recordingIdP),
  hasStartedM(hasStartedP),
  hasEndedM(hasEndedP),
  isRecordedM(isRecordedP),
  isReadyM(isReadyP),
  isWildcardM(isWildcardP),
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
const char* cElvisWidget::baseUrlViihdeS = "http://elisaviihde.fi/etvrecorder";
const char* cElvisWidget::baseUrlViihdeSslS = "https://elisaviihde.fi/etvrecorder";
const char* cElvisWidget::baseUrlVisioS = "http://www.saunavisio.fi/tvrecorder";
const char* cElvisWidget::baseUrlVisioSslS = "https://www.saunavisio.fi/tvrecorder";

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

size_t cElvisWidget::WriteCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cElvisWidget *obj = (cElvisWidget *)dataP;
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
  debug("cElvisWidget::Perform(%s)", msgP ? msgP : "unknown");

  if (handleM) {
     CURLcode err;
     long http_code = 0;

     // reset data
     dataM = "";

     curl_easy_setopt(handleM, CURLOPT_URL, urlP);
     err = curl_easy_perform(handleM);
     if (err != CURLE_OK) {
        error("cElvisWidget::Perform(%s): %s", msgP ? msgP : "unknown", curl_easy_strerror(err));
        return false;
        }

     err = curl_easy_getinfo(handleM, CURLINFO_RESPONSE_CODE, &http_code);
     if (err != CURLE_OK) {
        error("cElvisWidget::Perform(%s): getinfo (%s)", msgP ? msgP : "unknown", curl_easy_strerror(err));
        return false;
        }

     if (http_code != 200) {
        error("cElvisWidget::Perform(%s): invalid HTTP Code (%ld)", msgP ? msgP : "unknown", http_code);
        return false;
        }

     if (isempty(*dataM)) {
        debug("cElvisWidget::Perform(%s): empty data", msgP ? msgP : "unknown");
        return false;
        }
     }

  return true;
}

bool cElvisWidget::Login()
{
  if (isempty(ElvisConfig.Username) || isempty(ElvisConfig.Password)) {
     error("cElvisWidget::Login(): invalid credentials");
     return false;
     }

  if (!IsLogged() && handleM) {
#ifdef USE_COOKIE_JAR
     cString url = cString::sprintf("%s/login.sl?username=%s&password=%s&savelogin=true&ajax=true", GetBase(), ElvisConfig.Username, ElvisConfig.Password);
#else
     cString url = cString::sprintf("%s/login.sl?username=%s&password=%s&ajax=true", GetBase(), ElvisConfig.Username, ElvisConfig.Password);
#endif

     if (Perform(*url, "Login"))
        return strstr(*dataM, "TRUE");
     }

  return false;
}

bool cElvisWidget::Logout()
{
  if (IsLogged() && handleM) {
     cString url = cString::sprintf("%s/logout.sl?ajax=true", GetBase());

     return Perform(*url, "Logout");
     }

  return false;
}

bool cElvisWidget::IsLogged()
{
  if (handleM) {
     cString url = cString::sprintf("%s/login.sl?islogged", GetBase());

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
  if (ElvisConfig.Service == 0) {
     if (strstr(stringP, "evlogin"))
        return true;
     }
  else {
     if (strstr(stringP, "svlogin"))
        return true;
     }

  return false;
}

bool cElvisWidget::Load(const char *directoryP)
{
  // initialize the curl session
  if (!handleM)
     handleM = curl_easy_init();

  if (handleM) {
#ifdef DEBUG
     // verbose output
     curl_easy_setopt(handleM, CURLOPT_VERBOSE, 1L);
#endif
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

bool cElvisWidget::GetRecordings(cElvisWidgetRecordingCallbackIf &callbackP, int folderIdP)
{
  cMutexLock(mutexM);

  if (handleM) {
     cString url = (folderIdP < 0) ? cString::sprintf("%s/ready.sl?ajax=true&clear=true", GetBase()) :
                                     cString::sprintf("%s/ready.sl?folderid=%d&ajax=true&clear=true", GetBase(), folderIdP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetRecordings")) {
            if (IsLoginRequired(*dataM)) {
               info("cElvisWidget::GetRecordings(): relogin...");
               Login();
               continue;
               }
            else {
               json_object_iter it;
               json_object *json = json_tokener_parse(*dataM);
               if (!is_error(json)) {
                  json_object_object_foreachC(json, it) {
                    if (!strcmp(it.key, "ready_data")) {
                       for (int i = 0; i < json_object_array_length(it.val); ++i) {
                           json_object_iter it2;
                           json_object *json2 = json_object_array_get_idx(it.val, i);
                           json_object_object_foreachC(json2, it2) {
                             if (!strcmp(it2.key, "folders")) {
                                for (int j = 0; j < json_object_array_length(it2.val); ++j) {
                                    json_object_iter it3;
                                    json_object *json3 = json_object_array_get_idx(it2.val, j);
                                    cString name = "", size = "";
                                    int id = 0, count = 0;
                                    json_object_object_foreachC(json3, it3) {
                                      if (!strcmp(it3.key, "id"))
                                         id = json_object_get_int(it3.val);
                                      else if (!strcmp(it3.key, "name"))
                                         name = Unescape(json_object_get_string(it3.val));
                                      else if (!strcmp(it3.key, "size"))
                                         size = Unescape(json_object_get_string(it3.val));
                                      else if (!strcmp(it3.key, "recordings_count"))
                                         count = json_object_get_int(it3.val);
                                      }
                                    debug("id: %d count: %d name: '%s' size: '%s'", id, count, *name, *size);
                                    callbackP.AddFolder(id, count, *name, *size);
                                    }
                                }
                             else if (!strcmp(it2.key, "recordings")) {
                                for (int j = 0; j < json_object_array_length(it2.val); ++j) {
                                    json_object_iter it3;
                                    json_object *json3 = json_object_array_get_idx(it2.val, j);
                                    cString name = "", channel = "", start_time = "", timestamp = "";
                                    int id = 0, program_id = 0, folder_id = 0, count = 0, length = 0;
                                    json_object_object_foreachC(json3, it3) {
                                      if (!strcmp(it3.key, "id"))
                                         id = json_object_get_int(it3.val);
                                      else if (!strcmp(it3.key, "program_id"))
                                         program_id = json_object_get_int(it3.val);
                                      else if (!strcmp(it3.key, "folder_id"))
                                         folder_id = json_object_get_int(it3.val);
                                      else if (!strcmp(it3.key, "name"))
                                         name = Unescape(json_object_get_string(it3.val));
                                      else if (!strcmp(it3.key, "channel"))
                                         channel = Unescape(json_object_get_string(it3.val));
                                      else if (!strcmp(it3.key, "start_time"))
                                         start_time = Unescape(json_object_get_string(it3.val));
                                      else if (!strcmp(it3.key, "timestamp"))
                                         timestamp = Unescape(json_object_get_string(it3.val));
                                      else if (!strcmp(it3.key, "viewcount"))
                                         count = json_object_get_int(it3.val);
                                      else if (!strcmp(it3.key, "length"))
                                         length = json_object_get_int(it3.val);
                                      }
                                    debug("id: %d program_id: %d folder_id: %d count: %d length: %d name: '%s' channel: '%s' start_time: '%s'", id, program_id, folder_id, count, length, *name, *channel, *start_time);
                                    callbackP.AddRecording(id, program_id, folder_id, count, length, *name, *channel, *start_time);
                                    }
                                }
                             }
                           }
                       }
                    }
                  json_object_put(json);
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
     cString url = cString::sprintf("%s/program.sl?remove=true&removep=%d&ajax=true", GetBase(), idP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "RemoveRecording")) {
            if (IsLoginRequired(*dataM)) {
               info("cElvisWidget::RemoveRecording(): relogin...");
               Login();
               continue;
               }
            else {
               debug("added recording id: %d", idP);
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
     cString url = cString::sprintf("%s/recordings.sl?ajax=true", GetBase());
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetTimers")) {
            if (IsLoginRequired(*dataM)) {
               info("cElvisWidget::GetTimers(): relogin...");
               Login();
               continue;
               }
            else {
               json_object_iter it;
               json_object *json = json_tokener_parse(*dataM);
               if (!is_error(json)) {
                  json_object_object_foreachC(json, it) {
                    if (!strcmp(it.key, "recordings")) {
                       for (int i = 0; i < json_object_array_length(it.val); ++i) {
                           json_object_iter it2;
                           json_object *json2 = json_object_array_get_idx(it.val, i);
                           int id = 0, program_id = 0, length = 0;
                           cString name = "", channel = "", start_time = "", wild_card = "";
                           json_object_object_foreachC(json2, it2) {
                             if (!strcmp(it2.key, "id"))
                                id = json_object_get_int(it2.val);
                             else if (!strcmp(it2.key, "program_id"))
                                program_id = json_object_get_int(it2.val);
                             else if (!strcmp(it2.key, "length"))
                                length = json_object_get_int(it2.val);
                             else if (!strcmp(it2.key, "name"))
                                name = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "channel"))
                                channel = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "start_time"))
                                start_time = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "wild_card"))
                                wild_card = Unescape(json_object_get_string(it2.val));
                             }
                           debug("program_id: %d length: %d name: '%s' channel: '%s' start_time: '%s' wild_card: '%s'", program_id, length, *name, *channel, *start_time, *wild_card);
                           callbackP.AddTimer(program_id, length, *name, *channel, *start_time, *wild_card);
                           }
                       }
                    }
                  json_object_put(json);
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
     cString url = (folderIdP < 0) ? cString::sprintf("%s/program.sl?programid=%d&record=%d&ajax=true", GetBase(), programIdP, programIdP) :
                                     cString::sprintf("%s/program.sl?programid=%d&record=%d&folderid=%d&ajax=true", GetBase(), programIdP, programIdP, folderIdP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "AddTimer")) {
            if (IsLoginRequired(*dataM)) {
               info("cElvisWidget::AddTimer(): relogin...");
               Login();
               continue;
               }
            else if (strstr(*dataM, "TRUE")) {
               debug("added timer id: %d", programIdP);
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
     cString url = cString::sprintf("%s/program.sl?remover=%d&ajax=true", GetBase(), idP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "RemoveTimer")) {
            if (IsLoginRequired(*dataM)) {
               info("cElvisWidget::RemoveTimer(): relogin...");
               Login();
               continue;
               }
            else {
               debug("removed timer id: %d", idP);
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
     cString url = cString::sprintf("%s/wildcards.sl?ajax=true", GetBase());
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetSearchTimers")) {
            if (IsLoginRequired(*dataM)) {
               info("cElvisWidget::GetSearchTimers(): relogin...");
               Login();
               continue;
               }
            else {
               json_object_iter it;
               json_object *json = json_tokener_parse(*dataM);
               if (!is_error(json)) {
                  json_object_object_foreachC(json, it) {
                    if (!strcmp(it.key, "wildcardrecordings")) {
                       for (int i = 0; i < json_object_array_length(it.val); ++i){
                           json_object_iter it2;
                           json_object *json2 = json_object_array_get_idx(it.val, i);
                           int recording_id = 0;
                           cString folder = "", added = "", wild_card_channel = "", wild_card = "";
                           json_object_object_foreachC(json2, it2) {
                             if (!strcmp(it2.key, "recording_id"))
                                recording_id = json_object_get_int(it2.val);
                             else if (!strcmp(it2.key, "folder"))
                                folder = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "added"))
                                added = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "wild_card_channel"))
                                wild_card_channel = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "wild_card"))
                                wild_card = Unescape(json_object_get_string(it2.val));
                             }
                           debug("recording_id: %d folder: '%s' added: '%s' wild_card_channel: '%s' wild_card: '%s'", recording_id, *folder, *added, *wild_card_channel, *wild_card);
                           callbackP.AddSearchTimer(recording_id, *folder, *added, *wild_card_channel, *wild_card);
                           }
                       }
                    }
                  json_object_put(json);
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
     cString url = (wildcardIdP < 0) ? cString::sprintf("%s/wildcards.sl?channel=%s&folderid=%s&wildcard=%s&record=true&ajax=true", GetBase(), *Escape(channelP),
                                                        (folderIdP < 0) ? "" : *cString::sprintf("%d", folderIdP), *Escape(wildcardP)) :
                                       cString::sprintf("%s/wildcards.sl?edit_wildcard=%d&channel=%s&folderid=%s&wildcard=%s&record=true&ajax=true", GetBase(),
                                                        wildcardIdP, *Escape(channelP), (folderIdP < 0) ? "" : *cString::sprintf("%d", folderIdP), *Escape(wildcardP));
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "AddSearchTimer")) {
            if (IsLoginRequired(*dataM)) {
               info("cElvisWidget::AddSearchTimer(): relogin...");
               Login();
               continue;
               }
            else if (strstr(*dataM, "TRUE")) {
               if (wildcardIdP < 0) {
                  debug("added search timer: %s (folder:%d channel:%s)", wildcardP, folderIdP, channelP);
                  }
               else {
                  debug("edited search timer: %s (id: %d folder:%d channel:%s)", wildcardP, wildcardIdP, folderIdP, channelP);
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
     cString url = cString::sprintf("%s/wildcards.sl?remover=%d&ajax=true", GetBase(), idP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "RemoveSearchTimer")) {
            if (IsLoginRequired(*dataM)) {
               info("cElvisWidget::RemoveSearchTimer(): relogin...");
               Login();
               continue;
               }
            else {
               debug("removed search timer id: %d", idP);
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
     cString url = cString::sprintf("%s/ajaxprograminfo.sl?channel%s&ajax=true", GetBase(), (ElvisConfig.Service == 0) ? "list" : "s");
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetChannels")) {
            if (IsLoginRequired(*dataM)) {
               info("cElvisWidget::GetChannels(): relogin...");
               Login();
               continue;
               }
            else {
               json_object_iter it;
               json_object *json = json_tokener_parse(*dataM);
               if (!is_error(json)) {
                  json_object_object_foreachC(json, it) {
                    if (!strcmp(it.key, "channels")) {
                       for (int i = 0; i < json_object_array_length(it.val); ++i) {
                           json_object *json2 = json_object_array_get_idx(it.val, i);
                           if (ElvisConfig.Service == 0) {
                              json_object_iter it2;
                              cString name = "", logo = "";
                              json_object_object_foreachC(json2, it2) {
                                if (!strcmp(it2.key, "name"))
                                   name = Unescape(json_object_get_string(it2.val));
                                else if (!strcmp(it2.key, "logo"))
                                   logo = Unescape(json_object_get_string(it2.val));
                                }
                              debug("channel: '%s' Logo: '%s'", *name, *logo);
                              callbackP.AddChannel(*name, *logo);
                              }
                           else {
                              cString name = json_object_get_string(json2);
                              debug("channel: '%s'", *name);
                              callbackP.AddChannel(*name, "");
                              }
                           }
                       }
                    }
                  json_object_put(json);
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
     cString url = cString::sprintf("%s/ajaxprograminfo.sl?channel=%s&ajax=true", GetBase(), *Escape(channelP));
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetEvents")) {
            if (IsLoginRequired(*dataM)) {
               info("cElvisWidget::GetEvents(): relogin...");
               Login();
               continue;
               }
            else {
               json_object_iter it;
               json_object *json = json_tokener_parse(*dataM);
               if (!is_error(json)) {
                  json_object_object_foreachC(json, it) {
                    if (!strcmp(it.key, "channelname")) {
                       cString channelname = Unescape(json_object_get_string(it.val));
                       if (strcmp(channelP, *channelname))
                          error("cElvisWidget::GetEvents(): invalid channel: '%s' <> '%s'", channelP, *channelname);
                       }
                    else if (!strcmp(it.key, "programs")) {
                       for (int i = 0; i < json_object_array_length(it.val); ++i) {
                           json_object_iter it2;
                           json_object *json2 = json_object_array_get_idx(it.val, i);
                           int id = 0;
                           cString name = "", simple_start_time = "", simple_end_time = "", start_time = "", end_time = "";
                           json_object_object_foreachC(json2, it2) {
                             if (!strcmp(it2.key, "id"))
                                id = json_object_get_int(it2.val);
                             else if (!strcmp(it2.key, "name"))
                                name = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "simple_start_time"))
                                simple_start_time = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "simple_end_time"))
                                simple_end_time = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "start_time"))
                                start_time = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "end_time"))
                                end_time = Unescape(json_object_get_string(it2.val));
                             }
                           debug("id: %d name: '%s' simple_start_time: '%s' simple_end_time: '%s' start_time: '%s' end_time: '%s'", id, *name, *simple_start_time, *simple_end_time, *start_time, *end_time);
                           callbackP.AddEvent(id, *name, *simple_start_time, *simple_end_time, *start_time, *end_time);
                           }
                       }
                    }
                  json_object_put(json);
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
     cString url = cString::sprintf("%s/ajaxprograminfo.sl?ajax=true", GetBase());
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetEPG")) {
            if (IsLoginRequired(*dataM)) {
               info("cElvisWidget::GetEPG(): relogin...");
               Login();
               continue;
               }
            else {
               json_object_iter it;
               json_object *json = json_tokener_parse(*dataM);
               if (!is_error(json)) {
                  json_object_object_foreachC(json, it) {
                    if (!strcmp(it.key, "channels")) {
                       for (int i = 0; i < json_object_array_length(it.val); ++i) {
                           json_object_iter it2;
                           json_object *json2 = json_object_array_get_idx(it.val, i);
                           json_object_object_foreachC(json2, it2) {
                             cString channel = Unescape(it2.key);
                             for (int j = 0; j < json_object_array_length(it2.val); ++j) {
                                 json_object_iter it3;
                                 json_object *json3 = json_object_array_get_idx(it2.val, j);
                                 int id = 0;
                                 cString name = "", simple_start_time = "", simple_end_time = "", start_time = "", end_time = "", short_text = "";
                                 json_object_object_foreachC(json3, it3) {
                                   if (!strcmp(it3.key, "id"))
                                      id = json_object_get_int(it3.val);
                                   else if (!strcmp(it3.key, "name"))
                                      name = Unescape(json_object_get_string(it3.val));
                                   else if (!strcmp(it3.key, "simple_start_time"))
                                      simple_start_time = Unescape(json_object_get_string(it3.val));
                                   else if (!strcmp(it3.key, "simple_end_time"))
                                      simple_end_time = Unescape(json_object_get_string(it3.val));
                                   else if (!strcmp(it3.key, "start_time"))
                                      start_time = Unescape(json_object_get_string(it3.val));
                                   else if (!strcmp(it3.key, "end_time"))
                                      end_time = Unescape(json_object_get_string(it3.val));
                                   else if (!strcmp(it3.key, "short_text"))
                                      short_text = Unescape(json_object_get_string(it3.val));
                                   }
                                 debug("channel: '%s' id: %d name: '%s' simple_start_time: '%s' simple_end_time: '%s' start_time: '%s' end_time: '%s' short_text: '%s'", *channel, id, *name, *simple_start_time, *simple_end_time, *start_time, *end_time, *short_text);
                                 callbackP.AddEvent(*channel, id, *name, *simple_start_time, *simple_end_time, *start_time, *end_time, *short_text);
                                 }
                             }
                           }
                       }
                    }
                  json_object_put(json);
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
     cString url = cString::sprintf("%s/channels.sl?ajax=true", GetBase());
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetEvents")) {
            if (IsLoginRequired(*dataM)) {
               info("cElvisWidget::GetTopEvents(): relogin...");
               Login();
               continue;
               }
            else {
               json_object_iter it;
               json_object *json = json_tokener_parse(*dataM);
               if (!is_error(json)) {
                  json_object_object_foreachC(json, it) {
                    if (!strcmp(it.key, "programs")) {
                       for (int i = 0; i < json_object_array_length(it.val); ++i) {
                           json_object_iter it2;
                           json_object *json2 = json_object_array_get_idx(it.val, i);
                           int id = 0;
                           cString name = "", channel = "", start_time = "", end_time = "";
                           json_object_object_foreachC(json2, it2) {
                             if (!strcmp(it2.key, "program_id"))
                                id = json_object_get_int(it2.val);
                             else if (!strcmp(it2.key, "name"))
                                name = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "channel"))
                                channel = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "start_time"))
                                start_time = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "end_time"))
                                end_time = Unescape(json_object_get_string(it2.val));
                             }
                           debug("id: %d name: '%s' channel: '%s' start_time: '%s' end_time: '%s'", id, *name, *channel, *start_time, *end_time);
                           callbackP.AddEvent(id, *name, *channel, *start_time, *end_time);
                           }
                       }
                    }
                  json_object_put(json);
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
     cString url = cString::sprintf("%s/vod.sl?data=true&category=%s&count=%d&ajax=true", GetBase(), *Escape(categoryP), countP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetVOD")) {
            if (IsLoginRequired(*dataM)) {
               info("cElvisWidget::GetVOD(): relogin...");
               Login();
               continue;
               }
            else {
               json_object_iter it;
               json_object *json = json_tokener_parse(*dataM);
               if (!is_error(json)) {
                  json_object_object_foreachC(json, it) {
                    if (!strcmp(it.key, "vods")) {
                       for (int i = 0; i < json_object_array_length(it.val); ++i) {
                           json_object_iter it2;
                           json_object *json2 = json_object_array_get_idx(it.val, i);
                           int id = 0, length = 0, agelimit = 0, year = 0, price = 0;
                           cString title = "", currency = "", cover = "", trailer = "";
                           json_object_object_foreachC(json2, it2) {
                             if (!strcmp(it2.key, "id"))
                                id = json_object_get_int(it2.val);
                             else if (!strcmp(it2.key, "length"))
                                length = json_object_get_int(it2.val);
                             else if (!strcmp(it2.key, "agelimit"))
                                agelimit = json_object_get_int(it2.val);
                             else if (!strcmp(it2.key, "year"))
                                year = json_object_get_int(it2.val);
                             else if (!strcmp(it2.key, "price"))
                                price = json_object_get_int(it2.val);
                             else if (!strcmp(it2.key, "title"))
                                title = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "currency"))
                                currency = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "cover"))
                                cover = Unescape(json_object_get_string(it2.val));
                             else if (!strcmp(it2.key, "trailer_url"))
                                trailer = Unescape(json_object_get_string(it2.val));
                             }
                           debug("id: %d length: %d agelimit: %d year: %d price: %d title: '%s' currency: '%s' cover: '%s' trailer: '%s'", id, length, agelimit, year, price, *title, *currency, *cover, *trailer);
                           callbackP.AddVOD(id, length, agelimit, year, price, *title, *currency, *cover, *trailer);
                           }
                       }
                    }
                  json_object_put(json);
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
     cString url = cString::sprintf("%s/program.sl?programid=%d&ajax=true", GetBase(), idP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetEventInfo")) {
            if (IsLoginRequired(*dataM)) {
               info("cElvisWidget::GetEventInfo(): relogin...");
               Login();
               continue;
               }
            else {
               json_object_iter it;
               json_object *json = json_tokener_parse(*dataM);
               if (!is_error(json)) {
                  bool has_started = false, has_ended = false, recorded = false, ready = false, is_wildcard = false;
                  int id = 0, length = 0, programviewid = 0, recordingid = 0;
                  cString name = "", channel = "", short_text = "", description = "", flength = "", tn = "", start_time = "", end_time = "", url = "";
                  json_object_object_foreachC(json, it) {
                    if (!strcmp(it.key, "id"))
                       id = json_object_get_int(it.val);
                    else if (!strcmp(it.key, "length"))
                       length = json_object_get_int(it.val);
                    else if (!strcmp(it.key, "programviewid"))
                       programviewid = json_object_get_int(it.val);
                    else if (!strcmp(it.key, "recordingid"))
                       recordingid = json_object_get_int(it.val);
                    else if (!strcmp(it.key, "name"))
                       name = Unescape(json_object_get_string(it.val));
                    else if (!strcmp(it.key, "channel"))
                       channel = Unescape(json_object_get_string(it.val));
                    else if (!strcmp(it.key, "short_text"))
                       short_text = Unescape(json_object_get_string(it.val));
                    else if (!strcmp(it.key, "description"))
                       description = Unescape(json_object_get_string(it.val));
                    else if (!strcmp(it.key, "flength"))
                       flength = Unescape(json_object_get_string(it.val));
                    else if (!strcmp(it.key, "tn"))
                       tn = Unescape(json_object_get_string(it.val));
                    else if (!strcmp(it.key, "start_time"))
                       start_time = Unescape(json_object_get_string(it.val));
                    else if (!strcmp(it.key, "end_time"))
                       end_time = Unescape(json_object_get_string(it.val));
                    else if (!strcmp(it.key, "url"))
                       url = Unescape(json_object_get_string(it.val));
                    else if (!strcmp(it.key, "has_started"))
                       has_started = json_object_get_boolean(it.val);
                    else if (!strcmp(it.key, "has_ended"))
                       has_ended = json_object_get_boolean(it.val);
                    else if (!strcmp(it.key, "recorded"))
                       recorded = json_object_get_boolean(it.val);
                    else if (!strcmp(it.key, "ready"))
                       ready = json_object_get_boolean(it.val);
                    else if (!strcmp(it.key, "is_wildcard"))
                       is_wildcard = json_object_get_boolean(it.val);
                    }
                  debug("id: %d name: '%s' channel: '%s' short_text: '%s' description: '%s' length: %d flength: '%s' tn: '%s' start_time: '%s' end_time: '%s' url: '%s' "
                        "programviewid: %d recordingid: %d has_started: %d has_ended: %d recorded: %d ready: %d is_wildcard: %d",
                        id, *name, *channel, *short_text, *description, length, *flength, *tn, *start_time, *end_time, *url, programviewid, recordingid, has_started,
                        has_ended, recorded, ready, is_wildcard);
                  json_object_put(json);
                  return new cElvisWidgetEventInfo(id, *name, *channel, *short_text, *description, length, *flength, *tn, *start_time, *end_time, *url, programviewid,
                                               recordingid, has_started, has_ended, recorded, ready, is_wildcard);
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
     cString url = cString::sprintf("%s/vod.sl?data=true&vod=%d&ajax=true", GetBase(), idP);
     for (int retries = 0; retries < eLoginRetries; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(eLoginTimeout);
         if (Perform(*url, "GetVODInfo")) {
            if (IsLoginRequired(*dataM)) {
               info("cElvisWidget::GetVODInfo(): relogin...");
               Login();
               continue;
               }
            else {
               json_object_iter it;
               json_object *json = json_tokener_parse(*dataM);
               if (!is_error(json)) {
                  int id = 0, length = 0, agelimit = 0, year = 0, price = 0;
                  cString title = "", original_title = "", currency = "", short_desc = "", info = "", info2 = "", trailer_url = "", categories = "";
                  json_object_object_foreachC(json, it) {
                    if (!strcmp(it.key, "id"))
                       id = json_object_get_int(it.val);
                    else if (!strcmp(it.key, "length"))
                       length = json_object_get_int(it.val);
                    else if (!strcmp(it.key, "agelimit"))
                       agelimit = json_object_get_int(it.val);
                    else if (!strcmp(it.key, "year"))
                       year = json_object_get_int(it.val);
                    else if (!strcmp(it.key, "price"))
                       price = json_object_get_int(it.val);
                    else if (!strcmp(it.key, "title"))
                       title = Unescape(json_object_get_string(it.val));
                    else if (!strcmp(it.key, "original_title"))
                       original_title = Unescape(json_object_get_string(it.val));
                    else if (!strcmp(it.key, "currency"))
                       currency = Unescape(json_object_get_string(it.val));
                    else if (!strcmp(it.key, "short_desc"))
                       short_desc = Unescape(json_object_get_string(it.val));
                    else if (!strcmp(it.key, "info"))
                       info = Unescape(strstrip(json_object_get_string(it.val), "\r"));
                    else if (!strcmp(it.key, "info2"))
                       info2 = Unescape(json_object_get_string(it.val));
                    else if (!strcmp(it.key, "trailer_url"))
                       trailer_url = Unescape(json_object_get_string(it.val));
                    else if (!strcmp(it.key, "categories")) {
                       for (int i = 0; i < json_object_array_length(it.val); ++i) {
                           json_object_iter it2;
                           json_object *json2 = json_object_array_get_idx(it.val, i);
                           json_object_object_foreachC(json2, it2) {
                             if (!strcmp(it2.key, "cat"))
                                categories = cString::sprintf("%s%s%s", *categories, isempty(*categories) ? "" : ", ", *Unescape(json_object_get_string(it2.val)));
                             }
                           }
                       }
                    }
                  debug("id: %d length: %d agelimit: %d year: %d price: %d title: '%s' original_title: '%s' currency: '%s' short_desc: '%s' info: '%s' info2: '%s' "
                        "trailer_url: '%s' categories: '%s'", id, length, agelimit, year, price, *title, *original_title, *currency, *short_desc, *info, *info2,
                        *trailer_url, *categories);
                  json_object_put(json);
                  return new cElvisWidgetVODInfo(id, length, agelimit, year, price, *title, *original_title, *currency, *short_desc, *info, *info2, *trailer_url, *categories);
                  }
               }
            }
         }
     }

  return NULL;
}
