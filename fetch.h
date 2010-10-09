/*
 * fetch.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_FETCH_H
#define __ELVIS_FETCH_H

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include <vdr/thread.h>

// --- cElvisIndexGenerator --------------------------------------------

class cElvisIndexGenerator : public cThread {
private:
  cString recordingNameM;
protected:
  virtual void Action(void);
public:
  cElvisIndexGenerator(const char *recordingNameP);
  virtual ~cElvisIndexGenerator();
};

// --- cElvisFetchItem -------------------------------------------------

class cElvisFetchItem {
private:
  static size_t WriteCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP);
  CURL *handleM;
  struct curl_slist *headerListM;
  cString urlM;
  cString nameM;
  cString descriptionM;
  cString startTimeM;
  unsigned int lengthM;
  cString dirNameM;
  cFileName *fileNameM;
  cUnbufferedFile *recordFileM;
  cElvisIndexGenerator *indexGeneratorM;
  void WriteData(uchar *dataP, int lenP);
public:
  cElvisFetchItem(const char *urlP, const char *nameP, const char *descriptionP, const char *startTimeP, unsigned int lengthP);
  virtual ~cElvisFetchItem();
  void GenerateIndex();
  void Remove();
  CURL *Handle() { return handleM; }
  const char *Url() { return *urlM; }
  const char *Name() { return *nameM; }
  const char *Description() { return *descriptionM; }
  unsigned int Length() { return lengthM; }
  bool Ready() { return (indexGeneratorM && !indexGeneratorM->Active()); }
};

// --- cElvisFetcher ---------------------------------------------------

class cElvisFetcher : public cThread {
private:
  enum {
    eTimeoutMs = 10
  };
  static cElvisFetcher *instanceS;
  cMutex mutexM;
  cVector<cElvisFetchItem *> itemsM;
  CURLM *multiM;
  // constructor
  cElvisFetcher();
  // to prevent copy constructor and assignment
  cElvisFetcher(const cElvisFetcher&);
  cElvisFetcher& operator=(const cElvisFetcher&);
  void Remove(CURL *handleP, bool statusP);
  bool Cleanup();
protected:
  virtual void Action();
public:
  static cElvisFetcher *GetInstance();
  static void Destroy();
  virtual ~cElvisFetcher();
  void New(const char *urlP, const char *nameP, const char *descriptionP, const char *startTimeP, unsigned int lengthP);
  void Abort();
  cString List(int prefixP = 900);
  unsigned int FetchCount() { return itemsM.Size(); }
  bool Fetching() { return (itemsM.Size() > 0); }
};

#endif // __ELVIS_FETCH_H
