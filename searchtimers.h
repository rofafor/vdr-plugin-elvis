/*
 * searchtimers.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_SEARCHTIMERS_H
#define __ELVIS_SEARCHTIMERS_H

#include <vdr/thread.h>
#include <vdr/tools.h>

#include "widget.h"

// --- cElvisSearchTimer -----------------------------------------------

class cElvisSearchTimer : public cListObject {
  friend class cElvisSearchTimers;
private:
  int idM;
  cString folderM;
  cString addedM;
  cString channelM;
  cString wildcardM;
  // to prevent default constructor
  cElvisSearchTimer();
  // to prevent copy constructor and assignment
  cElvisSearchTimer(const cElvisSearchTimer&);
  cElvisSearchTimer &operator=(const cElvisSearchTimer &);
public:
  cElvisSearchTimer(int idP, const char *folderP, const char *added, const char *channelP, const char *wildcardP);
  virtual ~cElvisSearchTimer();
  int Id() { return idM; }
  const char *Folder() { return *folderM; }
  const char *Added() { return *addedM; }
  const char *Channel() { return *channelM; }
  const char *Wildcard() { return *wildcardM; }
};

// --- cElvisSearchTimers ----------------------------------------------

class cElvisSearchTimers : public cList<cElvisSearchTimer>, public cThread, public cElvisWidgetSearchTimerCallbackIf {
private:
  static cElvisSearchTimers *instanceS;
  enum {
    eUpdateInterval = 900 // 15min
  };
  cMutex mutexM;
  int stateM;
  time_t lastUpdateM;
  void Refresh(bool foregroundP = false);
  // constructor
  cElvisSearchTimers();
  // to prevent copy constructor and assignment
  cElvisSearchTimers(const cElvisSearchTimers&);
  cElvisSearchTimers& operator=(const cElvisSearchTimers&);
protected:
  void Action();
public:
  static cElvisSearchTimers *GetInstance();
  static void Destroy();
  virtual ~cElvisSearchTimers();
  virtual void AddSearchTimer(int idP, const char *folderP, const char *addedP, const char *channelP, const char *wildcardP);
  bool Update(bool waitP = false);
  void ChangeState() { ++stateM; }
  bool StateChanged(int &stateP);
  bool Create(cElvisSearchTimer *timerP, const char *channelP, const char *wildcardP, int folderIdP);
  bool Delete(cElvisSearchTimer *timerP);
};

#endif // __ELVIS_SEARCHTIMERS_H
