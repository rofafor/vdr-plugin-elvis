/*
 * elvisservice.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVISSERVICE_H
#define __ELVISSERVICE_H

#include <vdr/epg.h>

struct ElvisService_Timer_v1_0 {
  bool     addMode;
  tEventID eventId;
};

struct ElvisService_Update_v1_0 {
  bool     forceUpdate;
  bool     timers;
  bool     searchTimers;
  bool     favourites;
  bool     recordings;
  bool     epg;
};

#endif //__ELVISSERVICE_H

