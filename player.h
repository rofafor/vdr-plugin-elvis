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
    eTimeoutMs = 10
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
  void ClearData();
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
    eTrickplaySkipLength = 3,   // in seconds
    eTrickplayTimeoutMs  = 1000 // in milliseconds
  };
  enum eSpeedValues {
    spMaxOffset      = 3,  // offset of the maximum speed from normal speed in either direction
    spNormalIndex    = 4,  // index of the '1' entry in the speedS array
    spSkipLimitIndex = 6,  // index of entry when fastforwarding is done by skipping in the speedS array
    spMultiplier     = 12, // speed multiplier
    spClampValue     = 63  // max value for clamping
  };
  enum ePlayModes {
    pmPlay,
    pmPause,
    pmSlow,
    pmFast,
    pmStill
  };
  enum ePlayDirs {
    pdForward,
    pdBackward
  };
  static int speedS[];
  ePlayModes playModeM;
  ePlayDirs playDirM;
  int trickSpeedM;
  const unsigned long lengthM;
  cElvisReader *readerM;
  unsigned long readSizeM;
  cMutex mutexM;
  cRingBufferFrame *ringBufferM;
  cFrame *readFrameM;
  cFrame *playFrameM;
  cFrame *dropFrameM;
  void TrickSpeed(int incrementP);
protected:
  virtual void Activate(bool onP);
  virtual void Action();
public:
  cElvisPlayer(const char *urlP, unsigned long lengthP);
  virtual ~cElvisPlayer();
  void Play();
  void Pause();
  void Clear();
  void Backward();
  void Forward();
  void SkipTime(long secondsP, bool relativeP = true, bool playP = true);
  bool Finished() { return !Active(); }
  unsigned long Total() { return lengthM; }
  unsigned long Current() { return (readerM && readerM->GetRangeSize() && lengthM) ? (readSizeM / (readerM->GetRangeSize() / lengthM)) : 0; }
  unsigned int Progress() { return (readerM && readerM->GetRangeSize()) ? (unsigned int)((double)readSizeM / (double)readerM->GetRangeSize() * 100.0) : 0; }
  bool GetReplayMode(bool &playP, bool &forwardP, int &speedP);
};

// --- cElvisPlayerControl ---------------------------------------------

class cElvisPlayerControl : public cControl {
private:
  cElvisPlayer *playerM;
public:
  cElvisPlayerControl(const char *urlP, unsigned int lengthP);
  virtual ~cElvisPlayerControl();
  bool Active();
  void Stop();
  void Pause();
  void Play();
  void Forward();
  void Backward();
  void SkipSeconds(int secondsP);
  bool GetProgress(int &currentP, int &totalP);
  bool GetIndex(unsigned long &currentP, unsigned long &totalP);
  bool GetReplayMode(bool &playP, bool &forwardP, int &speedP);
  void Goto(int secondsP, bool playP = true);
};

// --- cElvisReplayControl ---------------------------------------------

class cElvisReplayControl : public cElvisPlayerControl {
private:
  enum {
    eModeTimeout       = 3,
    eStaySecondsOffEnd = 10
  };
  cSkinDisplayReplay *displayReplayM;
  cString urlM;
  cString nameM;
  cString descriptionM;
  cString startTimeM;
  unsigned int lengthM;
  bool visibleM;
  bool modeOnlyM;
  bool shownM;
  unsigned long lastCurrentM;
  unsigned long lastTotalM;
  bool lastPlayM;
  bool lastForwardM;
  int lastSpeedM;
  time_t timeoutShowM;
  bool timeSearchActiveM;
  bool timeSearchHideM;
  int timeSearchTimeM;
  int timeSearchPosM;
  void TimeSearchDisplay();
  void TimeSearchProcess(eKeys keyP);
  void TimeSearch();
  void ShowTimed(int secondsP = 0);
  void ShowMode();
  bool ShowProgress(bool initialP);
  cString SecondsToHMSF(unsigned long secondsP);
public:
  cElvisReplayControl(const char *urlP, const char *nameP, const char *descriptionP, const char *startTimeP, unsigned int lengthP);
  virtual ~cElvisReplayControl();
  void Stop();
  virtual cOsdObject *GetInfo();
  virtual eOSState ProcessKey(eKeys keyP);
  virtual void Show();
  virtual void Hide();
};

#endif // __ELVIS_PLAYER_H
