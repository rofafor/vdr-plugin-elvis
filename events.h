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

class cElvisChannels : public cList<cElvisChannel>, public cElvisWidgetChannelCallbackIf {
private:
  static cElvisChannels *instanceS;
  cMutex mutexM;
  // constructor
  cElvisChannels();
  // to prevent copy constructor and assignment
  cElvisChannels(const cElvisChannels&);
  cElvisChannels& operator=(const cElvisChannels&);
public:
  static cElvisChannels *GetInstance();
  static void Destroy();
  virtual ~cElvisChannels();
  virtual void AddChannel(const char *nameP);
  bool Update();
};

// --- cElvisTopEvents -------------------------------------------------

class cElvisTopEvents : public cList<cElvisEvent>, public cElvisWidgetTopEventCallbackIf {
private:
  static cElvisTopEvents *instanceS;
  cMutex mutexM;
  // constructor
  cElvisTopEvents();
  // to prevent copy constructor and assignment
  cElvisTopEvents(const cElvisTopEvents&);
  cElvisTopEvents& operator=(const cElvisTopEvents&);
public:
  static cElvisTopEvents *GetInstance();
  static void Destroy();
  virtual ~cElvisTopEvents();
  virtual void AddEvent(int idP, const char *nameP, const char *channelP, const char *starttimeP, const char *endtimeP);
  bool Update();
};

#endif // __ELVIS_EVENTS_H
