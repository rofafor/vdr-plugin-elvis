/*
 * config.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_CONFIG_H
#define __ELVIS_CONFIG_H

#include <vdr/config.h>

class cElvisConfig : public cConfig<cSetupLine> {
private:
  enum {
    eMaxCredentials = 32
  };
  static const char *confBaseNameS;
  bool Parse(const char *nameP, const char *valueP);
  cSetupLine *Get(const char *nameP);
  void Store(const char *nameP, const char *valueP);
  void Store(const char *nameP, int valueP);
public:
  int __BeginData__;
  int HideMenu;
  int Service;
  int Ssl;
  int ReplaceSchedule;
  int ReplaceTimers;
  int ReplaceRecordings;
  char Username[eMaxCredentials];
  char Password[eMaxCredentials];
  int __EndData__;
  cElvisConfig();
  cElvisConfig& operator= (const cElvisConfig &objP);
  bool Load(const char *directoryP);
  bool Save();
};

extern cElvisConfig ElvisConfig;

#endif // __ELVIS_CONFIG_H
