/*
 * recordings.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_RECORDINGS_H
#define __ELVIS_RECORDINGS_H

#include <vdr/thread.h>
#include <vdr/tools.h>

#include "widget.h"

// --- cElvisRecording -------------------------------------------------

class cElvisRecording : public cListObject {
  friend class cElvisRecordings;
private:
  int idM;
  int programIdM;
  int folderIdM;
  int watchCountM;
  cString nameM;
  cString channelM;
  cString startTimeM;
  cString sizeM;
  cElvisWidgetInfo *infoM;
  time_t startTimeValueM;
  // to prevent default constructor
  cElvisRecording();
  // to prevent copy constructor and assignment
  cElvisRecording(const cElvisRecording&);
  cElvisRecording &operator=(const cElvisRecording &);
public:
  cElvisRecording(int idP, int programIdP, int folderIdP, const char *nameP, const char *channelP, const char *startTimeP, const char *sizeP);
  virtual ~cElvisRecording();
  cElvisWidgetInfo *Info();
  int Id() { return idM; }
  int ProgramId() { return programIdM; }
  int FolderId() { return folderIdM; }
  const char *Name() { return *nameM; }
  const char *Channel() { return *channelM; }
  const char *StartTime() { return *startTimeM; }
  time_t StartTimeValue() { return startTimeValueM; }
  bool IsFolder() { return (programIdM < 0); }
  bool IsNew() { return (watchCountM == 0); }
};

// --- cElvisRecordings ------------------------------------------------

class cElvisRecordings : public cList<cElvisRecording>, public cElvisWidgetRecordingCallbackIf {
private:
  static cElvisRecordings *instanceS;
  cMutex mutexM;
  // constructor
  cElvisRecordings();
  // to prevent copy constructor and assignment
  cElvisRecordings(const cElvisRecordings&);
  cElvisRecordings& operator=(const cElvisRecordings&);
public:
  static cElvisRecordings *GetInstance();
  static void Destroy();
  virtual ~cElvisRecordings();
  virtual void AddRecording(int id, int program_id, int folder_id, const char *name, const char *channel, const char *start_time, const char *size);
  bool Update(bool waitP = false);
  bool Update(int folderIdP = -1);
  bool Delete(cElvisRecording *recordingP);
};

#endif // __ELVIS_RECORDINGS_H
