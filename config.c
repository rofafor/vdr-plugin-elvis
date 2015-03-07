/*
 * config.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "log.h"
#include "config.h"

cElvisConfig ElvisConfig;

const char *cElvisConfig::confBaseNameS = PLUGIN_NAME_I18N ".conf";

cElvisConfig::cElvisConfig()
: traceModeM(eTraceModeNormal),
  hideMenuM(0),
  replaceScheduleM(0),
  replaceTimersM(0),
  replaceRecordingsM(0)
{
  memset(usernameM, 0, sizeof(usernameM));
  memset(passwordM, 0, sizeof(passwordM));
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
            error("Unknown config parameter: %s = %s", l->Name(), l->Value());
            result = false;
            }
         }
     return result;
     }
  return false;
}

bool cElvisConfig::Parse(const char *nameP, const char *valueP)
{
  if      (!strcasecmp(nameP, "Username")) Utf8Strn0Cpy(usernameM, valueP, sizeof(usernameM));
  else if (!strcasecmp(nameP, "Password")) Utf8Strn0Cpy(passwordM, valueP, sizeof(passwordM));
  else if (!strcasecmp(nameP, "HideMenu")) hideMenuM = atoi(valueP);
  else
     return false;
  return true;
}

bool cElvisConfig::Save()
{
  Store("HideMenu",  hideMenuM);
  Store("Username",  usernameM);
  Store("Password",  passwordM);

  Sort();

  if (cConfig<cSetupLine>::Save()) {
     debug3("%s Saved setup to %s", __PRETTY_FUNCTION__, FileName());
     return true;
     }
  return false;
}
