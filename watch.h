/*
 * targavfd plugin for VDR (C++)
 *
 * (C) 2010 Andreas Brachold <vdr07 AT deltab de>
 *
 * This targavfd plugin is free software: you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as published 
 * by the Free Software Foundation, version 3 of the License.
 *
 * See the files README and COPYING for details.
 *
 */

#ifndef __VFD_WATCH_H
#define __VFD_WATCH_H

#include <vdr/thread.h>
#include <vdr/status.h>
#include "vfd.h"

enum eWatchMode {
    eUndefined,
    eLiveTV,
    eReplayNormal,
    eReplayMusic,
    eReplayDVD,
    eReplayFile,
    eReplayImage,
    eReplayAudioCD
};

enum eReplayState {
    eReplayNone,
  	eReplayPlay,
  	eReplayPaused,
  	eReplayForward1,
  	eReplayForward2,
  	eReplayForward3,
  	eReplayBackward1,
  	eReplayBackward2,
  	eReplayBackward3
};

enum eIconState { 
  eIconStateQuery, 
  eIconStateOn, 
  eIconStateOff, 
  eIconStateAuto 
};


class cVFDWatch
 : public  cVFD
 , protected cThread {
private:
  cMutex mutex;

  volatile bool m_bShutdown;

  eWatchMode m_eWatchMode;

  int   m_nScrollOffset;
  bool  m_bScrollBackward;
  bool  m_bScrollNeeded;
  bool  m_bUpdateScreen;

  int   m_nCardIsRecording[MAXDEVICES];

  unsigned int m_nIconsForceOn;
  unsigned int m_nIconsForceOff;
  unsigned int m_nIconsForceMask;

  const cControl *m_pControl;

  tChannelID  chID;
  tEventID    chEventID;
  time_t      chPresentTime;
  time_t      chFollowingTime;
  cString*    chName;
  cString*    chPresentTitle;
  cString*    chPresentShortTitle;

  int   m_nLastVolume;
  bool  m_bVolumeMute;
  time_t  tsVolumeLast;


  cString*    osdTitle;
  cString*    osdItem;
  cString*    osdMessage;

  cString* replayTitle;
  cString* replayTitleLast;
  cString* replayTime;

  time_t   tsCurrentLast;
  cString* currentTime;
protected:
  virtual void Action(void);
  bool Program();
  bool Replay();
  bool RenderScreen(bool bReDraw);
  eReplayState ReplayMode() const;
  bool ReplayPosition(int &current, int &total) const;
  bool CurrentTime(time_t ts);
  bool ReplayTime();
  const char * FormatReplayTime(int current, int total) const;
public:
  cVFDWatch();
  virtual ~cVFDWatch();

  virtual bool open();
  virtual void close ();

  void Replaying(const cControl *pControl, const char *szName, const char *szFileName, bool bOn);
  void Recording(const cDevice *pDevice, const char *szName, const char *szFileName, bool bOn);
  void Channel(int nChannelNumber);
  void Volume(int nVolume, bool bAbsolute);

  void OsdClear();
  void OsdTitle(const char *sz);
  void OsdCurrentItem(const char *sz);
  void OsdStatusMessage(const char *sz);

  virtual bool SetFont(const char *szFont, bool bTwoLineMode, int nBigFontHeight, int nSmallFontHeight);

  eIconState ForceIcon(unsigned int nIcon, eIconState nState);
};

#endif

