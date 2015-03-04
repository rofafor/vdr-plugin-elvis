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
  unsigned int traceModeM;
  static const char *confBaseNameS;
  bool Parse(const char *nameP, const char *valueP);
  cSetupLine *Get(const char *nameP);
  void Store(const char *nameP, const char *valueP);
  void Store(const char *nameP, int valueP);
public:
  enum eTraceMode {
    eTraceModeNormal  = 0x0000,
    eTraceModeDebug1  = 0x0001,
    eTraceModeDebug2  = 0x0002,
    eTraceModeDebug3  = 0x0004,
    eTraceModeDebug4  = 0x0008,
    eTraceModeDebug5  = 0x0010,
    eTraceModeDebug6  = 0x0020,
    eTraceModeDebug7  = 0x0040,
    eTraceModeDebug8  = 0x0080,
    eTraceModeDebug9  = 0x0100,
    eTraceModeDebug10 = 0x0200,
    eTraceModeDebug11 = 0x0400,
    eTraceModeDebug12 = 0x0800,
    eTraceModeDebug13 = 0x1000,
    eTraceModeDebug14 = 0x2000,
    eTraceModeDebug15 = 0x4000,
    eTraceModeDebug16 = 0x8000,
    eTraceModeMask    = 0xFFFF
  };
  int __BeginData__;
  int HideMenu;
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
  void SetTraceMode(unsigned int modeP) { traceModeM = (modeP & eTraceModeMask); }
  unsigned int GetTraceMode(void) const { return traceModeM; }
  bool IsTraceMode(eTraceMode modeP) const { return (traceModeM & modeP); }
};

extern cElvisConfig ElvisConfig;

#endif // __ELVIS_CONFIG_H
