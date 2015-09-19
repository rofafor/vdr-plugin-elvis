/*
 * events.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "config.h"
#include "log.h"
#include "timers.h"
#include "events.h"

cElvisEvent::cElvisEvent(int idP, const char *nameP, const char *simpleStartTimeP, const char *simpleEndTimeP, const char *startTimeP, const char *endTimeP, const char *descriptionP)
: taggedM(true),
  idM(idP),
  nameM(nameP),
  channelM(NULL),
  simpleStartTimeM(simpleStartTimeP),
  simpleEndTimeM(simpleEndTimeP),
  startTimeM(startTimeP),
  endTimeM(endTimeP),
  descriptionM(descriptionP),
  infoM(NULL),
  startTimeValueM(strtotime(startTimeP)),
  endTimeValueM(strtotime(endTimeM)),
  lengthM(int(endTimeValueM - startTimeValueM) / 60)
{
}

cElvisEvent::cElvisEvent(int idP, const char *nameP, const char *channelP, const char *startTimeP, const char *endTimeP)
: taggedM(true),
  idM(idP),
  nameM(nameP),
  channelM(channelP),
  simpleStartTimeM(NULL),
  simpleEndTimeM(NULL),
  startTimeM(startTimeP),
  endTimeM(endTimeP),
  descriptionM(NULL),
  infoM(NULL),
  startTimeValueM(strtotime(startTimeP)),
  endTimeValueM(0),
  lengthM(0)
{
}

cElvisEvent::~cElvisEvent()
{
  DELETE_POINTER(infoM);
}

cElvisWidgetEventInfo *cElvisEvent::Info()
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
}

void cElvisChannel::AddEvent(int idP, const char *nameP, const char *simpleStartTimeP, const char *simpleEndTimeP, const char *startTimeP, const char *endTimeP, const char *descriptionP)
{
  Add(new cElvisEvent(idP, nameP, simpleStartTimeP, simpleEndTimeP, startTimeP, endTimeP, descriptionP));
}

cElvisEvent *cElvisChannel::GetEvent(int idP)
{
  for (cElvisEvent *i = cList<cElvisEvent>::First(); i; i = cList<cElvisEvent>::Next(i)) {
      if (i->Id() == idP)
         return i;
      }

  return NULL;
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
  LOCK_THREAD;
  Cancel(3);
}

void cElvisChannels::AddEvent(const char *channelP, int idP, const char *nameP, const char *simpleStartTimeP, const char *simpleEndTimeP, const char *startTimeP, const char *endTimeP, const char *descriptionP)
{
  cElvisChannel *channel = NULL;
  LOCK_THREAD;
  for (cElvisChannel *c = First(); c; c = Next(c)) {
      if (!strcmp(c->Name(), channelP)) {
         channel = c;
         break;
         }
      }
  if (!channel) {
     channel = new cElvisChannel(channelP);
     Add(channel);
     }
  if (channel)
     channel->AddEvent(idP, nameP, simpleStartTimeP, simpleEndTimeP, startTimeP, endTimeP, descriptionP);
  ChangeState();
}

void cElvisChannels::Refresh(bool foregroundP)
{
  lastUpdateM = time(NULL);
  {
    LOCK_THREAD;
    Clear();
    ChangeState();
  }
  cElvisWidget::GetInstance()->GetEPG(*this);
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
  LOCK_THREAD;
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
  const cEvent *event = NULL;
  const cChannel *channel = NULL;
  debug7("%s (%d)", __PRETTY_FUNCTION__, eventIdP);
  LOCK_CHANNELS_READ;
  LOCK_SCHEDULES_WRITE;
  for (const cSchedule *s = Schedules->First(); s; s = Schedules->Next(s)) {
      for (const cEvent *e = s->Events()->First(); e; e = s->Events()->Next(e)) {
          if (e->EventID() == eventIdP) {
             channel = Channels->GetByChannelID(e->ChannelID(), true);
             event = e;
             info("%s (%d) Found event='%s' channel='%s'", __PRETTY_FUNCTION__, e->EventID(), e->Title(), channel->Name());
             break;
             }
          }
      if (event && channel)
         break;
      }
  if (event && channel) {
     if ((time(NULL) - lastUpdateM) >= eUpdateInterval)
        Update(true);
     LOCK_THREAD;
     for (cElvisChannel *c = First(); c; c = Next(c)) {
         if (!strcmp(c->Name(), channel->Name())) {
            for (cElvisEvent *i = c->cList<cElvisEvent>::First(); i; i = c->cList<cElvisEvent>::Next(i)) {
                if (!strcmp(event->Title(), i->Name()) && (abs((int)(event->StartTime() - i->StartTimeValue())) < 60)) {
                   info("%s (%d) Creating id=%d", __PRETTY_FUNCTION__, event->EventID(), i->Id());
                   return cElvisTimers::GetInstance()->Create(i->Id());
                   }
                }
            break;
            }
         }
     }

  return false;
}

bool cElvisChannels::DelTimer(tEventID eventIdP)
{
  const cEvent *event = NULL;
  const cChannel *channel = NULL;
  debug7("%s (%d)", __PRETTY_FUNCTION__, eventIdP);
  LOCK_CHANNELS_READ;
  LOCK_SCHEDULES_WRITE;
  for (cSchedule *s = Schedules->First(); s; s = Schedules->Next(s)) {
      for (const cEvent *e = s->Events()->First(); e; e = s->Events()->Next(e)) {
          if (e->EventID() == eventIdP) {
             channel = Channels->GetByChannelID(e->ChannelID(), true);
             event = e;
             info("%s (%d) Found event '%s' on '%s'", __PRETTY_FUNCTION__, e->EventID(), e->Title(), channel->Name());
             break;
             }
          }
      if (event && channel)
         break;
      }
  if (event && channel) {
     if ((time(NULL) - lastUpdateM) >= eUpdateInterval)
        Update(true);
     LOCK_THREAD;
     for (cElvisChannel *c = First(); c; c = Next(c)) {
         if (!strcmp(c->Name(), channel->Name())) {
            for (cElvisEvent *i = c->cList<cElvisEvent>::First(); i; i = c->cList<cElvisEvent>::Next(i)) {
                if (!strcmp(event->Title(), i->Name()) && (abs((int)(event->StartTime() - i->StartTimeValue())) < 60)) {
                   info("%s (%d) Deleting %d", __PRETTY_FUNCTION__, event->EventID(), i->Id());
                   return cElvisTimers::GetInstance()->Delete(i->Id());
                   }
                }
            break;
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
  LOCK_THREAD;
  Cancel(3);
}

void cElvisTopEvents::AddEvent(int idP, const char *nameP, const char *channelP, const char *startTimeP, const char *endTimeP)
{
  LOCK_THREAD;
  //cElvisEvent *event = GetEvent(idP);
  //if (event)
  //   event->Tag(true);
  //else {
     Add(new cElvisEvent(idP, nameP, channelP, startTimeP, endTimeP));
     ChangeState();
  //   }
}

cElvisEvent *cElvisTopEvents::GetEvent(int idP)
{
  for (cElvisEvent *i = First(); i; i = Next(i)) {
      if (i->Id() == idP)
         return i;
      }

  return NULL;
}

void cElvisTopEvents::Refresh(bool foregroundP)
{
  lastUpdateM = time(NULL);
  {
    LOCK_THREAD;
    //if (foregroundP) {
       Clear();
       ChangeState();
    //   }
    //else {
    //   for (cElvisEvent *i = First(); i; i = Next(i))
    //       i->Tag(false);
    //   }
  }
  cElvisWidget::GetInstance()->GetTopEvents(*this);
  //{
    //LOCK_THREAD;
    //for (cElvisEvent *i = First(); i; i = Next(i)) {
    //    if (!i->IsTagged())
    //       Del(i);
    //    }
    //ChangeState();
  //}
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
  LOCK_THREAD;
  bool result = (stateP != stateM);

  stateP = stateM;

  return result;
}

void cElvisTopEvents::Action()
{
  Refresh();
}

