/*
 * searchtimers.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "config.h"
#include "searchtimers.h"

// --- cElvisSearchTimer -----------------------------------------------

cElvisSearchTimer::cElvisSearchTimer(int idP, const char *folderP, const char *addedP, const char *channelP, const char *wildcardP)
: idM(idP),
  folderM(folderP),
  addedM(addedP),
  channelM(channelP),
  wildcardM(wildcardP)
{
}

cElvisSearchTimer::~cElvisSearchTimer()
{
}

// --- cElvisSearchTimers ----------------------------------------------

cElvisSearchTimers *cElvisSearchTimers::instanceS = NULL;

cElvisSearchTimers *cElvisSearchTimers::GetInstance()
{
  if (!instanceS)
     instanceS = new cElvisSearchTimers();

  return instanceS;
}

void cElvisSearchTimers::Destroy()
{
  DELETE_POINTER(instanceS);
}

cElvisSearchTimers::cElvisSearchTimers()
: cThread("cElvisSearchTimers"),
  mutexM(),
  stateM(0),
  lastUpdateM(0)
{
  Clear();
}

cElvisSearchTimers::~cElvisSearchTimers()
{
  cMutexLock(mutexM);

  Cancel(3);
}

void cElvisSearchTimers::AddSearchTimer(int idP, const char *folderP, const char *addedP, const char *channelP, const char *wildcardP)
{
  cMutexLock(mutexM);
  Add(new cElvisSearchTimer(idP, folderP, addedP, channelP, wildcardP));
  ChangeState();
}

bool cElvisSearchTimers::Create(cElvisSearchTimer *timerP, const char *channelP, const char *wildcardP, int folderIdP)
{
  cMutexLock(mutexM);
  if (cElvisWidget::GetInstance()->AddSearchTimer(channelP, wildcardP, folderIdP, timerP ? timerP->Id() : -1)) {
     ChangeState();
     return true;
     }

  return false;
}

bool cElvisSearchTimers::Delete(cElvisSearchTimer *timerP)
{
  cMutexLock(mutexM);
  if (timerP && cElvisWidget::GetInstance()->RemoveSearchTimer(timerP->Id())) {
     ChangeState();
     return true;
     }

  return false;
}

void cElvisSearchTimers::Refresh(bool foregroundP)
{
  lastUpdateM = time(NULL);
  mutexM.Lock();
  Clear();
  ChangeState();
  mutexM.Unlock();
  cElvisWidget::GetInstance()->GetSearchTimers(*this);
}

bool cElvisSearchTimers::Update(bool waitP)
{
  if (waitP) {
     Refresh(true);
     return (Count() > 0);
     }
  else if ((time(NULL) - lastUpdateM) >= eUpdateInterval)
     Start();

  return false;
}

bool cElvisSearchTimers::StateChanged(int &stateP)
{
  cMutexLock(mutexM);
  bool result = (stateP != stateM);

  stateP = stateM;

  return result;
}

void cElvisSearchTimers::Action()
{
  Refresh();
}
