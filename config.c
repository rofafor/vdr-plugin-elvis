/*
 * config.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "config.h"
#include "common.h"

cElvisConfig ElvisConfig;

cElvisConfig::cElvisConfig()
: hidemenu(0),
  service(0)
{
  Utf8Strn0Cpy(username, "foo", sizeof(username));
  Utf8Strn0Cpy(password, "bar", sizeof(password));
}
