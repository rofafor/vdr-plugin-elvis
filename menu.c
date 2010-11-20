/*
 * menu.c: Elvis plugin for the Video Disk Recorder
 *
 *
 */

#include <vdr/status.h>
#include <vdr/remote.h>
#include <vdr/menuitems.h>
#include <vdr/interface.h>

#include "common.h"
#include "fetch.h"
#include "player.h"
#include "resume.h"
#include "menu.h"

// --- cElvisRecordingInfoMenu -----------------------------------------

cElvisRecordingInfoMenu::cElvisRecordingInfoMenu(const char *urlP, const char *nameP, const char *descriptionP, const char *startTimeP, unsigned int lengthP)
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), trVDR("Recordings"))),
  urlM(urlP),
  nameM(nameP),
  descriptionM(descriptionP),
  startTimeM(startTimeP),
  lengthM(lengthP)
{
  SetHelp(trVDR("Button$Play"), tr("Button$Fetch"), NULL, NULL);
}

void cElvisRecordingInfoMenu::Display()
{
  cOsdMenu::Display();
  DisplayMenu()->SetText(*descriptionM, false);
  cStatus::MsgOsdTextItem(*descriptionM);
}

eOSState cElvisRecordingInfoMenu::ProcessKey(eKeys keyP)
{
  if (!HasSubMenu()) {
     switch (keyP) {
       case kUp|k_Repeat:
       case kUp:
       case kDown|k_Repeat:
       case kDown:
       case kLeft|k_Repeat:
       case kLeft:
       case kRight|k_Repeat:
       case kRight:
            DisplayMenu()->Scroll(NORMALKEY(keyP) == kUp || NORMALKEY(keyP) == kLeft, NORMALKEY(keyP) == kLeft || NORMALKEY(keyP) == kRight);
            cStatus::MsgOsdTextItem(NULL, NORMALKEY(keyP) == kUp);
            return osContinue;
       case kInfo:
            return osBack;
       default:
            break;
       }
     }
  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kPlay:
       case kRed:
            cRemote::Put(kOk, true);
       case kOk:
            return osBack;
       case kGreen:
            cElvisFetcher::GetInstance()->New(*urlM, *nameM, *descriptionM, *startTimeM, lengthM);
            break;
       default:
            break;
       }
     state = osContinue;
     }

  return state;
}

// --- cElvisRecordingItem ---------------------------------------------

cElvisRecordingItem::cElvisRecordingItem(cElvisRecording *recordingP)
: recordingM(recordingP),
  descriptionM("")
{
  if (recordingM) {
     unsigned long offset = 0;
     unsigned long size = 0;
     bool resume = cElvisResumeItems::GetInstance()->HasResume(recordingM->ProgramId(), offset, size);
     if (recordingM->IsFolder())
        SetText(cString::sprintf("%s\t%d\t\t%s", tr("[DIR]"), recordingM->Count(), recordingM->Name()));
     else
        SetText(cString::sprintf("%s\t%s\t%c\t%s", *ShortDateString(recordingM->StartTimeValue()), *TimeString(recordingM->StartTimeValue()),
                                 resume ? ' ' : '*', recordingM->Name()));
     }
}

const char *cElvisRecordingItem::Description()
{
  if (isempty(descriptionM) && recordingM) {
     unsigned long offset = 0;
     unsigned long size = 0;
     bool resume = cElvisResumeItems::GetInstance()->HasResume(recordingM->ProgramId(), offset, size);
     int watched = size ? (int)(100L * offset / size) : 0;
     if (recordingM->Info()) {
        if (resume)
           descriptionM = cString::sprintf("%s %s - %s (%s; %d; %d%%)\n%s\n\n%s\n\n%s\n\n%s", *ShortDateString(recordingM->StartTimeValue()), *TimeString(recordingM->StartTimeValue()),
                                           *TimeString(recordingM->StartTimeValue() + (recordingM->Info()->LengthValue() * 60)), recordingM->Info()->Length(), recordingM->Count(), watched,
                                           recordingM->Channel(), recordingM->Name(), recordingM->Info()->ShortText(), recordingM->Info()->Description());
        else
           descriptionM = cString::sprintf("%s %s - %s (%s; %d)\n%s\n\n%s\n\n%s\n\n%s", *ShortDateString(recordingM->StartTimeValue()), *TimeString(recordingM->StartTimeValue()),
                                           *TimeString(recordingM->StartTimeValue() + (recordingM->Info()->LengthValue() * 60)), recordingM->Info()->Length(), recordingM->Count(),
                                           recordingM->Channel(), recordingM->Name(), recordingM->Info()->ShortText(), recordingM->Info()->Description());
        }
     else {
        if (resume)
           descriptionM = cString::sprintf("%s %s (%d; %d%%)\n%s\n\n%s", *ShortDateString(recordingM->StartTimeValue()), *TimeString(recordingM->StartTimeValue()), recordingM->Count(),
                                           watched, recordingM->Channel(), recordingM->Name());
        else
           descriptionM = cString::sprintf("%s %s (%d)\n%s\n\n%s", *ShortDateString(recordingM->StartTimeValue()), *TimeString(recordingM->StartTimeValue()), recordingM->Count(),
                                           recordingM->Channel(), recordingM->Name());
        }
     }

  return *descriptionM;
}

// --- cElvisRecordingsMenu --------------------------------------------

cElvisRecordingsMenu::cElvisRecordingsMenu(int folderIdP, int levelP)
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), trVDR("Recordings")), 9, 7, 2),
  folderM(cElvisRecordings::GetInstance()->GetFolder(folderIdP)),
  levelM(levelP)
{
  if (folderM) {
     if (levelP)
        SetTitle(*cString::sprintf("%s - %s - %s", tr("Elvis"), trVDR("Recordings"), folderM->Name()));
     folderM->StateChanged(stateM);
     folderM->Update();
     }

  Setup();
  SetHelpKeys();
}

void cElvisRecordingsMenu::SetHelpKeys()
{
  cElvisRecordingItem *item = (cElvisRecordingItem *)Get(Current());
  if (item) {
     unsigned long offset = 0;
     unsigned long size = 0;
     if (item->IsFolder())
        SetHelp(trVDR("Button$Open"), NULL, NULL, NULL);
     else
        SetHelp(trVDR("Button$Play"), (item->Recording() && cElvisResumeItems::GetInstance()->HasResume(item->Recording()->ProgramId(), offset, size) && (offset > 0)) ?
                trVDR("Button$Rewind") : NULL, trVDR("Button$Delete"), trVDR("Button$Info"));
     }
  else
     SetHelp(NULL, NULL, NULL, NULL);
}

void cElvisRecordingsMenu::Setup()
{
  int current = Current();

  Clear();

  if (folderM) {
     cThreadLock lock(folderM);
     for (cElvisRecording *item = folderM->cList<cElvisRecording>::First(); item; item = folderM->cList<cElvisRecording>::Next(item))
         Add(new cElvisRecordingItem(item));
     }

  SetCurrent(Get(current));
  Display();
}

eOSState cElvisRecordingsMenu::Delete()
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  cElvisRecordingItem *item = (cElvisRecordingItem *)Get(Current());
  if (item) {
     if (item->IsFolder())
        Skins.Message(mtInfo, tr("Cannot delete folder!"));
     else if (Interface->Confirm(trVDR("Delete recording?"))) {
        if (!folderM->DeleteRecording(item->Recording()))
           Skins.Message(mtError, tr("Cannot delete recording!"));
        if (!Count())
           return osBack;
        Setup();
        }
     }

  return osContinue;
}

eOSState cElvisRecordingsMenu::Info()
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  cElvisRecordingItem *item = (cElvisRecordingItem *)Get(Current());
  if (item && item->Recording() && item->Recording()->Info() && !item->IsFolder())
     return AddSubMenu(new cElvisRecordingInfoMenu(item->Recording()->Info()->Url(), item->Recording()->Name(), item->Description(),
                                                   item->Recording()->Info()->StartTime(), item->Recording()->Info()->LengthValue()));

  return osContinue;
}

eOSState cElvisRecordingsMenu::Play(bool rewindP)
{
  cElvisRecordingItem *item = (cElvisRecordingItem *)Get(Current());
  if (item) {
     if (item->IsFolder())
        return AddSubMenu(new cElvisRecordingsMenu(item->Recording()->Id(), levelM + 1));
     else if (item->Recording()->Info()) {
        if (rewindP)
           cElvisResumeItems::GetInstance()->Rewind(item->Recording()->ProgramId());
        cControl::Launch(new cElvisReplayControl(item->Recording()->ProgramId(), item->Recording()->Info()->Url(), item->Recording()->Name(),
                                                 item->Description(), item->Recording()->Info()->StartTime(), item->Recording()->Info()->LengthValue()));
        return osEnd;
        }
     }

  return osContinue;
}

eOSState cElvisRecordingsMenu::Fetch()
{
  cElvisRecordingItem *item = (cElvisRecordingItem *)Get(Current());
  if (item && item->Recording() && item->Recording()->Info() && !item->IsFolder())
     cElvisFetcher::GetInstance()->New(item->Recording()->Info()->Url(), item->Recording()->Name(), item->Description(),
                                       item->Recording()->Info()->StartTime(), item->Recording()->Info()->LengthValue());

  return osContinue;
}

eOSState cElvisRecordingsMenu::ProcessKey(eKeys keyP)
{
  bool HadSubMenu = HasSubMenu();
  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kRed:
       case kPlay:
       case kOk:
            return Play();
       case kGreen:
            return Play(true);
       case kYellow:
            return Delete();
       case kBlue:
       case kInfo:
            return Info();
       case k5:
            if (folderM)
               folderM->Update(true);
            return osContinue;
       case kNone:
            if (folderM) {
               folderM->Update();
               if (folderM->StateChanged(stateM)) {
                  Setup();
                  SetHelpKeys();
                  }
               }
            break;
       default:
            break;
       }
     }

  if (HadSubMenu && !HasSubMenu()) {
     Setup();
     if (!Count())
        return osBack;
     }

  if (!HasSubMenu() && (keyP != kNone))
     SetHelpKeys();

  return state;
}

// --- cElvisTimerCreateMenu -------------------------------------------

cElvisTimerCreateMenu::cElvisTimerCreateMenu(cElvisEvent *eventP)
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), tr("Create new timer")), 17),
  eventM(eventP)
{
  int i = 0;

  if (cElvisRecordings::GetInstance()->Count() == 0)
     cElvisRecordings::GetInstance()->Reset();
  folderM = i;
  numFoldersM = cElvisRecordings::GetInstance()->Count();
  folderNamesM = new const char*[numFoldersM];
  folderIdsM = new int[numFoldersM];
  for (cElvisRecordingFolder *item = cElvisRecordings::GetInstance()->First(); item && (i < numFoldersM); item = cElvisRecordings::GetInstance()->Next(item)) {
      folderNamesM[i] = item->Name();
      folderIdsM[i] = item->Id();
      if (item->Id() == -1)
         folderM = i;
      ++i;
      }

  Setup();
}

cElvisTimerCreateMenu::~cElvisTimerCreateMenu()
{
  delete [] folderNamesM;
  delete [] folderIdsM;
}

void cElvisTimerCreateMenu::Setup()
{
  int current = Current();

  Clear();

  if (eventM) {
     Add(new cOsdItem(cString::sprintf("%s:\t%s", trVDR("Name"), eventM->Name()), osUnknown, false));
     if (!isempty(eventM->Channel()))
        Add(new cOsdItem(cString::sprintf("%s:\t%s", trVDR("Channel"), eventM->Channel()), osUnknown, false));
     else if (eventM->Info() && !isempty(eventM->Info()->Channel()))
        Add(new cOsdItem(cString::sprintf("%s:\t%s", trVDR("Channel"), eventM->Info()->Channel()), osUnknown, false));
     Add(new cOsdItem(cString::sprintf("%s:\t%s", trVDR("Start"), eventM->StartTime()), osUnknown, false));
     Add(new cOsdItem(cString::sprintf("%s:\t%s", trVDR("Stop"), eventM->EndTime()), osUnknown, false));
     Add(new cMenuEditStraItem(tr("Folder"), &folderM, numFoldersM, folderNamesM));
     }
  SetCurrent(Get(current));
  Display();
}

eOSState cElvisTimerCreateMenu::ProcessKey(eKeys keyP)
{
 eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kOk:
            if (eventM) {
               if (cElvisTimers::GetInstance()->Create(eventM->Id(), folderIdsM[folderM]))
                  Skins.Message(mtInfo, tr("Timer created"));
               else
                  Skins.Message(mtError, tr("Cannot create timer!"));
               }
            return osBack;
       default:
            break;
       }
     state = osContinue;
     }

  return state;
}

// --- cElvisTimerInfoMenu ---------------------------------------------

cElvisTimerInfoMenu::cElvisTimerInfoMenu(cElvisTimer *timerP)
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), trVDR("Timers")))
{
  if (timerP)
     textM = cString::sprintf("%s %s - %s (%d %s)\n%s%s\n\n%s\n\n%s\n\n%s", *DateString(timerP->Info()->StartTimeValue()), *TimeString(timerP->Info()->StartTimeValue()),
                              *TimeString(timerP->Info()->EndTimeValue()), timerP->Info()->LengthValue(), tr("min"), timerP->Channel(),
                              isempty(timerP->WildCard()) ? "" : *cString::sprintf(" (%s)", timerP->WildCard()), timerP->Name(), timerP->Info()->ShortText(),
                              timerP->Info()->Description());

}

void cElvisTimerInfoMenu::Display()
{
  cOsdMenu::Display();
  DisplayMenu()->SetText(*textM, false);
  cStatus::MsgOsdTextItem(*textM);
}

eOSState cElvisTimerInfoMenu::ProcessKey(eKeys keyP)
{
  if (!HasSubMenu()) {
     switch (keyP) {
       case kUp|k_Repeat:
       case kUp:
       case kDown|k_Repeat:
       case kDown:
       case kLeft|k_Repeat:
       case kLeft:
       case kRight|k_Repeat:
       case kRight:
            DisplayMenu()->Scroll(NORMALKEY(keyP) == kUp || NORMALKEY(keyP) == kLeft, NORMALKEY(keyP) == kLeft || NORMALKEY(keyP) == kRight);
            cStatus::MsgOsdTextItem(NULL, NORMALKEY(keyP) == kUp);
            return osContinue;
       case kInfo:
            return osBack;
       default:
            break;
       }
     }

  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kOk:
            return osBack;
       default:
            break;
       }
     state = osContinue;
     }

  return state;
}

// --- cElvisTimerItem -------------------------------------------------

cElvisTimerItem::cElvisTimerItem(cElvisTimer *timerP)
: timerM(timerP)
{
  if (timerM)
     SetText(cString::sprintf("%s\t%s\t%s\t%s\t%s", timerM->Channel(), *WeekDateString(timerM->StartTimeValue()), *TimeString(timerM->StartTimeValue()),
                              (timerM->Id() > 0) ? "" : "*", timerM->Name()));
}

// --- cElvisTimersMenu ------------------------------------------------

cElvisTimersMenu::cElvisTimersMenu()
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), trVDR("Timers")), 10, 7, 7, 2)
{
  cElvisTimers::GetInstance()->StateChanged(stateM);
  cElvisTimers::GetInstance()->Update();
  Setup();
  SetHelpKeys();
}

void cElvisTimersMenu::SetHelpKeys()
{
  cElvisTimerItem *item = (cElvisTimerItem *)Get(Current());
  if (item) {
     if (item->Wildcard())
        SetHelp(NULL, NULL, NULL, trVDR("Button$Info"));
     else
        SetHelp(NULL, NULL, trVDR("Button$Delete"), trVDR("Button$Info"));
     }
  else
     SetHelp(NULL, NULL, NULL, NULL);
}

void cElvisTimersMenu::Setup()
{
  int current = Current();

  Clear();

  if (true)
  {
     cThreadLock lock(cElvisTimers::GetInstance());
     for (cElvisTimer *item = cElvisTimers::GetInstance()->First(); item; item = cElvisTimers::GetInstance()->Next(item))
         Add(new cElvisTimerItem(item));
  }

  SetCurrent(Get(current));
  Display();
}

eOSState cElvisTimersMenu::Delete()
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  cElvisTimerItem *item = (cElvisTimerItem *)Get(Current());
  if (item) {
     if (item->Wildcard())
        Skins.Message(mtInfo, tr("Cannot delete search timer!"));
     else if (Interface->Confirm(tr("Delete timer?"))){
        if (!cElvisTimers::GetInstance()->Delete(item->Timer()))
           Skins.Message(mtError, tr("Cannot delete timer!"));
        if (!Count())
           return osBack;
        Setup();
        }
     }

  return osContinue;
}

eOSState cElvisTimersMenu::Info()
{
  if (HasSubMenu() || Count() == 0)
     return (osContinue);

  cElvisTimerItem *item = (cElvisTimerItem *)Get(Current());
  if (item)
     return AddSubMenu(new cElvisTimerInfoMenu(item->Timer()));

  return osContinue;
}

eOSState cElvisTimersMenu::ProcessKey(eKeys keyP)
{
  bool HadSubMenu = HasSubMenu();
  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case k5:
            cElvisTimers::GetInstance()->Update(true);
            return osContinue;
       case kYellow:
            return Delete();
       case kBlue:
       case kOk:
       case kInfo:
            return Info();
       case kNone:
            cElvisTimers::GetInstance()->Update();
            if (cElvisTimers::GetInstance()->StateChanged(stateM)) {
               Setup();
               SetHelpKeys();
               return osContinue;
               }
            break;
       default:
            break;
       }
     state = osContinue;
     }

  if (HadSubMenu && !HasSubMenu()) {
     Setup();
     if (!Count())
        return osBack;
     }

  if (!HasSubMenu() && (keyP != kNone))
     SetHelpKeys();

  return state;
}

// --- cElvisSearchTimersMenu ------------------------------------------

cElvisSearchTimerEditMenu::cElvisSearchTimerEditMenu(cElvisSearchTimer *timerP)
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), timerP ? tr("Edit search timer") : tr("New search timer")), 17),
  timerM(timerP)
{
  int i = 0;

  if (cElvisChannels::GetInstance()->Count() == 0)
     cElvisChannels::GetInstance()->Update(true);
  channelM = i;
  numChannelsM = cElvisChannels::GetInstance()->Count();
  channelNamesM = new const char*[numChannelsM];
  for (cElvisChannel *channel = cElvisChannels::GetInstance()->First(); channel; channel = cElvisChannels::GetInstance()->Next(channel)) {
      channelNamesM[channel->Index()] = channel->Name();
      if (timerM && !strcmp(timerM->Channel(), channel->Name()))
         channelM = i;
      ++i;
      }

  if (cElvisRecordings::GetInstance()->Count() == 0)
     cElvisRecordings::GetInstance()->Reset();
  i = 0;
  folderM = i;
  numFoldersM = cElvisRecordings::GetInstance()->Count();
  folderNamesM = new const char*[numFoldersM];
  folderIdsM = new int[numFoldersM];
  for (cElvisRecordingFolder *item = cElvisRecordings::GetInstance()->First(); item && (i < numFoldersM); item = cElvisRecordings::GetInstance()->Next(item)) {
      folderNamesM[i] = item->Name();
      folderIdsM[i] = item->Id();
      if (timerM && !strcmp(timerM->Folder(), item->Name()))
         folderM = i;
      ++i;
      }

  memset(wildcardM, 0, sizeof(wildcardM));
  if (timerM)
     Utf8Strn0Cpy(wildcardM, timerM->Wildcard(), sizeof(wildcardM));

  Setup();
}

cElvisSearchTimerEditMenu::~cElvisSearchTimerEditMenu()
{
  delete[] channelNamesM;
  delete[] folderNamesM;
  delete[] folderIdsM;
}

void cElvisSearchTimerEditMenu::Setup()
{
  int current = Current();

  Clear();

  Add(new cMenuEditStrItem(tr("Search term"), wildcardM, sizeof(wildcardM), tr(FileNameChars)));
  Add(new cMenuEditStraItem(trVDR("Channel"), &channelM, numChannelsM,      channelNamesM));
  Add(new cMenuEditStraItem(tr("Folder"),     &folderM,  numFoldersM,       folderNamesM));

  SetCurrent(Get(current));
  Display();
}

eOSState cElvisSearchTimerEditMenu::ProcessKey(eKeys keyP)
{
  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kOk:
               if (timerM) {
                  if (((strcmp(timerM->Channel(), channelNamesM[channelM]) || strcmp(timerM->Wildcard(), wildcardM) || strcmp(timerM->Folder(), folderNamesM[folderM]))) &&
                      !cElvisSearchTimers::GetInstance()->Create(timerM, channelNamesM[channelM], wildcardM, folderIdsM[folderM]))
                     Skins.Message(mtError, tr("Cannot edit search timer!"));
                  }
               else {
                  if (!isempty(wildcardM) && !cElvisSearchTimers::GetInstance()->Create(timerM, channelNamesM[channelM], wildcardM, folderIdsM[folderM]))
                     Skins.Message(mtError, tr("Cannot create search timer!"));
               }
            return osBack;
       default:
            break;
       }
     state = osContinue;
     }

  return state;
}

// --- cElvisSearchTimerItem -------------------------------------------

cElvisSearchTimerItem::cElvisSearchTimerItem(cElvisSearchTimer *timerP)
: timerM(timerP)
{
  if (timerM)
     SetText(*cString::sprintf("%s\t%s\t%s", timerM->Channel(), timerM->Folder(), timerM->Wildcard()));
}

// --- cElvisSearchTimersMenu ------------------------------------------

cElvisSearchTimersMenu::cElvisSearchTimersMenu()
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), tr("Search timers")), 10, 12)
{
  cElvisSearchTimers::GetInstance()->StateChanged(stateM);
  cElvisSearchTimers::GetInstance()->Update();
  Setup();
  SetHelpKeys();
}

void cElvisSearchTimersMenu::SetHelpKeys()
{
  cElvisSearchTimer *item = (cElvisSearchTimer *)Get(Current());
  if (item)
     SetHelp(trVDR("Button$Edit"), trVDR("Button$New"), trVDR("Button$Delete"), NULL);
  else
     SetHelp(NULL, NULL, NULL, NULL);
}

void cElvisSearchTimersMenu::Setup()
{
  int current = Current();

  Clear();

  if (true) {
     cThreadLock lock(cElvisSearchTimers::GetInstance());
     for (cElvisSearchTimer *item = cElvisSearchTimers::GetInstance()->First(); item; item = cElvisSearchTimers::GetInstance()->Next(item))
         Add(new cElvisSearchTimerItem(item));
     }

  SetCurrent(Get(current));
  Display();
}

eOSState cElvisSearchTimersMenu::Delete()
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  cElvisSearchTimerItem *item = (cElvisSearchTimerItem *)Get(Current());
  if (item && Interface->Confirm(tr("Delete search timer?"))) {
     if (!cElvisSearchTimers::GetInstance()->Delete(item->Timer()))
        Skins.Message(mtError, tr("Cannot delete search timer!"));
     if (!Count())
        return osBack;
     Setup();
     }

  return osContinue;
}

eOSState cElvisSearchTimersMenu::New()
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  return AddSubMenu(new cElvisSearchTimerEditMenu(NULL));
}

eOSState cElvisSearchTimersMenu::Edit()
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  cElvisSearchTimerItem *item = (cElvisSearchTimerItem *)Get(Current());
  if (item)
     return AddSubMenu(new cElvisSearchTimerEditMenu(item->Timer()));

  return osContinue;
}

eOSState cElvisSearchTimersMenu::ProcessKey(eKeys keyP)
{
  bool HadSubMenu = HasSubMenu();
  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kOk:
       case kRed:
            return Edit();
       case kGreen:
            return New();
       case kYellow:
            return Delete();
       case k5:
            cElvisSearchTimers::GetInstance()->Update(true);
            return osContinue;
       case kNone:
            cElvisSearchTimers::GetInstance()->Update();
            if (cElvisSearchTimers::GetInstance()->StateChanged(stateM)) {
               Setup();
               SetHelpKeys();
               return osContinue;
               }
            break;
       default:
            break;
       }
     state = osContinue;
     }

  if (HadSubMenu && !HasSubMenu()) {
     Setup();
     if (!Count())
        return osBack;
     }

  if (!HasSubMenu() && (keyP != kNone))
     SetHelpKeys();

  return state;
}

// --- cElvisChannelEventInfoMenu --------------------------------------

cElvisChannelEventInfoMenu::cElvisChannelEventInfoMenu(cElvisEvent *eventP, const char *channelP)
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), trVDR("EPG"))),
  eventM(eventP)
{
  if (eventM) {
     if (!isempty(eventM->Description()))
        textM = cString::sprintf("%s %s - %s (%d %s)\n%s\n\n%s\n\n%s", *DateString(eventM->StartTimeValue()), *TimeString(eventP->StartTimeValue()),
                                 *TimeString(eventM->EndTimeValue()), eventM->LengthValue(), tr("min"),
                                 channelP ? channelP : "", eventM->Name(), eventM->Description());
     else if (eventM->Info())
        textM = cString::sprintf("%s %s - %s (%d %s)\n%s\n\n%s\n\n%s\n\n%s", *DateString(eventM->Info()->StartTimeValue()), *TimeString(eventP->Info()->StartTimeValue()),
                                 *TimeString(eventM->Info()->EndTimeValue()), eventM->Info()->LengthValue(), tr("min"),
                                 channelP ? channelP : "", eventM->Name(), eventM->Info()->ShortText(), eventM->Info()->Description());
     }
  SetHelpKeys();
}

void cElvisChannelEventInfoMenu::SetHelpKeys()
{
  if (eventM)
     SetHelp(trVDR("Button$Record"), trVDR("Button$Folder"), NULL, NULL);
}

void cElvisChannelEventInfoMenu::Display()
{
  if (!HasSubMenu()) {
     cOsdMenu::Display();
     DisplayMenu()->SetText(*textM, false);
     cStatus::MsgOsdTextItem(*textM);
     }
}

eOSState cElvisChannelEventInfoMenu::Record(bool quickP)
{
  if (eventM) {
     if (quickP) {
        if (cElvisTimers::GetInstance()->Create(eventM->Id()))
           Skins.Message(mtInfo, tr("Timer created"));
        else
           Skins.Message(mtError, tr("Cannot create timer!"));
        }
     else
        return AddSubMenu(new cElvisTimerCreateMenu(eventM));
     }

  return osContinue;
}

eOSState cElvisChannelEventInfoMenu::ProcessKey(eKeys keyP)
{
  if (!HasSubMenu()) {
     switch (keyP) {
       case kUp|k_Repeat:
       case kUp:
       case kDown|k_Repeat:
       case kDown:
       case kLeft|k_Repeat:
       case kLeft:
       case kRight|k_Repeat:
       case kRight:
            DisplayMenu()->Scroll(NORMALKEY(keyP) == kUp || NORMALKEY(keyP) == kLeft, NORMALKEY(keyP) == kLeft || NORMALKEY(keyP) == kRight);
            cStatus::MsgOsdTextItem(NULL, NORMALKEY(keyP) == kUp);
            return osContinue;
       default:
            break;
       }
     }

  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kRecord:
       case kRed:
            return Record(true);
       case kGreen:
            return Record(false);
       case kOk:
            return osBack;
       default:
            break;
       }
     state = osContinue;
     }

  if (!HasSubMenu() && (keyP != kNone))
     SetHelpKeys();

  return state;
}

// --- cElvisChannelEventItem -----------------------------------------------

cElvisChannelEventItem::cElvisChannelEventItem(cElvisEvent *eventP)
: eventM(eventP)
{
  if (eventM)
     SetText(cString::sprintf("%s\t%s\t%s", *WeekDateString(eventM->StartTimeValue()), *TimeString(eventM->StartTimeValue()), eventM->Name()));
}

// --- cElvisChannelEventsMenu ------------------------------------------------

cElvisChannelEventsMenu::cElvisChannelEventsMenu(cElvisChannel *channelP)
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), channelP ? channelP->Name() : trVDR("EPG")), 7, 7),
  channelM(channelP)
{
  Setup();
  SetHelpKeys();
}

void cElvisChannelEventsMenu::SetHelpKeys()
{
  cElvisEvent *item = (cElvisEvent *)Get(Current());
  if (item)
     SetHelp(trVDR("Button$Record"), trVDR("Button$Folder"), NULL, trVDR("Button$Info"));
  else
     SetHelp(NULL, NULL, NULL, NULL);
}

void cElvisChannelEventsMenu::Setup()
{
  int current = Current();

  Clear();

  if (channelM) {
     cThreadLock lock(cElvisChannels::GetInstance());
     for (cElvisEvent *item = channelM->cList<cElvisEvent>::First(); item; item = channelM->cList<cElvisEvent>::Next(item))
         Add(new cElvisChannelEventItem(item));
     }

  SetCurrent(Get(current));
  Display();
}

eOSState cElvisChannelEventsMenu::Record(bool quickP)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  cElvisChannelEventItem *item = (cElvisChannelEventItem *)Get(Current());
  if (item) {
        if (quickP) {
           if (cElvisTimers::GetInstance()->Create(item->Event()->Id()))
              Skins.Message(mtInfo, tr("Timer created"));
           else
              Skins.Message(mtError, tr("Cannot create timer!"));
           Setup();
           }
        else
           return AddSubMenu(new cElvisTimerCreateMenu(item->Event()));
     }

  return osContinue;
}

eOSState cElvisChannelEventsMenu::Info()
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  cElvisChannelEventItem *item = (cElvisChannelEventItem *)Get(Current());
  if (item)
     return AddSubMenu(new cElvisChannelEventInfoMenu(item->Event(), channelM->Name()));

  return osContinue;
}

eOSState cElvisChannelEventsMenu::ProcessKey(eKeys keyP)
{
  bool HadSubMenu = HasSubMenu();
  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kRecord:
       case kRed:
            return Record(true);
       case kGreen:
            return Record(false);
       case k5:
            return osBack;
       case kBlue:
       case kOk:
       case kInfo:
            return Info();
       case kNone:
            break;
       default:
            break;
       }
     state = osContinue;
     }

  if (HadSubMenu && !HasSubMenu()) {
     Setup();
     if (!Count())
        return osBack;
     }

  if (!HasSubMenu() && (keyP != kNone))
     SetHelpKeys();

  return state;
}

// --- cElvisChannelItem -----------------------------------------------

cElvisChannelItem::cElvisChannelItem(cElvisChannel *channelP)
: cOsdItem(channelP ? *cString::sprintf("%s", channelP->Name()) : ""),
  channelM(channelP)
{
}

// --- cElvisEPGMenu ---------------------------------------------------

cElvisEPGMenu::cElvisEPGMenu()
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), trVDR("EPG")), 17)
{
  cElvisChannels::GetInstance()->StateChanged(stateM);
  cElvisChannels::GetInstance()->Update();
  Setup();
  SetHelpKeys();
}

void cElvisEPGMenu::SetHelpKeys()
{
  cElvisChannelItem *item = (cElvisChannelItem *)Get(Current());
  if (item)
     SetHelp(trVDR("Button$Open"), NULL, NULL, NULL);
  else
     SetHelp(NULL, NULL, NULL, NULL);
}

void cElvisEPGMenu::Setup()
{
  int current = Current();

  Clear();

  if (true) {
     cThreadLock lock(cElvisChannels::GetInstance());
     for (cElvisChannel *item = cElvisChannels::GetInstance()->First(); item; item = cElvisChannels::GetInstance()->Next(item)) {
         if (item->Name())
            Add(new cElvisChannelItem(item));
         }
     }

  SetCurrent(Get(current));
  Display();
}

eOSState cElvisEPGMenu::Select()
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  cElvisChannelItem *item = (cElvisChannelItem *)Get(Current());
  if (item)
     return AddSubMenu(new cElvisChannelEventsMenu(item->Channel()));

  return osContinue;
}

eOSState cElvisEPGMenu::ProcessKey(eKeys keyP)
{
  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kRed:
       case kOk:
            return Select();
       case k5:
            cElvisChannels::GetInstance()->Update(true);
            return osContinue;
       case kNone:
            cElvisChannels::GetInstance()->Update();
            if (cElvisChannels::GetInstance()->StateChanged(stateM)) {
               Setup();
               SetHelpKeys();
               return osContinue;
               }
            break;
       default:
            break;
       }
     state = osContinue;
     }

  if (!HasSubMenu() && (keyP != kNone))
     SetHelpKeys();

  return state;
}

// --- cElvisTopEventsMenu ---------------------------------------------

cElvisTopEventsMenu::cElvisTopEventsMenu()
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), tr("Top events")), 7, 7)
{
  cElvisTopEvents::GetInstance()->StateChanged(stateM);
  cElvisTopEvents::GetInstance()->Update();
  Setup();
  SetHelpKeys();
}

void cElvisTopEventsMenu::SetHelpKeys()
{
  cElvisChannelEventItem *item = (cElvisChannelEventItem *)Get(Current());
  if (item)
     SetHelp(trVDR("Button$Record"), trVDR("Button$New"), NULL, trVDR("Button$Info"));
  else
     SetHelp(NULL, NULL, NULL, NULL);
}

void cElvisTopEventsMenu::Setup()
{
  int current = Current();

  Clear();

  if (true) {
     cThreadLock lock(cElvisTopEvents::GetInstance());
     for (cElvisEvent *item = cElvisTopEvents::GetInstance()->First(); item; item = cElvisTopEvents::GetInstance()->Next(item))
         Add(new cElvisChannelEventItem(item));
     }

  SetCurrent(Get(current));
  Display();
}

eOSState cElvisTopEventsMenu::Record(bool quickP)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  cElvisChannelEventItem *item = (cElvisChannelEventItem *)Get(Current());
  if (item && item->Event()) {
        if (quickP) {
           if (cElvisTimers::GetInstance()->Create(item->Event()->Id()))
              Skins.Message(mtInfo, tr("Timer created"));
           else
              Skins.Message(mtError, tr("Cannot create timer!"));
           Setup();
           }
        else
           return AddSubMenu(new cElvisTimerCreateMenu(item->Event()));
     }

  return osContinue;
}

eOSState cElvisTopEventsMenu::Info()
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  cElvisChannelEventItem *item = (cElvisChannelEventItem *)Get(Current());
  if (item && item->Event())
     return AddSubMenu(new cElvisChannelEventInfoMenu(item->Event(), item->Event()->Channel()));

  return osContinue;
}

eOSState cElvisTopEventsMenu::ProcessKey(eKeys keyP)
{
  bool HadSubMenu = HasSubMenu();
  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kRecord:
       case kRed:
            return Record(true);
       case kGreen:
            return Record(false);
       case k5:
            cElvisTopEvents::GetInstance()->Update(true);
            return osContinue;
       case kOk:
       case kBlue:
       case kInfo:
            return Info();
       case kNone:
            cElvisTopEvents::GetInstance()->Update();
            if (cElvisTopEvents::GetInstance()->StateChanged(stateM)) {
               Setup();
               SetHelpKeys();
               return osContinue;
               }
            break;
       default:
            break;
       }
     state = osContinue;
     }

  if (HadSubMenu && !HasSubMenu()) {
     Setup();
     if (!Count())
        return osBack;
     }

  if (!HasSubMenu() && (keyP != kNone))
     SetHelpKeys();

  return state;
}

// --- cElvisMenu ------------------------------------------------------

cElvisMenu::cElvisMenu()
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), ElvisConfig.Service ? tr("Saunavisio") : tr("Elisa Viihde"))),
  updateTimeoutM(0),
  fetchCountM(cElvisFetcher::GetInstance()->FetchCount())
{
  Setup();
  SetHelpKeys();
}

void cElvisMenu::SetHelpKeys()
{
  SetHelp(NULL, NULL, fetchCountM ? tr("Abort fetch") : NULL, NULL);
}

void cElvisMenu::Setup()
{
  int current = Current();
  Clear();

  SetHasHotkeys();
  Add(new cOsdItem(hk(trVDR("Recordings")), osUser1));
  Add(new cOsdItem(hk(trVDR("Timers")),     osUser2));
  Add(new cOsdItem(hk(tr("Search timers")), osUser3));
  Add(new cOsdItem(hk(trVDR("EPG")),        osUser4));
  Add(new cOsdItem(hk(tr("Top events")),    osUser5));
  if (fetchCountM > 0) {
     Add(new cOsdItem(*cString::sprintf(tr("Now fetching recordings: %d"), fetchCountM), osUnknown, false));
     for (unsigned int i = 0; i < fetchCountM; ++i) {
         cElvisFetchItem *obj = cElvisFetcher::GetInstance()->Get(i);
         if (obj)
            Add(new cOsdItem(*cString::sprintf("%2d%%: %s", obj->Progress(), obj->Name()), osUnknown, false));
         }
     }

  SetCurrent(Get(current));
  Display();
}

eOSState cElvisMenu::ProcessKey(eKeys keyP)
{
  eOSState state = cOsdMenu::ProcessKey(keyP);

  switch (state) {
    case osUser1:
         return AddSubMenu(new cElvisRecordingsMenu);
    case osUser2:
         return AddSubMenu(new cElvisTimersMenu);
    case osUser3:
         return AddSubMenu(new cElvisSearchTimersMenu);
    case osUser4:
         return AddSubMenu(new cElvisEPGMenu);
    case osUser5:
         return AddSubMenu(new cElvisTopEventsMenu);
    case osUnknown:
         switch (keyP) {
           case kYellow:
                if (Interface->Confirm(tr("Abort fetching?")))
                   cElvisFetcher::GetInstance()->Abort();
                state = osContinue;
           default:
                break;
           }
         break;
    default:
         break;
    }

  unsigned int count = cElvisFetcher::GetInstance()->FetchCount();
  if ((count != fetchCountM) || (updateTimeoutM.Elapsed() >= eUpdateTimeoutMs)) {
     updateTimeoutM.Set(0);
     fetchCountM = count;
     Setup();
     }

  if (!HasSubMenu() && (keyP != kNone))
     SetHelpKeys();

  return state;
}
