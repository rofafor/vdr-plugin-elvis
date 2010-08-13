/*
 * timers.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_TIMERS_H
#define __ELVIS_TIMERS_H

#include <vdr/thread.h>
#include <vdr/tools.h>

#include "widget.h"

// --- cElvisTimer -----------------------------------------------------

class cElvisTimer : public cListObject {
  friend class cElvisTimers;
private:
  int idM;
  int programIdM;
  int lengthM;
  cString nameM;
  cString channelM;
  cString startTimeM;
  cString wildcardM;
  cElvisWidgetInfo *infoM;
  // to prevent default constructor
  cElvisTimer();
  // to prevent copy constructor and assignment
  cElvisTimer(const cElvisTimer&);
  cElvisTimer &operator=(const cElvisTimer &);
public:
  cElvisTimer(int idP, int programIdP, int lengthP, const char *nameP, const char *channelP, const char *startTimeP, const char *wildcardP);
  virtual ~cElvisTimer();
  cElvisWidgetInfo *Info();
  int Id() { return idM; }
  int ProgramId() { return programIdM; }
  int Length() { return lengthM; }
  const char *Name() { return *nameM; }
  const char *Channel() { return *channelM; }
  const char *StartTime() { return *startTimeM; }
  const char *WildCard() { return *wildcardM; }
};

// --- cElvisTimers ----------------------------------------------------

class cElvisTimers : public cList<cElvisTimer>, public cElvisWidgetTimerCallbackIf {
private:
  static cElvisTimers *instanceS;
  cMutex mutexM;
  // constructor
  cElvisTimers();
  // to prevent copy constructor and assignment
  cElvisTimers(const cElvisTimers&);
  cElvisTimers& operator=(const cElvisTimers&);
public:
  static cElvisTimers *GetInstance();
  static void Destroy();
  virtual ~cElvisTimers();
  virtual void AddTimer(int idP, int programIdP, int lengthP, const char *nameP, const char *channelP, const char *starttimeP, const char *wildcardP);
  bool Update();
  bool Create(int idP, int folderIdP = -1);
  bool Delete(cElvisTimer *timerP);
};

#endif // __ELVIS_TIMERS_H
