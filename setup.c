/*
 * setup.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "config.h"
#include "resume.h"
#include "widget.h"
#include "setup.h"

cPluginElvisSetup::cPluginElvisSetup()
: hideMenuM(ElvisConfig.GetHideMenu()),
  replaceScheduleM(ElvisConfig.GetReplaceSchedule()),
  replaceTimersM(ElvisConfig.GetReplaceTimers()),
  replaceRecordingsM(ElvisConfig.GetReplaceRecordings())
{
  strn0cpy(usernameM, ElvisConfig.GetUsername(), sizeof(usernameM));
  strn0cpy(passwordM, ElvisConfig.GetPassword(), sizeof(passwordM));

 SetMenuCategory(mcSetupPlugins);
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

  Add(new cMenuEditBoolItem(tr("Hide main menu entry"), &hideMenuM));
  helpM.Append(tr("Define whether the main manu entry is hidden."));

  Add(new cMenuEditStrItem(tr("Username"), usernameM, sizeof(usernameM)));
  helpM.Append(tr("Define your Elisa Viihde username."));

  Add(new cMenuEditHiddenStrItem(tr("Password"), passwordM, sizeof(passwordM)));
  helpM.Append(tr("Define your Elisa Viihde password."));

#if defined(MAINMENUHOOKSVERSNUM)
  Add(new cMenuEditBoolItem(tr("Replace 'Schedule' in main menu"), &replaceScheduleM));
  helpM.Append(tr("Define whether this plugin replaces the original 'Schedule' entry in the main menu. MainMenuHook patch is required."));

  Add(new cMenuEditBoolItem(tr("Replace 'Timers' in main menu"), &replaceTimersM));
  helpM.Append(tr("Define whether this plugin replaces the original 'Timers' entry in the main menu. MainMenuHook patch is required."));

  Add(new cMenuEditBoolItem(tr("Replace 'Recordings' menu entry"), &replaceRecordingsM));
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
  ElvisConfig.SetHideMenu(hideMenuM);
  ElvisConfig.SetUsername(usernameM);
  ElvisConfig.SetPassword(passwordM);
  ElvisConfig.SetReplaceSchedule(replaceScheduleM);
  ElvisConfig.SetReplaceTimers(replaceTimersM);
  ElvisConfig.SetReplaceRecordings(replaceRecordingsM);
  ElvisConfig.Save();
  cElvisWidget::GetInstance()->Invalidate();
}
