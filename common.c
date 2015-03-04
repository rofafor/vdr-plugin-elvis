/*
 * common.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <ctype.h>
#include <wctype.h>

#include <vdr/remote.h>

#include "log.h"
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

cString strstrip(const char *s, const char *r)
{
  cString res("");

  if (s) {
     char *sd = strdup(s);
     while (char *p = strstr(sd, r)) {
       int of = (int)(p - sd);
       int l  = (int)strlen(sd);
       int lr = (int)strlen(r);
       char *sof = sd + of;
       memmove(sof, sof + lr, l - of - lr + 1);
       }
     res = sd;
     free(sd);
     }

  return res;
}

cString WeekDateString(time_t t)
{
  char buf[32];
  struct tm tm_r;
  tm *tm = localtime_r(&t, &tm_r);
  char *p = stpcpy(buf, WeekDayName(tm->tm_wday));

  *p++ = ' ';
  strftime(p, sizeof(buf) - (p - buf), "%d", tm);

  return buf;
}

cString ShortDateString(time_t t)
{
  char buf[32];
  struct tm tm_r;
  tm *tm = localtime_r(&t, &tm_r);
  char *p = buf;

  strftime(p, sizeof(buf) - (p - buf), "%d.%m.%y", tm);

  return buf;
}

time_t strtotime(const char *s)
{
  time_t r = 0;

  if (s && *s) {
     struct tm t;
     char *weekday;

     // example inputs: "ke 22.09.2010 21:00", "19.10.2010 23:50:00"
     if ((sscanf(s, "%m[^ ] %d.%d.%d %d:%d", &weekday, &t.tm_mday, &t.tm_mon, &t.tm_year, &t.tm_hour, &t.tm_min) == 6) ||
         (sscanf(s, "%d.%d.%d %d:%d", &t.tm_mday, &t.tm_mon, &t.tm_year, &t.tm_hour, &t.tm_min) == 5)) {

        t.tm_sec   = 0;
        t.tm_mon  -= 1;
        t.tm_year -= 1900;
        t.tm_isdst = -1;
        t.tm_zone  = "EET";

        r = mktime(&t);
        }
     else
        error("%s (%s) Conversion failed", __PRETTY_FUNCTION__, s);

     if (weekday)
        free(weekday);
     }

  return r;
}

// --- cMenuEditHiddenStrItem -------------------------------------------

#define AUTO_ADVANCE_TIMEOUT  1500 // ms before auto advance when entering characters via numeric keys

// class modified from VDR's cMenuEditStrItem
cMenuEditHiddenStrItem::cMenuEditHiddenStrItem(const char *Name, char *Value, int Length, const char *Allowed)
:cMenuEditItem(Name)
{
  value = Value;
  length = Length;
  allowed = Allowed ? Allowed : trVDR(FileNameChars);
  pos = -1;
  offset = 0;
  insert = uppercase = false;
  newchar = true;
  lengthUtf8 = 0;
  valueUtf8 = NULL;
  allowedUtf8 = NULL;
  charMapUtf8 = NULL;
  currentCharUtf8 = NULL;
  lastKey = kNone;
  Set();
}

cMenuEditHiddenStrItem::~cMenuEditHiddenStrItem()
{
  delete[] valueUtf8;
  delete[] allowedUtf8;
  delete[] charMapUtf8;
}

void cMenuEditHiddenStrItem::EnterEditMode()
{
  if (!valueUtf8) {
     valueUtf8 = new uint[length];
     lengthUtf8 = Utf8ToArray(value, valueUtf8, length);
     int l = (int)(strlen(allowed) + 1);
     allowedUtf8 = new uint[l];
     Utf8ToArray(allowed, allowedUtf8, l);
     const char *charMap = trVDR("CharMap$ 0\t-.,1#~\\^$[]|()*+?{}/:%@&\tabc2\tdef3\tghi4\tjkl5\tmno6\tpqrs7\ttuv8\twxyz9");
     l = (int)(strlen(charMap) + 1);
     charMapUtf8 = new uint[l];
     Utf8ToArray(charMap, charMapUtf8, l);
     currentCharUtf8 = charMapUtf8;
     AdvancePos();
     }
}

void cMenuEditHiddenStrItem::LeaveEditMode(bool SaveValue)
{
  if (valueUtf8) {
     if (SaveValue) {
        Utf8FromArray(valueUtf8, value, length);
        stripspace(value);
        }
     lengthUtf8 = 0;
     delete[] valueUtf8;
     valueUtf8 = NULL;
     delete[] allowedUtf8;
     allowedUtf8 = NULL;
     delete[] charMapUtf8;
     charMapUtf8 = NULL;
     pos = -1;
     offset = 0;
     newchar = true;
     }
}

void cMenuEditHiddenStrItem::SetHelpKeys()
{
  if (InEditMode())
     cSkinDisplay::Current()->SetButtons(trVDR("Button$ABC/abc"), insert ? trVDR("Button$Overwrite") : trVDR("Button$Insert"), trVDR("Button$Delete"));
  else
     cSkinDisplay::Current()->SetButtons(NULL);
}

uint *cMenuEditHiddenStrItem::IsAllowed(uint c)
{
  if (allowedUtf8) {
     for (uint *a = allowedUtf8; *a; a++) {
         if (c == *a)
            return a;
         }
     }
  return NULL;
}

void cMenuEditHiddenStrItem::AdvancePos()
{
  if (pos < length - 2 && pos < lengthUtf8) {
     if (++pos >= lengthUtf8) {
        if (pos >= 2 && valueUtf8[pos - 1] == ' ' && valueUtf8[pos - 2] == ' ')
           pos--; // allow only two blanks at the end
        else {
           valueUtf8[pos] = ' ';
           valueUtf8[pos + 1] = 0;
           lengthUtf8++;
           }
        }
     }
  newchar = true;
  if (!insert && Utf8is(alpha, valueUtf8[pos]))
     uppercase = Utf8is(upper, valueUtf8[pos]);
}

void cMenuEditHiddenStrItem::Set()
{
  if (InEditMode()) {
     // This is an ugly hack to make editing strings work with the 'skincurses' plugin.
     const cFont *font = dynamic_cast<cSkinDisplayMenu *>(cSkinDisplay::Current())->GetTextAreaFont(false);
     if (!font || font->Width("W") != 1) // all characters have with == 1 in the font used by 'skincurses'
        font = cFont::GetFont(fontOsd);

     int width = cSkinDisplay::Current()->EditableWidth();
     width -= font->Width("[]");
     width -= font->Width("<>"); // reserving this anyway make the whole thing simpler

     if (pos < offset)
        offset = pos;
     int WidthFromOffset = 0;
     int EndPos = lengthUtf8;
     for (int i = offset; i < lengthUtf8; i++) {
         WidthFromOffset += font->Width(valueUtf8[i]);
         if (WidthFromOffset > width) {
            if (pos >= i) {
               do {
                  WidthFromOffset -= font->Width(valueUtf8[offset]);
                  offset++;
                  } while (WidthFromOffset > width && offset < pos);
               EndPos = pos + 1;
               }
            else {
               EndPos = i;
               break;
               }
            }
         }

     int len;
     char buf[1000];
     char *p = buf;
     if (offset)
        *p++ = '<';
     len = Utf8FromArray(valueUtf8 + offset, p, (int)(sizeof(buf) - (p - buf)), pos - offset);
     for (int i = 0; i < len; ++i)
         *p++ = '*';
     *p++ = '[';
     if (insert && newchar)
        *p++ = ']';
     p += Utf8FromArray(&valueUtf8[pos], p, (int)(sizeof(buf) - (p - buf)), 1);
     if (!(insert && newchar))
        *p++ = ']';
     len = Utf8FromArray(&valueUtf8[pos + 1], p, (int)(sizeof(buf) - (p - buf)), EndPos - pos - 1);
     for (int i = 0; i < len; ++i)
         *p++ = '*';
     if (EndPos != lengthUtf8)
        *p++ = '>';
     *p = 0;

     SetValue(buf);
     }
  else {
     char buf[1000];
     char *p = buf;
     for (int i = 0; i < Utf8StrLen(value); ++i)
         *p++ = '*';
     *p = 0;

     SetValue(buf);
     }
}

uint cMenuEditHiddenStrItem::Inc(uint c, bool Up)
{
  uint *p = IsAllowed(c);
  if (!p)
     p = allowedUtf8;
  if (Up) {
     if (!*++p)
        p = allowedUtf8;
     }
  else if (--p < allowedUtf8) {
     p = allowedUtf8;
     while (*p && *(p + 1))
           p++;
     }
  return *p;
}

void cMenuEditHiddenStrItem::Type(uint c)
{
  if (insert && lengthUtf8 < length - 1)
     Insert();
  valueUtf8[pos] = c;
  if (pos < length - 2)
     pos++;
  if (pos >= lengthUtf8) {
     valueUtf8[pos] = ' ';
     valueUtf8[pos + 1] = 0;
     lengthUtf8 = pos + 1;
     }
}

void cMenuEditHiddenStrItem::Insert()
{
  memmove(valueUtf8 + pos + 1, valueUtf8 + pos, (lengthUtf8 - pos + 1) * sizeof(*valueUtf8));
  lengthUtf8++;
  valueUtf8[pos] = ' ';
}

void cMenuEditHiddenStrItem::Delete()
{
  memmove(valueUtf8 + pos, valueUtf8 + pos + 1, (lengthUtf8 - pos) * sizeof(*valueUtf8));
  lengthUtf8--;
}

eOSState cMenuEditHiddenStrItem::ProcessKey(eKeys Key)
{
  bool SameKey = NORMALKEY(Key) == lastKey;
  if (Key != kNone)
     lastKey = NORMALKEY(Key);
  else if (!newchar && k0 <= lastKey && lastKey <= k9 && autoAdvanceTimeout.TimedOut()) {
     AdvancePos();
     newchar = true;
     currentCharUtf8 = NULL;
     Set();
     return osContinue;
     }
  switch (int(Key)) {
    case kRed:   // Switch between upper- and lowercase characters
                 if (InEditMode()) {
                    if (!insert || !newchar) {
                       uppercase = !uppercase;
                       valueUtf8[pos] = uppercase ? Utf8to(upper, valueUtf8[pos]) : Utf8to(lower, valueUtf8[pos]);
                       }
                    }
                 else
                    return osUnknown;
                 break;
    case kGreen: // Toggle insert/overwrite modes
                 if (InEditMode()) {
                    insert = !insert;
                    newchar = true;
                    SetHelpKeys();
                    }
                 else
                    return osUnknown;
                 break;
    case kYellow|k_Repeat:
    case kYellow: // Remove the character at the current position; in insert mode it is the character to the right of the cursor
                 if (InEditMode()) {
                    if (lengthUtf8 > 1) {
                       if (!insert || pos < lengthUtf8 - 1)
                          Delete();
                       else if (insert && pos == lengthUtf8 - 1)
                          valueUtf8[pos] = ' '; // in insert mode, deleting the last character replaces it with a blank to keep the cursor position
                       // reduce position, if we removed the last character
                       if (pos == lengthUtf8)
                          pos--;
                       }
                    else if (lengthUtf8 == 1)
                       valueUtf8[0] = ' '; // This is the last character in the string, replace it with a blank
                    if (Utf8is(alpha, valueUtf8[pos]))
                       uppercase = Utf8is(upper, valueUtf8[pos]);
                    newchar = true;
                    }
                 else
                    return osUnknown;
                 break;
    case kBlue|k_Repeat:
    case kBlue:  // consume the key only if in edit-mode
                 if (!InEditMode())
                    return osUnknown;
                 break;
    case kLeft|k_Repeat:
    case kLeft:  if (pos > 0) {
                    if (!insert || newchar)
                       pos--;
                    newchar = true;
                    if (!insert && Utf8is(alpha, valueUtf8[pos]))
                       uppercase = Utf8is(upper, valueUtf8[pos]);
                    }
                 break;
    case kRight|k_Repeat:
    case kRight: if (InEditMode())
                    AdvancePos();
                 else {
                    EnterEditMode();
                    SetHelpKeys();
                    }
                 break;
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:  if (InEditMode()) {
                    if (insert && newchar) {
                       // create a new character in insert mode
                       if (lengthUtf8 < length - 1)
                          Insert();
                       }
                    if (uppercase)
                       valueUtf8[pos] = Utf8to(upper, Inc(Utf8to(lower, valueUtf8[pos]), NORMALKEY(Key) == kUp));
                    else
                       valueUtf8[pos] =               Inc(              valueUtf8[pos],  NORMALKEY(Key) == kUp);
                    newchar = false;
                    }
                 else
                    return cMenuEditItem::ProcessKey(Key);
                 break;
   case k0|k_Repeat ... k9|k_Repeat:
    case k0 ... k9: {
                 if (InEditMode()) {
                    if (Setup.NumberKeysForChars) {
                       if (!SameKey) {
                          if (!newchar)
                             AdvancePos();
                          currentCharUtf8 = NULL;
                          }
                       if (!currentCharUtf8 || !*currentCharUtf8 || *currentCharUtf8 == '\t') {
                          // find the beginning of the character map entry for Key
                          int n = NORMALKEY(Key) - k0;
                          currentCharUtf8 = charMapUtf8;
                          while (n > 0 && *currentCharUtf8) {
                                if (*currentCharUtf8++ == '\t')
                                   n--;
                                }
                          // find first allowed character
                          while (*currentCharUtf8 && *currentCharUtf8 != '\t' && !IsAllowed(*currentCharUtf8))
                                currentCharUtf8++;
                          }
                       if (*currentCharUtf8 && *currentCharUtf8 != '\t') {
                          if (insert && newchar) {
                             // create a new character in insert mode
                             if (lengthUtf8 < length - 1)
                                Insert();
                             }
                          valueUtf8[pos] = *currentCharUtf8;
                          if (uppercase)
                             valueUtf8[pos] = Utf8to(upper, valueUtf8[pos]);
                          // find next allowed character
                          do {
                             currentCharUtf8++;
                             } while (*currentCharUtf8 && *currentCharUtf8 != '\t' && !IsAllowed(*currentCharUtf8));
                          newchar = false;
                          autoAdvanceTimeout.Set(AUTO_ADVANCE_TIMEOUT);
                          }
                       }
                    else
                       Type('0' + NORMALKEY(Key) - k0);
                    }
                 else
                    return cMenuEditItem::ProcessKey(Key);
                 }
                 break;
    case kBack:
    case kOk:    if (InEditMode()) {
                    LeaveEditMode(Key == kOk);
                    SetHelpKeys();
                    break;
                    }
                 // run into default
    default:     if (InEditMode() && BASICKEY(Key) == kKbd) {
                    int c = KEYKBD(Key);
                    if (c <= 0xFF) { // FIXME what about other UTF-8 characters?
                       if (IsAllowed(Utf8to(lower, c)))
                          Type(c);
                       else {
                          switch (c) {
                            case 0x7F: // backspace
                                       if (pos > 0) {
                                          pos--;
                                          return ProcessKey(kYellow);
                                          }
                                       break;
                            default: ;
                            }
                          }
                       }
                    else {
                       switch (c) {
                         case kfHome: pos = 0; break;
                         case kfEnd:  pos = lengthUtf8 - 1; break;
                         case kfIns:  return ProcessKey(kGreen);
                         case kfDel:  return ProcessKey(kYellow);
                         default: ;
                         }
                       }
                    }
                 else
                    return cMenuEditItem::ProcessKey(Key);
    }
  Set();
  return osContinue;
}
