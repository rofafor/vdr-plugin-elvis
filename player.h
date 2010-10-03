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
    MODE_NORMAL              = 0,
    MODE_TRICKPLAY_FORWARD,
    MODE_TRICKPLAY_BACKWARD,
    MODE_PAUSE,
    TIMEOUT_TRICKPLAY_MS     = 3000
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
  void SkipSeconds(int secondsP);
  bool Finished() { return !Active(); }
  unsigned long Total() { return lengthM; }
  unsigned long Current() { return (readerM && readerM->GetRangeSize() && lengthM) ? (readSizeM / (readerM->GetRangeSize() / lengthM)) : 0; }
  unsigned int Progress() { return (readerM && readerM->GetRangeSize()) ? (unsigned int)((double)readSizeM / (double)readerM->GetRangeSize() * 100.0) : 0; }
  bool IsPaused() { return (modeM == MODE_PAUSE); }
  bool IsFastForwarding() { return (modeM == MODE_TRICKPLAY_FORWARD); }
  bool IsRewinding() { return (modeM == MODE_TRICKPLAY_BACKWARD); }
};

// --- cElvisPlayerControl ---------------------------------------------

class cElvisPlayerControl : public cControl {
private:
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
