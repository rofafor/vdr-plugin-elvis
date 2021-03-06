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
  bool taggedM;
  int idM;
  int lengthM;
  cString nameM;
  cString channelM;
  cString startTimeM;
  cString wildcardM;
  cElvisWidgetEventInfo *infoM;
  time_t startTimeValueM;
  // to prevent default constructor
  cElvisTimer();
  // to prevent copy constructor and assignment
  cElvisTimer(const cElvisTimer&);
  cElvisTimer &operator=(const cElvisTimer &);
public:
  cElvisTimer(int idP, int lengthP, const char *nameP, const char *channelP, const char *startTimeP, const char *wildcardP);
  virtual ~cElvisTimer();
  cElvisWidgetEventInfo *Info();
  void Tag(bool onOffP) { taggedM = onOffP; }
  bool IsTagged() { return taggedM; }
  int Id() { return idM; }
  int Length() { return lengthM; }
  const char *Name() { return *nameM; }
  const char *Channel() { return *channelM; }
  const char *StartTime() { return *startTimeM; }
  const char *WildCard() { return *wildcardM; }
  time_t StartTimeValue() { return startTimeValueM; }
};

// --- cElvisTimers ----------------------------------------------------

class cElvisTimers : public cList<cElvisTimer>, public cThread, public cElvisWidgetTimerCallbackIf {
private:
  static cElvisTimers *instanceS;
  enum {
    eUpdateInterval = 900 // 15min
  };
  int stateM;
  time_t lastUpdateM;
  void Refresh(bool foregroundP = false);
  cElvisTimer *GetTimer(int idP);
  // constructor
  cElvisTimers();
  // to prevent copy constructor and assignment
  cElvisTimers(const cElvisTimers&);
  cElvisTimers& operator=(const cElvisTimers&);
protected:
  void Action();
public:
  static cElvisTimers *GetInstance();
  static void Destroy();
  virtual ~cElvisTimers();
  virtual void AddTimer(int idP, int lengthP, const char *nameP, const char *channelP, const char *startTimeP, const char *wildcardP);
  bool Update(bool waitP = false);
  void ChangeState() { ++stateM; }
  bool StateChanged(int &stateP);
  bool Create(int idP, int folderIdP = -1);
  bool Delete(int idP);
  bool Delete(cElvisTimer *timerP);
};

#endif // __ELVIS_TIMERS_H
