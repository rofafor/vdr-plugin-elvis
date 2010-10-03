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
                 debug("cElvisReader::Action(done): %s", curl_easy_strerror(msg->data.result));
                 if (msg->data.result == CURLE_PARTIAL_FILE)
                    Retry();
                 else {
                    eofM = true;
                    break;
                    }
                 }
              }

           timeout.tv_sec  = 0;
           timeout.tv_usec = TIMEOUT_MS * 1000;
           FD_ZERO(&fdread);
           FD_ZERO(&fdwrite);
           FD_ZERO(&fdexcep);
           err = curl_multi_fdset(multiM, &fdread, &fdwrite, &fdexcep, &maxfd);
           if (maxfd >= 0)
              select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
           else
              cCondWait::SleepMs(TIMEOUT_MS);
           }
     Disconnect();
     }
}

// --- cElvisPlayer ----------------------------------------------------

cElvisPlayer::cElvisPlayer(const char *urlP, unsigned long lengthP)
: cThread("elvisplayer"),
  lengthM((lengthP + 1 + 5) * 60), // remember to add start & stop marginals
  readerM(new cElvisReader(urlP)),
  readSizeM(0),
  modeM(MODE_NORMAL),
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

void cElvisPlayer::Play()
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisPlayer::Play()");
  if (modeM != MODE_NORMAL) {
     modeM = MODE_NORMAL;
     DevicePlay();
     }
  if (readerM)
     readerM->Pause(false);
}

void cElvisPlayer::Pause()
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisPlayer::Pause()");
  if (modeM == MODE_PAUSE) {
     modeM = MODE_NORMAL;
     DevicePlay();
     }
  else {
     modeM = MODE_PAUSE;
     DeviceFreeze();
     }
  if (readerM)
     readerM->Pause((modeM == MODE_PAUSE));
}

void cElvisPlayer::Stop()
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisPlayer::Stop()");
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
  if ((modeM != MODE_NORMAL) || (modeM != MODE_TRICKPLAY_FORWARD) || (modeM != MODE_TRICKPLAY_BACKWARD))
     DevicePlay();
  modeM = MODE_TRICKPLAY_FORWARD;
}

void cElvisPlayer::Backward()
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisPlayer::Backward()");
  if ((modeM != MODE_NORMAL) || (modeM != MODE_TRICKPLAY_FORWARD) || (modeM != MODE_TRICKPLAY_BACKWARD))
     DevicePlay();
  modeM = MODE_TRICKPLAY_BACKWARD;
}

void cElvisPlayer::SkipSeconds(int secondsP)
{
  cMutexLock MutexLock(&mutexM);
  debug("cElvisPlayer::SkipSeconds(%d)", secondsP);
  unsigned long filesize = readerM ? readerM->GetRangeSize() : 0;
  long skip = lengthM ? secondsP * (filesize / lengthM) : 0;
  if ((skip < 0) && (readSizeM < (unsigned long)labs(skip)))
     readSizeM = 0;
  else if ((readSizeM + skip) > filesize)
     return;
  else
     readSizeM += skip;
  if (readerM)
     readerM->JumpRequest(readSizeM);
  playFrameM = NULL;
  dropFrameM = NULL;
  DELETE_POINTER(readFrameM);
  ringBufferM->Clear();
  DeviceClear();
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
           if (modeM == MODE_PAUSE) {
              sleep = true;
              continue;
              }
           {
             LOCK_THREAD;

             if (!readFrameM) {
                if (modeM == MODE_TRICKPLAY_FORWARD) {
                   if (timeout.TimedOut()) {
                      timeout.Set(TIMEOUT_TRICKPLAY_MS);
                      SkipSeconds(1);
                      }
                   }
                else if (modeM == MODE_TRICKPLAY_BACKWARD) {
                   if (timeout.TimedOut()) {
                      timeout.Set(TIMEOUT_TRICKPLAY_MS);
                      SkipSeconds(-1);
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
     playerM->Stop();
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
    case kFastRew:
    case kLeft:
         if (playerM)
            playerM->Backward();
         break;
    case kFastFwd|k_Release:
    case kRight|k_Release:
    case kFastFwd:
    case kRight:
         if (playerM)
            playerM->Forward();
         break;
    case kGreen|k_Repeat:
    case kGreen:
         if (playerM)
            playerM->SkipSeconds(-60);
         break;
    case kYellow|k_Repeat:
    case kYellow:
         if (playerM)
            playerM->SkipSeconds(60);
         break;
    case k1|k_Repeat:
    case k1:
         if (playerM)
            playerM->SkipSeconds(-20);
         break;
    case k3|k_Repeat:
    case k3:
         if (playerM)
            playerM->SkipSeconds(20);
         break;
    case k4|k_Repeat:
    case k4:
         if (playerM)
            playerM->SkipSeconds(-300);
         break;
    case k6|k_Repeat:
    case k6:
         if (playerM)
            playerM->SkipSeconds(300);
         break;
    case kStop:
    case kBlue:
    case kBack:
         Hide();
         if (playerM)
            playerM->Stop();
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
        displayM->SetCurrent(playerM ? *cString::sprintf("%ld:%02ld:%02ld", playerM->Current() / 3600, (playerM->Current() % 3600) / 60, playerM->Current() % 60) : "0:00:00");
        displayM->SetTotal(playerM ? *cString::sprintf("%ld:%02ld:%02ld", playerM->Total() / 3600, (playerM->Total() % 3600) / 60, playerM->Total() % 60) : "0:00:00");
        displayM->SetProgress(playerM ? playerM->Progress() : 0, 100);
        if (playerM->IsPaused())
           displayM->SetMode(false, true, -1);
        else if (playerM->IsFastForwarding() || playerM->IsRewinding())
           displayM->SetMode(true, playerM->IsFastForwarding(), 0);
        else
           displayM->SetMode(true, true, -1);
    }

  return state;
}
