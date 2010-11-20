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
: taggedM(true),
  idM(idP),
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
  stateM(0),
  lastUpdateM(0)
{
}

cElvisSearchTimers::~cElvisSearchTimers()
{
  cThreadLock(this);
  Cancel(3);
}

void cElvisSearchTimers::AddSearchTimer(int idP, const char *folderP, const char *addedP, const char *channelP, const char *wildcardP)
{
  cThreadLock(this);
  cElvisSearchTimer *timer = GetSearchTimer(idP);
  if (timer)
     timer->Tag(true);
  else {
     Add(new cElvisSearchTimer(idP, folderP, addedP, channelP, wildcardP));
     ChangeState();
     }
}

bool cElvisSearchTimers::Create(cElvisSearchTimer *timerP, const char *channelP, const char *wildcardP, int folderIdP)
{
  cThreadLock(this);
  if (cElvisWidget::GetInstance()->AddSearchTimer(channelP, wildcardP, folderIdP, timerP ? timerP->Id() : -1)) {
     Start();
     return true;
     }

  return false;
}

bool cElvisSearchTimers::Delete(cElvisSearchTimer *timerP)
{
  cThreadLock(this);
  if (timerP && cElvisWidget::GetInstance()->RemoveSearchTimer(timerP->Id())) {
     Del(timerP);
     ChangeState();
     //Start();
     return true;
     }

  return false;
}

cElvisSearchTimer *cElvisSearchTimers::GetSearchTimer(int idP)
{
  for (cElvisSearchTimer *i = First(); i; i = Next(i)) {
      if (i->Id() == idP)
         return i;
      }

  return NULL;
}

void cElvisSearchTimers::Refresh(bool foregroundP)
{
  lastUpdateM = time(NULL);
  Lock();
  if (foregroundP) {
     Clear();
     ChangeState();
     }
  else {
     for (cElvisSearchTimer *i = First(); i; i = Next(i))
         i->Tag(false);
     }
  Unlock();
  cElvisWidget::GetInstance()->GetSearchTimers(*this);
  Lock();
  for (cElvisSearchTimer *i = First(); i; i = Next(i)) {
      if (!i->IsTagged())
         Del(i);
      }
  ChangeState();
  Unlock();
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
  cThreadLock(this);
  bool result = (stateP != stateM);

  stateP = stateM;

  return result;
}

void cElvisSearchTimers::Action()
{
  Refresh();
}
