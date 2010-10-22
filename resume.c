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

cElvisResumeItem::cElvisResumeItem(int programIdP, unsigned long byteOffsetP)
: programIdM(programIdP),
  byteOffsetM(byteOffsetP)
{
  debug("cElvisResumeItem:cElvisResumeItem(): programid=%d, byteoffset=%ld", programIdM, byteOffsetM);
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
     char *value = compactspace(p + 1);
     if (!isempty(key) && !isempty(value)) {
        programIdM = strtoul(key, NULL, 10);
        byteOffsetM = strtoul(value, NULL, 10);
        debug("cElvisResumeItem:Parse(%s): programid=%d, byteoffset=%ld", strP, programIdM, byteOffsetM);
        return true;
        }
     }
  return false;
}

bool cElvisResumeItem::Save(FILE *fdP)
{
  debug("cElvisResumeItem:Save(): programid=%d, byteoffset=%ld", programIdM, byteOffsetM);
  return fprintf(fdP, "%d:%ld\n", programIdM, byteOffsetM) > 0;
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

bool cElvisResumeItems::Store(int programIdP, unsigned long byteOffsetP)
{
  cMutexLock(mutexM);
  debug("cElvisResumeItems::Store(%d, %ld)", programIdP, byteOffsetP);
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
     Add(new cElvisResumeItem(programIdP, byteOffsetP));
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

bool cElvisResumeItems::HasResume(int programIdP, unsigned long &byteOffsetP)
{
  cMutexLock(mutexM);
  debug("cElvisResumeItems::HasResume(%d)", programIdP);
   for (cElvisResumeItem *item = First(); item; item = Next(item)) {
       if (item->ProgramId() == programIdP) {
          byteOffsetP = item->ByteOffset();
          return true;
          }
       }
  return false;
}
