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

cElvisTimer::cElvisTimer(int idP, int lengthP, const char *nameP, const char *channelP, const char *startTimeP, const char *wildcardP)
: taggedM(true),
  idM(idP),
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

cElvisWidgetEventInfo *cElvisTimer::Info()
{
  if (!infoM)
     infoM = cElvisWidget::GetInstance()->GetEventInfo(idM);

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
  LOCK_THREAD;
  Cancel(3);
}

void cElvisTimers::AddTimer(int idP, int lengthP, const char *nameP, const char *channelP, const char *startTimeP, const char *wildcardP)
{
  LOCK_THREAD;
  cElvisTimer *timer = GetTimer(idP);
  if (timer)
     timer->Tag(true);
  else {
     Add(new cElvisTimer(idP, lengthP, nameP, channelP, startTimeP, wildcardP));
     ChangeState();
     }
}

bool cElvisTimers::Create(int idP, int folderIdP)
{
  LOCK_THREAD;
  if (cElvisWidget::GetInstance()->AddTimer(idP, folderIdP)) {
     Start();
     return true;
     }

  return false;
}

bool cElvisTimers::Delete(int idP)
{
  LOCK_THREAD;
  Update();
  for (cElvisTimer *timer = First(); timer; timer = Next(timer)) {
      if (timer->Id() == idP)
         return Delete(timer);
      }

  return false;
}

bool cElvisTimers::Delete(cElvisTimer *timerP)
{
  LOCK_THREAD;
  if (timerP && cElvisWidget::GetInstance()->RemoveTimer(timerP->Id())) {
     Del(timerP);
     ChangeState();
     return true;
     }

  return false;
}

cElvisTimer *cElvisTimers::GetTimer(int idP)
{
  for (cElvisTimer *i = First(); i; i = Next(i)) {
      if (i->Id() == idP)
         return i;
      }

  return NULL;
}

void cElvisTimers::Refresh(bool foregroundP)
{
  lastUpdateM = time(NULL);
  {
    LOCK_THREAD;
    if (foregroundP) {
       Clear();
       ChangeState();
       }
    else {
       for (cElvisTimer *i = First(); i; i = Next(i))
           i->Tag(false);
       }
  }
  cElvisWidget::GetInstance()->GetTimers(*this);
  {
    LOCK_THREAD;
    for (cElvisTimer *i = First(); i; i = Next(i)) {
        if (!i->IsTagged())
           Del(i);
        }
    ChangeState();
  }
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
  LOCK_THREAD;
  bool result = (stateP != stateM);

  stateP = stateM;

  return result;
}

void cElvisTimers::Action()
{
  Refresh();
}
