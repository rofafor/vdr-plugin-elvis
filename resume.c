/*
 * resume.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "log.h"
#include "resume.h"

// --- cElvisResumeItem ------------------------------------------------

cElvisResumeItem::cElvisResumeItem()
: programIdM(-1),
  byteOffsetM(0),
  fileSizeM(0)
{
  debug1("%s", __PRETTY_FUNCTION__);
}

cElvisResumeItem::cElvisResumeItem(int programIdP, unsigned long byteOffsetP, unsigned long fileSizeP)
: programIdM(programIdP),
  byteOffsetM(byteOffsetP),
  fileSizeM(fileSizeP)
{
  debug1("%s (%d, %ld, %ld)", __PRETTY_FUNCTION__, programIdM, byteOffsetM, fileSizeM);
}

cElvisResumeItem::~cElvisResumeItem()
{
  debug1("%s", __PRETTY_FUNCTION__);
}

bool cElvisResumeItem::Parse(const char *strP)
{
  debug1("%s (%s)", __PRETTY_FUNCTION__, strP);
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
           debug6("%s (%s) programid=%d, byteoffset=%ld filesize=%ld", __PRETTY_FUNCTION__, strP, programIdM, byteOffsetM, fileSizeM);
           return true;
           }
        }
     }
  return false;
}

bool cElvisResumeItem::Save(FILE *fdP)
{
  debug1("%s programid=%d, byteoffset=%ld filesize=%ld", __PRETTY_FUNCTION__, programIdM, byteOffsetM, fileSizeM);
  return fprintf(fdP, "%d:%lu:%lu\n", programIdM, byteOffsetM, fileSizeM) > 0;
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
  debug1("%s", __PRETTY_FUNCTION__);
}

cElvisResumeItems::~cElvisResumeItems()
{
  cMutexLock(mutexM);
  debug1("%s", __PRETTY_FUNCTION__);
  Save();
}

bool cElvisResumeItems::Load(const char *directoryP)
{
  cMutexLock(mutexM);
  debug1("%s (%s)", __PRETTY_FUNCTION__, directoryP);
  if (cConfig<cElvisResumeItem>::Load(*cString::sprintf("%s/%s", directoryP, resumeBaseNameS), true)) {
     return true;
     }
  return false;
}

bool cElvisResumeItems::Save()
{
  cMutexLock(mutexM);
  debug1("%s", __PRETTY_FUNCTION__);
  Sort();
  if (cConfig<cElvisResumeItem>::Save()) {
     debug6("%s: Successful", __PRETTY_FUNCTION__);
     return true;
     }
  return false;
}

bool cElvisResumeItems::Store(int programIdP, unsigned long byteOffsetP, unsigned long fileSizeP)
{
  cMutexLock(mutexM);
  debug1("%s (%d, %ld, %ld)", __PRETTY_FUNCTION__, programIdP, byteOffsetP, fileSizeP);
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
  debug1("%s (%d)", __PRETTY_FUNCTION__, programIdP);
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
  debug1("%s", __PRETTY_FUNCTION__);
  for (cElvisResumeItem *item = First(); item; item = Next(item))
      Del(item);
  Save();
}

bool cElvisResumeItems::HasResume(int programIdP, unsigned long &byteOffsetP, unsigned long &fileSizeP)
{
  cMutexLock(mutexM);
  debug1("%s (%d, ,)", __PRETTY_FUNCTION__, programIdP);
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
  debug1("%s (%d)", __PRETTY_FUNCTION__, programIdP);
  for (cElvisResumeItem *item = First(); item; item = Next(item)) {
      if (item->ProgramId() == programIdP) {
         return item;
         }
      }
  return NULL;
}
