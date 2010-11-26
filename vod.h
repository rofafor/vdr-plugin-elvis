/*
 * vod.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_VOD_H
#define __ELVIS_VOD_H

#include <vdr/thread.h>
#include <vdr/tools.h>

#include "widget.h"

class cElvisVOD : public cListObject {
private:
  bool taggedM;
  int idM;
  int lengthM;
  int ageLimitM;
  int yearM;
  int priceM;
  cString titleM;
  cString currencyM;
  cString coverM;
  cString trailerM;
  cElvisWidgetVODInfo *infoM;
  // to prevent default constructor
  cElvisVOD();
  // to prevent copy constructor and assignment
  cElvisVOD(const cElvisVOD&);
  cElvisVOD &operator=(const cElvisVOD &);
public:
  cElvisVOD(int idP, int lengthP, int ageLimitP, int yearP, int priceP, const char *titleP, const char *currencyP, const char *coverP, const char *trailerP);
  virtual ~cElvisVOD();
  cElvisWidgetVODInfo *Info();
  void Tag(bool onOffP) { taggedM = onOffP; } 
  bool IsTagged() { return taggedM; }
  int Id() { return idM; }
  int Length() { return lengthM; }
  int AgeLimit() { return ageLimitM; }
  int Year() { return yearM; }
  int Price() { return priceM; }
  const char *Title() { return *titleM; }
  const char *Currency() { return *currencyM; }
  const char *Cover() { return *coverM; }
  const char *Trailer() { return *trailerM; }
};

// --- cElvisVODCategory -----------------------------------------------

class cElvisVODCategory : public cThread, public cListObject, public cList<cElvisVOD>, public cElvisWidgetVODCallbackIf {
private:
  enum {
    eUpdateInterval = 7200 // 120min
  };
  int stateM;
  time_t lastUpdateM;
  cString categoryM;
  void Refresh(bool foregroundP = false);
  cElvisVOD *GetVOD(int idP);
  // constructor
  cElvisVODCategory();
  // to prevent copy constructor and assignment
  cElvisVODCategory(const cElvisVODCategory&);
  cElvisVODCategory& operator=(const cElvisVODCategory&);
protected:
  void Action();
public:
  cElvisVODCategory(const char *categoryP);
  virtual ~cElvisVODCategory();
  virtual void AddVOD(int idP, int lengthP, int ageLimitP, int yearP, int priceP, const char *titleP, const char *currencyP, const char *coverP, const char *trailerP);
  bool Update(bool waitP = false);
  void ChangeState() { ++stateM; }
  bool StateChanged(int &stateP);
  const char *Name() { return *categoryM; }
};

// --- cElvisVODCategories ---------------------------------------------

class cElvisVODCategories : public cList<cElvisVODCategory> {
private:
  static cElvisVODCategories *instanceS;
  cMutex mutexM;
  // constructor
  cElvisVODCategories();
  // to prevent copy constructor and assignment
  cElvisVODCategories(const cElvisVODCategories&);
  cElvisVODCategories& operator=(const cElvisVODCategories&);
public:
  static cElvisVODCategories *GetInstance();
  static void Destroy();
  virtual ~cElvisVODCategories();
  cElvisVODCategory *AddCategory(const char *categoryP);
  bool DeleteCategory(const char *categoryP);
  cElvisVODCategory *GetCategory(const char *categoryP);
  void Reset(bool foregroundP = true);
};

#endif // __ELVIS_VOD_H
