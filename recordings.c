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
  idM(idP),
  programIdM(-1),
  folderIdM(-1),
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
  DELETE_POINTER(infoM);
}

cElvisWidgetEventInfo *cElvisRecording::Info()
{
  if (!infoM)
     infoM = cElvisWidget::GetInstance()->GetEventInfo(programIdM);

 return infoM;
}

// --- cElvisRecordingFolder -------------------------------------------

cElvisRecordingFolder::cElvisRecordingFolder(int folderIdP, const char *folderNameP)
: cThread(*cString::sprintf("cElvisRecordingFolder(%d)", folderIdP)),
  taggedM(true),
  stateM(0),
  lastUpdateM(0),
  folderIdM(folderIdP),
  folderNameM(folderNameP)
{
}

cElvisRecordingFolder::~cElvisRecordingFolder()
{
  cThreadLock(this);

  Cancel(3);
}

void cElvisRecordingFolder::AddFolder(int idP, int countP, const char *nameP, const char *sizeP)
{
  cThreadLock(this);
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
  cThreadLock(this);
  cElvisRecording *rec = GetRecording(idP);
  if (rec)
     rec->Tag(true);
  else {
     Add(new cElvisRecording(idP, programIdP, folderIdP, countP, lengthP, nameP, channelP, startTimeP));
     ChangeState();
     }
}

bool cElvisRecordingFolder::DeleteRecording(cElvisRecording *recordingP)
{
  cThreadLock(this);
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
  Lock();
  if (foregroundP) {
     Clear();
     ChangeState();
     }
  else {
     for (cElvisRecording *i = cList<cElvisRecording>::First(); i; i = cList<cElvisRecording>::Next(i))
         i->Tag(false);
     }
  Unlock();
  cElvisWidget::GetInstance()->GetRecordings(*this, folderIdM);
  Lock();
  for (cElvisRecording *i = cList<cElvisRecording>::First(); i; i = cList<cElvisRecording>::Next(i)) {
      if (!i->IsTagged()) {
         if (i->IsFolder())
            cElvisRecordings::GetInstance()->DeleteFolder(i->FolderId());
         Del(i);
         }
      }
  ChangeState();
  Unlock();
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
  cThreadLock(this);
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
{
}

cElvisRecordings::~cElvisRecordings()
{
}

cElvisRecordingFolder *cElvisRecordings::AddFolder(int folderIdP, const char *folderNameP)
{
  cMutexLock(mutexM);
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
  cMutexLock(mutexM);
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
  cMutexLock(mutexM);
  cElvisRecordingFolder *folder = NULL;
  for (cElvisRecordingFolder *i = First(); i; i = Next(i)) {
      if (i->Id() == folderIdP)
         return i;
      }

  if (folderIdP < 0) {
     folder = new cElvisRecordingFolder(-1, tr("(default)"));
     Add(folder);
     }

  return folder;
}

void cElvisRecordings::Reset(bool foregroundP)
{
  cMutexLock(mutexM);
  Clear();
  cElvisRecordingFolder *folder = new cElvisRecordingFolder(-1, tr("(default)"));
  if (folder) {
     Add(folder);
     folder->Update(foregroundP);
     }
}
