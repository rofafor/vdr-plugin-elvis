/*
 * config.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "config.h"

cElvisConfig ElvisConfig;

const char *cElvisConfig::confBaseNameS = PLUGIN_NAME_I18N ".conf";

cElvisConfig::cElvisConfig()
: HideMenu(0),
  ReplaceSchedule(0),
  ReplaceTimers(0),
  ReplaceRecordings(0)
{
  memset(Username, 0, sizeof(Username));
  memset(Password, 0, sizeof(Password));
}

cElvisConfig& cElvisConfig::operator=(const cElvisConfig &objP)
{
  memcpy(&__BeginData__, &objP.__BeginData__, (char *)&objP.__EndData__ - (char *)&objP.__BeginData__);
  return *this;
}

cSetupLine *cElvisConfig::Get(const char *nameP)
{
  for (cSetupLine *l = First(); l; l = Next(l)) {
      if ((l->Plugin() == NULL) && (strcasecmp(l->Name(), nameP) == 0))
         return l;
      }
  return NULL;
}

void cElvisConfig::Store(const char *nameP, const char *valueP)
{
  if (nameP && *nameP) {
     cSetupLine *l = Get(nameP);
     if (l)
        Del(l);
     if (valueP)
        Add(new cSetupLine(nameP, valueP));
     }
}

void cElvisConfig::Store(const char *nameP, int valueP)
{
  Store(nameP, cString::sprintf("%d", valueP));
}

bool cElvisConfig::Load(const char *directoryP)
{
  if (cConfig<cSetupLine>::Load(*cString::sprintf("%s/%s", directoryP, confBaseNameS), true)) {
     bool result = true;
     for (cSetupLine *l = First(); l; l = Next(l)) {
         bool error = false;
         if (!l->Plugin()) {
            if (!Parse(l->Name(), l->Value()))
               error = true;
            }
         if (error) {
            error("unknown config parameter: %s = %s", l->Name(), l->Value());
            result = false;
            }
         }
     return result;
     }
  return false;
}

bool cElvisConfig::Parse(const char *nameP, const char *valueP)
{
  if      (!strcasecmp(nameP, "Username")) Utf8Strn0Cpy(Username, valueP, sizeof(Username));
  else if (!strcasecmp(nameP, "Password")) Utf8Strn0Cpy(Password, valueP, sizeof(Password));
  else if (!strcasecmp(nameP, "HideMenu")) HideMenu = atoi(valueP);
  else
     return false;
  return true;
}

bool cElvisConfig::Save()
{
  Store("HideMenu",  HideMenu);
  Store("Username",  Username);
  Store("Password",  Password);

  Sort();

  if (cConfig<cSetupLine>::Save()) {
     debug("saved setup to %s", FileName());
     return true;
     }
  return false;
}
