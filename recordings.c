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

cElvisRecording::cElvisRecording(int idP, int programIdP, int folderIdP, const char *nameP, const char *channelP, const char *startTimeP, const char *sizeP)
: idM(idP),
  programIdM(programIdP),
  folderIdM(folderIdP),
  nameM(nameP),
  channelM(channelP),
  startTimeM(startTimeP),
  sizeM(sizeP),
  infoM(NULL),
  startTimeValueM(strtotime(startTimeP))
{
}

cElvisRecording::~cElvisRecording()
{
  DELETE_POINTER(infoM);
}

cElvisWidgetInfo *cElvisRecording::Info()
{
  if (!infoM)
     infoM = cElvisWidget::GetInstance()->GetEventInfo(programIdM);

 return infoM;
}

// --- cElvisRecordingFolder -------------------------------------------

cElvisRecordingFolder::cElvisRecordingFolder(int folderIdP, const char *folderNameP)
: cThread(*cString::sprintf("cElvisRecordingFolder(%d)", folderIdP)),
  mutexM(),
  stateM(0),
  lastUpdateM(0),
  folderIdM(folderIdP),
  folderNameM(folderNameP)
{
}

cElvisRecordingFolder::~cElvisRecordingFolder()
{
  cMutexLock(mutexM);

  Cancel(3);
}

void cElvisRecordingFolder::AddRecording(int idP, int programIdP, int folderIdP, const char *nameP, const char *channelP, const char *startTimeP, const char *sizeP)
{
  cMutexLock(mutexM);
  cElvisRecording *rec = new cElvisRecording(idP, programIdP, folderIdP, nameP, channelP, startTimeP, sizeP);
  Add(rec);
  if (rec->IsFolder())
     cElvisRecordings::GetInstance()->AddFolder(rec->Id(), rec->Name());
  ChangeState();
}

bool cElvisRecordingFolder::DeleteRecording(cElvisRecording *recordingP)
{
  cMutexLock(mutexM);
  if (recordingP && cElvisWidget::GetInstance()->RemoveRecording(recordingP->Id())) {
     Del(recordingP);
     return true;
     }

  return false;
}

void cElvisRecordingFolder::Refresh(bool foregroundP)
{
  lastUpdateM = time(NULL);
  mutexM.Lock();
  Clear();
  ChangeState();
  mutexM.Unlock();
  cElvisWidget::GetInstance()->GetRecordings(*this, folderIdM);
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
  cMutexLock(mutexM);
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

cElvisRecordingFolder *cElvisRecordings::GetFolder(int folderIdP)
{
  cMutexLock(mutexM);
  cElvisRecordingFolder *folder = NULL;
  for (cElvisRecordingFolder *i = First(); i; i = Next(i)) {
      if (i->Id() == folderIdP)
         return i;
      }

  return folder;
}

void cElvisRecordings::Reset()
{
  cMutexLock(mutexM);
  Clear();
  cElvisRecordingFolder *folder = new cElvisRecordingFolder(-1, tr("(default)"));
  if (folder) {
     Add(folder);
     folder->Update(true);
     }
}
