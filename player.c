/*
 * player.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/status.h>

#include "common.h"
#include "menu.h"
#include "player.h"

// --- cElvisReader ----------------------------------------------------

cElvisReader::cElvisReader(const char *urlP)
: cThread("elvisreader"),
  urlM(urlP),
  rangeStartM(0),
  rangeSizeM(0),
  rangePendingM(0),
  pauseToggledM(false),
  pausedM(false),
  eofM(false),
  handleM(NULL),
  multiM(NULL),
  headerListM(NULL),
  mutexM(),
  ringBufferM(new cRingBufferLinear(MEGABYTE(1), 7 * TS_SIZE))
{
  debug("cElvisReader::cElvisReader()");
  if (ringBufferM)
     ringBufferM->SetTimeouts(10, 0);
  Start();
}

cElvisReader::~cElvisReader()
{
  debug("cElvisReader::~cElvisReader()");
  Cancel(3);
  Disconnect();
  DELETE_POINTER(ringBufferM);
}

size_t cElvisReader::WriteCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cElvisReader *obj = (cElvisReader *)dataP;
  size_t len = sizeP * nmembP;

  if (obj && !obj->PutData((uchar *)ptrP, (int)len))
     return CURL_WRITEFUNC_PAUSE;

  return len;
}

size_t cElvisReader::HeaderCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cElvisReader *obj = (cElvisReader *)dataP;
  size_t len = sizeP * nmembP;

  if (obj && strstr((const char*)ptrP, "Content-Range:")) {
     unsigned long start, stop, size;
     if (sscanf((const char*)ptrP, "Content-Range: bytes %ld-%ld/%ld", &start, &stop, &size) == 3)
        obj->SetRange(start, stop, size);
     }

  return len;
}

void cElvisReader::SetRange(unsigned long startP, unsigned long stopP, unsigned long sizeP)
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisReader::SetRange(): start=%ld stop=%ld filesize=%ld", startP, stopP, sizeP);
  rangeStartM = startP;
  rangeSizeM = sizeP;
}

bool cElvisReader::PutData(uchar *dataP, int lenP)
{
  cMutexLock MutexLock(&mutexM);
  //debug("cElvisReader::PutData(%d)", lenP);
  if (pausedM)
     return false;
  if (ringBufferM && (lenP >= 0)) {
     // should be pause the transfer?
     if (ringBufferM->Free() < (2 * CURL_MAX_WRITE_SIZE)) {
        debug("cElvisReader::PutData(pause): free=%d available=%d len=%d", ringBufferM->Free(), ringBufferM->Available(), lenP);
        pausedM = true;
        return false;
        }
     int p = ringBufferM->Put(dataP, lenP);
     if (p != lenP)
        ringBufferM->ReportOverflow(lenP - p);
     }

  return true;
}

void cElvisReader::DelData(int lenP)
{
  cMutexLock MutexLock(&mutexM);
  //debug("cElvisReader::DelData()");
  if (ringBufferM && (lenP >= 0))
     ringBufferM->Del(lenP);
}

void cElvisReader::ClearData()
{
  cMutexLock MutexLock(&mutexM);
  //debug("cElvisReader::ClearData()");
  if (ringBufferM)
     ringBufferM->Clear();
}

uchar *cElvisReader::GetData(int *lenP)
{
  cMutexLock MutexLock(&mutexM);
  //debug("cElvisReader::GetData()");
  uchar *p = NULL;
  *lenP = 0;
  if (ringBufferM) {
     int count = 0;
     p = ringBufferM->Get(count);
     if (p && count >= TS_SIZE) {
        if (*p != TS_SYNC_BYTE) {
           for (int i = 1; i < count; ++i) {
               if (p[i] == TS_SYNC_BYTE) {
                  count = i;
                  break;
                  }
               }
           error("Skipped %d bytes to sync on TS packet\n", count);
           ringBufferM->Del(count);
           *lenP = 0;
           return NULL;
           }
        }
     count -= (count % TS_SIZE);
     *lenP = count;
     }
  if (eofM)
     *lenP = -1;

  return p;
}

void cElvisReader::JumpRequest(unsigned long startbyteP)
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisReader::JumpRequest(%ld)", startbyteP);
  rangePendingM = startbyteP;
}

void cElvisReader::Jump(unsigned long startbyteP)
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisReader::Jump(%ld)", startbyteP);
  rangePendingM = 0;
  rangeStartM = startbyteP;
  curl_multi_remove_handle(multiM, handleM);
  if (ringBufferM)
     ringBufferM->Clear();
  curl_easy_setopt(handleM, CURLOPT_RANGE, *cString::sprintf("%ld-", rangeStartM));
  curl_multi_add_handle(multiM, handleM);
}

void cElvisReader::Pause(bool onoffP)
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisReader::Pause(%d)", onoffP);
  pauseToggledM = true;
  pausedM = onoffP;
}

bool cElvisReader::Connect()
{
  cMutexLock MutexLock(&mutexM);
  bool initialConnect = false;
  debug("cElvisReader::Connect()");

  // initialize the curl session
  if (!handleM)
     handleM = curl_easy_init();
  if (!multiM) {
     multiM = curl_multi_init();
     initialConnect = true;
     }

  if (handleM && multiM) {
     pausedM = false;
#ifdef DEBUG
     // verbose output
     curl_easy_setopt(handleM, CURLOPT_VERBOSE, 1L);
#endif
     // set callbacks
     curl_easy_setopt(handleM, CURLOPT_WRITEFUNCTION, cElvisReader::WriteCallback);
     curl_easy_setopt(handleM, CURLOPT_WRITEDATA, this);
     curl_easy_setopt(handleM, CURLOPT_HEADERFUNCTION, cElvisReader::HeaderCallback);
     curl_easy_setopt(handleM, CURLOPT_HEADERDATA, this);

     // no progress meter and no signaling
     curl_easy_setopt(handleM, CURLOPT_NOPROGRESS, 1L);
     curl_easy_setopt(handleM, CURLOPT_NOSIGNAL, 1L);

     // set timeout
     curl_easy_setopt(handleM, CURLOPT_CONNECTTIMEOUT, 5L);

     // set user-agent
     curl_easy_setopt(handleM, CURLOPT_USERAGENT, *cString::sprintf("vdr-%s/%s", PLUGIN_NAME_I18N, VERSION));

     // follow location
     curl_easy_setopt(handleM, CURLOPT_FOLLOWLOCATION, 1L);

     // set url
     curl_easy_setopt(handleM, CURLOPT_URL, *urlM);

     // set range
     curl_easy_setopt(handleM, CURLOPT_RANGE, *cString::sprintf("%ld-", rangeStartM));

     // set additional headers to prevent caching
     if (initialConnect) {
        headerListM = curl_slist_append(headerListM, "Cache-Control: no-store, no-cache, must-revalidate");
        headerListM = curl_slist_append(headerListM, "Cache-Control: post-check=0, pre-check=0");
        headerListM = curl_slist_append(headerListM, "Pragma: no-cache");
        headerListM = curl_slist_append(headerListM, "Expires: Mon, 26 Jul 1997 05:00:00 GMT");
        curl_easy_setopt(handleM, CURLOPT_HTTPHEADER, headerListM); 
        }

     // add handle into multi set
     curl_multi_add_handle(multiM, handleM);

     return true;
     }

  return false;
}

bool cElvisReader::Disconnect()
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisReader::Disconnect()");
  if (handleM) {
     // cleanup curl stuff
     curl_slist_free_all(headerListM);
     headerListM = NULL;
     curl_multi_remove_handle(multiM, handleM);
     curl_multi_cleanup(multiM);
     multiM = NULL;
     curl_easy_cleanup(handleM);
     handleM = NULL;
     }

  return true;
}

void cElvisReader::Retry()
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisReader::Retry()");
  if (handleM) {
     double downloaded = 0.0;

     // remove handle
     curl_multi_remove_handle(multiM, handleM);

     // check how much we downloaded already
     curl_easy_getinfo(handleM, CURLINFO_SIZE_DOWNLOAD, &downloaded);
     debug("cElvisReader::Retry(): rangeStart=%ld, downloaded=%f", rangeStartM, downloaded);

     // continue
     rangeStartM += (unsigned long)downloaded;
     curl_easy_setopt(handleM, CURLOPT_RANGE, *cString::sprintf("%ld-", rangeStartM));
     curl_multi_add_handle(multiM, handleM);
     }
}

void cElvisReader::Action()
{
  debug("cElvisReader::Action(): start");
  if (ringBufferM && Connect()) {
     while (Running()) {
           CURLMcode err;
           int running_handles, maxfd;
           fd_set fdread, fdwrite, fdexcep;
           struct timeval timeout;

           if (rangePendingM)
              Jump(rangePendingM);

           do {
             err = curl_multi_perform(multiM, &running_handles);
           } while (err == CURLM_CALL_MULTI_PERFORM);

           // shall be continue filling up the buffer?
           mutexM.Lock();
           if (pauseToggledM) {
              curl_easy_pause(handleM, pausedM ? CURLPAUSE_ALL : CURLPAUSE_CONT);
              pauseToggledM = false;
              }
           if (pausedM && (ringBufferM->Free() > ringBufferM->Available())) {
              debug("cElvisReader::Action(continue): free=%d available=%d", ringBufferM->Free(), ringBufferM->Available());
              pausedM = false;
              curl_easy_pause(handleM, CURLPAUSE_CONT);
              }
           mutexM.Unlock();

           // check end of file
           if (running_handles == 0) {
              int msgcount;
              CURLMsg *msg = curl_multi_info_read(multiM, &msgcount);
              if (msg && (msg->msg == CURLMSG_DONE)) {
                 if (msg->data.result == CURLE_PARTIAL_FILE)
                    Retry();
                 else {
                    if (msg->data.result != CURLE_OK)
                       info("cElvisReader::Action(done): %s (%d)", curl_easy_strerror(msg->data.result), msg->data.result);
                    eofM = true;
                    break;
                    }
                 }
              }

           timeout.tv_sec  = 0;
           timeout.tv_usec = eTimeoutMs * 1000;
           FD_ZERO(&fdread);
           FD_ZERO(&fdwrite);
           FD_ZERO(&fdexcep);
           err = curl_multi_fdset(multiM, &fdread, &fdwrite, &fdexcep, &maxfd);
           if (maxfd >= 0)
              select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
           else
              cCondWait::SleepMs(eTimeoutMs);
           }
     Disconnect();
     }
}

// --- cElvisPlayer ----------------------------------------------------

int cElvisPlayer::speedS[] = { 0, -2, -4, -8, 1, 2, 4, 12, 0 };

cElvisPlayer::cElvisPlayer(const char *urlP, unsigned long lengthP)
: cThread("elvisplayer"),
  playModeM(pmPlay),
  playDirM(pdForward),
  trickSpeedM(spNormalIndex),
  lengthM((lengthP + 1 + 5) * 60), // remember to add start & stop marginals
  readerM(new cElvisReader(urlP)),
  readSizeM(0),
  ringBufferM(new cRingBufferFrame(MEGABYTE(1))),
  readFrameM(NULL),
  playFrameM(NULL),
  dropFrameM(NULL)
{
  debug("cElvisPlayer::cElvisPlayer()");
}

cElvisPlayer::~cElvisPlayer()
{
  debug("cElvisPlayer::~cElvisPlayer()");
  Activate(false);
  DELETE_POINTER(readerM);
  DELETE_POINTER(readFrameM);
  DELETE_POINTER(ringBufferM);
}

void cElvisPlayer::Activate(bool onP)
{
  debug("cElvisPlayer::Activate(%d)", onP);
  if (onP)
     Start();
  else
     Cancel(9);
}

void cElvisPlayer::TrickSpeed(int incrementP)
{
  int nts = trickSpeedM + incrementP;
  if (speedS[nts] == 1) {
     trickSpeedM = nts;
     if (playModeM == pmFast)
        Play();
     else
        Pause();
     }
  else if (speedS[nts]) {
     trickSpeedM = nts;
     int mul = ((playModeM == pmSlow) && (playDirM == pdForward)) ? 1 : spMultiplier;
     int sp = (speedS[nts] > 0) ? mul / speedS[nts] : -speedS[nts] * mul;
     if (sp > spClampValue)
        sp = spClampValue;
     DeviceTrickSpeed(sp);
     }
}

void cElvisPlayer::Play()
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisPlayer::Play()");

  if (playModeM != pmPlay) {
     if ((playModeM == pmStill) || (playModeM == pmFast) || ((playModeM == pmSlow) && (playDirM == pdBackward))) {
        if (!(DeviceHasIBPTrickSpeed() && (playDirM == pdForward)))
           Clear();
        }
     DevicePlay();
     playModeM = pmPlay;
     playDirM = pdForward;
     if (readerM)
        readerM->Pause(false);
     }
}

void cElvisPlayer::Pause()
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisPlayer::Pause()");

  if ((playModeM == pmPause) || (playModeM == pmStill)) {
     Play();
     if (readerM)
        readerM->Pause(false);
     }
  else {
     if ((playModeM == pmFast) || ((playModeM == pmSlow) && (playDirM == pdBackward))) {
        if (!(DeviceHasIBPTrickSpeed() && (playDirM == pdForward)))
           Clear();
        }
     DeviceFreeze();
     playModeM = pmPause;
     if (readerM)
        readerM->Pause(true);
     }
}

void cElvisPlayer::Clear()
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisPlayer::Clear()");
  if (readerM)
     readerM->ClearData();
  playFrameM = NULL;
  dropFrameM = NULL;
  DELETE_POINTER(readFrameM);
  ringBufferM->Clear();
  DeviceClear();
}

void cElvisPlayer::Forward()
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisPlayer::Forward()");
  switch (playModeM) {
    case pmFast:
         if (Setup.MultiSpeedMode) {
            TrickSpeed(playDirM == pdForward ? 1 : -1);
            break;
            }
         else if (playDirM == pdForward) {
            Play();
            break;
            }
         // run into pmPlay
    case pmPlay:
         if (!(DeviceHasIBPTrickSpeed() && (playDirM == pdForward)))
            Clear();
         if (DeviceIsPlayingVideo())
            DeviceMute();
         playModeM = pmFast;
         playDirM = pdForward;
         trickSpeedM = spNormalIndex;
         TrickSpeed(Setup.MultiSpeedMode ? 1 : spMaxOffset);
         break;
    case pmSlow:
         if (Setup.MultiSpeedMode) {
            TrickSpeed(playDirM == pdForward ? -1 : 1);
            break;
            }
         else if (playDirM == pdForward) {
            Pause();
            break;
            }
         Clear();
         // run into pmPause
    case pmPause:
    case pmStill:
         DeviceMute();
         playModeM = pmSlow;
         playDirM = pdForward;
         trickSpeedM = spNormalIndex;
         TrickSpeed(Setup.MultiSpeedMode ? -1 : -spMaxOffset);
         break;
    default:
         error("cElvisPlayer::Forward(): Unknown playmode=%d", playModeM);
         break;
    }
}

void cElvisPlayer::Backward()
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisPlayer::Backward()");
  switch (playModeM) {
    case pmFast:
         if (Setup.MultiSpeedMode) {
            TrickSpeed(playDirM == pdBackward ? 1 : -1);
            break;
            }
         else if (playDirM == pdBackward) {
            Play();
            break;
            }
         // run into pmPlay
    case pmPlay:
         Clear();
         if (DeviceIsPlayingVideo())
            DeviceMute();
         playModeM = pmFast;
         playDirM = pdBackward;
         trickSpeedM = spNormalIndex;
         TrickSpeed(Setup.MultiSpeedMode ? 1 : spMaxOffset);
         break;
    case pmSlow:
         if (Setup.MultiSpeedMode) {
            TrickSpeed(playDirM == pdBackward ? -1 : 1);
            break;
            }
         else if (playDirM == pdBackward) {
            Pause();
            break;
            }
         Clear();
         // run into pmPause
    case pmPause:
    case pmStill:
         Clear();
         DeviceMute();
         playModeM = pmSlow;
         playDirM = pdBackward;
         trickSpeedM = spNormalIndex;
         TrickSpeed(Setup.MultiSpeedMode ? -1 : -spMaxOffset);
         break;
    default:
         error("cElvisPlayer::Backward(): Unknown playmode=%d", playModeM);
         break;
    }
}

void cElvisPlayer::SkipTime(long secondsP, bool relativeP, bool playP)
{
  cMutexLock MutexLock(&mutexM);
  unsigned long filesize = readerM ? readerM->GetRangeSize() : 0;
  long skip = lengthM ? secondsP * (filesize / lengthM) : 0;
  debug("cElvisPlayer::SkipTime(%ld): skip=%ld filesize=%ld", secondsP, skip, filesize);
  if (!relativeP)
     readSizeM = 0;
  if ((skip < 0) && (readSizeM < (unsigned long)labs(skip)))
     readSizeM = 0;
  else if ((readSizeM + skip) > filesize)
     return;
  else
     readSizeM += skip;
  Clear();
  if (readerM)
     readerM->JumpRequest(readSizeM);
  if (playP)
     Play();
}

bool cElvisPlayer::GetReplayMode(bool &playP, bool &forwardP, int &speedP)
{
  cMutexLock MutexLock(&mutexM);
  //debug("cElvisPlayer::GetReplayMode()");
  playP = ((playModeM == pmPlay) || (playModeM == pmFast));
  forwardP = (playDirM == pdForward);
  if ((playModeM == pmFast) || (playModeM == pmSlow))
     speedP = Setup.MultiSpeedMode ? abs(trickSpeedM - spNormalIndex) : 0;
  else
     speedP = -1;
  return true;
}

void cElvisPlayer::Action()
{
  debug("cElvisPlayer::Action(): start");
  if (ringBufferM) {
     uchar *p = NULL;
     int pc = 0;
     bool sleep = false, firstPacket = true;
     cTimeMs timeout;

     while (Running()) {
           if (sleep) {
              cPoller Poller;
              DevicePoll(Poller, 10);
              sleep = false;
              }
           if ((playModeM == pmPause) || (playModeM == pmStill)) {
              sleep = true;
              continue;
              }
           {
             cMutexLock MutexLock(&mutexM);

             if (!readFrameM) {
                if (playDirM == pdBackward) {
                   if (timeout.TimedOut()) {
                      timeout.Set(eTrickplayTimeout * 1000);
                      SkipTime((playModeM == pmFast) ? (trickSpeedM - spNormalIndex) * -eTrickplaySkipLength : -eTrickplaySkipLength, true, false);
                      }
                   }
                else if (Setup.MultiSpeedMode && (playDirM == pdForward) && (playModeM == pmFast) && (trickSpeedM >= spSkipLimitIndex)) {
                   if (timeout.TimedOut()) {
                      timeout.Set(eTrickplayTimeout * 1000);
                      SkipTime((trickSpeedM - spNormalIndex) * eTrickplaySkipLength, true, false);
                      }
                   }
                if (readerM) {
                   int len = 0;
                   uchar *data = readerM->GetData(&len);
                   if (len < 0) {
                      debug("cElvisPlayer::Action(recv): EOF");
                      break;
                      }
                   else if (data && (len > 0)) {
                      readSizeM += len;
                      readFrameM = new cFrame(data, len, ftUnknown);
                      readerM->DelData(len);
                      }
                   }
                }

             if (readFrameM) {
                if (ringBufferM->Put(readFrameM))
                   readFrameM = NULL;
                else
                   sleep = true;
                }

             if (dropFrameM) {
                ringBufferM->Drop(dropFrameM);
                dropFrameM = NULL;
                }

             if (!playFrameM) {
                playFrameM = ringBufferM->Get();
                p = NULL;
                pc = 0;
                }

             if (playFrameM) {
                if (!p) {
                   p = playFrameM->Data();
                   pc = playFrameM->Count();
                   }
                }

             playFrameM = ringBufferM->Get();
             if (playFrameM) {
                if (!p) {
                   p = playFrameM->Data();
                   pc = playFrameM->Count();
                   if (p && firstPacket) {
                      PlayTs(NULL, 0);
                      firstPacket = false;
                      }
                   }
                if (p) {
                   int w = PlayTs(p, pc);
                   if (w > 0) {
                      p += w;
                      pc -= w;
                      }
                   else if (w < 0 && FATALERRNO)
                      LOG_ERROR;
                   else
                      sleep = true;
                   }
                if (pc <= 0) {
                   dropFrameM = playFrameM;
                   playFrameM = NULL;
                   p = NULL;
                   }
                }
             else
                sleep = true;
             }
           }
           }
}

// --- cElvisPlayerControl ---------------------------------------------

cElvisPlayerControl::cElvisPlayerControl(const char *urlP, const char *nameP, const char *descriptionP, const char *startTimeP, unsigned int lengthP)
: cControl(playerM = new cElvisPlayer(urlP, lengthP)),
  displayM(NULL),
  urlM(urlP),
  nameM(nameP),
  descriptionM(descriptionP),
  startTimeM(startTimeP),
  lengthM(lengthP)
{
  cStatus::MsgReplaying(this, nameP, "elvis.ts", true);
}

cElvisPlayerControl::~cElvisPlayerControl()
{
  Hide();
  cStatus::MsgReplaying(this, NULL, "elvis.ts", false);
  if (playerM)
     playerM->Clear();
  DELETE_POINTER(playerM);
}

void cElvisPlayerControl::Show()
{
}

void cElvisPlayerControl::Hide()
{
  DELETENULL(displayM);
}

cOsdObject *cElvisPlayerControl::GetInfo()
{
  if (!isempty(*descriptionM))
    return new cElvisRecordingInfoMenu(*urlM, *nameM, *descriptionM, *startTimeM, lengthM);

  return NULL;
}

eOSState cElvisPlayerControl::ProcessKey(eKeys keyP)
{
  if (playerM->Finished()) {
     DELETE_POINTER(playerM);
     return osEnd;
     }

  eOSState state = cControl::ProcessKey(keyP);

  switch (keyP) {
    case kPlay:
    case kUp:
         if (playerM)
            playerM->Play();
         break;
    case kPause:
    case kDown:
         if (playerM)
            playerM->Pause();
         break;
    case kFastRew|k_Release:
    case kLeft|k_Release:
         if (Setup.MultiSpeedMode)
            break;
    case kFastRew:
    case kLeft:
         if (playerM)
            playerM->Backward();
         break;
    case kFastFwd|k_Release:
    case kRight|k_Release:
         if (Setup.MultiSpeedMode)
            break;
    case kFastFwd:
    case kRight:
         if (playerM)
            playerM->Forward();
         break;
    case kGreen|k_Repeat:
    case kGreen:
         if (playerM)
            playerM->SkipTime(-60);
         break;
    case kYellow|k_Repeat:
    case kYellow:
         if (playerM)
            playerM->SkipTime(60);
         break;
    case k1|k_Repeat:
    case k1:
         if (playerM)
            playerM->SkipTime(-20);
         break;
    case k3|k_Repeat:
    case k3:
         if (playerM)
            playerM->SkipTime(20);
         break;
    case k4|k_Repeat:
    case k4:
         if (playerM)
            playerM->SkipTime(-5 * 60);
         break;
    case k6|k_Repeat:
    case k6:
         if (playerM)
            playerM->SkipTime(5 * 60);
         break;
    case k7|k_Repeat:
    case k7:
         if (playerM)
            playerM->SkipTime(-15 * 60);
         break;
    case k9|k_Repeat:
    case k9:
         if (playerM)
            playerM->SkipTime(15 * 60);
         break;
    case kStop:
    case kBlue:
    case kBack:
         Hide();
         if (playerM)
            playerM->Clear();
         DELETE_POINTER(playerM);
         return osEnd;
    case kOk:
         if (!displayM) {
            displayM = Skins.Current()->DisplayReplay(false);
            displayM->SetTitle(*nameM);
            }
         else
            Hide();
   default:
         break;
   }

   if (displayM && playerM) {
        bool play = true, forward = true;
        int speed = -1;
        displayM->SetCurrent(playerM ? *cString::sprintf("%ld:%02ld:%02ld", playerM->Current() / 3600, (playerM->Current() % 3600) / 60, playerM->Current() % 60) : "0:00:00");
        displayM->SetTotal(playerM ? *cString::sprintf("%ld:%02ld:%02ld", playerM->Total() / 3600, (playerM->Total() % 3600) / 60, playerM->Total() % 60) : "0:00:00");
        displayM->SetProgress(playerM ? playerM->Progress() : 0, 100);
        if (playerM->GetReplayMode(play, forward, speed))
           displayM->SetMode(play, forward, speed);
    }

  return state;
}
