/*
 * recordings.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "config.h"
#include "recordings.h"

// --- cElvisRecording -------------------------------------------------

cElvisRecording::cElvisRecording(int idP, int programIdP, int folderIdP, int countP, int lengthP, const char *nameP, const char *channelP, const char *startTimeP)
: taggedM(true),
  protectedM(false),
  idM(idP),
  programIdM(programIdP),
  folderIdM(folderIdP),
  countM(countP),
  lengthM(lengthP),
  nameM(nameP),
  channelM(channelP),
  startTimeM(startTimeP),
  sizeM(NULL),
  infoM(NULL),
  startTimeValueM(strtotime(startTimeP))
{
}

cElvisRecording::cElvisRecording(int idP, int countP, const char *nameP, const char *sizeP)
: taggedM(true),
  protectedM(false),
  idM(idP),
  programIdM(-1),
  folderIdM(idP),
  countM(countP),
  lengthM(-1),
  nameM(nameP),
  channelM(NULL),
  startTimeM(NULL),
  sizeM(sizeP),
  infoM(NULL),
  startTimeValueM(0)
{
}

cElvisRecording::~cElvisRecording()
{
  DeleteInfo();
}

cElvisWidgetEventInfo *cElvisRecording::Info()
{
  if (!infoM)
     infoM = cElvisWidget::GetInstance()->GetEventInfo(programIdM);

 return infoM;
}

void cElvisRecording::DeleteInfo()
{
  DELETE_POINTER(infoM);
}

// --- cElvisRecordingFolder -------------------------------------------

cElvisRecordingFolder::cElvisRecordingFolder(int folderIdP, const char *folderNameP)
: cThread(*cString::sprintf("cElvisRecordingFolder(%d)", folderIdP)),
  taggedM(true),
  stateM(0),
  lastUpdateM(0),
  folderIdM(folderIdP),
  folderNameM(folderNameP),
  protectedM(false),
  countM(0)
{
}

cElvisRecordingFolder::~cElvisRecordingFolder()
{
  LOCK_THREAD;

  Cancel(3);
}

void cElvisRecordingFolder::UpdateFolder(bool protectedP, int countP)
{
  LOCK_THREAD;
  protectedM = protectedP;
  countM = countP;
}

void cElvisRecordingFolder::AddFolder(int idP, int countP, const char *nameP, const char *sizeP)
{
  LOCK_THREAD;
  cElvisRecording *rec = GetRecording(idP);
  if (rec)
     rec->Tag(true);
  else {
     rec = new cElvisRecording(idP, countP, nameP, sizeP);
     Add(rec);
     cElvisRecordings::GetInstance()->AddFolder(rec->Id(), rec->Name());
     ChangeState();
     }
}

void cElvisRecordingFolder::AddRecording(int idP, int programIdP, int folderIdP, int countP, int lengthP, const char *nameP, const char *channelP, const char *startTimeP)
{
  LOCK_THREAD;
  cElvisRecording *rec = GetRecording(idP);
  if (rec) {
     rec->Tag(true);
     rec->DeleteInfo();
     }
  else {
     Add(new cElvisRecording(idP, programIdP, folderIdP, countP, lengthP, nameP, channelP, startTimeP));
     ChangeState();
     }
}

bool cElvisRecordingFolder::DeleteRecording(cElvisRecording *recordingP)
{
  LOCK_THREAD;
  if (recordingP && cElvisWidget::GetInstance()->RemoveRecording(recordingP->Id())) {
     Del(recordingP);
     return true;
     }

  return false;
}

cElvisRecording *cElvisRecordingFolder::GetRecording(int idP)
{
  for (cElvisRecording *i = cList<cElvisRecording>::First(); i; i = cList<cElvisRecording>::Next(i)) {
      if (i->Id() == idP)
         return i;
      }

  return NULL;
}

void cElvisRecordingFolder::Refresh(bool foregroundP)
{
  lastUpdateM = time(NULL);
  {
    LOCK_THREAD;
    if (foregroundP) {
       Clear();
       ChangeState();
       }
    else {
       for (cElvisRecording *i = cList<cElvisRecording>::First(); i; i = cList<cElvisRecording>::Next(i))
           i->Tag(false);
       }
  }
  cElvisWidget::GetInstance()->GetRecordings(*this, folderIdM);
  {
    LOCK_THREAD;
    for (cElvisRecording *i = cList<cElvisRecording>::First(); i; i = cList<cElvisRecording>::Next(i)) {
        if (!i->IsTagged()) {
           if (i->IsFolder())
              cElvisRecordings::GetInstance()->DeleteFolder(i->FolderId());
           Del(i);
           }
        }
    ChangeState();
  }
}

bool cElvisRecordingFolder::Update(bool waitP)
{
  if (waitP) {
     Refresh(true);
     return (Count() > 0);
     }
  else if ((time(NULL) - lastUpdateM) >= eUpdateInterval)
     Start();

  return false;
}

bool cElvisRecordingFolder::StateChanged(int &stateP)
{
  LOCK_THREAD;
  bool result = (stateP != stateM);

  stateP = stateM;

  return result;
}

void cElvisRecordingFolder::Action()
{
  Refresh();
}


// --- cElvisRecordings ------------------------------------------------

cElvisRecordings *cElvisRecordings::instanceS = NULL;

cElvisRecordings *cElvisRecordings::GetInstance()
{
  if (!instanceS)
     instanceS = new cElvisRecordings();

  return instanceS;
}

void cElvisRecordings::Destroy()
{
  DELETE_POINTER(instanceS);
}

cElvisRecordings::cElvisRecordings()
: cThread("cElvisRecordings"),
  stateM(0),
  lastUpdateM(0)
{
  AddFolder(-1, tr("(default)"));
}

cElvisRecordings::~cElvisRecordings()
{
  LOCK_THREAD;

  Cancel(3);
}

void cElvisRecordings::AddFolder(int folderIdP, const char *folderNameP, int countP, bool protectedP)
{
  LOCK_THREAD;
  cElvisRecordingFolder *folder = NULL;
  for (cElvisRecordingFolder *i = First(); i; i = Next(i)) {
      if (i->Id() == folderIdP) {
         folder = i;
         break;
         }
      }

  if (folder)
     folder->UpdateFolder(protectedP, countP);
  else {
     folder = new cElvisRecordingFolder(folderIdP, folderNameP);
     if (folder) {
        folder->UpdateFolder(protectedP, countP);
        Add(folder);
        Sort();
        }
     }
}

cElvisRecordingFolder *cElvisRecordings::AddFolder(int folderIdP, const char *folderNameP)
{
  LOCK_THREAD;
  cElvisRecordingFolder *folder = NULL;
  for (cElvisRecordingFolder *i = First(); i; i = Next(i)) {
      if (i->Id() == folderIdP)
         return i;
      }

  folder = new cElvisRecordingFolder(folderIdP, folderNameP);
  if (folder) {
     Add(folder);
     Sort();
     }

  return folder;
}

bool cElvisRecordings::DeleteFolder(int folderIdP)
{
  LOCK_THREAD;
  cElvisRecordingFolder *folder = GetFolder(folderIdP);

  if (folder) {
     Del(folder);
     Sort();

     return true;
     }

  return false;
}

cElvisRecordingFolder *cElvisRecordings::GetFolder(int folderIdP)
{
  LOCK_THREAD;

  for (cElvisRecordingFolder *i = First(); i; i = Next(i)) {
      if (i->Id() == folderIdP)
         return i;
      }

  return NULL;
}

bool cElvisRecordings::RemoveRecordingFolder(int folderIdP)
{
  LOCK_THREAD;

  return cElvisWidget::GetInstance()->RemoveFolder(folderIdP) ? DeleteFolder(folderIdP) : false;
}

bool cElvisRecordings::RenameRecordingFolder(int folderIdP, const char *folderNameP)
{
  LOCK_THREAD;

  if (cElvisWidget::GetInstance()->RenameFolder(folderIdP, folderNameP)) {
     cElvisRecordingFolder *folder = GetFolder(folderIdP);
     if (folder)
        folder->SetName(folderNameP);

     return true;
     }

  return false;
}

bool cElvisRecordings::Update(bool waitP)
{
  if (waitP) {
     Refresh(true);
     return (Count() > 0);
     }
  else if ((time(NULL) - lastUpdateM) >= eUpdateInterval)
     Start();

  return false;
}

void cElvisRecordings::Refresh(bool foregroundP)
{
  lastUpdateM = time(NULL);
  {
    LOCK_THREAD;
    if (foregroundP) {
       Clear();
       AddFolder(-1, tr("(default)"));
       ChangeState();
       }
    else {
       for (cElvisRecordingFolder *i = cList<cElvisRecordingFolder>::First(); i; i = cList<cElvisRecordingFolder>::Next(i))
           i->Tag(false);
       }
  }
  cElvisWidget::GetInstance()->GetFolders(*this);
  {
    LOCK_THREAD;
    for (cElvisRecordingFolder *i = cList<cElvisRecordingFolder>::First(); i; i = cList<cElvisRecordingFolder>::Next(i)) {
        if (!i->IsTagged()) {
           DeleteFolder(i->Id());
           Del(i);
           }
        }
    ChangeState();
  }
}

void cElvisRecordings::Action()
{
  Refresh();
}
