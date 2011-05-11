/*
 * elvis.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/plugin.h>
#include <vdr/menu.h>

#include "common.h"
#include "config.h"
#include "fetch.h"
#include "menu.h"
#include "resume.h"
#include "widget.h"
#include "elvisservice.h"

#if defined(APIVERSNUM) && APIVERSNUM < 10716
#error "VDR-1.7.16 API version or greater is required!"
#endif

       const char VERSION[]       = "0.2.0";
static const char DESCRIPTION[]   = trNOOP("Elisa Viihde Widget");
static const char MAINMENUENTRY[] = trNOOP("Elvis");

class cPluginElvis : public cPlugin {
private:
  // Add any member variables or functions you may need here.
public:
  cPluginElvis();
  virtual ~cPluginElvis();
  virtual const char *Version() { return VERSION; }
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
  virtual const char *MainMenuEntry() { return ((ElvisConfig.HideMenu || isempty(ElvisConfig.Username) || isempty(ElvisConfig.Password)) ? NULL : tr(MAINMENUENTRY)); }
  virtual cOsdObject *MainMenuAction();
  virtual cMenuSetupPage *SetupMenu();
  virtual bool SetupParse(const char *nameP, const char *valueP);
  virtual bool Service(const char *idP, void *dataP = NULL);
  virtual const char **SVDRPHelpPages();
  virtual cString SVDRPCommand(const char *commandP, const char *optionP, int &replyCodeP);
  };

class cPluginElvisSetup : public cMenuSetupPage
{
private:
  cElvisConfig dataM;
  cVector<const char*> helpM;
  void SetHelpKeys();
  void Setup();
protected:
  virtual eOSState ProcessKey(eKeys keyP);
  virtual void Store();
public:
  cPluginElvisSetup();
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
  return NULL;
}

bool cPluginElvis::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
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
        ElvisService_Timer_v1_0 *data = (ElvisService_Timer_v1_0*)dataP;
        if (data->addMode)
           cElvisChannels::GetInstance()->AddTimer(data->eventId);
        else
           cElvisChannels::GetInstance()->DelTimer(data->eventId);
        }
     return true;
     }
#if defined(MAINMENUHOOKSVERSNUM)
  else if (ElvisConfig.ReplaceSchedule && (strcmp(idP, "MainMenuHooksPatch-v1.0::osSchedule") == 0)) {
     if (dataP) {
        cOsdMenu **menu = (cOsdMenu**)dataP;
        if (menu)
           *menu = new cElvisEPGMenu();
        }
     return true;
     }
  else if (ElvisConfig.ReplaceTimers && (strcmp(idP, "MainMenuHooksPatch-v1.0::osTimers") == 0)) {
     if (dataP) {
        cOsdMenu **menu = (cOsdMenu**)dataP;
        if (menu)
           *menu = new cElvisTimersMenu();
        }
     return true;
     }
  else if (ElvisConfig.ReplaceRecordings && (strcmp(idP, "MainMenuHooksPatch-v1.0::osRecordings") == 0)) {
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

  return NULL;
}

cPluginElvisSetup::cPluginElvisSetup()
{
  dataM = ElvisConfig;
  Setup();
  SetHelpKeys();
}

void cPluginElvisSetup::SetHelpKeys()
{
  SetHelp(trVDR("Button$Reset"), NULL, NULL, NULL);
}

void cPluginElvisSetup::Setup()
{
  int current = Current();

  Clear();
  helpM.Clear();

  Add(new cMenuEditBoolItem(tr("Hide main menu entry"), &dataM.HideMenu));
  helpM.Append(tr("Define whether the main manu entry is hidden."));

  Add(new cMenuEditStrItem(tr("Username"), dataM.Username, sizeof(dataM.Username)));
  helpM.Append(tr("Define your Elisa Viihde username."));

  Add(new cMenuEditHiddenStrItem(tr("Password"), dataM.Password, sizeof(dataM.Password)));
  helpM.Append(tr("Define your Elisa Viihde password."));

#if defined(MAINMENUHOOKSVERSNUM)
  Add(new cMenuEditBoolItem(tr("Replace 'Schedule' in main menu"), &dataM.ReplaceSchedule));
  helpM.Append(tr("Define whether this plugin replaces the original 'Schedule' entry in the main menu. MainMenuHook patch is required."));

  Add(new cMenuEditBoolItem(tr("Replace 'Timers' in main menu"), &dataM.ReplaceTimers));
  helpM.Append(tr("Define whether this plugin replaces the original 'Timers' entry in the main menu. MainMenuHook patch is required."));

  Add(new cMenuEditBoolItem(tr("Replace 'Recordings' menu entry"), &dataM.ReplaceRecordings));
  helpM.Append(tr("Define whether this plugin replaces the original 'Recordings' entry in the main menu. MainMenuHook patch is required."));
#endif

  SetCurrent(Get(current));
  Display();
}

eOSState cPluginElvisSetup::ProcessKey(eKeys keyP)
{
  eOSState state = cMenuSetupPage::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kRed:
            Skins.Message(mtInfo, tr("Reseting..."));
            cElvisResumeItems::GetInstance()->Reset();
            cElvisWidget::GetInstance()->Invalidate();
            Skins.Message(mtInfo, NULL);
            state = osContinue;
            break;
       case kInfo:
            if (Current() < helpM.Size())
               return AddSubMenu(new cMenuText(cString::sprintf("%s - %s '%s'", tr("Help"), trVDR("Plugin"), PLUGIN_NAME_I18N), helpM[Current()]));
            break;
       default:
            break;
            }
     }

  return state;
}

void cPluginElvisSetup::Store()
{
  ElvisConfig = dataM;
  ElvisConfig.Save();
  cElvisWidget::GetInstance()->Invalidate();
}

VDRPLUGINCREATOR(cPluginElvis); // Don't touch this!
