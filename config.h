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
  bool Parse(const char *Name, const char *Value);
  cSetupLine *Get(const char *Name);
  void Store(const char *Name, const char *Value);
  void Store(const char *Name, int Value);
public:
  int __BeginData__;
  int HideMenu;
  int Service;
  int Ssl;
  char Username[eMaxCredentials];
  char Password[eMaxCredentials];
  int __EndData__;
  cElvisConfig(void);
  cElvisConfig& operator= (const cElvisConfig &s);
  bool Load(const char *Directory);
  bool Save(void);
};

extern cElvisConfig ElvisConfig;

#endif // __ELVIS_CONFIG_H
