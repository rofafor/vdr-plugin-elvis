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
#include "widget.h"
#include "elvisservice.h"

#if defined(APIVERSNUM) && APIVERSNUM < 10716
#error "VDR-1.7.16 API version or greater is required!"
#endif

       const char VERSION[]       = "0.0.5";
static const char DESCRIPTION[]   = trNOOP("Elisa Viihde Widget");
static const char MAINMENUENTRY[] = trNOOP("Elvis");

class cPluginElvis : public cPlugin {
private:
  // Add any member variables or functions you may need here.
public:
  cPluginElvis(void);
  virtual ~cPluginElvis();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return tr(DESCRIPTION); }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Initialize(void);
  virtual bool Start(void);
  virtual void Stop(void);
  virtual void Housekeeping(void);
  virtual void MainThreadHook(void);
  virtual cString Active(void);
  virtual time_t WakeupTime(void);
  virtual const char *MainMenuEntry(void) { return ((ElvisConfig.HideMenu || isempty(ElvisConfig.Username) || isempty(ElvisConfig.Password)) ? NULL : tr(MAINMENUENTRY)); }
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
  virtual bool Service(const char *Id, void *Data = NULL);
  virtual const char **SVDRPHelpPages(void);
  virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);
  };

class cPluginElvisSetup : public cMenuSetupPage
{
private:
  cElvisConfig data;
  cVector<const char*> help;
  void Setup(void);
protected:
  virtual eOSState ProcessKey(eKeys Key);
  virtual void Store(void);
public:
  cPluginElvisSetup(void);
};

cPluginElvis::cPluginElvis(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cPluginElvis::~cPluginElvis()
{
  // Clean up after yourself!
}

const char *cPluginElvis::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return NULL;
}

bool cPluginElvis::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  return true;
}

bool cPluginElvis::Initialize(void)
{
  // Initialize any background activities the plugin shall perform.
  ElvisConfig.Load(ConfigDirectory(PLUGIN_NAME_I18N));
  return true;
}

bool cPluginElvis::Start(void)
{
  // Start any background activities the plugin shall perform.
  curl_global_init(CURL_GLOBAL_ALL);
  return true;
}

void cPluginElvis::Stop(void)
{
  // Stop any background activities the plugin is performing.
  cElvisRecordings::Destroy();
  cElvisTimers::Destroy();
  cElvisSearchTimers::Destroy();
  cElvisTopEvents::Destroy();
  cElvisChannels::Destroy();
  cElvisWidget::Destroy();
  cElvisFetcher::Destroy();
  curl_global_cleanup();
}

void cPluginElvis::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

void cPluginElvis::MainThreadHook(void)
{
  // Perform actions in the context of the main program thread.
  // WARNING: Use with great care - see PLUGINS.html!
}

cString cPluginElvis::Active(void)
{
  // Return a message string if shutdown should be postponed
  return cElvisFetcher::GetInstance()->Fetching() ? tr("Elvis is alive!") : NULL;
}

time_t cPluginElvis::WakeupTime(void)
{
  // Return custom wakeup time for shutdown script
  return 0;
}

cOsdObject *cPluginElvis::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  return new cElvisMenu();
}

cMenuSetupPage *cPluginElvis::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return new cPluginElvisSetup();
}

bool cPluginElvis::SetupParse(const char *Name, const char *Value)
{
  return false;
}

bool cPluginElvis::Service(const char *Id, void *Data)
{
  if (strcmp(Id, "ElvisService-Timer-v1.0") == 0) {
     if (Data) {
        ElvisService_Timer_v1_0 *data = (ElvisService_Timer_v1_0*)Data;
        if (data->addMode)
           cElvisChannels::GetInstance()->AddTimer(data->eventId);
        else
           cElvisChannels::GetInstance()->DelTimer(data->eventId);
        }
     return true;
     }
  else if (strcmp(Id, "ElvisService-Update-v1.0") == 0) {
     if (Data) {
        ElvisService_Update_v1_0 *data = (ElvisService_Update_v1_0*)Data;
        if (data->timers)
           cElvisTimers::GetInstance()->Update(data->forceUpdate ? true : false);
        if (data->searchTimers)
           cElvisSearchTimers::GetInstance()->Update(data->forceUpdate ? true : false);
        if (data->favourites)
           cElvisTopEvents::GetInstance()->Update(data->forceUpdate ? true : false);
        if (data->recordings)
           cElvisRecordings::GetInstance()->Update(/*data->forceUpdate ? true : false*/);
        if (data->epg)
           cElvisChannels::GetInstance()->Update(data->forceUpdate ? true : false);
        }
     return true;
     }

  return false;
}

const char **cPluginElvis::SVDRPHelpPages(void)
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
    "UPDT [force]\n"
    "    Update timers.",
    NULL
    };
  return HelpPages;
}

cString cPluginElvis::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
{
  if (strcasecmp(Command, "ABRT") == 0) {
     int index = -1;
     if (*Option && isnumber(Option))
        index = (int)strtol(Option, NULL, 10);
     cElvisFetcher::GetInstance()->Abort(index);
     return cString("Fetching aborted");
     }
  else if (strcasecmp(Command, "LIST") == 0) {
     cString list = cElvisFetcher::GetInstance()->List();
     if (isempty(*list)) {
        ReplyCode = 901;
        list = "Empty fetch queue";
        }
     return list;
     }
  else if (strcasecmp(Command, "ADDT") == 0) {
     tEventID eventid = 0;
     if (*Option && isnumber(Option))
        eventid = (int)strtol(Option, NULL, 10);
     if (!cElvisChannels::GetInstance()->AddTimer(eventid)) {
        ReplyCode = 901;
        return cString("Timer action failed");
        }
     return cString("Timer added");
     }
  else if (strcasecmp(Command, "DELT") == 0) {
     tEventID eventid = 0;
     if (*Option && isnumber(Option))
        eventid = (int)strtol(Option, NULL, 10);
     if (!cElvisChannels::GetInstance()->DelTimer(eventid)) {
        ReplyCode = 901;
        return cString("Timer action failed");
        }
     return cString("Timer deleted");
     }
  else if (strcasecmp(Command, "UPDT") == 0) {
     cElvisChannels::GetInstance()->Update(*Option ? true : false);
     cElvisTimers::GetInstance()->Update(*Option ? true : false);
     return cString("Timers updated");
     }

  return NULL;
}

cPluginElvisSetup::cPluginElvisSetup(void)
{
  data = ElvisConfig;
  Setup();
}

void cPluginElvisSetup::Setup(void)
{
  int current = Current();

  Clear();
  help.Clear();

  Add(new cMenuEditBoolItem(tr("Hide main menu entry"), &data.HideMenu));
  help.Append(tr("Define whether the main manu entry is hidden."));

  Add(new cMenuEditBoolItem(tr("Use service"), &data.Service, tr("Elisa Viihde"), tr("Saunavisio")));
  help.Append(tr("Define whether your service is Elisa Viihde or Saunavisio."));

  Add(new cMenuEditBoolItem(tr("Use SSL connection"), &data.Ssl));
  help.Append(tr("Define whether SSL connection is used."));

  Add(new cMenuEditStrItem(tr("Username"), data.Username, sizeof(data.Username)));
  help.Append(tr("Define your username for the service."));

  Add(new cMenuEditHiddenStrItem(tr("Password"), data.Password, sizeof(data.Password)));
  help.Append(tr("Define your password for the service."));

  SetCurrent(Get(current));
  Display();
}

eOSState cPluginElvisSetup::ProcessKey(eKeys Key)
{
  eOSState state = cMenuSetupPage::ProcessKey(Key);

  if ((state == osUnknown) && (Key == kInfo) && (Current() < help.Size()))
     return AddSubMenu(new cMenuText(cString::sprintf("%s - %s '%s'", tr("Help"), trVDR("Plugin"), PLUGIN_NAME_I18N), help[Current()]));

  return state;
}

void cPluginElvisSetup::Store(void)
{
  ElvisConfig = data;
  ElvisConfig.Save();
}

VDRPLUGINCREATOR(cPluginElvis); // Don't touch this!
