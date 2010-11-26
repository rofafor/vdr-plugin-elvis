/*
 * events.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "config.h"
#include "vod.h"

cElvisVOD::cElvisVOD(int idP, int lengthP, int ageLimitP, int yearP, int priceP, const char *titleP, const char *currencyP, const char *coverP, const char *trailerP)
: taggedM(true),
  idM(idP),
  lengthM(lengthP),
  ageLimitM(ageLimitP),
  yearM(yearP),
  priceM(priceP),
  titleM(titleP),
  currencyM(currencyP),
  coverM(coverP),
  trailerM(trailerP),
  infoM(NULL)
{
}

cElvisVOD::~cElvisVOD()
{
}

cElvisWidgetVODInfo *cElvisVOD::Info()
{
  if (!infoM)
     infoM = cElvisWidget::GetInstance()->GetVODInfo(idM);

  return infoM;
}

// --- cElvisVODCategory -----------------------------------------------

cElvisVODCategory::cElvisVODCategory(const char *categoryP)
: cThread(*cString::sprintf("cElvisVODCategory:%s", categoryP)),
  stateM(0),
  lastUpdateM(0),
  categoryM(categoryP)
{
}

cElvisVODCategory::~cElvisVODCategory()
{
  cThreadLock(this);
  Cancel(3);
}

void cElvisVODCategory::AddVOD(int idP, int lengthP, int ageLimitP, int yearP, int priceP, const char *titleP, const char *currencyP, const char *coverP, const char *trailerP)
{
  cThreadLock(this);
  cElvisVOD *vod = GetVOD(idP);

  if (vod)
     vod->Tag(true);
  else {
     Add(new cElvisVOD(idP, lengthP, ageLimitP, yearP, priceP, titleP, currencyP, coverP, trailerP));
     ChangeState();
     }
}

cElvisVOD *cElvisVODCategory::GetVOD(int idP)
{
  for (cElvisVOD *i = cList<cElvisVOD>::First(); i; i = cList<cElvisVOD>::Next(i)) {
      if (i->Id() == idP)
         return i;
      }

  return NULL;
}

void cElvisVODCategory::Refresh(bool foregroundP)
{
  lastUpdateM = time(NULL);
  Lock();
  if (foregroundP) {
     Clear();
     ChangeState();
     }
  else {
     for (cElvisVOD *i = cList<cElvisVOD>::First(); i; i = cList<cElvisVOD>::Next(i))
         i->Tag(false);
     }
  Unlock();
  cElvisWidget::GetInstance()->GetVOD(*this, *categoryM);
  Lock();
  for (cElvisVOD *i = cList<cElvisVOD>::First(); i; i = cList<cElvisVOD>::Next(i)) {
      if (!i->IsTagged())
         Del(i);
      }
  ChangeState();
  Unlock();
}

bool cElvisVODCategory::Update(bool waitP)
{
  if (waitP) {
     Refresh(true);
     return (Count() > 0);
     }
  else if ((time(NULL) - lastUpdateM) >= eUpdateInterval)
     Start();

  return false;
}

bool cElvisVODCategory::StateChanged(int &stateP)
{
  cThreadLock(this);
  bool result = (stateP != stateM);

  stateP = stateM;

  return result;
}

void cElvisVODCategory::Action()
{
  Refresh();
}

// --- cElvisVODCategories ---------------------------------------------

cElvisVODCategories *cElvisVODCategories::instanceS = NULL;

cElvisVODCategories *cElvisVODCategories::GetInstance()
{
  if (!instanceS)
     instanceS = new cElvisVODCategories();

  return instanceS;
}

void cElvisVODCategories::Destroy()
{
  DELETE_POINTER(instanceS);
}

cElvisVODCategories::cElvisVODCategories()
{
  Add(new cElvisVODCategory("newest"));
  Add(new cElvisVODCategory("popular"));
}

cElvisVODCategories::~cElvisVODCategories()
{
}

cElvisVODCategory *cElvisVODCategories::AddCategory(const char *categoryP)
{
  cMutexLock(mutexM);
  cElvisVODCategory *category = NULL;
  for (cElvisVODCategory *i = First(); i; i = Next(i)) {
      if (strcmp(i->Name(), categoryP) == 0)
         return i;
      }

  category = new cElvisVODCategory(categoryP);
  if (category) {
     Add(category);
     Sort();
     }

  return category;
}

bool cElvisVODCategories::DeleteCategory(const char *categoryP)
{
  cMutexLock(mutexM);
  cElvisVODCategory *category = GetCategory(categoryP);

  if (category) {
     Del(category);
     Sort();

     return true;
     }

  return false;
}

cElvisVODCategory *cElvisVODCategories::GetCategory(const char *categoryP)
{
  cMutexLock(mutexM);
  cElvisVODCategory *category = NULL;
  for (cElvisVODCategory *i = First(); i; i = Next(i)) {
      if (strcmp(i->Name(), categoryP) == 0)
         return i;
      }

  return category;
}

void cElvisVODCategories::Reset(bool foregroundP)
{
  cMutexLock(mutexM);
  Clear();
  Add(new cElvisVODCategory("newest"));
  Add(new cElvisVODCategory("popular"));
  for (cElvisVODCategory *i = First(); i; i = Next(i))
      i->Update(foregroundP);
}
