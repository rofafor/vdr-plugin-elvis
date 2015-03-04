/*
 * player.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/remote.h>
#include <vdr/status.h>

#include "common.h"
#include "log.h"
#include "menu.h"
#include "resume.h"
#include "player.h"

// --- cElvisReader ----------------------------------------------------

cElvisReader::cElvisReader(const char *urlP)
: cThread("cElvisReader"),
  urlM(urlP),
  rangeStartM(0),
  rangeSizeM(0),
  rangePendingM(0),
  durationM(0),
  pauseToggledM(false),
  pausedM(false),
  eofM(false),
  handleM(NULL),
  multiM(NULL),
  headerListM(NULL),
  ringBufferM(new cRingBufferLinear(MEGABYTE(2), 7 * TS_SIZE))
{
  debug1("%s", __PRETTY_FUNCTION__);
  if (ringBufferM) {
     ringBufferM->SetTimeouts(10, 0);
     ringBufferM->SetIoThrottle();
     }
  Start();
}

cElvisReader::~cElvisReader()
{
  debug1("%s", __PRETTY_FUNCTION__);
  Cancel(3);
  Disconnect();
  DELETE_POINTER(ringBufferM);
}

int cElvisReader::DebugCallback(CURL *handleP, curl_infotype typeP, char *dataP, size_t sizeP, void *userPtrP)
{
  cElvisReader *obj = reinterpret_cast<cElvisReader *>(userPtrP);

  if (obj) {
     switch (typeP) {
       case CURLINFO_TEXT:
            debug8("%s INFO %.*s", __PRETTY_FUNCTION__, (int)sizeP, dataP);
            break;
       case CURLINFO_HEADER_IN:
            debug8("%s HEAD <<< %.*s", __PRETTY_FUNCTION__,  (int)sizeP, dataP);
            break;
       case CURLINFO_HEADER_OUT:
            debug8("%s HEAD >>>\n%.*s", __PRETTY_FUNCTION__, (int)sizeP, dataP);
            break;
       case CURLINFO_DATA_IN:
            debug8("%s DATA <<< %zu", __PRETTY_FUNCTION__,  sizeP);
            break;
       case CURLINFO_DATA_OUT:
            debug8("%s DATA >>> %zu", __PRETTY_FUNCTION__, sizeP);
            break;
       default:
            break;
       }
     }

  return 0;
}

size_t cElvisReader::WriteCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cElvisReader *obj = reinterpret_cast<cElvisReader *>(dataP);
  size_t len = sizeP * nmembP;

  if (obj && !obj->PutData((uchar *)ptrP, (int)len))
     return CURL_WRITEFUNC_PAUSE;

  return len;
}

size_t cElvisReader::HeaderCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cElvisReader *obj = reinterpret_cast<cElvisReader *>(dataP);
  size_t len = sizeP * nmembP;

  if (obj) {
     if (strstr((const char*)ptrP, "Content-Range:")) {
        unsigned long start, stop, size;
        if (sscanf((const char*)ptrP, "Content-Range: bytes %lu-%lu/%lu", &start, &stop, &size) == 3)
           obj->SetRange(start, stop, size);
        }
     else if (strstr((const char*)ptrP, "Content-Duration:")) {
        unsigned long duration;
        if (sscanf((const char*)ptrP, "Content-Duration: %lu", &duration) == 1)
           obj->SetDuration(duration);
        }
    }

  return len;
}

void cElvisReader::SetRange(unsigned long startP, unsigned long stopP, unsigned long sizeP)
{
  LOCK_THREAD;
  debug1("%s (%ld, %ld, %ld)", __PRETTY_FUNCTION__, startP, stopP, sizeP);
  rangeStartM = startP;
  rangeSizeM = sizeP;
}

void cElvisReader::SetDuration(unsigned long durationP)
{
  LOCK_THREAD;
  debug1("%s (%ld)", __PRETTY_FUNCTION__, durationP);
  durationM = durationP;
}

bool cElvisReader::PutData(uchar *dataP, int lenP)
{
  LOCK_THREAD;
  debug16("%s (%d)", __PRETTY_FUNCTION__, lenP);
  if (pausedM)
     return false;
  if (ringBufferM && (lenP >= 0)) {
     // should be pause the transfer?
     if (ringBufferM->Free() < (2 * CURL_MAX_WRITE_SIZE)) {
        debug5("%s (%d) Pausing free=%d available=%d", __PRETTY_FUNCTION__, lenP, ringBufferM->Free(), ringBufferM->Available());
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
  LOCK_THREAD;
  debug16("%s (%d)", __PRETTY_FUNCTION__, lenP);
  if (ringBufferM && (lenP >= 0))
     ringBufferM->Del(lenP);
}

void cElvisReader::ClearData()
{
  LOCK_THREAD;
  debug16("%s", __PRETTY_FUNCTION__);
  if (ringBufferM)
     ringBufferM->Clear();
}

uchar *cElvisReader::GetData(int *lenP)
{
  LOCK_THREAD;
  debug16("%s", __PRETTY_FUNCTION__);
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
  LOCK_THREAD;
  debug1("%s (%ld)", __PRETTY_FUNCTION__, startbyteP);
  rangePendingM = startbyteP;
}

void cElvisReader::Jump(unsigned long startbyteP)
{
  LOCK_THREAD;
  debug1("%s (%ld)", __PRETTY_FUNCTION__, startbyteP);
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
  LOCK_THREAD;
  debug1("%s (%d)", __PRETTY_FUNCTION__, onoffP);
  pauseToggledM = true;
  pausedM = onoffP;
}

bool cElvisReader::Connect()
{
  LOCK_THREAD;
  bool initialConnect = false;
  debug1("%s", __PRETTY_FUNCTION__);

  // initialize the curl session
  if (!handleM)
     handleM = curl_easy_init();
  if (!multiM) {
     multiM = curl_multi_init();
     initialConnect = true;
     }

  if (handleM && multiM) {
     pausedM = false;
     // verbose output
     curl_easy_setopt(handleM, CURLOPT_VERBOSE, 1L);
     curl_easy_setopt(handleM, CURLOPT_DEBUGFUNCTION, cElvisReader::DebugCallback);
     curl_easy_setopt(handleM, CURLOPT_DEBUGDATA, this);

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
     curl_easy_setopt(handleM, CURLOPT_LOW_SPEED_LIMIT, 100L);
     curl_easy_setopt(handleM, CURLOPT_LOW_SPEED_TIME, 3L);

     // set user-agent
     curl_easy_setopt(handleM, CURLOPT_USERAGENT, *cString::sprintf("vdr-%s/%s", PLUGIN_NAME_I18N, VERSION));

     // limit download speed (bytes/s)
     curl_easy_setopt(handleM, CURLOPT_MAX_RECV_SPEED_LARGE, eMaxDownloadSpeedMBits * 131072L);

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
  LOCK_THREAD;
  debug1("%s", __PRETTY_FUNCTION__);
  if (handleM) {
     // cleanup curl stuff
     if (headerListM) {
        curl_slist_free_all(headerListM);
        headerListM = NULL;
        }
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
  LOCK_THREAD;
  debug1("%s", __PRETTY_FUNCTION__);
  if (handleM) {
     double downloaded = 0.0;

     // remove handle
     curl_multi_remove_handle(multiM, handleM);

     // check how much we downloaded already
     curl_easy_getinfo(handleM, CURLINFO_SIZE_DOWNLOAD, &downloaded);
     debug5("%s rangestart=%ld, downloaded=%f", __PRETTY_FUNCTION__, rangeStartM, downloaded);

     // continue
     rangeStartM += (unsigned long)downloaded;
     curl_easy_setopt(handleM, CURLOPT_RANGE, *cString::sprintf("%ld-", rangeStartM));
     curl_multi_add_handle(multiM, handleM);
     }
}

void cElvisReader::Action()
{
  debug1("%s Start", __PRETTY_FUNCTION__);
  // set to higher priority
  SetPriority(-1);
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
           Lock();
           if (pauseToggledM) {
              curl_easy_pause(handleM, pausedM ? CURLPAUSE_ALL : CURLPAUSE_CONT);
              pauseToggledM = false;
              }
           if (pausedM && (ringBufferM->Free() > ringBufferM->Available())) {
              debug5("%s Continuing free=%d available=%d", __PRETTY_FUNCTION__, ringBufferM->Free(), ringBufferM->Available());
              pausedM = false;
              curl_easy_pause(handleM, CURLPAUSE_CONT);
              }
           Unlock();

           // check end of file
           if (running_handles == 0) {
              int msgcount;
              CURLMsg *msg = curl_multi_info_read(multiM, &msgcount);
              if (msg && (msg->msg == CURLMSG_DONE)) {
                 if (msg->data.result == CURLE_PARTIAL_FILE)
                    Retry();
                 else {
                    if (msg->data.result != CURLE_OK)
                       info("%s %s (%d)", __PRETTY_FUNCTION__, curl_easy_strerror(msg->data.result), msg->data.result);
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

cElvisPlayer::cElvisPlayer(int programIdP, const char *urlP)
: cThread("cElvisPlayer"),
  playModeM(pmPlay),
  playDirM(pdForward),
  trickSpeedM(0),
  programIdM(programIdP),
  durationM(0),
  readerM(new cElvisReader(urlP)),
  readSizeM(0),
  fileSizeM(0),
  ringBufferM(new cRingBufferFrame(MEGABYTE(2))),
  readFrameM(NULL),
  playFrameM(NULL),
  dropFrameM(NULL)
{
  unsigned long offset = 0;
  unsigned long size = 0;
  debug1("%s (%d, %s)", __PRETTY_FUNCTION__, programIdP, urlP);
  if (cElvisResumeItems::GetInstance()->HasResume(programIdM, offset, size) && (offset > 0)) {
     readSizeM = offset;
     fileSizeM = size;
     debug1("%s (%d, %s) Resuming to %ld/%ld", __PRETTY_FUNCTION__, programIdP, urlP, readSizeM, fileSizeM);
     if (readerM)
        readerM->JumpRequest(readSizeM);
     }
}

cElvisPlayer::~cElvisPlayer()
{
  debug1("%s", __PRETTY_FUNCTION__);
  Activate(false);
  DELETE_POINTER(readerM);
  DELETE_POINTER(readFrameM);
  DELETE_POINTER(ringBufferM);
  cElvisResumeItems::GetInstance()->Store(programIdM, IsEOF() ? 0 : readSizeM, fileSizeM);
}

void cElvisPlayer::Activate(bool onP)
{
  debug1("%s (%d)", __PRETTY_FUNCTION__, onP);
  if (onP)
     Start();
  else
     Cancel(9);
}

bool cElvisPlayer::IsEOF()
{
  LOCK_THREAD;
  unsigned long limit = durationM ? (durationM - eEOFMark) * (fileSizeM / durationM) : 0;
  debug1("%s readSize=%ld limit=%ld", __PRETTY_FUNCTION__, readSizeM, limit);
  return (readSizeM >= limit);
}

void cElvisPlayer::TrickSpeed(int incrementP)
{
  int nts = trickSpeedM + incrementP;
  debug1("%s (%d) old=%d, new=%d", __PRETTY_FUNCTION__, incrementP, trickSpeedM, nts);
  if (nts == 0) {
     trickSpeedM = nts;
     if (playModeM == pmFast)
        Play();
     else
        Pause();
     }
  else if ((nts >= -3) && (nts <= 3)) {
     trickSpeedM = nts;
     if ((trickSpeedM < 0) && (playDirM == pdBackward)) {
        DevicePlay();
        DeviceMute();
        }
     else if (playDirM == pdForward) {
        switch (trickSpeedM) {
          case 1:
               DeviceTrickSpeed((playModeM == pmSlow) ? 8 : 6, true);
               break;
          case 2:
               DeviceTrickSpeed((playModeM == pmSlow) ? 4 : 3, true);
               break;
          case 3:
               DeviceTrickSpeed((playModeM == pmSlow) ? 2 : 1, true);
               break;
          default:
               break;
          }
        }
     }
}

void cElvisPlayer::Play()
{
  LOCK_THREAD;
  debug1("%s", __PRETTY_FUNCTION__);
  if (playModeM != pmPlay) {
     DevicePlay();
     playModeM = pmPlay;
     playDirM = pdForward;
     if (readerM)
        readerM->Pause(false);
     }
}

void cElvisPlayer::Pause()
{
  LOCK_THREAD;
  debug1("%s", __PRETTY_FUNCTION__);
  if (playModeM == pmPause) {
     Play();
     if (readerM)
        readerM->Pause(false);
     }
  else {
     DeviceFreeze();
     playModeM = pmPause;
     if (readerM)
        readerM->Pause(true);
     }
}

void cElvisPlayer::Clear()
{
  LOCK_THREAD;
  debug1("%s", __PRETTY_FUNCTION__);
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
  LOCK_THREAD;
  debug1("%s", __PRETTY_FUNCTION__);
  switch (playModeM) {
    case pmFast:
         if (Setup.MultiSpeedMode) {
            TrickSpeed(1);
            break;
            }
         else if (playDirM == pdForward) {
            Play();
            break;
            }
         // run into pmPlay
    case pmPlay:
         if (DeviceIsPlayingVideo())
            DeviceMute();
         playModeM = pmFast;
         playDirM = pdForward;
         trickSpeedM = Setup.MultiSpeedMode ? 0 : 1;
         TrickSpeed(1);
         break;
    case pmSlow:
         if (Setup.MultiSpeedMode) {
            TrickSpeed(1);
            break;
            }
         else if (playDirM == pdForward) {
            Pause();
            break;
            }
         Clear();
         // run into pmPause
    case pmPause:
         DeviceMute();
         playModeM = pmSlow;
         playDirM = pdForward;
         trickSpeedM = Setup.MultiSpeedMode ? 0 : 1;
         TrickSpeed(1);
         break;
    default:
         error("%s Unknown playmode=%d", __PRETTY_FUNCTION__, playModeM);
         break;
    }
}

void cElvisPlayer::Backward()
{
  LOCK_THREAD;
  debug1("%s", __PRETTY_FUNCTION__);
  switch (playModeM) {
    case pmFast:
         if (Setup.MultiSpeedMode) {
            TrickSpeed(-1);
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
         trickSpeedM = Setup.MultiSpeedMode ? 0 : -1;
         TrickSpeed(-1);
         break;
    case pmSlow:
         if (Setup.MultiSpeedMode) {
            TrickSpeed(-1);
            break;
            }
         else if (playDirM == pdBackward) {
            Pause();
            break;
            }
         // run into pmPause
    case pmPause:
         Clear();
         DeviceMute();
         playModeM = pmSlow;
         playDirM = pdBackward;
         trickSpeedM = Setup.MultiSpeedMode ? 0 : -1;
         TrickSpeed(-1);
         break;
    default:
         error("%s Unknown playmode=%d", __PRETTY_FUNCTION__, playModeM);
         break;
    }
}

void cElvisPlayer::SkipTime(long secondsP, bool relativeP, bool playP)
{
  LOCK_THREAD;
  long skip = durationM ? secondsP * (fileSizeM / durationM) : 0;
  debug1("%s (%ld, %d, %d): skip=%ld filesize=%ld", __PRETTY_FUNCTION__, secondsP, relativeP, playP, skip, fileSizeM);
  if (!relativeP)
     readSizeM = 0;
  if ((skip < 0) && (readSizeM < (unsigned long)labs(skip)))
     readSizeM = 0;
  else if ((readSizeM + skip) > fileSizeM)
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
  LOCK_THREAD;
  debug16("%s", __PRETTY_FUNCTION__);
  playP = ((playModeM == pmPlay) || (playModeM == pmFast));
  forwardP = (playDirM == pdForward);
  if ((playModeM == pmFast) || (playModeM == pmSlow))
     speedP = Setup.MultiSpeedMode ? abs(trickSpeedM) : 0;
  else
     speedP = -1;
  return true;
}

int cElvisPlayer::GetForwardJumpPeriod()
{
  int interval = eTrickplayJumpBase;

  if (trickSpeedM > 0) {
     switch (playModeM) {
       case pmFast:
            interval *= trickSpeedM * 2;
            break;
       default:
            break;
       }
     }

  return interval;
}

int cElvisPlayer::GetBackwardJumpPeriod()
{
  int interval = eTrickplayJumpBase;

  if (trickSpeedM < 0) {
     switch (playModeM) {
       case pmFast:
            interval *= trickSpeedM * 2;
            break;
       case pmSlow:
            interval *= trickSpeedM;
            break;
       default:
            break;
       }
    }
  else
    interval = -interval;

  return interval;
}

void cElvisPlayer::Action()
{
  debug1("%s Start", __PRETTY_FUNCTION__);
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
           if (playModeM == pmPause) {
              sleep = true;
              continue;
              }
           { // start of block
             LOCK_THREAD;

             if (!readFrameM) {
                if (playDirM == pdBackward) {
                   if (timeout.TimedOut()) {
                      timeout.Set(eTrickplayTimeoutMs);
                      SkipTime(GetBackwardJumpPeriod(), true, false);
                      }
                   }
                else if (Setup.MultiSpeedMode && (trickSpeedM > 2) && (playDirM == pdForward) && (playModeM == pmFast)) {
                   if (timeout.TimedOut()) {
                      timeout.Set(eTrickplayTimeoutMs);
                      SkipTime(GetForwardJumpPeriod(), true, false);
                      }
                   }
                if (readerM) {
                   int len = 0;
                   uchar *data = readerM->GetData(&len);
                   if (fileSizeM == 0)
                      fileSizeM = readerM->GetRangeSize();
                   if (durationM == 0)
                      durationM = readerM->GetDuration();
                   if (len < 0) {
                      debug1("%s EOF", __PRETTY_FUNCTION__);
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

             // to prevent busylooping with xineliboutput
             if (!readFrameM && !playFrameM)
                cCondWait::SleepMs(2);

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
           } // end of block
           }
     }
}

// --- cElvisPlayerControl ---------------------------------------------

cElvisPlayerControl::cElvisPlayerControl(int programIdP, const char *urlP)
: cControl(playerM = new cElvisPlayer(programIdP, urlP))
{
  debug1("%s (%d, %s)", __PRETTY_FUNCTION__, programIdP, urlP);
}

cElvisPlayerControl::~cElvisPlayerControl()
{
  debug1("%s", __PRETTY_FUNCTION__);
  Stop();
}

bool cElvisPlayerControl::Active()
{
  debug16("%s", __PRETTY_FUNCTION__);
  return (playerM && !playerM->Finished());
}

void cElvisPlayerControl::Stop()
{
  debug16("%s", __PRETTY_FUNCTION__);
  DELETENULL(playerM);
}

void cElvisPlayerControl::Pause()
{
  debug1("%s", __PRETTY_FUNCTION__);
  if (playerM) {
     playerM->ClearJump();
     playerM->Pause();
     }
}

void cElvisPlayerControl::Play()
{
  debug1("%s", __PRETTY_FUNCTION__);
  if (playerM) {
     playerM->ClearJump();
     playerM->Play();
     }
}

void cElvisPlayerControl::Forward()
{
  debug1("%s", __PRETTY_FUNCTION__);
  if (playerM) {
     playerM->ClearJump();
     playerM->Forward();
     }
}

void cElvisPlayerControl::Backward()
{
  debug1("%s", __PRETTY_FUNCTION__);
  if (playerM) {
     playerM->ClearJump();
     playerM->Backward();
     }
}

void cElvisPlayerControl::SkipSeconds(int secondsP)
{
  debug1("%s (%d)", __PRETTY_FUNCTION__, secondsP);
  if (playerM) {
     playerM->ClearJump();
     playerM->SkipTime(secondsP);
     }
}

bool cElvisPlayerControl::GetDuration(unsigned long &durationP)
{
  debug16("%s", __PRETTY_FUNCTION__);
  if (playerM) {
     durationP = playerM->Total();
     return true;
     }
  return false;
}

bool cElvisPlayerControl::GetProgress(int &currentP, int &totalP)
{
  debug16("%s", __PRETTY_FUNCTION__);
  if (playerM) {
     currentP = playerM->Progress();
     totalP = 100;
     return true;
     }
  return false;
}

bool cElvisPlayerControl::GetIndex(unsigned long &currentP, unsigned long &totalP)
{
  debug16("%s", __PRETTY_FUNCTION__);
  if (playerM) {
     currentP = playerM->Current();
     totalP = playerM->Total();
     return true;
     }
  return false;
}

bool cElvisPlayerControl::GetReplayMode(bool &playP, bool &forwardP, int &speedP)
{
  debug16("%s", __PRETTY_FUNCTION__);
  return (playerM && playerM->GetReplayMode(playP, forwardP, speedP));
}

void cElvisPlayerControl::Goto(int secondsP, bool playP)
{
  debug1("%s (%d, %d)", __PRETTY_FUNCTION__, secondsP, playP);
  if (playerM) {
     playerM->ClearJump();
     playerM->SkipTime(secondsP, false, playP);
     }
}

// --- cElvisReplayControl ---------------------------------------------------

cElvisReplayControl::cElvisReplayControl(int programIdP, const char *urlP, const char *nameP, const char *descriptionP, const char *startTimeP, unsigned int lengthP)
: cElvisPlayerControl(programIdP, urlP),
  displayReplayM(NULL),
  urlM(urlP),
  nameM(nameP),
  descriptionM(descriptionP),
  startTimeM(startTimeP),
  lengthM(lengthP),
  visibleM(false),
  modeOnlyM(false),
  shownM(false),
  lastCurrentM(-1),
  lastTotalM(-1),
  lastPlayM(false),
  lastForwardM(false),
  lastSpeedM(-2), // an invalid value
  timeoutShowM(0),
  timeSearchActiveM(false),
  timeSearchHideM(false),
  timeSearchTimeM(-1),
  timeSearchPosM(-1)
{
  debug1("%s (%d, %s, %s, %s, %s, %u)", __PRETTY_FUNCTION__, programIdP, urlP, nameP, descriptionP, startTimeP, lengthP);
  cStatus::MsgReplaying(this, nameP, PLUGIN_NAME_I18N ".ts", true);
  cDevice::PrimaryDevice()->ClrAvailableTracks(true);
}

cElvisReplayControl::~cElvisReplayControl()
{
  debug1("%s", __PRETTY_FUNCTION__);
  Hide();
  cStatus::MsgReplaying(this, NULL, PLUGIN_NAME_I18N ".ts", false);
  Stop();
}

void cElvisReplayControl::Stop()
{
  debug1("%s", __PRETTY_FUNCTION__);
  cElvisPlayerControl::Stop();
}

void cElvisReplayControl::ShowTimed(int secondsP)
{
  debug16("%s (%d)", __PRETTY_FUNCTION__, secondsP);
  if (modeOnlyM)
     Hide();
  if (!visibleM) {
     shownM = ShowProgress(true);
     timeoutShowM = (shownM && secondsP > 0) ? time(NULL) + secondsP : 0;
     }
}

void cElvisReplayControl::Show()
{
  debug16("%s", __PRETTY_FUNCTION__);
  ShowTimed();
}

void cElvisReplayControl::Hide()
{
  debug16("%s", __PRETTY_FUNCTION__);
  if (visibleM) {
     DELETENULL(displayReplayM);
     SetNeedsFastResponse(false);
     visibleM = false;
     modeOnlyM = false;
     lastPlayM = lastForwardM = false;
     lastSpeedM = -2; // an invalid value
     timeSearchActiveM = false;
     }
}

void cElvisReplayControl::ShowMode()
{
  debug16("%s", __PRETTY_FUNCTION__);
  if (visibleM || (Setup.ShowReplayMode && !cOsd::IsOpen())) {
     bool play, forward;
     int speed;
     if (GetReplayMode(play, forward, speed) && (!visibleM || play != lastPlayM || forward != lastForwardM || speed != lastSpeedM)) {
        bool normalPlay = (play && speed == -1);
        if (!visibleM) {
           if (normalPlay)
              return; // no need to do indicate ">" unless there was a different mode displayed before
           visibleM = modeOnlyM = true;
           displayReplayM = Skins.Current()->DisplayReplay(modeOnlyM);
           }

        if (modeOnlyM && !timeoutShowM && normalPlay)
           timeoutShowM = time(NULL) + eModeTimeout;
        displayReplayM->SetMode(play, forward, speed);
        lastPlayM = play;
        lastForwardM = forward;
        lastSpeedM = speed;
        }
     }
}

cString cElvisReplayControl::SecondsToHMSF(unsigned long secondsP)
{
  debug16("%s (%ld)", __PRETTY_FUNCTION__, secondsP);
  return cString::sprintf("%ld:%02ld:%02ld", secondsP / 3600, (secondsP % 3600) / 60, secondsP % 60);
}

bool cElvisReplayControl::ShowProgress(bool initialP)
{
  unsigned long current, total;
  debug16("%s (%d)", __PRETTY_FUNCTION__, initialP);

  if (GetIndex(current, total) && (total > 0)) {
     if (!visibleM) {
        displayReplayM = Skins.Current()->DisplayReplay(modeOnlyM);
        SetNeedsFastResponse(true);
        visibleM = true;
        }
     if (initialP) {
        if (!isempty(*nameM))
           displayReplayM->SetTitle(*nameM);
        lastCurrentM = lastTotalM = -1;
        }
     if (total != lastTotalM) {
        displayReplayM->SetTotal(SecondsToHMSF(total));
        if (!initialP)
           displayReplayM->Flush();
        }
     if (current != lastCurrentM || total != lastTotalM) {
        int current2, total2;
        if (GetProgress(current2, total2))
           displayReplayM->SetProgress(current2, total2);
        if (!initialP)
           displayReplayM->Flush();
        displayReplayM->SetCurrent(SecondsToHMSF(current));
        displayReplayM->Flush();
        lastCurrentM = current;
        }
     lastTotalM = total;
     ShowMode();
     return true;
     }
  return false;
}

void cElvisReplayControl::TimeSearchDisplay()
{
  char buf[64];
  debug16("%s", __PRETTY_FUNCTION__);
  strcpy(buf, trVDR("Jump: "));
  size_t len = strlen(buf);
  char h10 = (char)('0' + (timeSearchTimeM >> 24));
  char h1  = (char)('0' + ((timeSearchTimeM & 0x00FF0000) >> 16));
  char m10 = (char)('0' + ((timeSearchTimeM & 0x0000FF00) >> 8));
  char m1  = (char)('0' + (timeSearchTimeM & 0x000000FF));
  char ch10 = timeSearchPosM > 3 ? h10 : '-';
  char ch1  = timeSearchPosM > 2 ? h1  : '-';
  char cm10 = timeSearchPosM > 1 ? m10 : '-';
  char cm1  = timeSearchPosM > 0 ? m1  : '-';
  sprintf(buf + len, "%c%c:%c%c", ch10, ch1, cm10, cm1);
  displayReplayM->SetJump(buf);
}

void cElvisReplayControl::TimeSearchProcess(eKeys keyP)
{
  debug16("%s (%d)", __PRETTY_FUNCTION__, keyP);
  int seconds = (timeSearchTimeM >> 24) * 36000 + ((timeSearchTimeM & 0x00FF0000) >> 16) * 3600 + ((timeSearchTimeM & 0x0000FF00) >> 8) * 600 + (timeSearchTimeM & 0x000000FF) * 60;
  unsigned long current = lastCurrentM;
  unsigned long total = lastTotalM;
  switch (keyP) {
    case k0 ... k9:
         if (timeSearchPosM < 4) {
            timeSearchTimeM <<= 8;
            timeSearchTimeM |= keyP - k0;
            timeSearchPosM++;
            TimeSearchDisplay();
            }
         break;
    case kFastRew:
    case kLeft:
    case kFastFwd:
    case kRight: {
         int dir = (((keyP == kRight) || (keyP == kFastFwd)) ? 1 : -1);
         if (dir > 0)
            seconds = min((int)(total - current - eStaySecondsOffEnd), seconds);
         SkipSeconds(seconds * dir);
         timeSearchActiveM = false;
         }
         break;
    case kPlay:
    case kUp:
    case kPause:
    case kDown:
    case kOk:
         seconds = min((int)(total - eStaySecondsOffEnd), seconds);
         Goto(seconds, ((keyP != kDown) && (keyP != kPause) && (keyP != kOk)));
         timeSearchActiveM = false;
         break;
    default:
         if (!(keyP & k_Flags)) // ignore repeat/release keys
            timeSearchActiveM = false;
         break;
    }

  if (!timeSearchActiveM) {
     if (timeSearchHideM)
        Hide();
     else
        displayReplayM->SetJump(NULL);
     ShowMode();
     }
}

void cElvisReplayControl::TimeSearch()
{
  debug16("%s", __PRETTY_FUNCTION__);
  timeSearchTimeM = 0;
  timeSearchPosM = 0;
  timeSearchHideM = false;
  if (modeOnlyM)
     Hide();
  if (!visibleM) {
     Show();
     if (visibleM)
        timeSearchHideM = true;
     else
        return;
     }
  timeoutShowM = 0;
  TimeSearchDisplay();
  timeSearchActiveM = true;
}

cOsdObject *cElvisReplayControl::GetInfo()
{
  debug16("%s", __PRETTY_FUNCTION__);
  if (!isempty(*descriptionM)) {
     unsigned long duration = 0;
     if (!GetDuration(duration) || (duration == 0))
        duration = lengthM;
     return new cElvisRecordingInfoMenu(*urlM, *nameM, *descriptionM, *startTimeM, (unsigned int)duration, false);
     }
  return NULL;
}

eOSState cElvisReplayControl::ProcessKey(eKeys keyP)
{
  debug16("%s (%d)", __PRETTY_FUNCTION__, keyP);
  if (!Active()) {
     cRemote::CallPlugin(PLUGIN_NAME_I18N);
     cRemote::Put(k1);
     return osEnd;
     }

  if (visibleM) {
     if (timeoutShowM && (time(NULL) > timeoutShowM)) {
        Hide();
        ShowMode();
        timeoutShowM = 0;
        }
     else if (modeOnlyM)
        ShowMode();
     else
        shownM = ShowProgress(!shownM) || shownM;
     }

  if (timeSearchActiveM && (keyP != kNone)) {
     TimeSearchProcess(keyP);
     return osContinue;
     }

  switch (int(keyP)) {
    case kPlay:
    case kUp:
         Play();
         break;
    case kPause:
    case kDown:
         Pause();
         break;
    case kFastRew|k_Release:
    case kLeft|k_Release:
         if (Setup.MultiSpeedMode)
            break;
    case kFastRew:
    case kLeft:
         Backward();
         break;
    case kFastFwd|k_Release:
    case kRight|k_Release:
         if (Setup.MultiSpeedMode)
            break;
    case kFastFwd:
    case kRight:
         Forward();
         break;
    case kRed:
         TimeSearch();
         break;
    case kGreen|k_Repeat:
    case kGreen:
         SkipSeconds(-60);
         break;
    case kYellow|k_Repeat:
    case kYellow:
         SkipSeconds(60);
         break;
    case k1|k_Repeat:
    case k1:
         SkipSeconds(-20);
         break;
    case k3|k_Repeat:
    case k3:
         SkipSeconds(20);
         break;
    case k4|k_Repeat:
    case k4:
         SkipSeconds(-5 * 60);
         break;
    case k6|k_Repeat:
    case k6:
         SkipSeconds(5 * 60);
         break;
    case k7|k_Repeat:
    case k7:
         SkipSeconds(-15 * 60);
         break;
    case k9|k_Repeat:
    case k9:
         SkipSeconds(15 * 60);
         break;
    case kBack:
    case kStop:
    case kBlue:
         Hide();
         Stop();
         cRemote::CallPlugin(PLUGIN_NAME_I18N);
         cRemote::Put(k1);
         return osEnd;
    case kOk:
         if (visibleM && !modeOnlyM)
            Hide();
         else
            Show();
         break;
    default:
         break;
    }

  ShowMode();

  return osContinue;
}
