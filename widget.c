/*
 * widget.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <string.h>

#include "common.h"
#include "widget.h"

// --- cElvisWidgetInfo ------------------------------------------------

cElvisWidgetInfo::cElvisWidgetInfo(int idP, const char *nameP, const char *channelP, const char *shortTextP, const char *descriptionP, int lengthP, const char *fLengthP,
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
  urlM(urlP)
{
}

cElvisWidgetInfo::~cElvisWidgetInfo()
{
}

// --- cElvisWidget ----------------------------------------------------

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
:  dataM(NULL),
   headerListM(NULL)
{
  // initialize the curl session
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
     curl_easy_setopt(handleM, CURLOPT_TIMEOUT, 10L);

     // set user-agent
     curl_easy_setopt(handleM, CURLOPT_USERAGENT, *cString::sprintf("vdr-%s/%s", PLUGIN_NAME_I18N, VERSION));

     // follow location
     curl_easy_setopt(handleM, CURLOPT_FOLLOWLOCATION, 1L);

     // enable cookies
     curl_easy_setopt(handleM, CURLOPT_COOKIEFILE, "");

     // set additional headers to prevent caching
     headerListM = curl_slist_append(headerListM, "Cache-Control: no-store, no-cache, must-revalidate");
     headerListM = curl_slist_append(headerListM, "Cache-Control: post-check=0, pre-check=0");
     headerListM = curl_slist_append(headerListM, "Pragma: no-cache");
     headerListM = curl_slist_append(headerListM, "Expires: Mon, 26 Jul 1997 05:00:00 GMT");
     curl_easy_setopt(handleM, CURLOPT_HTTPHEADER, headerListM); 

     Login();
     }
}

cElvisWidget::~cElvisWidget()
{
  if (handleM) {
     Logout();

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

void cElvisWidget::ResetData()
{
  dataM = "";
}

void cElvisWidget::PutData(const char *dataP, unsigned int lenP)
{
  dataM = cString::sprintf("%s%s", *dataM, dataP);
}

bool cElvisWidget::IsValidData()
{
  return !isempty(*dataM);
}

bool cElvisWidget::Perform(const char *msgP, const char *testP)
{
  debug("Perform: %s", msgP ? msgP : "unknown");

  if (handleM) {
     ResetData();

     CURLcode err = curl_easy_perform(handleM);
     if (err != CURLE_OK) {
        error("cElvisWidget::Perform(%s): %s", msgP ? msgP : "unknown", curl_easy_strerror(err));
        return false;
        }

     int http_code = 0;
     err = curl_easy_getinfo(handleM, CURLINFO_HTTP_CODE, &http_code);
     if (err != CURLE_OK) {
        error("cElvisWidget::Perform(Getinfo): %s", curl_easy_strerror(err));
        return false;
        }
     if (http_code != 200) {
        error("cElvisWidget::Perform(%s): invalid HTTP Code (%d)", msgP ? msgP : "unknown", http_code);
        return false;
        }

     if (!IsValidData()) {
        error("cElvisWidget::Perform(%s): empty data", msgP ? msgP : "unknown");
        return false;
        }

     if (testP)
        return (strstr(*dataM, testP) != NULL);
     }

  return true;
}

bool cElvisWidget::GetRecordings(cElvisWidgetRecordingCallbackIf &callbackP, int folderIdP)
{
  cMutexLock(mutexM);

  if (handleM) {
     if (folderIdP < 0)
        curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/ready.sl?ajax=true&clear=true", GetBase()));
     else
        curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/ready.sl?folderid=%d&ajax=true&clear=true", GetBase(), folderIdP));
     for (int retries = 0; retries < LOGIN_RETRIES; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(1000);
         if (Perform("GetRecordings")) {
            if (strstr(*dataM, "evlogin")) {
               error("cElvisWidget::GetRecordings(): relogin...");
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
                                    int id = 0;
                                    json_object_object_foreachC(json3, it3) {
                                      if (!strcmp(it3.key, "id"))
                                         id = json_object_get_int(it3.val);
                                      else if (!strcmp(it3.key, "name"))
                                         name = Unescape(json_object_get_string(it3.val));
                                      else if (!strcmp(it3.key, "size"))
                                         size = Unescape(json_object_get_string(it3.val));
                                      }
                                    debug("id: %d name: '%s' size: '%s'", id, *name, *size);
                                    callbackP.AddRecording(id, -1, -1, *name, NULL, NULL, *size);
                                    }
                                }
                             else if (!strcmp(it2.key, "recordings")) {
                                for (int j = 0; j < json_object_array_length(it2.val); ++j) {
                                    json_object_iter it3;
                                    json_object *json3 = json_object_array_get_idx(it2.val, j);
                                    cString name = "", channel = "", start_time = "";
                                    int id = 0, program_id = 0, folder_id = 0;
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
                                      }
                                    debug("id: %d program_id: %d folder_id: %d name: '%s' channel: '%s' start_time: '%s'", id, program_id, folder_id, *name, *channel, *start_time);
                                    callbackP.AddRecording(id, program_id, folder_id, *name, *channel, *start_time, NULL);
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
     curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/program.sl?remove=true&removep=%d&ajax=true", GetBase(), idP));
     for (int retries = 0; retries < LOGIN_RETRIES; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(1000);
         if (Perform("RemoveRecording")) {
            if (strstr(*dataM, "evlogin")) {
               error("cElvisWidget::RemoveRecording(): relogin...");
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
     curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/recordings.sl?ajax=true", GetBase()));
     for (int retries = 0; retries < LOGIN_RETRIES; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(1000);
         if (Perform("GetTimers")) {
            if (strstr(*dataM, "evlogin")) {
               error("cElvisWidget::GetTimers(): relogin...");
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
                           debug("id: %d program_id: %d length: %d name: '%s' channel: '%s' start_time: '%s' wild_card: '%s'", id, program_id, length, *name, *channel, *start_time, *wild_card);
                           callbackP.AddTimer(id, program_id, length, *name, *channel, *start_time, *wild_card);
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

bool cElvisWidget::AddTimer(int idP, int folderIdP)
{
  cMutexLock(mutexM);

  if (handleM && (idP > 0)) {
     if (folderIdP < 0)
        curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/program.sl?programid=%d&record=%d&ajax=true", GetBase(), idP, idP));
     else
        curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/program.sl?programid=%d&record=%d&folderid=%d&ajax=true", GetBase(), idP, idP, folderIdP));
     for (int retries = 0; retries < LOGIN_RETRIES; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(1000);
         if (Perform("AddTimer", "TRUE")) {
            if (strstr(*dataM, "evlogin")) {
               error("cElvisWidget::AddTimer(): relogin...");
               Login();
               continue;
               }
            else {
               debug("added timer id: %d", idP);
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
     curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/program.sl?remover=%d&ajax=true", GetBase(), idP));
     for (int retries = 0; retries < LOGIN_RETRIES; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(1000);
         if (Perform("RemoveTimer")) {
            if (strstr(*dataM, "evlogin")) {
               error("cElvisWidget::RemoveTimer(): relogin...");
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
     curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/wildcards.sl?ajax=true", GetBase()));
     for (int retries = 0; retries < LOGIN_RETRIES; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(1000);
         if (Perform("GetSearchTimers")) {
            if (strstr(*dataM, "evlogin")) {
               error("cElvisWidget::GetSearchTimers(): relogin...");
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
     if (wildcardIdP < 0)
        curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/wildcards.sl?channel=%s&folderid=%s&wildcard=%s&record=true&ajax=true",
                         GetBase(), *Escape(channelP), (folderIdP < 0) ? "" : *cString::sprintf("%d", folderIdP), *Escape(wildcardP)));
     else
        curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/wildcards.sl?edit_wildcard=%d&channel=%s&folderid=%s&wildcard=%s&record=true&ajax=true",
                         GetBase(), wildcardIdP, *Escape(channelP), (folderIdP < 0) ? "" : *cString::sprintf("%d", folderIdP), *Escape(wildcardP)));
     for (int retries = 0; retries < LOGIN_RETRIES; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(1000);
         if (Perform("AddSearchTimer", "TRUE")) {
            if (strstr(*dataM, "evlogin")) {
               error("cElvisWidget::AddSearchTimer(): relogin...");
               Login();
               continue;
               }
            else {
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
     curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/wildcards.sl?remover=%d&ajax=true", GetBase(), idP));
     for (int retries = 0; retries < LOGIN_RETRIES; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(1000);
         if (Perform("RemoveSearchTimer")) {
            if (strstr(*dataM, "evlogin")) {
               error("relogin...");
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
     curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/ajaxprograminfo.sl?channels", GetBase()));
     for (int retries = 0; retries < LOGIN_RETRIES; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(1000);
         if (Perform("GetChannels")) {
            if (strstr(*dataM, "evlogin")) {
               error("cElvisWidget::GetChannels(): relogin...");
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
                           cString channel = Unescape(json_object_get_string(json2));
                           debug("channel: '%s'", *channel);
                           callbackP.AddChannel(*channel);
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
     curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/ajaxprograminfo.sl?24h=%s", GetBase(), *Escape(channelP)));
     for (int retries = 0; retries < LOGIN_RETRIES; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(1000);
         if (Perform("GetEvents")) {
            if (strstr(*dataM, "evlogin")) {
               error("cElvisWidget::GetEvents(): relogin...");
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


bool cElvisWidget::GetTopEvents(cElvisWidgetTopEventCallbackIf &callbackP)
{
  cMutexLock(mutexM);

  if (handleM) {
     curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/channels.sl?ajax=true", GetBase()));
     for (int retries = 0; retries < LOGIN_RETRIES; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(1000);
         if (Perform("GetEvents")) {
            if (strstr(*dataM, "evlogin")) {
               error("cElvisWidget::GetTopEvents(): relogin...");
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

cElvisWidgetInfo *cElvisWidget::GetEventInfo(int idP)
{
  cMutexLock(mutexM);
  if (handleM && (idP > 0)) {
     curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/program.sl?programid=%d&ajax=true", GetBase(), idP));
     for (int retries = 0; retries < LOGIN_RETRIES; ++retries) {
         if (retries > 0)
            cCondWait::SleepMs(1000);
         if (Perform("GetEventInfo")) {
            if (strstr(*dataM, "evlogin")) {
               error("cElvisWidget::GetEventInfo(): relogin...");
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
                  return new cElvisWidgetInfo(id, *name, *channel, *short_text, *description, length, *flength, *tn, *start_time, *end_time, *url, programviewid,
                                               recordingid, has_started, has_ended, recorded, ready, is_wildcard);
                  }
               }
            }
         }
     }

  return NULL;
}

bool cElvisWidget::Login()
{
  cMutexLock(mutexM);

  if (isempty(ElvisConfig.Username) || isempty(ElvisConfig.Password)) {
     error("cElvisWidget::Login(): invalid credentials");
     return false;
     }

  if (handleM) {
     curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/default.sl?username=%s&password=%s&ajax=true",
                      GetBase(), ElvisConfig.Username, ElvisConfig.Password));
     return Perform("Login", "TRUE");
     }

  return false;
}

bool cElvisWidget::Logout()
{
  cMutexLock(mutexM);

  if (handleM) {
     curl_easy_setopt(handleM, CURLOPT_URL, *cString::sprintf("%s/logout.sl?ajax=true", GetBase()));
     return Perform("Logout");
     }

  return false;
}
