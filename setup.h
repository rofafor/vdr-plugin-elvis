/*
 * setup.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_SETUP_H
#define __ELVIS_SETUP_H

#include <vdr/menu.h>

#include "common.h"

class cPluginElvisSetup : public cMenuSetupPage
{
private:
  int hideMenuM;
  int replaceScheduleM;
  int replaceTimersM;
  int replaceRecordingsM;
  char usernameM[CREDENTIALS_MAX];
  char passwordM[CREDENTIALS_MAX];
  cVector<const char*> helpM;
  void SetHelpKeys();
  void Setup();

protected:
  virtual eOSState ProcessKey(eKeys keyP);
  virtual void Store();

public:
  cPluginElvisSetup();
};

#endif // __ELVIS_SETUP_H
