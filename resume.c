/*
 * resume.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "resume.h"

// --- cElvisResumeItem ------------------------------------------------

cElvisResumeItem::cElvisResumeItem()
: programIdM(-1),
  byteOffsetM(0)
{
  debug("cElvisResumeItem:cElvisResumeItem()");
}

cElvisResumeItem::cElvisResumeItem(int programIdP, unsigned long byteOffsetP, unsigned long fileSizeP)
: programIdM(programIdP),
  byteOffsetM(byteOffsetP),
  fileSizeM(fileSizeP)
{
  debug("cElvisResumeItem:cElvisResumeItem(): programid=%d, byteoffset=%ld filesize=%ld", programIdM, byteOffsetM, fileSizeM);
}

cElvisResumeItem::~cElvisResumeItem()
{
  debug("cElvisResumeItem:~cElvisResumeItem()");
}

bool cElvisResumeItem::Parse(const char *strP)
{
  debug("cElvisResumeItem:Parse(%s)", strP);
  char *p = (char*)strchr(strP, ':');
  if (p) {
     *p = 0;
     char *key = compactspace((char *)strP);
     char *s = ++p;
     p = (char*)strchr(s, ':');
     if (p) {
        *p = 0;
        char *value1 = compactspace(s);
        char *value2 = compactspace(p + 1);
        if (!isempty(key) && !isempty(value1) && !isempty(value2)) {
           programIdM = (int)strtoul(key, NULL, 10);
           byteOffsetM = strtoul(value1, NULL, 10);
           fileSizeM = strtoul(value2, NULL, 10);
           debug("cElvisResumeItem:Parse(%s): programid=%d, byteoffset=%ld filesize=%ld", strP, programIdM, byteOffsetM, fileSizeM);
           return true;
           }
        }
     }
  return false;
}

bool cElvisResumeItem::Save(FILE *fdP)
{
  debug("cElvisResumeItem:Save(): programid=%d, byteoffset=%ld filesize=%ld", programIdM, byteOffsetM, fileSizeM);
  return fprintf(fdP, "%d:%ld:%ld\n", programIdM, byteOffsetM, fileSizeM) > 0;
}

// --- cElvisResumeItems -----------------------------------------------

const char *cElvisResumeItems::resumeBaseNameS = "resume.conf";

cElvisResumeItems *cElvisResumeItems::instanceS = NULL;

cElvisResumeItems *cElvisResumeItems::GetInstance()
{
  if (!instanceS)
     instanceS = new cElvisResumeItems();

  return instanceS;
}

void cElvisResumeItems::Destroy()
{
  DELETE_POINTER(instanceS);
}

cElvisResumeItems::cElvisResumeItems()
{
  debug("cElvisResumeItems::cElvisResumeItems()");
}

cElvisResumeItems::~cElvisResumeItems()
{
  cMutexLock(mutexM);
  debug("cElvisResumeItems::~cElvisResumeItems()");
  Save();
}

bool cElvisResumeItems::Load(const char *directoryP)
{
  cMutexLock(mutexM);
  debug("cElvisResumeItems:Load(%s)", directoryP);
  if (cConfig<cElvisResumeItem>::Load(*cString::sprintf("%s/%s", directoryP, resumeBaseNameS), true)) {
     return true;
     }
  return false;
}

bool cElvisResumeItems::Save()
{
  cMutexLock(mutexM);
  debug("cElvisResumeItems:Save()");
  Sort();
  if (cConfig<cElvisResumeItem>::Save()) {
     debug("cElvisResumeItems:Save(): successful");
     return true;
     }
  return false;
}

bool cElvisResumeItems::Store(int programIdP, unsigned long byteOffsetP, unsigned long fileSizeP)
{
  cMutexLock(mutexM);
  debug("cElvisResumeItems::Store(%d, %ld, %ld)", programIdP, byteOffsetP, fileSizeP);
  if (programIdP > 0) {
     cElvisResumeItem *existing = NULL;
     for (cElvisResumeItem *item = First(); item; item = Next(item)) {
         if (item->ProgramId() == programIdP) {
            existing = item;
            break;
            }
         }
     if (existing)
        Del(existing);
     Add(new cElvisResumeItem(programIdP, byteOffsetP, fileSizeP));
     }
  return false;
}

bool cElvisResumeItems::Rewind(int programIdP)
{
  cMutexLock(mutexM);
  debug("cElvisResumeItems::Rewind(%d)", programIdP);
  if (programIdP > 0) {
     cElvisResumeItem *existing = NULL;
     unsigned long filesize = 0;
     for (cElvisResumeItem *item = First(); item; item = Next(item)) {
         if (item->ProgramId() == programIdP) {
            existing = item;
            break;
            }
         }
     if (existing) {
        filesize = existing->FileSize();
        Del(existing);
        }
     Add(new cElvisResumeItem(programIdP, 0, filesize));
     }
  return false;
}

void cElvisResumeItems::Reset()
{
  cMutexLock(mutexM);
  debug("cElvisResumeItems::Reset()");
  for (cElvisResumeItem *item = First(); item; item = Next(item))
      Del(item);
  Save();
}

bool cElvisResumeItems::HasResume(int programIdP, unsigned long &byteOffsetP, unsigned long &fileSizeP)
{
  cMutexLock(mutexM);
  debug("cElvisResumeItems::HasResume(%d)", programIdP);
   for (cElvisResumeItem *item = First(); item; item = Next(item)) {
       if (item->ProgramId() == programIdP) {
          byteOffsetP = item->ByteOffset();
          fileSizeP = item->FileSize();
          return true;
          }
       }
  return false;
}

cElvisResumeItem *cElvisResumeItems::GetResume(int programIdP)
{
  cMutexLock(mutexM);
  debug("cElvisResumeItems::GetResume(%d)", programIdP);
   for (cElvisResumeItem *item = First(); item; item = Next(item)) {
       if (item->ProgramId() == programIdP) {
          return item;
          }
       }
  return NULL;
}
