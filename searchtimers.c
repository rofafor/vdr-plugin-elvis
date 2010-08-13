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
{
  Clear();
}

cElvisSearchTimers::~cElvisSearchTimers()
{
}

void cElvisSearchTimers::AddSearchTimer(int idP, const char *folderP, const char *addedP, const char *channelP, const char *wildcardP)
{
  Add(new cElvisSearchTimer(idP, folderP, addedP, channelP, wildcardP));
}

bool cElvisSearchTimers::Update()
{
  cMutexLock(mutexM);
  Clear();
  cElvisWidget::GetInstance()->GetSearchTimers(*this);
  return (Count() > 0);
}

bool cElvisSearchTimers::Create(cElvisSearchTimer *timerP, const char *channelP, const char *wildcardP, int folderIdP)
{
  cMutexLock(mutexM);
  if (cElvisWidget::GetInstance()->AddSearchTimer(channelP, wildcardP, folderIdP, timerP ? timerP->Id() : -1)) {
     //Update();
     return true;
     }

  return false;
}

bool cElvisSearchTimers::Delete(cElvisSearchTimer *timerP)
{
  cMutexLock(mutexM);
  if (timerP && cElvisWidget::GetInstance()->RemoveSearchTimer(timerP->Id())) {
     //Del(timerP);
     //Update();
     return true;
     }

  return false;
}
