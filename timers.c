/*
 * timers.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "config.h"
#include "timers.h"

// --- cElvisTimer -----------------------------------------------------

cElvisTimer::cElvisTimer(int idP, int programIdP, int lengthP, const char *nameP, const char *channelP, const char *startTimeP, const char *wildcardP)
: idM(idP),
  programIdM(programIdP),
  lengthM(lengthP),
  nameM(nameP),
  channelM(channelP),
  startTimeM(startTimeP),
  wildcardM(wildcardP),
  infoM(NULL),
  startTimeValueM(strtotime(startTimeP))
{
}

cElvisTimer::~cElvisTimer()
{
  DELETE_POINTER(infoM);
}

cElvisWidgetInfo *cElvisTimer::Info()
{
  if (!infoM)
     infoM = cElvisWidget::GetInstance()->GetEventInfo(programIdM);

  return infoM;
}

// --- cElvisTimers ----------------------------------------------------

cElvisTimers *cElvisTimers::instanceS = NULL;

cElvisTimers *cElvisTimers::GetInstance()
{
  if (!instanceS)
     instanceS = new cElvisTimers();

  return instanceS;
}

void cElvisTimers::Destroy()
{
  DELETE_POINTER(instanceS);
}

cElvisTimers::cElvisTimers()
: cThread("cElvisTimers"),
  stateM(0),
  lastUpdateM(0)
{
}

cElvisTimers::~cElvisTimers()
{
  cThreadLock(this);
  Cancel(3);
}

void cElvisTimers::AddTimer(int idP, int programIdP, int lengthP, const char *nameP, const char *channelP, const char *startTimeP, const char *wildcardP)
{
  cThreadLock(this);
  Add(new cElvisTimer(idP, programIdP, lengthP, nameP, channelP, startTimeP, wildcardP));
  ChangeState();
}

bool cElvisTimers::Create(int idP, int folderIdP)
{
  cThreadLock(this);
  if (cElvisWidget::GetInstance()->AddTimer(idP, folderIdP)) {
     Start();
     return true;
     }

  return false;
}

bool cElvisTimers::Delete(int idP)
{
  cThreadLock(this);
  Update();
  for (cElvisTimer *timer = First(); timer; timer = Next(timer)) {
      if (timer->ProgramId() == idP)
         return Delete(timer);
      }

  return false;
}

bool cElvisTimers::Delete(cElvisTimer *timerP)
{
  cThreadLock(this);
  if (timerP && cElvisWidget::GetInstance()->RemoveTimer(timerP->Id())) {
     Del(timerP);
     ChangeState();
     //Start();
     return true;
     }

  return false;
}

void cElvisTimers::Refresh(bool foregroundP)
{
  lastUpdateM = time(NULL);
  Lock();
  Clear();
  ChangeState();
  Unlock();
  cElvisWidget::GetInstance()->GetTimers(*this);
}

bool cElvisTimers::Update(bool waitP)
{
  if (waitP) {
     Refresh(true);
     return (Count() > 0);
     }
  else if ((time(NULL) - lastUpdateM) >= eUpdateInterval)
     Start();

  return false;
}

bool cElvisTimers::StateChanged(int &stateP)
{
  cThreadLock(this);
  bool result = (stateP != stateM);

  stateP = stateM;

  return result;
}

void cElvisTimers::Action()
{
  Refresh();
}
