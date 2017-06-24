/*
 * elvis.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <getopt.h>
#include <vdr/plugin.h>
#include <vdr/menu.h>

#include "common.h"
#include "config.h"
#include "fetch.h"
#include "menu.h"
#include "resume.h"
#include "setup.h"
#include "elvisservice.h"

#if defined(APIVERSNUM) && APIVERSNUM < 20307
#error "VDR-2.3.7 API version or greater is required!"
#endif

#ifndef GITVERSION
#define GITVERSION ""
#endif

       const char VERSION[]       = "2.3.0" GITVERSION;
static const char DESCRIPTION[]   = trNOOP("Elisa Viihde Widget");
static const char MAINMENUENTRY[] = trNOOP("Elvis");

class cPluginElvis : public cPlugin {
private:
  // Add any member variables or functions you may need here.

public:
  cPluginElvis();
  virtual ~cPluginElvis();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description() { return tr(DESCRIPTION); }
  virtual const char *CommandLineHelp();
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Initialize();
  virtual bool Start();
  virtual void Stop();
  virtual void Housekeeping();
  virtual void MainThreadHook();
  virtual cString Active();
  virtual time_t WakeupTime();
  virtual const char *MainMenuEntry() { return ((ElvisConfig.GetHideMenu() || isempty(ElvisConfig.GetUsername()) || isempty(ElvisConfig.GetPassword())) ? NULL : tr(MAINMENUENTRY)); }
  virtual cOsdObject *MainMenuAction();
  virtual cMenuSetupPage *SetupMenu();
  virtual bool SetupParse(const char *nameP, const char *valueP);
  virtual bool Service(const char *idP, void *dataP = NULL);
  virtual const char **SVDRPHelpPages();
  virtual cString SVDRPCommand(const char *commandP, const char *optionP, int &replyCodeP);
  };

cPluginElvis::cPluginElvis()
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cPluginElvis::~cPluginElvis()
{
  // Clean up after yourself!
}

const char *cPluginElvis::CommandLineHelp()
{
  // Return a string that describes all known command line options.
  return "  -t <mode>, --trace=<mode>  set the tracing mode\n";
}

bool cPluginElvis::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  static const struct option long_options[] = {
    { "trace",    required_argument, NULL, 't' },
    { NULL,       no_argument,       NULL,  0  }
    };

  cString server;
  int c;
  while ((c = getopt_long(argc, argv, "t:", long_options, NULL)) != -1) {
    switch (c) {
      case 't':
           ElvisConfig.SetTraceMode(strtol(optarg, NULL, 0));
           break;
      default:
           return false;
      }
    }
  return true;
}

bool cPluginElvis::Initialize()
{
  // Initialize any background activities the plugin shall perform.
  ElvisConfig.Load(ConfigDirectory(PLUGIN_NAME_I18N));
  cElvisResumeItems::GetInstance()->Load(ConfigDirectory(PLUGIN_NAME_I18N));
  cElvisWidget::GetInstance()->Load(ConfigDirectory(PLUGIN_NAME_I18N));
  return true;
}

bool cPluginElvis::Start()
{
  // Start any background activities the plugin shall perform.
  curl_global_init(CURL_GLOBAL_ALL);
  return true;
}

void cPluginElvis::Stop()
{
  // Stop any background activities the plugin is performing.
  cElvisRecordings::Destroy();
  cElvisTimers::Destroy();
  cElvisSearchTimers::Destroy();
  cElvisTopEvents::Destroy();
  cElvisVODCategories::Destroy();
  cElvisChannels::Destroy();
  cElvisWidget::Destroy();
  cElvisFetcher::Destroy();
  cElvisResumeItems::Destroy();
  curl_global_cleanup();
}

void cPluginElvis::Housekeeping()
{
  // Perform any cleanup or other regular tasks.
}

void cPluginElvis::MainThreadHook()
{
  // Perform actions in the context of the main program thread.
  // WARNING: Use with great care - see PLUGINS.html!
}

cString cPluginElvis::Active()
{
  // Return a message string if shutdown should be postponed
  return cElvisFetcher::GetInstance()->Fetching() ? tr("Elvis is alive!") : NULL;
}

time_t cPluginElvis::WakeupTime()
{
  // Return custom wakeup time for shutdown script
  return 0;
}

cOsdObject *cPluginElvis::MainMenuAction()
{
  // Perform the action when selected from the main VDR menu.
  return new cElvisMenu();
}

cMenuSetupPage *cPluginElvis::SetupMenu()
{
  // Return a setup menu in case the plugin supports one.
  return new cPluginElvisSetup();
}

bool cPluginElvis::SetupParse(const char *nameP, const char *valueP)
{
  return false;
}

bool cPluginElvis::Service(const char *idP, void *dataP)
{
  if (strcmp(idP, "ElvisService-Timer-v1.0") == 0) {
     if (dataP) {
        ElvisService_Timer_v1_0 *data = reinterpret_cast<ElvisService_Timer_v1_0*>(dataP);
        if (data->addMode)
           cElvisChannels::GetInstance()->AddTimer(data->eventId);
        else
           cElvisChannels::GetInstance()->DelTimer(data->eventId);
        }
     return true;
     }
#if defined(MAINMENUHOOKSVERSNUM)
  else if (ElvisConfig.GetReplaceSchedule() && (strcmp(idP, "MainMenuHooksPatch-v1.0::osSchedule") == 0)) {
     if (dataP) {
        cOsdMenu **menu = (cOsdMenu**)dataP;
        if (menu)
           *menu = new cElvisEPGMenu();
        }
     return true;
     }
  else if (ElvisConfig.GetReplaceTimers() && (strcmp(idP, "MainMenuHooksPatch-v1.0::osTimers") == 0)) {
     if (dataP) {
        cOsdMenu **menu = (cOsdMenu**)dataP;
        if (menu)
           *menu = new cElvisTimersMenu();
        }
     return true;
     }
  else if (ElvisConfig.GetReplaceRecordings() && (strcmp(idP, "MainMenuHooksPatch-v1.0::osRecordings") == 0)) {
     if (dataP) {
        cOsdMenu **menu = (cOsdMenu**)dataP;
        if (menu)
           *menu = new cElvisRecordingsMenu();
        }
     return true;
     }
#endif

  return false;
}

const char **cPluginElvis::SVDRPHelpPages()
{
  static const char *HelpPages[] = {
    "ABRT [id]\n"
    "    Abort fetch queue transfers.",
    "LIST\n"
    "    List fetch queue.",
    "ADDT [eventid]\n"
    "    Add a new timer.",
    "DELT [eventid]\n"
    "    Delete an existing timer.",
    "TRAC [ <mode> ]\n"
    "    Gets and/or sets used tracing mode.\n",
    NULL
    };
  return HelpPages;
}

cString cPluginElvis::SVDRPCommand(const char *commandP, const char *optionP, int &replyCodeP)
{
  if (strcasecmp(commandP, "ABRT") == 0) {
     int index = -1;
     if (*optionP && isnumber(optionP))
        index = (int)strtol(optionP, NULL, 10);
     cElvisFetcher::GetInstance()->Abort(index);
     return cString("Fetching aborted");
     }
  else if (strcasecmp(commandP, "LIST") == 0) {
     cString list = cElvisFetcher::GetInstance()->List();
     if (isempty(*list)) {
        replyCodeP = 901;
        list = "Empty fetch queue";
        }
     return list;
     }
  else if (strcasecmp(commandP, "ADDT") == 0) {
     tEventID eventid = 0;
     if (*optionP && isnumber(optionP))
        eventid = (int)strtol(optionP, NULL, 10);
     if (!cElvisChannels::GetInstance()->AddTimer(eventid)) {
        replyCodeP = 901;
        return cString("Timer action failed");
        }
     return cString("Timer added");
     }
  else if (strcasecmp(commandP, "DELT") == 0) {
     tEventID eventid = 0;
     if (*optionP && isnumber(optionP))
        eventid = (int)strtol(optionP, NULL, 10);
     if (!cElvisChannels::GetInstance()->DelTimer(eventid)) {
        replyCodeP = 901;
        return cString("Timer action failed");
        }
     return cString("Timer deleted");
     }
  else if (strcasecmp(commandP, "TRAC") == 0) {
     if (optionP && *optionP)
        ElvisConfig.SetTraceMode(strtol(optionP, NULL, 0));
     return cString::sprintf("Tracing mode: 0x%04X\n", ElvisConfig.GetTraceMode());
     }

  return NULL;
}

VDRPLUGINCREATOR(cPluginElvis); // Don't touch this!
