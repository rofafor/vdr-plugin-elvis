/*
 * config.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "config.h"

cElvisConfig ElvisConfig;

const char *cElvisConfig::confBaseNameS = "elvis.conf";

cElvisConfig::cElvisConfig()
: HideMenu(0),
  Service(0)
{
  Utf8Strn0Cpy(Username, "foo", sizeof(Username));
  Utf8Strn0Cpy(Password, "bar", sizeof(Password));
}

cElvisConfig& cElvisConfig::operator=(const cElvisConfig &s)
{
  memcpy(&__BeginData__, &s.__BeginData__, (char *)&s.__EndData__ - (char *)&s.__BeginData__);
  return *this;
}

cSetupLine *cElvisConfig::Get(const char *Name)
{
  for (cSetupLine *l = First(); l; l = Next(l)) {
      if ((l->Plugin() == NULL) && (strcasecmp(l->Name(), Name) == 0))
         return l;
      }
  return NULL;
}

void cElvisConfig::Store(const char *Name, const char *Value)
{
  if (Name && *Name) {
     cSetupLine *l = Get(Name);
     if (l)
        Del(l);
     if (Value)
        Add(new cSetupLine(Name, Value));
     }
}

void cElvisConfig::Store(const char *Name, int Value)
{
  Store(Name, cString::sprintf("%d", Value));
}

bool cElvisConfig::Load(const char *Directory)
{
  if (cConfig<cSetupLine>::Load(*cString::sprintf("%s/%s", Directory, confBaseNameS), true)) {
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

bool cElvisConfig::Parse(const char *Name, const char *Value)
{
  if      (!strcasecmp(Name, "Username")) Utf8Strn0Cpy(Username, Value, sizeof(Username));
  else if (!strcasecmp(Name, "Password")) Utf8Strn0Cpy(Password, Value, sizeof(Password));
  else if (!strcasecmp(Name, "HideMenu")) HideMenu = atoi(Value);
  else if (!strcasecmp(Name, "Service"))  Service  = atoi(Value);
  else
     return false;
  return true;
}

bool cElvisConfig::Save(void)
{
  Store("HideMenu",  HideMenu);
  Store("Service",   Service);
  Store("Username",  Username);
  Store("Password",  Password);

  Sort();

  if (cConfig<cSetupLine>::Save()) {
     debug("saved setup to %s", FileName());
     return true;
     }
  return false;
}
