/*
 * log.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_LOG_H
#define __ELVIS_LOG_H

#include "config.h"

#define error(x...)   esyslog("ELVIS-ERROR: " x)
#define info(x...)    isyslog("ELVIS: " x)
// 0x0001: Generic call stack
#define debug1(x...)  void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug1)  ? dsyslog("ELVIS1: " x)  : void() )
// 0x0002: API access
#define debug2(x...)  void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug2)  ? dsyslog("ELVIS2: " x)  : void() )
// 0x0004: Data parsing
#define debug3(x...)  void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug3)  ? dsyslog("ELVIS3: " x)  : void() )
// 0x0008: Fetcher
#define debug4(x...)  void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug4)  ? dsyslog("ELVIS4: " x)  : void() )
// 0x0010: Player
#define debug5(x...)  void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug5)  ? dsyslog("ELVIS5: " x)  : void() )
// 0x0020: Resume
#define debug6(x...)  void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug6)  ? dsyslog("ELVIS6: " x)  : void() )
// 0x0040: Timers
#define debug7(x...)  void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug7)  ? dsyslog("ELVIS7: " x)  : void() )
// 0x0080: CURL
#define debug8(x...)  void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug8)  ? dsyslog("ELVIS8: " x)  : void() )
// 0x0100: TBD
#define debug9(x...)  void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug9)  ? dsyslog("ELVIS9: " x)  : void() )
// 0x0200: TBD
#define debug10(x...) void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug10) ? dsyslog("ELVIS10: " x) : void() )
// 0x0400: TBD
#define debug11(x...) void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug11) ? dsyslog("ELVIS11: " x) : void() )
// 0x0800: TBD
#define debug12(x...) void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug12) ? dsyslog("ELVIS12: " x) : void() )
// 0x1000: TBD
#define debug13(x...) void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug13) ? dsyslog("ELVIS13: " x) : void() )
// 0x2000: TBD
#define debug14(x...) void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug14) ? dsyslog("ELVIS14: " x) : void() )
// 0x4000: TBD
#define debug15(x...) void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug15) ? dsyslog("ELVIS15: " x) : void() )
// 0x8000; Extra call stack
#define debug16(x...) void( ElvisConfig.IsTraceMode(cElvisConfig::eTraceModeDebug16) ? dsyslog("ELVIS16: " x) : void() )

#endif // __ELVIS_LOG_H
