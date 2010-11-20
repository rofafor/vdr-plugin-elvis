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
  friend class cElvisRecordingFolder;
private:
  bool taggedM;
  int idM;
  int programIdM;
  int folderIdM;
  int countM;
  int lengthM;
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
  cElvisRecording(int idP, int programIdP, int folderIdP, int countP, int lengthP, const char *nameP, const char *channelP, const char *startTimeP);
  cElvisRecording(int idP, int countP, const char *nameP, const char *sizeP);
  virtual ~cElvisRecording();
  cElvisWidgetInfo *Info();
  void Tag(bool onOffP) { taggedM = onOffP; } 
  bool IsTagged() { return taggedM; }
  int Id() { return idM; }
  int ProgramId() { return programIdM; }
  int FolderId() { return folderIdM; }
  int Count() { return countM; }
  int LengthInMinutes() { return lengthM; }
  const char *Name() { return *nameM; }
  const char *Channel() { return *channelM; }
  const char *StartTime() { return *startTimeM; }
  time_t StartTimeValue() { return startTimeValueM; }
  bool IsFolder() { return (programIdM < 0); }
};

// --- cElvisRecordingFolder -------------------------------------------

class cElvisRecordingFolder : public cThread, public cListObject, public cList<cElvisRecording>, public cElvisWidgetRecordingCallbackIf {
private:
  enum {
    eUpdateInterval = 3600 // 60min
  };
  bool taggedM;
  int stateM;
  time_t lastUpdateM;
  int folderIdM;
  cString folderNameM;
  cElvisRecording *GetRecording(int idP);
  void Refresh(bool foregroundP = false);
  // to prevent default constructor
  cElvisRecordingFolder();
  // to prevent copy constructor and assignment
  cElvisRecordingFolder(const cElvisRecordingFolder&);
  cElvisRecordingFolder &operator=(const cElvisRecordingFolder &);
protected:
  void Action();
public:
  cElvisRecordingFolder(int folderIdP, const char *folderNameP);
  virtual ~cElvisRecordingFolder();
  virtual void AddFolder(int idP, int countP, const char *nameP, const char *sizeP);
  virtual void AddRecording(int idP, int programIdP, int folderIdP, int countP, int lengthP, const char *nameP, const char *channelP, const char *startTimeP);
  bool DeleteRecording(cElvisRecording *recordingP);
  void Tag(bool onOffP) { taggedM = onOffP; }
  bool IsTagged() { return taggedM; }
  bool Update(bool waitP = false);
  void ChangeState(void) { ++stateM; }
  bool StateChanged(int &stateP);
  int Id() { return folderIdM; }
  const char *Name() { return *folderNameM; }
};

// --- cElvisRecordings ------------------------------------------------

class cElvisRecordings : public cList<cElvisRecordingFolder> {
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
  cElvisRecordingFolder *AddFolder(int folderIdP, const char *folderNameP);
  bool DeleteFolder(int folderIdP);
  cElvisRecordingFolder *GetFolder(int folderIdP);
  void Reset(bool foregroundP = true);
};

#endif // __ELVIS_RECORDINGS_H
