/*
 * player.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_PLAYER_H
#define __ELVIS_PLAYER_H

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include <vdr/player.h>
#include <vdr/ringbuffer.h>

// --- cElvisReader ----------------------------------------------------

class cElvisReader : public cThread {
private:
  enum {
    TIMEOUT_MS               = 10
  };
  static size_t WriteCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP);
  static size_t HeaderCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP);
  const cString urlM;
  unsigned long rangeStartM;
  unsigned long rangeSizeM;
  unsigned long rangePendingM;
  bool pauseToggledM;
  bool pausedM;
  bool eofM;
  CURL *handleM;
  CURLM *multiM;
  struct curl_slist *headerListM;
  cMutex mutexM;
  cRingBufferLinear *ringBufferM;
  bool Connect();
  bool Disconnect();
  void Retry();
  void Jump(unsigned long startbyteP);
protected:
  virtual void Action();
public:
  cElvisReader(const char *urlP);
  virtual ~cElvisReader();
  void SetRange(unsigned long startP, unsigned long stopP, unsigned long sizeP);
  bool PutData(uchar *dataP, int lenP);
  void DelData(int lenP);
  uchar *GetData(int *lenP);
  void Pause(bool onoffP);
  void JumpRequest(unsigned long startbyteP);
  unsigned long GetRangeStart() { return rangeStartM; }
  unsigned long GetRangeSize() { return rangeSizeM; }
};

// --- cElvisPlayer ----------------------------------------------------

class cElvisPlayer : public cPlayer, cThread {
private:
  enum {
    MODE_PLAY            = 0,
    MODE_PAUSE           = 1,
    MODE_FFWD            = 2,
    MODE_REW             = 3,
    MODE_SFWD            = 4,
    MODE_SREW            = 5,

    TRICKPLAY_TIMEOUT_MS = 1000L,
    TRICKPLAY_SKIP_MS    = 1000L
  };
  const unsigned long lengthM;
  cElvisReader *readerM;
  unsigned long readSizeM;
  unsigned int modeM;
  cMutex mutexM;
  cRingBufferFrame *ringBufferM;
  cFrame *readFrameM;
  cFrame *playFrameM;
  cFrame *dropFrameM;
protected:
  virtual void Activate(bool onP);
  virtual void Action();
public:
  cElvisPlayer(const char *urlP, unsigned long lengthP);
  virtual ~cElvisPlayer();
  void Play();
  void Pause();
  void Stop();
  void Backward();
  void Forward();
  void SkipTime(long milliSecondsP);
  bool Finished() { return !Active(); }
  unsigned long Total() { return lengthM; }
  unsigned long Current() { return (readerM && readerM->GetRangeSize() && lengthM) ? (readSizeM / (readerM->GetRangeSize() / lengthM)) : 0; }
  unsigned int Progress() { return (readerM && readerM->GetRangeSize()) ? (unsigned int)((double)readSizeM / (double)readerM->GetRangeSize() * 100.0) : 0; }
  bool GetReplayMode(bool &playP, bool &forwardP, int &speedP);
};

// --- cElvisPlayerControl ---------------------------------------------

class cElvisPlayerControl : public cControl {
private:
  enum {
    TIME_20_SECONDS_MS   = 20000L,
    TIME_1_MINUTE_MS     = 60000L,
    TIME_5_MINUTES_MS    = 300000L,
    TIME_15_MINUTES_MS   = 900000L,
  };
  cElvisPlayer *playerM;
  cSkinDisplayReplay *displayM;
  cString urlM;
  cString nameM;
  cString descriptionM;
  cString startTimeM;
  unsigned int lengthM;
public:
  cElvisPlayerControl(const char *urlP, const char *nameP, const char *descriptionP, const char *startTimeP, unsigned int lengthP);
  virtual ~cElvisPlayerControl();
  virtual void Show();
  virtual void Hide();
  virtual eOSState ProcessKey(eKeys Key);
  virtual cOsdObject *GetInfo();
};

#endif // __ELVIS_PLAYER_H
