/*
 * events.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_EVENTS_H
#define __ELVIS_EVENTS_H

#include <vdr/thread.h>
#include <vdr/tools.h>
#include <vdr/epg.h>

#include "widget.h"

class cElvisEvent : public cListObject {
  friend class cElvisChannel;
private:
  int idM;
  cString nameM;
  cString channelM;
  cString simpleStartTimeM;
  cString simpleEndTimeM;
  cString startTimeM;
  cString endTimeM;
  cElvisWidgetInfo *infoM;
  time_t startTimeValueM;
  // to prevent default constructor
  cElvisEvent();
  // to prevent copy constructor and assignment
  cElvisEvent(const cElvisEvent&);
  cElvisEvent &operator=(const cElvisEvent &);
public:
  cElvisEvent(int idP, const char *nameP, const char *simpleStartTimeP, const char *simpleEndtimeP, const char *startTimeP, const char *endTimeP);
  cElvisEvent(int idP, const char *nameP, const char *channelP, const char *startTimeP, const char *endTimeP);
  virtual ~cElvisEvent();
  cElvisWidgetInfo *Info();
  int Id() { return idM; }
  const char *Name() { return *nameM; }
  const char *Channel() { return *channelM; }
  const char *StartTime() { return *startTimeM; }
  const char *EndTime() { return *endTimeM; }
  time_t StartTimeValue() { return startTimeValueM; }
};

// --- cElvisChannel ---------------------------------------------------

class cElvisChannel : public cListObject, public cList<cElvisEvent>, public cElvisWidgetEventCallbackIf {
  friend class cElvisChannels;
private:
  cMutex  mutexM;
  cString nameM;
  // to prevent default constructor
  cElvisChannel();
  // to prevent copy and assignment constructors
  cElvisChannel(const cElvisChannel&);
  cElvisChannel &operator=(const cElvisChannel &);
public:
  cElvisChannel(const char *nameP);
  virtual ~cElvisChannel();
  virtual void AddEvent(int idP, const char *nameP, const char *simpleStarttimeP, const char *simpleEndtimeP, const char *starttimeP, const char *endtimeP);
  bool Update();
  const char *Name() { return *nameM; }
};

// --- cElvisChannels --------------------------------------------------

class cElvisChannels : public cList<cElvisChannel>, public cThread, public cElvisWidgetChannelCallbackIf {
private:
  static cElvisChannels *instanceS;
  enum {
    eUpdateInterval = 7200 // 120min
  };
  cMutex mutexM;
  int stateM;
  time_t lastUpdateM;
  void Refresh(bool foregroundP = false);
  // constructor
  cElvisChannels();
  // to prevent copy constructor and assignment
  cElvisChannels(const cElvisChannels&);
  cElvisChannels& operator=(const cElvisChannels&);
protected:
  void Action();
public:
  static cElvisChannels *GetInstance();
  static void Destroy();
  virtual ~cElvisChannels();
  virtual void AddChannel(const char *nameP);
  bool Update(bool waitP = false);
  void ChangeState(void) { ++stateM; }
  bool StateChanged(int &stateP);
  bool AddTimer(tEventID eventIdP);
  bool DelTimer(tEventID eventIdP);
};

// --- cElvisTopEvents -------------------------------------------------

class cElvisTopEvents : public cList<cElvisEvent>, public cThread, public cElvisWidgetTopEventCallbackIf {
private:
  static cElvisTopEvents *instanceS;
  enum {
    eUpdateInterval = 900 // 15min
  };
  cMutex mutexM;
  int stateM;
  time_t lastUpdateM;
  void Refresh(bool foregroundP = false);
  // constructor
  cElvisTopEvents();
  // to prevent copy constructor and assignment
  cElvisTopEvents(const cElvisTopEvents&);
  cElvisTopEvents& operator=(const cElvisTopEvents&);
protected:
  void Action();
public:
  static cElvisTopEvents *GetInstance();
  static void Destroy();
  virtual ~cElvisTopEvents();
  virtual void AddEvent(int idP, const char *nameP, const char *channelP, const char *starttimeP, const char *endtimeP);
  bool Update(bool waitP = false);
  void ChangeState(void) { ++stateM; }
  bool StateChanged(int &stateP);
};

#endif // __ELVIS_EVENTS_H
