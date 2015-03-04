/*
 * player.h: Elvis plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __ELVIS_PLAYER_H
#define __ELVIS_PLAYER_H

#include <curl/curl.h>
#include <curl/easy.h>

#include <vdr/player.h>
#include <vdr/ringbuffer.h>

// --- cElvisReader ----------------------------------------------------

class cElvisReader : public cThread {
private:
  enum {
    eTimeoutMs             = 10, // in milliseconds
    eMaxDownloadSpeedMBits = 18  // in megabits per second
  };
  static int DebugCallback(CURL *handleP, curl_infotype typeP, char *dataP, size_t sizeP, void *userPtrP);
  static size_t WriteCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP);
  static size_t HeaderCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP);
  const cString urlM;
  unsigned long rangeStartM;
  unsigned long rangeSizeM;
  unsigned long rangePendingM;
  unsigned long durationM;
  bool pauseToggledM;
  bool pausedM;
  bool eofM;
  CURL *handleM;
  CURLM *multiM;
  struct curl_slist *headerListM;
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
  void SetDuration(unsigned long durationP);
  bool PutData(uchar *dataP, int lenP);
  void DelData(int lenP);
  void ClearData();
  uchar *GetData(int *lenP);
  void Pause(bool onoffP);
  void JumpRequest(unsigned long startbyteP);
  unsigned long GetRangeStart() { return rangeStartM; }
  unsigned long GetRangeSize() { return rangeSizeM; }
  unsigned long GetDuration() { return durationM; }
};

// --- cElvisPlayer ----------------------------------------------------

class cElvisPlayer : public cPlayer, cThread {
private:
  enum {
    eTrickplayJumpBase  = 2,   // in seconds
    eTrickplayTimeoutMs = 750, // in milliseconds
    eEOFMark            = 15   // in seconds
  };
  enum ePlayModes {
    pmPlay,
    pmPause,
    pmSlow,
    pmFast
  };
  enum ePlayDirs {
    pdForward,
    pdBackward
  };
  ePlayModes playModeM;
  ePlayDirs playDirM;
  int trickSpeedM;
  int programIdM;
  unsigned long durationM;
  cElvisReader *readerM;
  unsigned long readSizeM;
  unsigned long fileSizeM;
  cRingBufferFrame *ringBufferM;
  cFrame *readFrameM;
  cFrame *playFrameM;
  cFrame *dropFrameM;
  bool IsEOF();
  void TrickSpeed(int incrementP);
  int GetForwardJumpPeriod();
  int GetBackwardJumpPeriod();
protected:
  virtual void Activate(bool onP);
  virtual void Action();
public:
  cElvisPlayer(int programIdP, const char *urlP);
  virtual ~cElvisPlayer();
  void Play();
  void Pause();
  void Clear();
  void Backward();
  void Forward();
  void SkipTime(long secondsP, bool relativeP = true, bool playP = true);
  bool Finished() { return !Active(); }
  unsigned long Total() { return durationM; }
  unsigned long Current() { return (readerM && readerM->GetRangeSize() && durationM) ? (readSizeM / (readerM->GetRangeSize() / durationM)) : 0; }
  unsigned int Progress() { return (readerM && readerM->GetRangeSize()) ? (unsigned int)((double)readSizeM / (double)readerM->GetRangeSize() * 100.0) : 0; }
  void ClearJump() { if (readerM) readerM->JumpRequest(0); }
  bool GetReplayMode(bool &playP, bool &forwardP, int &speedP);
};

// --- cElvisPlayerControl ---------------------------------------------

class cElvisPlayerControl : public cControl {
private:
  cElvisPlayer *playerM;
public:
  cElvisPlayerControl(int programIdP, const char *urlP);
  virtual ~cElvisPlayerControl();
  bool Active();
  void Stop();
  void Pause();
  void Play();
  void Forward();
  void Backward();
  void SkipSeconds(int secondsP);
  bool GetDuration(unsigned long &durationP);
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
  cElvisReplayControl(int programIdP, const char *urlP, const char *nameP, const char *descriptionP, const char *startTimeP, unsigned int lengthP);
  virtual ~cElvisReplayControl();
  void Stop();
  virtual cOsdObject *GetInfo();
  virtual eOSState ProcessKey(eKeys keyP);
  virtual void Show();
  virtual void Hide();
};

#endif // __ELVIS_PLAYER_H
