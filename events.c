/*
 * events.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "config.h"
#include "events.h"

cElvisEvent::cElvisEvent(int idP, const char *nameP, const char *simpleStartTimeP, const char *simpleEndTimeP, const char *startTimeP, const char *endTimeP)
: idM(idP),
  nameM(nameP),
  channelM(NULL),
  simpleStartTimeM(simpleStartTimeP),
  simpleEndTimeM(simpleEndTimeP),
  startTimeM(startTimeP),
  endTimeM(endTimeP),
  infoM(NULL),
  startTimeValueM(strtotime(startTimeP))
{
}

cElvisEvent::cElvisEvent(int idP, const char *nameP, const char *channelP, const char *startTimeP, const char *endTimeP)
: idM(idP),
  nameM(nameP),
  channelM(channelP),
  simpleStartTimeM(NULL),
  simpleEndTimeM(NULL),
  startTimeM(startTimeP),
  endTimeM(endTimeP),
  infoM(NULL),
  startTimeValueM(strtotime(startTimeP))
{
}

cElvisEvent::~cElvisEvent()
{
  DELETE_POINTER(infoM);
}

cElvisWidgetInfo *cElvisEvent::Info()
{
  if (!infoM)
     infoM = cElvisWidget::GetInstance()->GetEventInfo(idM);

  return infoM;
}

// --- cElvisChannel ---------------------------------------------------

cElvisChannel::cElvisChannel(const char *nameP)
: nameM(nameP)
{
}

cElvisChannel::~cElvisChannel()
{
  cMutexLock(mutexM);
  Clear();
}

void cElvisChannel::AddEvent(int idP, const char *nameP, const char *simpleStarttimeP, const char *simpleEndtimeP, const char *starttimeP, const char *endtimeP)
{
  Add(new cElvisEvent(idP, nameP, simpleStarttimeP, simpleEndtimeP, starttimeP, endtimeP));
}

bool cElvisChannel::Update()
{
  cMutexLock(mutexM);
  Clear();
  cElvisWidget::GetInstance()->GetEvents(*this, *nameM);
  return (Count() > 0);
}

// --- cElvisChannels --------------------------------------------------

cElvisChannels *cElvisChannels::instanceS = NULL;

cElvisChannels *cElvisChannels::GetInstance()
{
  if (!instanceS)
     instanceS = new cElvisChannels();

  return instanceS;
}

void cElvisChannels::Destroy()
{
  DELETE_POINTER(instanceS);
}

cElvisChannels::cElvisChannels()
{
}

cElvisChannels::~cElvisChannels()
{
  cMutexLock(mutexM);
  Clear();
}

void cElvisChannels::AddChannel(const char *nameP)
{
  Add(new cElvisChannel(nameP));
}

bool cElvisChannels::Update()
{
  cMutexLock(mutexM);
  Clear();
  cElvisWidget::GetInstance()->GetChannels(*this);
  return (Count() > 0);
}

// --- cElvisTopEvents -------------------------------------------------

cElvisTopEvents *cElvisTopEvents::instanceS = NULL;

cElvisTopEvents *cElvisTopEvents::GetInstance()
{
  if (!instanceS)
     instanceS = new cElvisTopEvents();

  return instanceS;
}

void cElvisTopEvents::Destroy()
{
  DELETE_POINTER(instanceS);
}

cElvisTopEvents::cElvisTopEvents()
{
}

cElvisTopEvents::~cElvisTopEvents()
{
  cMutexLock(mutexM);
  Clear();
}

void cElvisTopEvents::AddEvent(int idP, const char *nameP, const char *channelP, const char *starttimeP, const char *endtimeP)
{
  Add(new cElvisEvent(idP, nameP, channelP, starttimeP, endtimeP));
}

bool cElvisTopEvents::Update()
{
  cMutexLock(mutexM);
  Clear();
  cElvisWidget::GetInstance()->GetTopEvents(*this);
  return (Count() > 0);
}

