/*
 * widget.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_WIDGET_H
#define __ELVIS_WIDGET_H

#include <string>

#include <jansson.h>

#include <curl/curl.h>
#include <curl/easy.h>

#include <vdr/thread.h>
#include <vdr/tools.h>

#include "config.h"

// --- cElvisWidgetCallbacks -------------------------------------------

class cElvisWidgetFolderCallbackIf {
public:
  cElvisWidgetFolderCallbackIf() {}
  virtual ~cElvisWidgetFolderCallbackIf() {}
  virtual void AddFolder(int folderIdP, const char *folderNameP, int recCountP, bool protectedP) = 0;
};

class cElvisWidgetRecordingCallbackIf {
public:
  cElvisWidgetRecordingCallbackIf() {}
  virtual ~cElvisWidgetRecordingCallbackIf() {}
  virtual void AddFolder(int idP, int countP, const char *nameP, const char *sizeP) = 0;
  virtual void AddRecording(int idP, int programIdP, int folderIdP, int countP, int lengthP, const char *nameP, const char *channelP, const char *startTimeP) = 0;
};

class cElvisWidgetTimerCallbackIf {
public:
  cElvisWidgetTimerCallbackIf() {}
  virtual ~cElvisWidgetTimerCallbackIf() {}
  virtual void AddTimer(int idP, int lengthP, const char *nameP, const char *channelP, const char *startTimeP, const char *wildcardP) = 0;
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
  virtual void AddChannel(const char *nameP, const char *logoP) = 0;
};

class cElvisWidgetEventCallbackIf {
public:
  cElvisWidgetEventCallbackIf() {}
  virtual ~cElvisWidgetEventCallbackIf() {}
  virtual void AddEvent(int idP, const char *nameP, const char *simpleStartTimeP, const char *simpleEndTimeP, const char *startTimeP, const char *endTimeP) = 0;
};

class cElvisWidgetEPGCallbackIf {
public:
  cElvisWidgetEPGCallbackIf() {}
  virtual ~cElvisWidgetEPGCallbackIf() {}
  virtual void AddEvent(const char *channelP, int idP, const char *nameP, const char *simpleStartTimeP, const char *simpleEndTimeP, const char *startTimeP, const char *endTimeP, const char *descriptionP) = 0;
};

class cElvisWidgetTopEventCallbackIf {
public:
  cElvisWidgetTopEventCallbackIf() {}
  virtual ~cElvisWidgetTopEventCallbackIf() {}
  virtual void AddEvent(int idP, const char *nameP, const char *channelP, const char *startTimeP, const char *endTimeP) = 0;
};

class cElvisWidgetVODCallbackIf {
public:
  cElvisWidgetVODCallbackIf() {}
  virtual ~cElvisWidgetVODCallbackIf() {}
  virtual void AddVOD(int idP, int lengthP, int ageLimitP, int yearP, int priceP, const char *titleP, const char *currencyP, const char *coverP, const char *trailerP) = 0;
};

// --- cElvisWidgetEventInfo ------------------------------------------------

class cElvisWidgetEventInfo {
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
  bool isEncryptedM;
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
  cElvisWidgetEventInfo();
  // to prevent copy constructor and assignment
  cElvisWidgetEventInfo(const cElvisWidgetEventInfo&);
  cElvisWidgetEventInfo& operator=(const cElvisWidgetEventInfo&);
public:
  cElvisWidgetEventInfo(int idP, const char *nameP, const char *channelP, const char *shortTextP, const char *descriptionP, int lengthP, const char *fLengthP,
                    const char *thumbnailP, const char *startTimeP, const char *endTimeP, const char *urlP, int programViewIdP, int recordingIdP,
                    bool hasStartedP, bool hasEndedP, bool isRecordedP, bool isReadyP, bool isWildcardP, bool isEncrypted);
  virtual ~cElvisWidgetEventInfo();
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
  bool Encrypted() { return isEncryptedM; }
};

// --- cElvisWidgetVODInfo --------------------------------------------------

class cElvisWidgetVODInfo {
private:
  int idM;
  int lengthM;
  int ageLimitM;
  int yearM;
  int priceM;
  cString titleM;
  cString originalTitleM;
  cString currencyM;
  cString shortDescriptionM;
  cString infoM;
  cString info2M;
  cString trailerUrlM;
  cString categoriesM;
  // to prevent default constructor
  cElvisWidgetVODInfo();
  // to prevent copy constructor and assignment
  cElvisWidgetVODInfo(const cElvisWidgetVODInfo&);
  cElvisWidgetVODInfo& operator=(const cElvisWidgetVODInfo&);
public:
  cElvisWidgetVODInfo(int idP, int lengthP, int ageLimitP, int yearP, int priceP, const char *titleP, const char *originalTitleP, const char *currencyP,
                      const char *shortDescriptionP, const char *infoP, const char *info2P, const char *trailerUrlP, const char *categoriesP);
  virtual ~cElvisWidgetVODInfo();
  int Id() { return idM; }
  int Length() { return lengthM; }
  int AgeLimit() { return ageLimitM; }
  int Year() { return yearM; }
  int Price() { return priceM; }
  const char *Title() { return *titleM; }
  const char *OriginalTitle() { return *originalTitleM; }
  const char *Currency() { return *currencyM; }
  const char *ShortDescription() { return *shortDescriptionM; }
  const char *Info() { return *infoM; }
  const char *Info2() { return *info2M; }
  const char *TrailerUrl() { return *trailerUrlM; }
  const char *Categories() { return *categoriesM; }
};

// --- cElvisWidget ----------------------------------------------------

class cElvisWidget {
private:
  enum {
    eLoginRetries = 2,
    eLoginTimeout = 1000 // in milliseconds
  };
  static const char *baseCookieNameS;
  static const char *baseUrlViihdeS;
  static cElvisWidget *instanceS;
  static size_t WriteCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP);
  cString dataM;
  cMutex mutexM;
  CURL *handleM;
  struct curl_slist *headerListM;
  cString Unescape(const char *s);
  cString Escape(const char *s);
  bool Perform(const char *urlP, const char *msgP);
  bool Login();
  bool Logout();
  bool IsLogged();
  bool IsLoginRequired(const char *stringP);
  void ParseFolders(cElvisWidgetFolderCallbackIf &callbackP, json_t *objP, int folderIdP = -1);
  // constructor
  cElvisWidget();
  // to prevent copy constructor and assignment
  cElvisWidget(const cElvisWidget&);
  cElvisWidget& operator=(const cElvisWidget&);
public:
  static cElvisWidget *GetInstance();
  static void Destroy();
  virtual ~cElvisWidget();
  bool Invalidate();
  bool Load(const char *directoryP);
  void PutData(const char *dataP, unsigned int lenP);
  bool GetFolders(cElvisWidgetFolderCallbackIf &callbackP);
  bool GetRecordings(cElvisWidgetRecordingCallbackIf &callbackP, int folderIdP = -1);
  bool RemoveRecording(int idP);
  bool RemoveFolder(int idP);
  bool RenameFolder(int idP, const char *nameP);
  bool CreateFolder(const char *nameP, int parentFolderIdP = 1);
  bool GetTimers(cElvisWidgetTimerCallbackIf &callbackP);
  bool AddTimer(int programIdP, int folderIdP = -1);
  bool RemoveTimer(int idP);
  bool GetSearchTimers(cElvisWidgetSearchTimerCallbackIf &callbackP);
  bool AddSearchTimer(const char *channelP, const char *wildcardP, int folderIdP = -1, int wildcardIdP = -1);
  bool RemoveSearchTimer(int idP);
  bool GetChannels(cElvisWidgetChannelCallbackIf &callbackP);
  bool GetEvents(cElvisWidgetEventCallbackIf &callbackP, const char *channelP);
  bool GetEPG(cElvisWidgetEPGCallbackIf &callbackP);
  bool GetTopEvents(cElvisWidgetTopEventCallbackIf &callbackP);
  bool GetVOD(cElvisWidgetVODCallbackIf &callbackP, const char *categoryP, unsigned int countP = 25);
  cElvisWidgetEventInfo *GetEventInfo(int idP);
  cElvisWidgetVODInfo *GetVODInfo(int idP);
  bool SearchVOD(cElvisWidgetVODCallbackIf &callbackP, const char *titleP, const char *descP, bool hdP);
  bool SetVODFavorite(int idP, bool onOffP);
};

#endif // __ELVIS_WIDGET_H
