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
  infoM(NULL)
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
  Clear();
}

cElvisRecordings::~cElvisRecordings()
{
}

void cElvisRecordings::AddRecording(int idP, int programIdP, int folderIdP, const char *nameP, const char *channelP, const char *starttimeP, const char *sizeP)
{
  Add(new cElvisRecording(idP, programIdP, folderIdP, nameP, channelP, starttimeP, sizeP));
}

bool cElvisRecordings::Update(int folderIdP)
{
  cMutexLock(mutexM);
  Clear();
  cElvisWidget::GetInstance()->GetRecordings(*this, folderIdP);
  return (Count() > 0);
}

bool cElvisRecordings::Delete(cElvisRecording *recordingP)
{
  cMutexLock(mutexM);
  if (recordingP && cElvisWidget::GetInstance()->RemoveRecording(recordingP->Id())) {
     //Del(recordingP);
     Update();
     return true;
     }

  return false;
}
