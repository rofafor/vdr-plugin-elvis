/*
 * common.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_COMMON_H
#define __ELVIS_COMMON_H

#include <vdr/tools.h>

#ifdef DEBUG
#define debug(x...) dsyslog("ELVIS: " x)
#define error(x...) esyslog("ELVIS: " x)
#else
#define debug(x...) ;
#define error(x...) esyslog("ELVIS: " x)
#endif

#define DELETE_POINTER(ptr)       \
  do {                            \
     if (ptr) {                   \
        typeof(*ptr) *tmp = ptr;  \
        ptr = NULL;               \
        delete(tmp);              \
        }                         \
  } while (0)

extern const char VERSION[];
extern cString    strunescape(const char *s);
extern cString    strescape(const char *s);

#endif // __ELVIS_COMMON_H

