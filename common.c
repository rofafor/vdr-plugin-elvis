/*
 * common.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <ctype.h>

#include "common.h"

static int hex2dec(char hex)
{
  if ((hex >= '0') && (hex <= '9'))
     return ( 0 + hex - '0');
  if ((hex >= 'A') && (hex <= 'F'))
     return (10 + hex - 'A');
  if ((hex >= 'a') && (hex <= 'f'))
     return (10 + hex - 'a');

  return -1;
}

cString strunescape(const char *s)
{
  if (s) {
     char *buffer = strdup(s);
     const char *p = s;
     const char *end = p + strlen(s);
     char *t = buffer;
     while (*p) {
           if (*p == '%') {
              if ((end - p) >= 2) {
                 int a = hex2dec(*++p) & 0xF;
                 int b = hex2dec(*++p) & 0xF;
                 if ((a >= 0) && (b >= 0))
                    *t++ = (char)((a << 4) + b);
                 }
              }
           else
              *t++ = *p;
           ++p;
           }
     *t = 0;
     return cString(buffer, true);
     }

  return cString("");
}

cString strescape(const char *s)
{
  cString res("");

  if (s) {
     while (*s) {
           if (isalnum(*s) || (*s == '-') || (*s == '_') || (*s == '.' )|| (*s == '~'))
              res = cString::sprintf("%s%c", *res, *s);
           else
              res = cString::sprintf("%s%%%02X", *res, (*s & 0xFF));
           ++s;
           }
     }

  return res;
}
