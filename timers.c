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
  infoM(NULL)
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
{
  Clear();
}

cElvisTimers::~cElvisTimers()
{
}

void cElvisTimers::AddTimer(int idP, int programIdP, int lengthP, const char *nameP, const char *channelP, const char *starttimeP, const char *wildcardP)
{
  Add(new cElvisTimer(idP, programIdP, lengthP, nameP, channelP, starttimeP, wildcardP));
}

bool cElvisTimers::Update()
{
  cMutexLock(mutexM);
  Clear();
  cElvisWidget::GetInstance()->GetTimers(*this);
  return (Count() > 0);
}

bool cElvisTimers::Create(int idP, int folderIdP)
{
  cMutexLock(mutexM);
  if (cElvisWidget::GetInstance()->AddTimer(idP, folderIdP)) {
     //Update();
     return true;
     }

  return false;
}

bool cElvisTimers::Delete(cElvisTimer *timerP)
{
  cMutexLock(mutexM);
  if (timerP && cElvisWidget::GetInstance()->RemoveTimer(timerP->Id())) {
     //Del(timerP);
     //Update();
     return true;
     }

  return false;
}
