/*
 * fetch.c: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/remux.h>
#include <vdr/ringbuffer.h>
#include <vdr/skins.h>
#include <vdr/videodir.h>

#include "common.h"
#include "log.h"
#include "fetch.h"

// --- cElvisIndexGenerator --------------------------------------------

cElvisIndexGenerator::cElvisIndexGenerator(const char *recordingNameP)
: cThread("cElvisIndexGenerator"),
  recordingNameM(recordingNameP)
{
  debug1("%s (%s)", __PRETTY_FUNCTION__, recordingNameP);
  Start();
}

cElvisIndexGenerator::~cElvisIndexGenerator()
{
  debug1("%s", __PRETTY_FUNCTION__);
  Cancel(3);
}

void cElvisIndexGenerator::Action()
{
  debug1("%s Start", __PRETTY_FUNCTION__);
  bool IndexFileComplete = false;
  bool IndexFileWritten = false;
  bool Rewind = false;
  cFileName FileName(recordingNameM, false);
  cUnbufferedFile *ReplayFile = FileName.Open();
  cRingBufferLinear Buffer(eIfgBufferSize, MIN_TS_PACKETS_FOR_FRAME_DETECTOR * TS_SIZE);
  cPatPmtParser PatPmtParser;
  cFrameDetector FrameDetector;
  cIndexFile IndexFile(recordingNameM, true);
  int BufferChunks = KILOBYTE(1); // no need to read a lot at the beginning when parsing PAT/PMT
  off_t FileSize = 0;
  off_t FrameOffset = -1;
  while (Running()) {
        // Rewind input file:
        if (Rewind) {
           ReplayFile = FileName.SetOffset(1);
           Buffer.Clear();
           Rewind = false;
           }
        // Process data:
        int Length;
        uchar *Data = Buffer.Get(Length);
        if (Data) {
           if (FrameDetector.Synced()) {
              // Step 3 - generate the index:
              if (TsPid(Data) == PATPID)
                 FrameOffset = FileSize; // the PAT/PMT is at the beginning of an I-frame
              int Processed = FrameDetector.Analyze(Data, Length);
              if (Processed > 0) {
                 if (FrameDetector.NewFrame()) {
                    IndexFile.Write(FrameDetector.IndependentFrame(), FileName.Number(), FrameOffset >= 0 ? FrameOffset : FileSize);
                    FrameOffset = -1;
                    IndexFileWritten = true;
                    }
                 FileSize += Processed;
                 Buffer.Del(Processed);
                 }
              }
           else if (PatPmtParser.Vpid()) {
              // Step 2 - sync FrameDetector:
              int Processed = FrameDetector.Analyze(Data, Length);
              if (Processed > 0) {
                 if (FrameDetector.Synced()) {
                    // Synced FrameDetector, so rewind for actual processing:
                    Rewind = true;
                    }
                 Buffer.Del(Processed);
                 }
              }
           else {
              // Step 1 - parse PAT/PMT:
              uchar *p = Data;
              while (Length >= TS_SIZE) {
                    int Pid = TsPid(p);
                    if (Pid == PATPID)
                       PatPmtParser.ParsePat(p, TS_SIZE);
                    else if (PatPmtParser.IsPmtPid(Pid))
                       PatPmtParser.ParsePmt(p, TS_SIZE);
                    Length -= TS_SIZE;
                    p += TS_SIZE;
                    if (PatPmtParser.Vpid()) {
                       // Found Vpid, so rewind to sync FrameDetector:
                       FrameDetector.SetPid(PatPmtParser.Vpid(), PatPmtParser.Vtype());
                       BufferChunks = eIfgBufferSize;
                       Rewind = true;
                       break;
                       }
                    }
              Buffer.Del((int)(p - Data));
              }
           }
        // Read data:
        else if (ReplayFile) {
           int Result = Buffer.Read(ReplayFile, BufferChunks);
           if (Result == 0) { // EOF
              ReplayFile = FileName.NextFile();
              FileSize = 0;
              FrameOffset = -1;
              Buffer.Clear();
              }
           }
        // Recording has been processed:
        else {
           IndexFileComplete = true;
           break;
           }
        }
  // Delete the index file if the recording has not been processed entirely:
  if (!IndexFileComplete || !IndexFileWritten) {
     debug1("%s Delete index", __PRETTY_FUNCTION__);
     IndexFile.Delete();
     }
}

// --- cElvisFetchItem -------------------------------------------------

cElvisFetchItem::cElvisFetchItem(const char *urlP, const char *nameP, const char *descriptionP, const char *startTimeP, unsigned int lengthP)
: handleM(NULL),
  headerListM(NULL),
  urlM(urlP),
  nameM(nameP),
  descriptionM(descriptionP),
  startTimeM(startTimeP),
  lengthM(lengthP),
  dirNameM(""),
  fileNameM(NULL),
  recordFileM(NULL),
  indexGeneratorM(NULL),
  sizeM(0),
  fetchedM(0)
{
  debug1("%s (%s, %s, %s, %s, %u)", __PRETTY_FUNCTION__, urlP, nameP, descriptionP, startTimeP, lengthP);

  // create video file
  int year = 2010, mon = 1, day = 1, hour = 12, min = 0, sec = 0;
  sscanf(startTimeP, "%d.%d.%d %d:%d:%d", &day, &mon, &year, &hour, &min, &sec);
  char *name = strdup(nameP);
  dirNameM = cString::sprintf("%s/Elvis/%s/%4d-%02d-%02d.%02d.%02d.99-0.rec", cVideoDirectory::Name(), ExchangeChars(name, true), year, mon, day, hour, min);
  free(name);
  if (DirectoryOk(*dirNameM, false)) {
     error("%s (%s, %s, %s, %s, %u) Directory already exists dirname='%s'", __PRETTY_FUNCTION__, urlP, nameP, descriptionP, startTimeP, lengthP, *dirNameM);
     return;
     }
  if (!MakeDirs(*dirNameM, true)) {
     error("%s (%s, %s, %s, %s, %u) Cannot create dirname='%s'", __PRETTY_FUNCTION__, urlP, nameP, descriptionP, startTimeP, lengthP, *dirNameM);
     return;
     }
  fileNameM = new cFileName(*dirNameM, true);
  recordFileM = fileNameM->Open();
  if (!recordFileM) {
     error("%s (%s, %s, %s, %s, %u) Cannot open file='%s'", __PRETTY_FUNCTION__, urlP, nameP, descriptionP, startTimeP, lengthP, fileNameM->Name());
     return;
     }

  // create info file
  if (fileNameM) {
     cSafeFile f(*cString::sprintf("%s/info", *dirNameM));
     if (f.Open()) {
        char *name = strdup(nameP);
        char *desc = strdup(descriptionP);
        strreplace(name, '\n', ' ');
        strreplace(desc, '\n', ' ');
        fprintf(f, "T %s\n", name);
        fprintf(f, "S %s\n", desc);
        free(name);
        free(desc);
        f.Close();
        }
     }

  // setup curl interface
  handleM = curl_easy_init();
  if (handleM) {
     // verbose output
     curl_easy_setopt(handleM, CURLOPT_VERBOSE, 1L);
     curl_easy_setopt(handleM, CURLOPT_DEBUGFUNCTION, cElvisFetchItem::DebugCallback);
     curl_easy_setopt(handleM, CURLOPT_DEBUGDATA, this);

     // set callbacks
     curl_easy_setopt(handleM, CURLOPT_WRITEFUNCTION, cElvisFetchItem::WriteCallback);
     curl_easy_setopt(handleM, CURLOPT_WRITEDATA, this);
     curl_easy_setopt(handleM, CURLOPT_HEADERFUNCTION, cElvisFetchItem::HeaderCallback);
     curl_easy_setopt(handleM, CURLOPT_HEADERDATA, this);

     // no progress meter and no signaling
     curl_easy_setopt(handleM, CURLOPT_NOPROGRESS, 1L);
     curl_easy_setopt(handleM, CURLOPT_NOSIGNAL, 1L);

     // set user-agent
     curl_easy_setopt(handleM, CURLOPT_USERAGENT, *cString::sprintf("vdr-%s/%s", PLUGIN_NAME_I18N, VERSION));

     // follow location
     curl_easy_setopt(handleM, CURLOPT_FOLLOWLOCATION, 1L);

     // set url
     curl_easy_setopt(handleM, CURLOPT_URL, *urlM);

     // set additional headers to prevent caching
     headerListM = curl_slist_append(headerListM, "Cache-Control: no-store, no-cache, must-revalidate");
     headerListM = curl_slist_append(headerListM, "Cache-Control: post-check=0, pre-check=0");
     headerListM = curl_slist_append(headerListM, "Pragma: no-cache");
     headerListM = curl_slist_append(headerListM, "Expires: Mon, 26 Jul 1997 05:00:00 GMT");
     curl_easy_setopt(handleM, CURLOPT_HTTPHEADER, headerListM);
     }
}

cElvisFetchItem::~cElvisFetchItem()
{
  debug1("%s", __PRETTY_FUNCTION__);
  if (handleM) {
     // cleanup curl stuff
     curl_slist_free_all(headerListM);
     headerListM = NULL;
     curl_easy_cleanup(handleM);
     handleM = NULL;
     }
  DELETE_POINTER(fileNameM);
  DELETE_POINTER(indexGeneratorM);
}

int cElvisFetchItem::DebugCallback(CURL *handleP, curl_infotype typeP, char *dataP, size_t sizeP, void *userPtrP)
{
  cElvisFetchItem *obj = reinterpret_cast<cElvisFetchItem *>(userPtrP);

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

size_t cElvisFetchItem::WriteCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cElvisFetchItem *obj = reinterpret_cast<cElvisFetchItem *>(dataP);
  size_t len = sizeP * nmembP;

  if (obj)
     obj->WriteData((uchar *)ptrP, (int)len);

  return len;
}

size_t cElvisFetchItem::HeaderCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cElvisFetchItem *obj = reinterpret_cast<cElvisFetchItem *>(dataP);
  size_t len = sizeP * nmembP;

  if (obj && strstr((const char*)ptrP, "Content-Range:")) {
     unsigned long start, stop, size;
     if (sscanf((const char*)ptrP, "Content-Range: bytes %lu-%lu/%lu", &start, &stop, &size) == 3)
        obj->SetRange(start, stop, size);
     }

  return len;
}

void cElvisFetchItem::WriteData(uchar *dataP, int lenP)
{
  debug16("%s (, %d)", __PRETTY_FUNCTION__, lenP);

  fetchedM += lenP;
  if (recordFileM && recordFileM->Write(dataP, lenP) < 0)
     LOG_ERROR_STR(fileNameM->Name());
}

void cElvisFetchItem::SetRange(unsigned long startP, unsigned long stopP, unsigned long sizeP)
{
  debug16("%s (%ld, %ld, %ld)", __PRETTY_FUNCTION__, startP, stopP, sizeP);
  sizeM = sizeP;
}

void cElvisFetchItem::GenerateIndex()
{
  debug1("%s", __PRETTY_FUNCTION__);

  if (fileNameM)
     indexGeneratorM = new cElvisIndexGenerator(*dirNameM);
}

void cElvisFetchItem::Remove()
{
  debug1("%s recording='%s'", __PRETTY_FUNCTION__, fileNameM ? fileNameM->Name() : "");

  if (fileNameM) {
     info("%s Removing recording='%s'", __PRETTY_FUNCTION__, fileNameM->Name());
     cVideoDirectory::RemoveVideoFile(*dirNameM);
     }
}

int cElvisFetchItem::Progress()
{
  int progress = 0;

  if (sizeM > 0)
     progress = (int)ceil((double)fetchedM / (double)sizeM * 100.0);

  return progress;
}

// --- cElvisFetcher ---------------------------------------------------

cElvisFetcher *cElvisFetcher::instanceS = NULL;

cElvisFetcher *cElvisFetcher::GetInstance()
{
  if (!instanceS)
     instanceS = new cElvisFetcher();

  return instanceS;
}

void cElvisFetcher::Destroy()
{
  DELETE_POINTER(instanceS);
}

cElvisFetcher::cElvisFetcher()
: cThread("cElvisFetcher"),
  itemsM(),
  multiM(NULL)
{
  debug1("%s", __PRETTY_FUNCTION__);
  multiM = curl_multi_init();
  Start();
}

cElvisFetcher::~cElvisFetcher()
{
  debug1("%s", __PRETTY_FUNCTION__);
  Cancel(3);
  Abort();
  if (multiM) {
     curl_multi_cleanup(multiM);
     multiM = NULL;
     }
}

void cElvisFetcher::New(const char *urlP, const char *nameP, const char *descriptionP, const char *startTimeP, unsigned int lengthP)
{
  LOCK_THREAD;
  debug1("%s (%s, %s, %s, %s, %d)", __PRETTY_FUNCTION__, urlP, nameP, descriptionP, startTimeP, lengthP);

  bool found = false;
  for (int i = 0; i < itemsM.Size(); ++i) {
      cElvisFetchItem *item = itemsM[i];
      if (!strcmp(urlP, item->Url())) {
         found = true;
         break;
         }
      }
  if (!found) {
     cElvisFetchItem *item = new cElvisFetchItem(urlP, nameP, descriptionP, startTimeP, lengthP);
     if (item->Handle()) {
        itemsM.Append(item);
        // add handle into multi set
        if (multiM) {
           curl_multi_add_handle(multiM, item->Handle());
           Skins.Message(mtInfo, *cString::sprintf(tr("Fetching: %s"), nameP));
           }
        }
     else {
        DELETE_POINTER(item);
        Skins.Message(mtWarning, *cString::sprintf(tr("Fetching failed: %s"), nameP));
        }
     }
  else
     Skins.Message(mtInfo, *cString::sprintf(tr("Already fetching: %s"), nameP));
}

void cElvisFetcher::Remove(CURL *handleP, bool statusP)
{
  LOCK_THREAD;
  debug1("%s (, %d)", __PRETTY_FUNCTION__, statusP);

  int i = 0;
  bool found = false;
  for (i = 0; i < itemsM.Size(); ++i) {
      cElvisFetchItem *item = itemsM[i];
      if (handleP == item->Handle()) {
         found = true;
         break;
         }
      }
  if (found) {
     cElvisFetchItem *item = itemsM[i];
     if (item) {
        debug4("%s (, %d) name='%s'", __PRETTY_FUNCTION__, statusP, item->Name());
        // remove handle from multi set
        if (multiM && item->Handle())
           curl_multi_remove_handle(multiM, item->Handle());
        if (!statusP) {
           item->Remove();
           Skins.QueueMessage(mtInfo, *cString::sprintf(tr("Fetching failed: %s"), item->Name()));
           }
        else
           item->GenerateIndex();
        }
     }
}

bool cElvisFetcher::Cleanup()
{
  LOCK_THREAD;
  bool found = false;
  debug16("%s", __PRETTY_FUNCTION__);

  for (int i = 0; i < itemsM.Size(); ++i) {
      cElvisFetchItem *item = itemsM[i];
      if (item->Handle() && item->Ready()) {
         found = true;
         itemsM.Remove(i);
         debug4("%s name='%s'", __PRETTY_FUNCTION__, item->Name());
         Skins.QueueMessage(mtInfo, *cString::sprintf(tr("Fetched: %s"), item->Name()));
         DELETE_POINTER(item);
         }
      }

  return found;
}

void cElvisFetcher::Abort(int indexP)
{
  LOCK_THREAD;
  debug1("%s", __PRETTY_FUNCTION__);

  if (indexP < 0) {
     for (int i = 0; i < itemsM.Size(); ++i) {
         cElvisFetchItem *item = itemsM[i];
         itemsM.Remove(i);
         if (item) {
            // remove handle from multi set
            if (multiM && item->Handle())
               curl_multi_remove_handle(multiM, item->Handle());
            // remove recording
            item->Remove();
            DELETE_POINTER(item);
            }
         }
     }
  else if (indexP < itemsM.Size()) {
     cElvisFetchItem *item = itemsM[indexP];
     if (item) {
        // remove handle from multi set
        if (multiM && item->Handle())
           curl_multi_remove_handle(multiM, item->Handle());
        // remove recording
        item->Remove();
        DELETE_POINTER(item);
        }
     }
}

cString cElvisFetcher::List(int prefixP)
{
  LOCK_THREAD;
  cString list("");
  debug1("%s (%d)", __PRETTY_FUNCTION__, prefixP);

  if (itemsM.Size() > 0) {
     for (int i = 0; i < itemsM.Size(); ++i) {
         cElvisFetchItem *item = itemsM[i];
         if (item)
            list = cString::sprintf("%s\n%03d%c%d;%d;%s;%s", *list, prefixP, (i == itemsM.Size()) ? '-' : ' ', i, item->Progress(), item->Url(), item->Name());
         }
     }

  return list;
}

cElvisFetchItem *cElvisFetcher::Get(int indexP)
{
  LOCK_THREAD;
  debug16("%s (%d)", __PRETTY_FUNCTION__, indexP);
  if (indexP < itemsM.Size())
     return itemsM[indexP];
  return NULL;
}

void cElvisFetcher::Action()
{
  debug1("%s Start", __PRETTY_FUNCTION__);
  while (Running()) {
        CURLMcode err;
        int running_handles, maxfd;
        fd_set fdread, fdwrite, fdexcep;
        struct timeval timeout;

        Lock();
        do {
          err = curl_multi_perform(multiM, &running_handles);
        } while (err == CURLM_CALL_MULTI_PERFORM);
        Unlock();

        // check end of transfers
        if (running_handles == 0) {
           int msgcount;
           CURLMsg *msg = curl_multi_info_read(multiM, &msgcount);
           if (msg && (msg->msg == CURLMSG_DONE)) {
              debug1("%s Done", __PRETTY_FUNCTION__);
              Remove(msg->easy_handle, (msg->data.result == CURLE_OK));
              }
           }

        // cleanup ready made transfers
        if (Cleanup()) {
           debug1("%s Touch", __PRETTY_FUNCTION__);
           Recordings.ChangeState();
           Recordings.TouchUpdate();
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
}
