/*
 * menu.c: Elvis plugin for the Video Disk Recorder
 *
 *
 */

#include <vdr/status.h>
#include <vdr/remote.h>
#include <vdr/menuitems.h>
#include <vdr/interface.h>
#include <vdr/plugin.h>

#include "common.h"
#include "fetch.h"
#include "player.h"
#include "resume.h"
#include "menu.h"

// --- cElvisRecordingInfoMenu -----------------------------------------

cElvisRecordingInfoMenu::cElvisRecordingInfoMenu(const char *urlP, const char *nameP, const char *descriptionP, const char *startTimeP, unsigned int lengthP, bool encryptedP)
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), trVDR("Recordings"))),
  urlM(urlP),
  nameM(nameP),
  descriptionM(descriptionP),
  startTimeM(startTimeP),
  lengthM(lengthP),
  encryptedM(encryptedP)
{
  SetHelp(encryptedM ? NULL : trVDR("Button$Play"), encryptedM ? NULL : tr("Button$Fetch"), NULL, NULL);
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
            if (!encryptedM)
               cRemote::Put(kOk, true);
       case kOk:
            return osBack;
       case kGreen:
            if (!encryptedM)
               cElvisFetcher::GetInstance()->New(*urlM, *nameM, *descriptionM, *startTimeM, lengthM);
            break;
       default:
            break;
       }
     state = osContinue;
     }

  return state;
}

// --- cElvisRecordingRenameMenu ---------------------------------------

cElvisRecordingRenameMenu::cElvisRecordingRenameMenu(cElvisRecordingFolder *parentFolderP, int folderIdP, const char *nameP)
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), tr("Rename folder")), 12),
  parentFolderM(parentFolderP),
  folderIdM(folderIdP),
  nameM(nameP)
{
  strn0cpy(newNameM, *nameM, sizeof(newNameM));
  Clear();
  Add(new cMenuEditStrItem(trVDR("Name"), newNameM, sizeof(newNameM), tr(FileNameChars)));
  Display();
  SetHelp(NULL, NULL, NULL, NULL);
}

eOSState cElvisRecordingRenameMenu::ProcessKey(eKeys keyP)
{
  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kOk:
            if (!cElvisRecordings::GetInstance()->RenameRecordingFolder(folderIdM, newNameM))
               Skins.Message(mtInfo, tr("Cannot rename folder!"));
            else if (parentFolderM)
               parentFolderM->Update(true);
            return osBack;
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
     if (item->IsFolder())
        SetHelp(trVDR("Button$Open"), NULL, trVDR("Button$Delete"), tr("Button$Rename"));
     else {
        unsigned long offset = 0, size = 0;
        SetHelp(!(item->Recording() && item->Recording()->Info() && item->Recording()->Info()->Encrypted()) ? trVDR("Button$Play") : NULL,
                (item->Recording() && cElvisResumeItems::GetInstance()->HasResume(item->Recording()->ProgramId(), offset, size) && (offset > 0)) ?
                trVDR("Button$Rewind") : NULL, trVDR("Button$Delete"), trVDR("Button$Info"));
        }
     }
  else
     SetHelp(NULL, NULL, NULL, NULL);
}

void cElvisRecordingsMenu::Setup()
{
  int current = Current();

  Clear();

  if (folderM) {
     LOCK_THREAD_INSTANCE(folderM);
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
     if (item->IsFolder()) {
        if (Interface->Confirm(tr("Delete folder?"))) {
           if (!cElvisRecordings::GetInstance()->RemoveRecordingFolder(item->FolderId()))
              Skins.Message(mtInfo, tr("Cannot delete folder!"));
           else if (folderM)
                folderM->Update(true);
           if (!Count())
              return osBack;
           Setup();
           }
        }
     else {
        if (Interface->Confirm(trVDR("Delete recording?"))) {
           if (!folderM->DeleteRecording(item->Recording()))
              Skins.Message(mtError, tr("Cannot delete recording!"));
           if (!Count())
              return osBack;
           Setup();
           }
        }
     }

  return osContinue;
}

eOSState cElvisRecordingsMenu::Info()
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  cElvisRecordingItem *item = (cElvisRecordingItem *)Get(Current());
  if (item && item->Recording()) {
     if (item->IsFolder())
        return AddSubMenu(new cElvisRecordingRenameMenu(folderM, item->Recording()->FolderId(), item->Recording()->Name()));
     else if (item->Recording()->Info())
        return AddSubMenu(new cElvisRecordingInfoMenu(item->Recording()->Info()->Url(), item->Recording()->Name(), item->Description(),
                                                      item->Recording()->Info()->StartTime(), item->Recording()->Info()->LengthValue(),
                                                      item->Recording()->Info()->Encrypted()));
     }

  return osContinue;
}

eOSState cElvisRecordingsMenu::Play(bool rewindP)
{
  cElvisRecordingItem *item = (cElvisRecordingItem *)Get(Current());
  if (item) {
     if (item->IsFolder())
        return AddSubMenu(new cElvisRecordingsMenu(item->Recording()->Id(), levelM + 1));
     else if (item->Recording()->Info() && !item->Recording()->Info()->Encrypted()) {
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
     LOCK_THREAD_INSTANCE(cElvisTimers::GetInstance());
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
     LOCK_THREAD_INSTANCE(cElvisSearchTimers::GetInstance());
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
     LOCK_THREAD_INSTANCE(cElvisChannels::GetInstance());
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
     LOCK_THREAD_INSTANCE(cElvisChannels::GetInstance());
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
     LOCK_THREAD_INSTANCE(cElvisTopEvents::GetInstance());
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

// --- cElvisVODInfoMenu -----------------------------------------------

cElvisVODInfoMenu::cElvisVODInfoMenu(const char *titleP, const char *descriptionP, const char *trailerP)
: cOsdMenu(*cString::sprintf("%s - %s - %s", tr("Elvis"), tr("VOD"), titleP ? titleP : "")),
  descriptionM(descriptionP),
  trailerM(trailerP)
{
  SetHelp(0 && !isempty(*trailerM) ? tr("Button$Preview") : NULL, NULL, NULL, NULL);
}

void cElvisVODInfoMenu::Display()
{
  cOsdMenu::Display();
  DisplayMenu()->SetText(*descriptionM, false);
  cStatus::MsgOsdTextItem(*descriptionM);
}

eOSState cElvisVODInfoMenu::Preview()
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  if (0 && !isempty(*trailerM))
     cPluginManager::CallFirstService("MediaPlayer-1.0", (void *)*trailerM);

  return osContinue;
}

eOSState cElvisVODInfoMenu::ProcessKey(eKeys keyP)
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
            return Preview();
       case kOk:
            return osBack;
       default:
            break;
       }
     state = osContinue;
     }

  return state;
}

// --- cElvisVODSearchMenu ---------------------------------------------

cElvisVODSearchMenu::cElvisVODSearchMenu()
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), tr("Search VOD")), 12),
  searchDataM(new cElvisVODSearch()),
  useDescriptionM(0),
  useHdM(0)
{
  strcpy(searchTermM, "");
  if (searchDataM)
     searchDataM->StateChanged(stateM);
  Setup();
  SetHelpKeys();
}

cElvisVODSearchMenu::~cElvisVODSearchMenu()
{
  DELETENULL(searchDataM);
}

void cElvisVODSearchMenu::Setup()
{
  int current = Current();

  Clear();

  Add(new cMenuEditStrItem(tr("Search term"), searchTermM, sizeof(searchTermM), tr(FileNameChars)));
  Add(new cMenuEditBoolItem(tr("Search criteria"), &useDescriptionM, tr("Title"), tr("Description")));
  Add(new cMenuEditBoolItem(tr("Show only HD"), &useHdM));
  Add(new cOsdItem("", osUnknown, false));
  if (searchDataM) {
     for (cElvisVOD *item = searchDataM->cList<cElvisVOD>::First(); item; item = searchDataM->cList<cElvisVOD>::Next(item))
         Add(new cElvisVODItem(item));
     }

  SetCurrent(Get(current));
  Display();
}

void cElvisVODSearchMenu::SetHelpKeys()
{
  if (Current() > 3)
     SetHelp(tr("Button$Search"), NULL, tr("Button$Add"), trVDR("Button$Info"));
  else
     SetHelp(tr("Button$Search"), NULL, NULL, NULL);
}

eOSState cElvisVODSearchMenu::Search()
{
  if (searchDataM)
     searchDataM->Search(useDescriptionM ? "" : searchTermM, useDescriptionM ? searchTermM : "", useHdM);

  return osContinue;
}

eOSState cElvisVODSearchMenu::Info()
{
  if (Current() < 3)
     return osContinue;

  cElvisVODItem *item = (cElvisVODItem *)Get(Current());
  if (item && item->Vod())
     return AddSubMenu(new cElvisVODInfoMenu(item->Vod()->Title(), item->Description(), item->Vod()->Trailer()));

  return osContinue;
}

eOSState cElvisVODSearchMenu::Favorite()
{
  if (Current() < 3)
     return osContinue;

  cElvisVODItem *item = (cElvisVODItem *)Get(Current());
  if (item && item->Vod())
     item->Vod()->SetFavorite(true);

  return osContinue;
}

eOSState cElvisVODSearchMenu::ProcessKey(eKeys keyP)
{
  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kRed:
       case kOk:
            return Search();
       case kYellow:
            return Favorite();
       case kBlue:
            return Info();
       case kNone:
            if (searchDataM && searchDataM->StateChanged(stateM)) {
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

// --- cElvisVODItem ---------------------------------------------------

cElvisVODItem::cElvisVODItem(cElvisVOD *vodP)
: vodM(vodP),
  descriptionM("")
{
  if (vodM)
     SetText(cString::sprintf("%s", vodM->Title()));
}

const char *cElvisVODItem::Description()
{
  if (isempty(descriptionM) && vodM) {
     if (vodM->Info())
           descriptionM = cString::sprintf("%s\n\n%d %s\n%s\n%d %s\n%s\n\n%s", vodM->Info()->Title(), vodM->Info()->Year(), vodM->Info()->OriginalTitle(),
                                          vodM->Info()->Categories(), vodM->Info()->Length(), tr("min"), (vodM->Info()->AgeLimit() > 0) ?
                                          *cString::sprintf("K-%d", vodM->Info()->AgeLimit()) : "S", vodM->Info()->Info());
     else
           descriptionM = cString::sprintf("%s\n%d\n%s", vodM->Title(), vodM->Year(), (vodM->AgeLimit() > 0) ? *cString::sprintf("K-%d", vodM->AgeLimit()) : "S");
     }

  return *descriptionM;
}

// --- cElvisVODMenu ---------------------------------------------------

cElvisVODMenu::cElvisVODMenu()
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), tr("VOD"))),
  showModeM(SHOWMODE_NEWEST)
{
  switch (showModeM % SHOWMODE_COUNT) {
    case SHOWMODE_NEWEST:
         categoryM = cElvisVODCategories::GetInstance()->GetCategory("newest");
         break;
    case SHOWMODE_POPULAR:
         categoryM = cElvisVODCategories::GetInstance()->GetCategory("popular");
         break;
    case SHOWMODE_FAVORITES:
         categoryM = cElvisVODCategories::GetInstance()->GetCategory("favorites");
         break;
    default:
         categoryM = NULL;
         break;
    }
  if (categoryM) {
     categoryM->StateChanged(stateM);
     categoryM->Update();
     }
  Setup();
  SetHelpKeys();
}

void cElvisVODMenu::SetHelpKeys()
{
  cElvisVODItem *item = (cElvisVODItem *)Get(Current());
  switch (showModeM % SHOWMODE_COUNT) {
    case SHOWMODE_NEWEST:
         SetHelp(tr("Button$Popular"), tr("Button$Search"), item ? tr("Button$Add") : NULL, item ? trVDR("Button$Info") : NULL);
         break;
    case SHOWMODE_POPULAR:
         SetHelp(tr("Button$Favorites"), tr("Button$Search"), item ? tr("Button$Add") : NULL, item ? trVDR("Button$Info") : NULL);
         break;
    case SHOWMODE_FAVORITES:
         SetHelp(tr("Button$Newest"), tr("Button$Search"), item ? trVDR("Button$Delete") : NULL, item ? trVDR("Button$Info") : NULL);
         break;
    default:
         SetHelp(NULL, tr("Button$Search"), NULL, item ? trVDR("Button$Info") : NULL);
         break;
    }
}

void cElvisVODMenu::Setup()
{
  int current = Current();

  Clear();

  if (categoryM) {
     LOCK_THREAD_INSTANCE(categoryM);
     switch (showModeM % SHOWMODE_COUNT) {
       case SHOWMODE_NEWEST:
            SetTitle(*cString::sprintf("%s - %s - %s", tr("Elvis"), tr("VOD"), tr("Button$Newest")));
            break;
       case SHOWMODE_POPULAR:
            SetTitle(*cString::sprintf("%s - %s - %s", tr("Elvis"), tr("VOD"), tr("Button$Popular")));
            break;
       case SHOWMODE_FAVORITES:
            SetTitle(*cString::sprintf("%s - %s - %s", tr("Elvis"), tr("VOD"), tr("Button$Favorites")));
            break;
       default:
            SetTitle(*cString::sprintf("%s - %s", tr("Elvis"), tr("VOD")));
            break;
       }
     for (cElvisVOD *item = categoryM->cList<cElvisVOD>::First(); item; item = categoryM->cList<cElvisVOD>::Next(item))
         Add(new cElvisVODItem(item));
     }

  SetCurrent(Get(current));
  Display();
}

eOSState cElvisVODMenu::Info()
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  cElvisVODItem *item = (cElvisVODItem *)Get(Current());
  if (item && item->Vod())
     return AddSubMenu(new cElvisVODInfoMenu(item->Vod()->Title(), item->Description(), item->Vod()->Trailer()));

  return osContinue;
}

eOSState cElvisVODMenu::Favorite()
{
  cElvisVODItem *item = (cElvisVODItem *)Get(Current());

  if (item && item->Vod())
     item->Vod()->SetFavorite((showModeM % SHOWMODE_COUNT) != SHOWMODE_FAVORITES);

  return osContinue;
}

eOSState cElvisVODMenu::ProcessKey(eKeys keyP)
{
  bool HadSubMenu = HasSubMenu();
  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kRed:
            switch (++showModeM % SHOWMODE_COUNT) {
              case SHOWMODE_NEWEST:
                   categoryM = cElvisVODCategories::GetInstance()->GetCategory("newest");
                   break;
              case SHOWMODE_POPULAR:
                   categoryM = cElvisVODCategories::GetInstance()->GetCategory("popular");
                   break;
              case SHOWMODE_FAVORITES:
                   categoryM = cElvisVODCategories::GetInstance()->GetCategory("favorites");
                   break;
              default:
                   categoryM = NULL;
                   break;
              }
            Setup();
            SetHelpKeys();
            return osContinue;
       case kGreen:
            return AddSubMenu(new cElvisVODSearchMenu());
       case kYellow:
            return Favorite();
       case k5:
            if (categoryM)
               categoryM->Update(true);
            return osContinue;
       case kOk:
       case kBlue:
       case kInfo:
            return Info();
       case kNone:
            if (categoryM) {
               categoryM->Update();
               if (categoryM->StateChanged(stateM)) {
                  Setup();
                  SetHelpKeys();
                  return osContinue;
                  }
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
: cOsdMenu(*cString::sprintf("%s - %s", tr("Elvis"), tr("Elisa Viihde"))),
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
  Add(new cOsdItem(hk(tr("VOD")),           osUser6));
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
    case osUser6:
         return AddSubMenu(new cElvisVODMenu);
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
