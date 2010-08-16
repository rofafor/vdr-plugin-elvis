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

#if defined(APIVERSNUM) && APIVERSNUM < 10715
#error "VDR-1.7.15 API version or greater is required!"
#endif

       const char VERSION[]       = "0.0.2";
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
  virtual const char *MainMenuEntry(void) { return (ElvisConfig.HideMenu ? NULL : tr(MAINMENUENTRY)); }
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
  // Handle custom service requests from other plugins
  return false;
}

const char **cPluginElvis::SVDRPHelpPages(void)
{
  // Return help text for SVDRP commands this plugin implements
  return NULL;
}

cString cPluginElvis::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
{
  // Process SVDRP commands this plugin implements
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

  Add(new cMenuEditStrItem(tr("Username"), data.Username, sizeof(data.Username)));
  help.Append(tr("Define your username for the service."));

  Add(new cMenuEditStrItem(tr("Password"), data.Password, sizeof(data.Password)));
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
