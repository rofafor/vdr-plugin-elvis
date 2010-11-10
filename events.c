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
: cThread(*cString::sprintf("cElvisChannel(%s)", nameP ? nameP : "")),
  stateM(0),
  lastUpdateM(0),
  nameM(nameP)
{
}

cElvisChannel::~cElvisChannel()
{
  cThreadLock(this);
  Cancel(3);
}

void cElvisChannel::AddEvent(int idP, const char *nameP, const char *simpleStartTimeP, const char *simpleEndTimeP, const char *startTimeP, const char *endTimeP)
{
  Add(new cElvisEvent(idP, nameP, simpleStartTimeP, simpleEndTimeP, startTimeP, endTimeP));
}

void cElvisChannel::Refresh(bool foregroundP)
{
  lastUpdateM = time(NULL);
  Lock();
  Clear();
  ChangeState();
  Unlock();
  cElvisWidget::GetInstance()->GetEvents(*this, *nameM);
}

bool cElvisChannel::Update(bool waitP)
{
  if (waitP) {
     Refresh(true);
     return (Count() > 0);
     }
  else if ((time(NULL) - lastUpdateM) >= eUpdateInterval)
     Start();

  return false;
}

bool cElvisChannel::StateChanged(int &stateP)
{
  cThreadLock(this);
  bool result = (stateP != stateM);

  stateP = stateM;

  return result;
}

void cElvisChannel::Action()
{
  Refresh();
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
: cThread("cElvisChannels"),
  stateM(0),
  lastUpdateM(0)
{
}

cElvisChannels::~cElvisChannels()
{
  cThreadLock(this);
  Cancel(3);
}

void cElvisChannels::AddChannel(const char *nameP)
{
  cThreadLock(this);
  Add(new cElvisChannel(nameP));
  ChangeState();
}

void cElvisChannels::Refresh(bool foregroundP)
{
  lastUpdateM = time(NULL);
  Lock();
  Clear();
  ChangeState();
  Unlock();
  cElvisWidget::GetInstance()->GetChannels(*this);
}

bool cElvisChannels::Update(bool waitP)
{
  if (waitP) {
     Refresh(true);
     return (Count() > 0);
     }
  else if ((time(NULL) - lastUpdateM) >= eUpdateInterval)
     Start();

  return false;
}

bool cElvisChannels::StateChanged(int &stateP)
{
  cThreadLock(this);
  bool result = (stateP != stateM);

  stateP = stateM;

  return result;
}

void cElvisChannels::Action()
{
  Refresh();
}

bool cElvisChannels::AddTimer(tEventID eventIdP)
{
  cEvent *event = NULL;
  cSchedulesLock SchedulesLock(false, 100);
  debug("cElvisChannels::AddTimer(%d)", eventIdP);
  const cSchedules *Schedules = cSchedules::Schedules(SchedulesLock);
  for (const cSchedule *s = Schedules->First(); s; s = Schedules->Next(s)) {
      for (cEvent *e = s->Events()->First(); e; e = s->Events()->Next(e)) {
          if (e->EventID() == eventIdP) {
             cChannel *c = Channels.GetByChannelID(e->ChannelID(), true);
             info("cElvisChannels::AddTimer(%d): found event '%s' on '%s'", e->EventID(), e->Title(), c->Name());
             event = e;
             break;
             }
          }
      if (event)
         break;
      }
  if (event) {
     cElvisChannel *channel = NULL;
     Update(true);
     for (cElvisChannel *c = First(); c; c = Next(c)) {
         cChannel *c2 = Channels.GetByChannelID(event->ChannelID(), true);
         if (!strcmp(c->Name(), c2->Name())) {
            channel = c;
            info("cElvisChannels::AddTimer(%d): found channel '%s'", event->EventID(), c->Name());
            break;
            }
         }
     if (channel) {
        channel->Update(true);
        for (cElvisEvent *i = channel->cList<cElvisEvent>::First(); i; i = channel->cList<cElvisEvent>::Next(i)) {
            if (!strcmp(event->Title(), i->Name()) && (abs((int)(event->StartTime() - i->StartTimeValue())) < 60)) {
               info("cElvisChannels::AddTimer(%d): creating %d", event->EventID(), i->Id());
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
             cChannel *c = Channels.GetByChannelID(e->ChannelID(), true);
             info("cElvisChannels::DelTimer(%d): found event '%s' on '%s'", e->EventID(), e->Title(), c->Name());
             event = e;
             break;
             }
          }
      if (event)
         break;
      }
  if (event) {
     cElvisChannel *channel = NULL;
     Update(true);
     for (cElvisChannel *c = First(); c; c = Next(c)) {
         cChannel *c2 = Channels.GetByChannelID(event->ChannelID(), true);
         if (!strcmp(c->Name(), c2->Name())) {
            channel = c;
            info("cElvisChannels::DelTimer(%d): found channel '%s'", event->EventID(), channel->Name());
            break;
            }
         }
     if (channel) {
        channel->Update(true);
        for (cElvisEvent *i = channel->cList<cElvisEvent>::First(); i; i = channel->cList<cElvisEvent>::Next(i)) {
            if (!strcmp(event->Title(), i->Name()) && (abs((int)(event->StartTime() - i->StartTimeValue())) < 60)) {
               info("cElvisChannels::DelTimer(%d): deleting %d", event->EventID(), i->Id());
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
  stateM(0),
  lastUpdateM(0)
{
}

cElvisTopEvents::~cElvisTopEvents()
{
  cThreadLock(this);
  Cancel(3);
}

void cElvisTopEvents::AddEvent(int idP, const char *nameP, const char *channelP, const char *startTimeP, const char *endTimeP)
{
  cThreadLock(this);
  Add(new cElvisEvent(idP, nameP, channelP, startTimeP, endTimeP));
  ChangeState();
}

void cElvisTopEvents::Refresh(bool foregroundP)
{
  lastUpdateM = time(NULL);
  Lock();
  Clear();
  ChangeState();
  Unlock();
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
  cThreadLock(this);
  bool result = (stateP != stateM);

  stateP = stateM;

  return result;
}

void cElvisTopEvents::Action()
{
  Refresh();
}

