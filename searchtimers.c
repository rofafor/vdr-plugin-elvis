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
  LOCK_THREAD;
  Cancel(3);
}

void cElvisSearchTimers::AddSearchTimer(int idP, const char *folderP, const char *addedP, const char *channelP, const char *wildcardP)
{
  LOCK_THREAD;
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
  LOCK_THREAD;
  if (cElvisWidget::GetInstance()->AddSearchTimer(channelP, wildcardP, folderIdP, timerP ? timerP->Id() : -1)) {
     Start();
     return true;
     }

  return false;
}

bool cElvisSearchTimers::Delete(cElvisSearchTimer *timerP)
{
  LOCK_THREAD;
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
  {
    LOCK_THREAD;
    if (foregroundP) {
       Clear();
       ChangeState();
       }
    else {
       for (cElvisSearchTimer *i = First(); i; i = Next(i))
           i->Tag(false);
       }
  }
  cElvisWidget::GetInstance()->GetSearchTimers(*this);
  {
    LOCK_THREAD;
    for (cElvisSearchTimer *i = First(); i; i = Next(i)) {
        if (!i->IsTagged())
           Del(i);
        }
    ChangeState();
  }
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
  LOCK_THREAD;
  bool result = (stateP != stateM);

  stateP = stateM;

  return result;
}

void cElvisSearchTimers::Action()
{
  Refresh();
}
