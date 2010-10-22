/*
 * common.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_COMMON_H
#define __ELVIS_COMMON_H

#include <vdr/menuitems.h>
#include <vdr/tools.h>

#ifdef DEBUG
#define debug(x...) dsyslog("ELVIS: " x)
#define info(x...)  isyslog("ELVIS: " x)
#define error(x...) esyslog("ELVIS: " x)
#else
#define debug(x...) ;
#define info(x...)  isyslog("ELVIS: " x)
#define error(x...) esyslog("ELVIS: " x)
#endif

#define DELETE_POINTER(ptr)       \
  do {                            \
     if (ptr) {                   \
        typeof(*ptr) *tmp = ptr;  \
        ptr = NULL;               \
        delete(tmp);              \
        }                         \
  } while (0)

extern const char VERSION[];
extern cString    strunescape(const char *s);
extern cString    strescape(const char *s);
extern cString    WeekDateString(time_t t);
extern cString    ShortDateString(time_t t);
extern time_t     strtotime(const char *s);

class cMenuEditHiddenStrItem : public cMenuEditItem {
private:
  char *value;
  int length;
  const char *allowed;
  int pos, offset;
  bool insert, newchar, uppercase;
  int lengthUtf8;
  uint *valueUtf8;
  uint *allowedUtf8;
  uint *charMapUtf8;
  uint *currentCharUtf8;
  eKeys lastKey;
  cTimeMs autoAdvanceTimeout;
  void SetHelpKeys();
  uint *IsAllowed(uint c);
  void AdvancePos();
  virtual void Set();
  uint Inc(uint c, bool Up);
  void Type(uint c);
  void Insert();
  void Delete();
protected:
  void EnterEditMode();
  void LeaveEditMode(bool SaveValue = false);
  bool InEditMode() { return valueUtf8 != NULL; }
public:
  cMenuEditHiddenStrItem(const char *Name, char *Value, int Length, const char *Allowed = NULL);
  ~cMenuEditHiddenStrItem();
  virtual eOSState ProcessKey(eKeys Key);
  };

#endif // __ELVIS_COMMON_H

