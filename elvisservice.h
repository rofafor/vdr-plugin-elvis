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

#endif //__ELVISSERVICE_H

