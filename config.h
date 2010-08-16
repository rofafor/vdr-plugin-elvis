/*
 * config.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_CONFIG_H
#define __ELVIS_CONFIG_H

struct cElvisConfig
{
public:
  cElvisConfig();
  int hidemenu;
  int service;
  char username[32];
  char password[32];
};

extern cElvisConfig ElvisConfig;

#endif // __ELVIS_CONFIG_H
