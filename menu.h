/*
 * menu.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_MENU_H
#define __ELVIS_MENU_H

#include <vdr/osdbase.h>
#include <vdr/tools.h>

#include "recordings.h"
#include "timers.h"
#include "searchtimers.h"
#include "events.h"

// --- cElvisRecordingInfoMenu -----------------------------------------

class cElvisRecordingInfoMenu : public cOsdMenu {
private:
  cString urlM;
  cString nameM;
  cString descriptionM;
  cString startTimeM;
  unsigned int lengthM;
public:
  cElvisRecordingInfoMenu(const char *urlP, const char *nameP, const char *descriptionP, const char *startTimeP, unsigned int lengthP);
  virtual void Display();
  virtual eOSState ProcessKey(eKeys keyP);
};

// --- cElvisRecordingItem ---------------------------------------------

class cElvisRecordingItem : public cOsdItem {
private:
  cElvisRecording *recordingM;
  cString descriptionM;
public:
  cElvisRecordingItem(cElvisRecording *recordingP);
  cElvisRecording *Recording() { return recordingM; }
  bool IsFolder() { return (recordingM && recordingM->IsFolder()); }
  const char *Description() { return *descriptionM; }
};

// --- cElvisRecordingsMenu --------------------------------------------

class cElvisRecordingsMenu : public cOsdMenu {
private:
  int folderIdM;
  int levelM;
  void SetHelpKeys();
  void Setup();
  eOSState Delete();
  eOSState Info();
  eOSState Play(bool rewindP = false);
  eOSState Fetch();
public:
  cElvisRecordingsMenu(int folderIdP = -1, int levelP = 0);
  virtual eOSState ProcessKey(eKeys keyP);
};

// --- cElvisTimerCreateMenu -------------------------------------------

class cElvisTimerCreateMenu : public cOsdMenu {
private:
  const char **folderNamesM;
  int *folderIdsM;
  int numFoldersM;
  int folderM;
  cElvisEvent *eventM;
  void Setup();
public:
  cElvisTimerCreateMenu(cElvisEvent *eventP);
  virtual ~cElvisTimerCreateMenu();
  virtual eOSState ProcessKey(eKeys keyP);
};

// --- cElvisTimerInfoMenu ---------------------------------------------

class cElvisTimerInfoMenu : public cOsdMenu {
private:
  cString textM;
public:
  cElvisTimerInfoMenu(cElvisTimer *timerP);
  virtual void Display();
  virtual eOSState ProcessKey(eKeys keyP);
};

// --- cElvisTimerItem -------------------------------------------------

class cElvisTimerItem : public cOsdItem {
private:
  cElvisTimer *timerM;
public:
  cElvisTimerItem(cElvisTimer *timerP);
  cElvisTimer *Timer() { return timerM; }
  bool Wildcard() { return (timerM && (timerM->Id() == 0)); }
};

// --- cElvisTimersMenu ------------------------------------------------

class cElvisTimersMenu : public cOsdMenu {
private:
  int stateM;
  void SetHelpKeys();
  void Setup();
  eOSState Delete();
  eOSState Info();
public:
  cElvisTimersMenu();
  virtual eOSState ProcessKey(eKeys keyP);
};

// --- cElvisSearchTimersMenu ------------------------------------------

class cElvisSearchTimerEditMenu : public cOsdMenu {
private:
  cElvisSearchTimer *timerM;
  char wildcardM[MaxFileName];
  const char **channelNamesM;
  const char **folderNamesM;
  int *folderIdsM;
  int numChannelsM;
  int numFoldersM;
  int channelM;
  int folderM;
  void Setup();
public:
  cElvisSearchTimerEditMenu(cElvisSearchTimer *timerP);
  virtual ~cElvisSearchTimerEditMenu();
  virtual eOSState ProcessKey(eKeys keyP);
};

// --- cElvisSearchTimerItem -------------------------------------------

class cElvisSearchTimerItem : public cOsdItem {
private:
  cElvisSearchTimer *timerM;
public:
  cElvisSearchTimerItem(cElvisSearchTimer *timerP);
  cElvisSearchTimer *Timer() { return timerM; }
};

// --- cElvisSearchTimersMenu ------------------------------------------

class cElvisSearchTimersMenu : public cOsdMenu {
private:
  int stateM;
  void SetHelpKeys();
  void Setup();
  eOSState Delete();
  eOSState New();
  eOSState Edit();
public:
  cElvisSearchTimersMenu();
  virtual eOSState ProcessKey(eKeys keyP);
};

// --- cElvisChannelEventInfoMenu --------------------------------------

class cElvisChannelEventInfoMenu : public cOsdMenu {
private:
  cElvisEvent *eventM;
  cString textM;
  void SetHelpKeys();
  eOSState Record(bool quickP = true);
public:
  cElvisChannelEventInfoMenu(cElvisEvent *eventP, const char *channelP);
  virtual void Display();
  virtual eOSState ProcessKey(eKeys keyP);
};

// --- cElvisChannelEventItem -----------------------------------------------

class cElvisChannelEventItem : public cOsdItem {
private:
  cElvisEvent *eventM;
public:
  cElvisChannelEventItem(cElvisEvent *eventP);
  cElvisEvent *Event() { return eventM; }
};

// --- cElvisChannelEventsMenu ------------------------------------------------

class cElvisChannelEventsMenu : public cOsdMenu {
private:
  cElvisChannel *channelM;
  int stateM;
  void SetHelpKeys();
  void Setup();
  eOSState Record(bool quickP = true);
  eOSState Info();
public:
  cElvisChannelEventsMenu(cElvisChannel *channelP);
  virtual eOSState ProcessKey(eKeys keyP);
};

// --- cElvisChannelItem -----------------------------------------------

class cElvisChannelItem : public cOsdItem {
private:
  cElvisChannel *channelM;
public:
  cElvisChannelItem(cElvisChannel *channelP);
  cElvisChannel *Channel() { return channelM; }
};

// --- cElvisEPGMenu ---------------------------------------------------

class cElvisEPGMenu : public cOsdMenu {
private:
  int stateM;
  void SetHelpKeys();
  void Setup();
  eOSState Select();
public:
  cElvisEPGMenu();
  virtual eOSState ProcessKey(eKeys keyP);
};

// --- cElvisTopEventsMenu ---------------------------------------------

class cElvisTopEventsMenu : public cOsdMenu {
private:
  int stateM;
  void SetHelpKeys();
  void Setup();
  eOSState Record(bool quickP = true);
  eOSState Info();
public:
  cElvisTopEventsMenu();
  virtual eOSState ProcessKey(eKeys keyP);
};

// --- cElvisMenu ------------------------------------------------------

class cElvisMenu: public cOsdMenu {
private:
  enum {
    eUpdateTimeoutMs = 5000
  };
  cTimeMs updateTimeoutM;
  unsigned int fetchCountM;
  void SetHelpKeys();
  void Setup();
public:
  cElvisMenu();
  eOSState ProcessKey(eKeys KeyP);
};

#endif // __ELVIS_MENU_H

