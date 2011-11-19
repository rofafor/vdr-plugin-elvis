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
#include "fetch.h"

// --- cElvisIndexGenerator --------------------------------------------

cElvisIndexGenerator::cElvisIndexGenerator(const char *recordingNameP)
: cThread("cElvisIndexGenerator"),
  recordingNameM(recordingNameP)
{
  debug("cElvisIndexGenerator::cElvisIndexGenerator(): rec=%s", recordingNameP);
  Start();
}

cElvisIndexGenerator::~cElvisIndexGenerator()
{
  debug("cElvisIndexGenerator::~cElvisIndexGenerator()");
  Cancel(3);
}

void cElvisIndexGenerator::Action()
{
  debug("cElvisIndexGenerator::Action()");
  bool IndexFileComplete = false;
  bool Rewind = false;
  cFileName FileName(recordingNameM, false);
  cUnbufferedFile *ReplayFile = FileName.Open();
  cRingBufferLinear Buffer(KILOBYTE(100), 2 * TS_SIZE);
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
                    FrameDetector.Reset();
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
                    if (Pid == 0)
                       PatPmtParser.ParsePat(p, TS_SIZE);
                    else if (Pid == PatPmtParser.PmtPid())
                       PatPmtParser.ParsePmt(p, TS_SIZE);
                    Length -= TS_SIZE;
                    p += TS_SIZE;
                    if (PatPmtParser.Vpid()) {
                       // Found Vpid, so rewind to sync FrameDetector:
                       FrameDetector.SetPid(PatPmtParser.Vpid(), PatPmtParser.Vtype());
                       BufferChunks = KILOBYTE(100);
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
              }
           }
        // Recording has been processed:
        else {
           IndexFileComplete = true;
           break;
           }
        }
  // Delete the index file if the recording has not been processed entirely:
  if (!IndexFileComplete) {
     debug("cElvisIndexGenerator::Action(delindex)");
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
  debug("cElvisFetchItem::cElvisFetchItem(): url=%s", *urlM);

  // create video file
  int year = 2010, mon = 1, day = 1, hour = 12, min = 0, sec = 0;
  sscanf(startTimeP, "%d.%d.%d %d:%d:%d", &day, &mon, &year, &hour, &min, &sec);
  char *name = strdup(nameP);
  dirNameM = cString::sprintf("%s/Elvis/%s/%4d-%02d-%02d.%02d.%02d.99-0.rec", VideoDirectory, ExchangeChars(name, true), year, mon, day, hour, min);
  free(name);
  if (DirectoryOk(*dirNameM, false)) {
     error("cElvisFetchItem(): directory already exists: '%s'", *dirNameM);
     return;
     }
  if (!MakeDirs(*dirNameM, true)) {
     error("cElvisFetchItem(): cannot create directory: '%s'", *dirNameM);
     return;
     }
  fileNameM = new cFileName(*dirNameM, true);
  recordFileM = fileNameM->Open();
  if (!recordFileM) {
     error("cElvisFetchItem(): cannot open file: '%s'", fileNameM->Name());
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
#ifdef DEBUG
     // verbose output
     curl_easy_setopt(handleM, CURLOPT_VERBOSE, 1L);
#endif
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
  debug("cElvisFetchItem::~cElvisFetchItem()");
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

size_t cElvisFetchItem::WriteCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cElvisFetchItem *obj = (cElvisFetchItem *)dataP;
  size_t len = sizeP * nmembP;

  if (obj)
     obj->WriteData((uchar *)ptrP, (int)len);

  return len;
}

size_t cElvisFetchItem::HeaderCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cElvisFetchItem *obj = (cElvisFetchItem *)dataP;
  size_t len = sizeP * nmembP;

  if (obj && strstr((const char*)ptrP, "Content-Range:")) {
     unsigned long start, stop, size;
     if (sscanf((const char*)ptrP, "Content-Range: bytes %ld-%ld/%ld", &start, &stop, &size) == 3)
        obj->SetRange(start, stop, size);
     }

  return len;
}

void cElvisFetchItem::WriteData(uchar *dataP, int lenP)
{
  //debug("cElvisFetchItem::WriteData(%d)", lenP);

  fetchedM += lenP;
  if (recordFileM && recordFileM->Write(dataP, lenP) < 0)
     LOG_ERROR_STR(fileNameM->Name());
}

void cElvisFetchItem::SetRange(unsigned long startP, unsigned long stopP, unsigned long sizeP)
{
  //debug("cElvisFetchItem::SetRange(): start=%ld stop=%ld filesize=%ld", startP, stopP, sizeP);
  sizeM = sizeP;
}

void cElvisFetchItem::GenerateIndex()
{
  debug("cElvisFetchItem::GenerateIndex()");

  if (fileNameM)
     indexGeneratorM = new cElvisIndexGenerator(*dirNameM);
}

void cElvisFetchItem::Remove()
{
  debug("cElvisFetchItem::Remove(%s)", fileNameM ? fileNameM->Name() : "");

  if (fileNameM) {
     info("removing recording '%s'", fileNameM->Name());
     RemoveVideoFile(*dirNameM);
     }
}

int cElvisFetchItem::Progress()
{
  int progress = 0;

  if (sizeM > 0)
     progress = (int)ceil((double)fetchedM / sizeM * 100.0);

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
  debug("cElvisFetcher::cElvisFetcher()");
  multiM = curl_multi_init();
  Start();
}

cElvisFetcher::~cElvisFetcher()
{
  debug("cElvisFetcher::~cElvisFetcher()");
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
  debug("cElvisFetcher::New(): url='%s'", urlP);

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
  debug("cElvisFetcher::Remove(): status=%d", statusP);

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
        debug("cElvisFetcher::Remove(%d): name='%s'", statusP, item->Name());
        // remove handle from multi set
        if (multiM && item->Handle())
           curl_multi_remove_handle(multiM, item->Handle());
        if (!statusP) {
           item->Remove();
           Skins.Message(mtInfo, *cString::sprintf(tr("Fetching failed: %s"), item->Name()));
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
  //debug("cElvisFetcher::Cleanup()");

  for (int i = 0; i < itemsM.Size(); ++i) {
      cElvisFetchItem *item = itemsM[i];
      if (item->Handle() && item->Ready()) {
         found = true;
         itemsM.Remove(i);
         debug("cElvisFetcher::Cleanup(): name='%s'", item->Name());
         Skins.Message(mtInfo, *cString::sprintf(tr("Fetched: %s"), item->Name()));
         DELETE_POINTER(item);
         }
      }

  return found;
}

void cElvisFetcher::Abort(int indexP)
{
  LOCK_THREAD;
  debug("cElvisFetcher::Abort()");

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
  debug("cElvisFetcher::List()");

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
  //debug("cElvisFetcher::Get(%d)", indexP);
  if (indexP < itemsM.Size())
     return itemsM[indexP];
  return NULL;
}

void cElvisFetcher::Action()
{
  debug("cElvisFetcher::Action(): start");
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
              debug("cElvisFetcher::Action(done)");
              Remove(msg->easy_handle, (msg->data.result == CURLE_OK));
              }
           }

        // cleanup ready made transfers
        if (Cleanup()) {
           debug("cElvisFetcher::Action(touch)");
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
