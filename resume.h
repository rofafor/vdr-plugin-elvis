/*
 * resume.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_RESUME_H
#define __ELVIS_RESUME_H

#include <vdr/thread.h>
#include <vdr/tools.h>

// --- cElvisResumeItem -------------------------------------------------

class cElvisResumeItem : public cListObject {
private:
  int programIdM;
  unsigned long byteOffsetM;
  unsigned long fileSizeM;
public:
  cElvisResumeItem();
  cElvisResumeItem(int programIdP, unsigned long byteOffsetP, unsigned long fileSizeP);
  virtual ~cElvisResumeItem();
  bool Parse(const char *strP);
  bool Save(FILE *fdP);
  int ProgramId() { return programIdM; }
  unsigned long ByteOffset() { return byteOffsetM; }
  unsigned long FileSize() { return fileSizeM; }
};

// --- cElvisResumeItems ------------------------------------------------

class cElvisResumeItems : public cConfig<cElvisResumeItem> {
private:
  static const char *resumeBaseNameS;
  static cElvisResumeItems *instanceS;
  cMutex mutexM;
  cString filenameM;
  // constructor
  cElvisResumeItems();
  // to prevent copy constructor and assignment
  cElvisResumeItems(const cElvisResumeItems&);
  cElvisResumeItems& operator=(const cElvisResumeItems&);
public:
  static cElvisResumeItems *GetInstance();
  static void Destroy();
  virtual ~cElvisResumeItems();
  bool Load(const char *directoryP);
  bool Save();
  bool Store(int programIdP, unsigned long byteOffsetP, unsigned long fileSizeP);
  bool Rewind(int programIdP);
  void Reset();
  bool HasResume(int programId, unsigned long &byteOffsetP, unsigned long &fileSizeP);
  cElvisResumeItem *GetResume(int programIdP);
};

#endif // __ELVIS_RESUME_H
