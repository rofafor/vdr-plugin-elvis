/*
 * events.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "config.h"
#include "timers.h"
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

bool cElvisChannels::AddTimer(tEventID eventIdP)
{
  cEvent *event = NULL;
  cSchedulesLock SchedulesLock(false, 100);
  error("cElvisChannels::AddTimer(%d)", eventIdP);
  const cSchedules *Schedules = cSchedules::Schedules(SchedulesLock);
  for (const cSchedule *s = Schedules->First(); s; s = Schedules->Next(s)) {
      for (cEvent *e = s->Events()->First(); e; e = s->Events()->Next(e)) {
          if (e->EventID() == eventIdP) {
             event = e;
             debug("cElvisChannels::AddTimer(%d): found event", eventIdP);
             break;
             }
          }
      }
  if (event) {
     cElvisChannel *channel = NULL;
     Update();
     for (cElvisChannel *c = First(); c; c = Next(c)) {
         cChannel *c2 = Channels.GetByChannelID(event->ChannelID(), true);
         if (!strcmp(c->Name(), c2->Name())) {
            channel = c;
            debug("cElvisChannels::AddTimer(%d): found channel '%s'", eventIdP, c->Name());
            break;
            }
         }
     if (channel) {
        channel->Update();
        for (cElvisEvent *i = channel->cList<cElvisEvent>::First(); i; i = channel->cList<cElvisEvent>::Next(i)) {
            if (!strcmp(event->Title(), i->Name()) && (abs((int)(event->StartTime() - i->StartTimeValue())) < 60)) {
               info("cElvisChannels::AddTimer(%d): creating %d", eventIdP, i->Id());
               return cElvisTimers::GetInstance()->Create(i->Id());
               }
            }
        }
     }

  return false;
}

bool cElvisChannels::DelTimer(tEventID eventIdP)
{
  cEvent *event = NULL;
  cSchedulesLock SchedulesLock(false, 100);
  debug("cElvisChannels::DelTimer(%d)", eventIdP);
  const cSchedules *Schedules = cSchedules::Schedules(SchedulesLock);
  for (const cSchedule *s = Schedules->First(); s; s = Schedules->Next(s)) {
      for (cEvent *e = s->Events()->First(); e; e = s->Events()->Next(e)) {
          if (e->EventID() == eventIdP) {
             event = e;
             debug("cElvisChannels::DelTimer(): found event %d", eventIdP);
             break;
             }
          }
      }
  if (event) {
     cElvisChannel *channel = NULL;
     Update();
     for (cElvisChannel *c = First(); c; c = Next(c)) {
         cChannel *c2 = Channels.GetByChannelID(event->ChannelID(), true);
         if (!strcmp(c->Name(), c2->Name())) {
            channel = c;
            debug("cElvisChannels::DelTimer(): found channel '%s'", channel->Name());
            break;
            }
         }
     if (channel) {
        channel->Update();
        for (cElvisEvent *i = channel->cList<cElvisEvent>::First(); i; i = channel->cList<cElvisEvent>::Next(i)) {
            if (!strcmp(event->Title(), i->Name()) && (abs((int)(event->StartTime() - i->StartTimeValue())) < 60)) {
               info("cElvisChannels::DelTimer(%d): deleting %d", eventIdP, i->Id());
               return cElvisTimers::GetInstance()->Delete(i->Id());
               }
            }
        }
     }

  return false;
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
: cThread("cElvisTimers"),
  mutexM(),
  stateM(0),
  lastUpdateM(0)
{
  Clear();
}

cElvisTopEvents::~cElvisTopEvents()
{
  cMutexLock(mutexM);

  Cancel(3);
}

void cElvisTopEvents::AddEvent(int idP, const char *nameP, const char *channelP, const char *starttimeP, const char *endtimeP)
{
  Add(new cElvisEvent(idP, nameP, channelP, starttimeP, endtimeP));
  ChangeState();
}

void cElvisTopEvents::Refresh(bool foregroundP)
{
  lastUpdateM = time(NULL);
  mutexM.Lock();
  Clear();
  ChangeState();
  mutexM.Unlock();
  cElvisWidget::GetInstance()->GetTopEvents(*this);
}

bool cElvisTopEvents::Update(bool waitP)
{
  if (waitP) {
     Refresh(true);
     return (Count() > 0);
     }
  else if ((time(NULL) - lastUpdateM) >= eUpdateInterval)
     Start();

  return false;
}

bool cElvisTopEvents::StateChanged(int &stateP)
{
  cMutexLock(mutexM);
  bool result = (stateP != stateM);

  stateP = stateM;

  return result;
}

void cElvisTopEvents::Action()
{
  Refresh();
}

