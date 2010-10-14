/*
 * widget.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_WIDGET_H
#define __ELVIS_WIDGET_H

#include <string>

#include <json/json.h>
#include <json/json_object_private.h>

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include <vdr/thread.h>
#include <vdr/tools.h>

#include "config.h"

// --- cElvisWidgetCallbacks -------------------------------------------

class cElvisWidgetRecordingCallbackIf {
public:
  cElvisWidgetRecordingCallbackIf() {}
  virtual ~cElvisWidgetRecordingCallbackIf() {}
  virtual void AddRecording(int idP, int program_idP, int folder_idP, const char *nameP, const char *channelP, const char *starttimeP, const char *sizeP) = 0;
};

class cElvisWidgetTimerCallbackIf {
public:
  cElvisWidgetTimerCallbackIf() {}
  virtual ~cElvisWidgetTimerCallbackIf() {}
  virtual void AddTimer(int idP, int programIdP, int lengthP, const char *nameP, const char *channelP, const char *starttimeP, const char *wildcard) = 0;
};

class cElvisWidgetSearchTimerCallbackIf {
public:
  cElvisWidgetSearchTimerCallbackIf() {}
  virtual ~cElvisWidgetSearchTimerCallbackIf() {}
  virtual void AddSearchTimer(int idP, const char *folderP, const char *addedP, const char *channelP, const char *wildcardP) = 0;
};

class cElvisWidgetChannelCallbackIf {
public:
  cElvisWidgetChannelCallbackIf() {}
  virtual ~cElvisWidgetChannelCallbackIf() {}
  virtual void AddChannel(const char *nameP) = 0;
};

class cElvisWidgetEventCallbackIf {
public:
  cElvisWidgetEventCallbackIf() {}
  virtual ~cElvisWidgetEventCallbackIf() {}
  virtual void AddEvent(int idP, const char *nameP, const char *simpleStarttimeP, const char *simpleEndtimeP, const char *starttimeP, const char *endtimeP) = 0;
};

class cElvisWidgetTopEventCallbackIf {
public:
  cElvisWidgetTopEventCallbackIf() {}
  virtual ~cElvisWidgetTopEventCallbackIf() {}
  virtual void AddEvent(int idP, const char *nameP, const char *channelP, const char *starttimeP, const char *endtimeP) = 0;
};

// --- cElvisWidgetInfo ------------------------------------------------

class cElvisWidgetInfo {
private:
  int idM;
  int lengthM;
  int programViewIdM;
  int recordingIdM;
  bool hasStartedM;
  bool hasEndedM;
  bool isRecordedM;
  bool isReadyM;
  bool isWildcardM;
  cString nameM;
  cString channelM;
  cString shortTextM;
  cString descriptionM;
  cString fLengthM;
  cString thumbnailM;
  cString startTimeM;
  cString endTimeM;
  cString urlM;
  time_t startTimeValueM;
  time_t endTimeValueM;
  // to prevent default constructor
  cElvisWidgetInfo();
  // to prevent copy constructor and assignment
  cElvisWidgetInfo(const cElvisWidgetInfo&);
  cElvisWidgetInfo& operator=(const cElvisWidgetInfo&);
public:
  cElvisWidgetInfo(int idP, const char *nameP, const char *channelP, const char *shortTextP, const char *descriptionP, int lengthP, const char *fLengthP,
                    const char *thumbnailP, const char *startTimeP, const char *endTimeP, const char *urlP, int programViewIdP, int recordingIdP,
                    bool hasStartedP, bool hasEndedP, bool isRecordedP, bool isReadyP, bool isWildcardP);
  virtual ~cElvisWidgetInfo();
  int Id() { return idM; }
  int LengthValue() { return lengthM; }
  const char *Name() { return *nameM; }
  const char *ShortText() { return *shortTextM; }
  const char *Description() { return *descriptionM; }
  const char *StartTime() { return *startTimeM; }
  const char *EndTime() { return *endTimeM; }
  const char *Channel() { return *channelM; }
  const char *Url() { return *urlM; }
  const char *Length() { return *fLengthM; }
  time_t StartTimeValue() { return startTimeValueM; }
  time_t EndTimeValue() { return endTimeValueM; }
};

// --- cElvisWidget ----------------------------------------------------

class cElvisWidget {
private:
  enum {
    eLoginRetries = 2
  };
  static const char *baseUrlViihdeS;
  static const char *baseUrlViihdeSslS;
  static const char *baseUrlVisioS;
  static const char *baseUrlVisioSslS;
  static cElvisWidget *instanceS;
  static size_t WriteCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP);
  cString dataM;
  cMutex mutexM;
  CURL *handleM;
  struct curl_slist *headerListM;
  cString Unescape(const char *s);
  cString Escape(const char *s);
  inline void ResetData();
  inline bool IsValidData();
  inline bool Perform(const char *msgP, const char *testP = NULL);
  bool Login();
  bool Logout();
  cElvisWidget();
  // to prevent copy constructor and assignment
  cElvisWidget(const cElvisWidget&);
  cElvisWidget& operator=(const cElvisWidget&);
public:
  static cElvisWidget *GetInstance();
  static void Destroy();
  virtual ~cElvisWidget();
  const char *GetBase() { return (ElvisConfig.Ssl == 0) ? ((ElvisConfig.Service == 0) ? baseUrlViihdeS : baseUrlVisioS) : ((ElvisConfig.Service == 0) ? baseUrlViihdeSslS : baseUrlVisioSslS); }
  void PutData(const char *dataP, unsigned int lenP);
  bool GetRecordings(cElvisWidgetRecordingCallbackIf &callbackP, int folderIdP = -1);
  bool RemoveRecording(int idP);
  bool GetTimers(cElvisWidgetTimerCallbackIf &callbackP);
  bool AddTimer(int idP, int folderIdP = -1);
  bool RemoveTimer(int idP);
  bool GetSearchTimers(cElvisWidgetSearchTimerCallbackIf &callbackP);
  bool AddSearchTimer(const char *channelP, const char *wildcardP, int folderIdP = -1, int wildcardIdP = -1);
  bool RemoveSearchTimer(int idP);
  bool GetChannels(cElvisWidgetChannelCallbackIf &callbackP);
  bool GetEvents(cElvisWidgetEventCallbackIf &callbackP, const char *channelP);
  bool GetTopEvents(cElvisWidgetTopEventCallbackIf &callbackP);
  cElvisWidgetInfo *GetEventInfo(int idP);
};

#endif // __ELVIS_WIDGET_H
