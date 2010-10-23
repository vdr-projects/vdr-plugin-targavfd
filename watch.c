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
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "watch.h"
#include "setup.h"
#include "ffont.h"

#include <vdr/tools.h>
#include <vdr/shutdown.h>

struct cMutexLooker {
  cMutex& mutex;
  cMutexLooker(cMutex& m):
  mutex(m){
    mutex.Lock();
  }
  virtual ~cMutexLooker() {
    mutex.Unlock();
  }
};


cVFDWatch::cVFDWatch()
: cThread("targaVFD: watch thread")
, m_bShutdown(false)
{
  m_nIconsForceOn = 0;
  m_nIconsForceOff = 0;
  m_nIconsForceMask = 0;

  unsigned int n;
  for(n=0;n<memberof(m_nCardIsRecording);++n) {
      m_nCardIsRecording[n] = 0;  
  }
  chPresentTime = 0;
  chFollowingTime = 0;
  chName = NULL;
  chPresentTitle = NULL;
  chPresentShortTitle = NULL;

  m_nLastVolume = cDevice::CurrentVolume();
  m_bVolumeMute = false;
  tsVolumeLast = time(NULL);

  osdTitle = NULL;
  osdItem = NULL;
  osdMessage = NULL;

  m_pControl = NULL;
  replayTitle = NULL;
  replayTitleLast = NULL;
  replayTime = NULL;

  currentTime = NULL;
  m_eWatchMode = eLiveTV;

  m_nScrollOffset = -1;
  m_bScrollBackward = false;
  m_bScrollNeeded = false;
}

cVFDWatch::~cVFDWatch()
{
  if(chName) { 
    delete chName;
    chName = NULL;
  }
  if(chPresentTitle) { 
    delete chPresentTitle;
    chPresentTitle = NULL;
  }
  if(chPresentShortTitle) { 
    delete chPresentShortTitle;
    chPresentShortTitle = NULL;
  }
  if(osdMessage) { 
    delete osdMessage;
    osdMessage = NULL;
  }
  if(osdTitle) { 
    delete osdTitle;
    osdTitle = NULL;
  }
  if(osdItem) { 
    delete osdItem;
    osdItem = NULL;
  }
  if(replayTitle) {
    delete replayTitle;
    replayTitle = NULL;
  }
  if(replayTitleLast) {
    delete replayTitleLast;
    replayTitleLast = NULL;
  }
  if(replayTime) {
    delete replayTime;
    replayTime = NULL;
  }
  if(currentTime) {
    delete currentTime;
    currentTime = NULL;
  }
}

bool cVFDWatch::open() {
  if(cVFD::open()) {
    m_bShutdown = false;
    m_bUpdateScreen = true;
    Start();
    return true;
  }
  return false;
}

void cVFDWatch::shutdown(int nExitMode) {

  if(Running()) {
    m_bShutdown = true;
    usleep(500000);
    Cancel();
  }

  if(this->isopen()) {
    cTimer* t = Timers.GetNextActiveTimer();    

    switch(nExitMode) {
      case eOnExitMode_NEXTTIMER:
      case eOnExitMode_NEXTTIMER_BLANKSCR: {
        isyslog("targaVFD: closing, show only next timer.");

        int nTop = (theSetup.m_cHeight - pFont->Height())/2;
        this->clear();
        if(t) {
          struct tm l;
          cString topic;
          time_t tn = time(NULL);
          time_t tt = t->StartTime();
          localtime_r(&tt, &l);
          if((tt - tn) > 86400) {
            // next timer more then 24h 
            topic = cString::sprintf("%d. %02d:%02d", l.tm_mday, l.tm_hour, l.tm_min);
          } else {
            // next timer (today)
            topic = cString::sprintf("%02d:%02d", l.tm_hour, l.tm_min);
          }

          int w = pFont->Width(topic);
          if(theSetup.m_nRenderMode == eRenderMode_DualLine) {
            this->DrawText(0,0,topic);
            if((w + 3) < theSetup.m_cWidth)
                this->DrawText(w + 3,0,t->Channel()->Name());
            this->DrawText(0,pFont->Height(), t->File());
          } else {
            this->DrawText(0,nTop<0?0:nTop, topic);
            if((w + 3) < theSetup.m_cWidth)
                this->DrawText(w + 3,nTop<0?0:nTop, t->File());
          }
          this->icons(eIconRECORD);
        } else {
          if(nExitMode == eOnExitMode_NEXTTIMER) 
            this->DrawText(0,nTop<0?0:nTop,tr("None active timer"));
          this->icons(0);
        }
        this->flush(true);
        break;
      }
      case eOnExitMode_SHOWMSG: {
        isyslog("targaVFD: closing, leaving \"last\" message.");
        break;
      } 
      default:
      case eOnExitMode_BLANKSCREEN: {
        isyslog("targaVFD: closing, turning backlight off.");
        SendCmdShutdown();
        break;
      } 
      case eOnExitMode_SHOWCLOCK: {
        isyslog("targaVFD: closing, showing clock.");
        icons(0);
        SendCmdClock();
        break;
      } 
    }
  }
  cVFD::close();
}

void cVFDWatch::Action(void)
{
  unsigned int nLastIcons = -1;
  int nBrightness = -1;

  unsigned int n;
  unsigned int nCnt = 0;

  cTimeMs runTime;
  struct tm tm_r;
  bool bLastSuspend = false;

  for (;!m_bShutdown;++nCnt) {
    
    LOCK_THREAD;

    unsigned int nIcons = 0;
    bool bFlush = false;
    bool bReDraw = false;
    bool bSuspend = false;
    if(m_bShutdown)
      break;
    else {
      cMutexLooker m(mutex);
      runTime.Set();

      time_t ts = time(NULL);
      if(theSetup.m_nSuspendMode != eSuspendMode_Never 
          && theSetup.m_nSuspendTimeOff != theSetup.m_nSuspendTimeOn) {
        struct tm *now = localtime_r(&ts, &tm_r);
        int clock = now->tm_hour * 100 + now->tm_min;
        if(theSetup.m_nSuspendTimeOff > theSetup.m_nSuspendTimeOn) { //like 8-20
          bSuspend = (clock >= theSetup.m_nSuspendTimeOn) 
                  && (clock <= theSetup.m_nSuspendTimeOff);
        } else { //like 0-8 and 20..24
          bSuspend = (clock >= theSetup.m_nSuspendTimeOn) 
                  || (clock <= theSetup.m_nSuspendTimeOff);
        }
        if(theSetup.m_nSuspendMode == eSuspendMode_Timed 
              && !ShutdownHandler.IsUserInactive()) {
          bSuspend = false;
        }
      }
      if(bSuspend != bLastSuspend) {
        bReDraw = true;
      }

      if(!bSuspend) { 
    	  // every second the clock need updates.
    	  if (theSetup.m_nRenderMode == eRenderMode_DualLine) {
          if((0 == (nCnt % 2))) {
            bReDraw = CurrentTime(ts);
            if(m_eWatchMode != eLiveTV) {
              bReDraw |= ReplayTime();
            }
          }
        }

        bFlush = RenderScreen(bReDraw);
        if(m_eWatchMode != eLiveTV) {
            switch(ReplayMode()) {
                case eReplayNone:
                case eReplayPaused:
                  nIcons |= eIconPAUSE;
                  break;
                default:
                case eReplayPlay:
                  nIcons |= eIconPLAY;
                  break;
                case eReplayBackward1:
                case eReplayBackward2:
                case eReplayBackward3:
                  break;
                case eReplayForward1:
                case eReplayForward2:
                case eReplayForward3:
                  break;
            }
        }

        for(n=0;n<memberof(m_nCardIsRecording);++n) {
            if(0 != m_nCardIsRecording[n]) {
              nIcons |= eIconRECORD;
              break;
            }
        }

        // update volume - bargraph or mute symbol
		    if(theSetup.m_nVolumeMode != eVolumeMode_ShowNever) { 
          if(m_bVolumeMute) {
            nIcons |= eIconMUTE;
          } else {
		        if(theSetup.m_nVolumeMode == eVolumeMode_ShowEver 
			        || ( theSetup.m_nVolumeMode == eVolumeMode_ShowTimed
				        && (ts - tsVolumeLast) < 15 )) { // if timed - delay 15 seconds
      		    nIcons |= eIconVOLUME;
      		    const int nVolSteps = (MAXVOLUME/14);
      		    nIcons |= (((1 << (m_nLastVolume / nVolSteps)) - 1) << 0x0B);
      			}
          }
        }
      }

      // Set Brightness if setup value changed or display set to suspend
      if(theSetup.m_nBrightness != nBrightness || 
         bSuspend != bLastSuspend) {
        nBrightness = theSetup.m_nBrightness;
        Brightness(bSuspend ? 0 : nBrightness);
        bLastSuspend = bSuspend;
        bFlush = true;
      }

      //Force icon state (defined by svdrp)
      nIcons &= ~(m_nIconsForceMask);
      nIcons |=  (m_nIconsForceOn);
      nIcons &= ~(m_nIconsForceOff);

      if(nIcons != nLastIcons) {
        icons(nIcons);
        nLastIcons = nIcons;
        bFlush = true;
      }

      if(bFlush) {
        flush(false);
      }
    }
    int nDelay = (bSuspend ? 1000 : 100) - runTime.Elapsed();
    if(nDelay <= 10) {
      nDelay = 10;
    }
    cCondWait::SleepMs(nDelay);
  }
  dsyslog("targaVFD: watch thread closed (pid=%d)", getpid());
}

bool cVFDWatch::RenderScreen(bool bReDraw) {
    cString* scRender;
    cString* scHeader = NULL;
    bool bForce = m_bUpdateScreen;
    bool bAllowCurrentTime = false;

    if(osdMessage) {
      scRender = osdMessage;
    } else if(osdItem) {
      scHeader = osdTitle;
      scRender = osdItem;
    } else if(m_eWatchMode == eLiveTV) {
        scHeader = chName;
        if(Program()) {
          bForce = true;
        }
        if(chPresentTitle && theSetup.m_nRenderMode != eRenderMode_SingleTopic) {
          scRender = chPresentTitle;
          bAllowCurrentTime = true;
        } else {
          scHeader = currentTime;
          scRender = chName;
        }
    } else {
        if(Replay()) {
          bForce = true;
        }
        scHeader = replayTime;
        scRender = replayTitle;
        bAllowCurrentTime = true;
    }


    if(bForce) {
      m_nScrollOffset = 0;
      m_bScrollBackward = false;
      m_bScrollNeeded = true;
    }
    if(bForce || bReDraw || m_nScrollOffset > 0 || m_bScrollBackward) {
      this->clear();
      if(scRender) {
    
        int iRet = -1;
        if(theSetup.m_nRenderMode == eRenderMode_DualLine) {
          iRet = this->DrawText(0 - m_nScrollOffset,pFont->Height(), *scRender);
        } else {
          int nTop = (theSetup.m_cHeight - pFont->Height())/2;
          iRet = this->DrawText(0 - m_nScrollOffset,nTop<0?0:nTop, *scRender);
        }
        if(m_bScrollNeeded) {
          switch(iRet) {
            case 0: 
              if(m_nScrollOffset <= 0) {
                m_nScrollOffset = 0;
                m_bScrollBackward = false;
                m_bScrollNeeded = false;
                break; //Fit to screen
              }
              m_bScrollBackward = true;
            case 2:
            case 1:
              if(m_bScrollBackward) m_nScrollOffset -= 2;
              else                  m_nScrollOffset += 2;
              if(m_nScrollOffset >= 0) {
                break;
              }
            case -1:
              m_nScrollOffset = 0;
              m_bScrollBackward = false;
              m_bScrollNeeded = false;
              break;
          }
        }
      }

      if(scHeader && theSetup.m_nRenderMode == eRenderMode_DualLine) {
        if(bAllowCurrentTime && currentTime) {
          int t = pFont->Width(*currentTime);
          int w = pFont->Width(*scHeader);
          if((w + t + 3) < theSetup.m_cWidth && t < theSetup.m_cWidth) {
            this->DrawText(theSetup.m_cWidth - t, 0, *currentTime);
          } 
        }
        this->DrawText(0, 0, *scHeader);
      }

      m_bUpdateScreen = false;
      return true;
    }
    return false;
}

bool cVFDWatch::CurrentTime(time_t ts) {

  if((ts / 60) != (tsCurrentLast / 60)) {

    if(currentTime)
      delete currentTime;

    tsCurrentLast = ts;
    currentTime = new cString(TimeString(ts));
    return currentTime != NULL;
  } 
  return false;
}

bool cVFDWatch::Replay() {
  
  if(!replayTitleLast 
      || !replayTitle 
      || strcmp(*replayTitleLast,*replayTitle)) {
    if(replayTitleLast) {
      delete replayTitleLast;
      replayTitleLast = NULL;
    }
    if(replayTitle) {
      replayTitleLast = new cString(*replayTitle);
    }
    return true;
  }
  return false;
}

void cVFDWatch::Replaying(const cControl * Control, const char * Name, const char *FileName, bool On)
{
    cMutexLooker m(mutex);
    m_bUpdateScreen = true;
    if (On)
    {
        m_pControl = (cControl *) Control;
        m_eWatchMode = eReplayNormal;
        if(replayTitle) {
          delete replayTitle;
          replayTitle = NULL;
        }
        if (Name && !isempty(Name))
        {
            int slen = strlen(Name);
            bool bFound = false;
            ///////////////////////////////////////////////////////////////////////
            //Looking for mp3/muggle-plugin replay : [LS] (444/666) title
            //
            if (slen > 6 &&
                *(Name+0)=='[' &&
                *(Name+3)==']' &&
                *(Name+5)=='(') {
                unsigned int i;
                for (i=6; *(Name + i) != '\0'; ++i) { //search for [xx] (xxxx) title
                    if (*(Name+i)==' ' && *(Name+i-1)==')') {
                        bFound = true;
                        break;
                    }
                }
                if (bFound) { //found mp3/muggle-plugin
                    if (strlen(Name+i) > 0) { //if name isn't empty, then copy
                        replayTitle = new cString(skipspace(Name + i));
                    }
                    m_eWatchMode = eReplayMusic;
                }
            }
            ///////////////////////////////////////////////////////////////////////
            //Looking for DVD-Plugin replay : 1/8 4/28,  de 2/5 ac3, no 0/7,  16:9, VOLUMENAME
            // cDvdPlayerControl::GetDisplayHeaderLine
            // titleinfo, audiolang, spulang, aspect, title
            if (!bFound && slen>7)
            {
                unsigned int i,n;
                for (n=0,i=0;*(Name+i) != '\0';++i)
                { //search volumelabel after 4*", " => xxx, xxx, xxx, xxx, title
                    if (*(Name+i)==' ' && *(Name+i-1)==',') {
                        if (++n == 4) {
                            bFound = true;
                            break;
                        }
                    }
                }
                if (bFound) //found DVD replay
                {
                    if (strlen(Name+i) > 0)
                    { // if name isn't empty, then copy
                        replayTitle = new cString(skipspace(Name + i));
                    }
                    m_eWatchMode = eReplayDVD;
                }
            }
            if (!bFound) {
                int i;
                for (i=slen-1;i>0;--i)
                {   //search reverse last Subtitle
                    // - filename contains '~' => subdirectory
                    // or filename contains '/' => subdirectory
                    switch (*(Name+i)) {
                        case '/': {
                            // look for file extentsion like .xxx or .xxxx
                            if (slen>5 && ((*(Name+slen-4) == '.') || (*(Name+slen-5) == '.')))
                            {
                                m_eWatchMode = eReplayFile;
                            }
                            else
                            {
                                break;
                            }
                        }
                        case '~': {
                            replayTitle = new cString(Name + i + 1);
                            bFound = true;
                            i = 0;
                        }
                        default:
                            break;
                    }
                }
            }

            if (0 == strncmp(Name,"[image] ",8)) {
                if (m_eWatchMode != eReplayFile) //if'nt already Name stripped-down as filename
                    replayTitle = new cString(Name + 8);
                m_eWatchMode = eReplayImage;
                bFound = true;
            }
            else if (0 == strncmp(Name,"[audiocd] ",10)) {
                replayTitle = new cString(Name + 10);
                m_eWatchMode = eReplayAudioCD;
                bFound = true;
            }
            if (!bFound) {
                replayTitle = new cString(Name);
            }
        }
        if (!replayTitle) {
            replayTitle = new cString(tr("Unknown title"));
        }
    }
    else
    {
      m_eWatchMode = eLiveTV;
      m_pControl = NULL;
    }
}


eReplayState cVFDWatch::ReplayMode() const
{
  bool Play = false, Forward = false;
  int Speed = -1;
	if (m_pControl
        && ((cControl *)m_pControl)->GetReplayMode(Play,Forward,Speed))
  {
    // 'Play' tells whether we are playing or pausing, 'Forward' tells whether
    // we are going forward or backward and 'Speed' is -1 if this is normal
    // play/pause mode, 0 if it is single speed fast/slow forward/back mode
    // and >0 if this is multi speed mode.
    switch(Speed) {
      default:
      case -1: 
        return Play ? eReplayPlay : eReplayPaused;
      case 0: 
      case 1: 
        return Forward ? eReplayForward1 : eReplayBackward1;
      case 2: 
        return Forward ? eReplayForward2 : eReplayBackward2;
      case 3: 
        return Forward ? eReplayForward3 : eReplayBackward3;
    }
  }
  return eReplayNone;
}

bool cVFDWatch::ReplayPosition(int &current, int &total) const
{
  if (m_pControl && ((cControl *)m_pControl)->GetIndex(current, total, false)) {
    total = (total == 0) ? 1 : total;
    return true;
  }
  return false;
}

const char * cVFDWatch::FormatReplayTime(int current, int total) const
{
    static char s[32];
#if VDRVERSNUM >= 10701
    int cs = (current / DEFAULTFRAMESPERSECOND);
    int ts = (total / DEFAULTFRAMESPERSECOND);
    bool g = (((current / DEFAULTFRAMESPERSECOND) / 3600) > 0)
            || (((total / DEFAULTFRAMESPERSECOND) / 3600) > 0);
#else
    int cs = (current / FRAMESPERSEC);
    int ts = (total / FRAMESPERSEC);
    bool g = (((current / FRAMESPERSEC) / 3600) > 0)
            || (((total / FRAMESPERSEC) / 3600) > 0);
#endif
    int cm = cs / 60;
    cs %= 60;
    int tm = ts / 60;
    ts %= 60;

    if (total > 1) {
      if(g) {
        snprintf(s, sizeof(s), "%s (%s)", (const char*)IndexToHMSF(current), (const char*)IndexToHMSF(total));
      } else {
        snprintf(s, sizeof(s), "%02d:%02d (%02d:%02d)", cm, cs, tm, ts);
      } 
    }
    else {
      if(g) {
        snprintf(s, sizeof(s), "%s", (const char*)IndexToHMSF(current));
      } else {
        snprintf(s, sizeof(s), "%02d:%02d", cm, cs);
      }
    }
    return s;
}

bool cVFDWatch::ReplayTime() {
    int current = 0,total = 0;
    if(ReplayPosition(current,total)) {
      const char * sz = FormatReplayTime(current,total);
      if(!replayTime || strcmp(sz,*replayTime)) {
        if(replayTime)
          delete replayTime;
        replayTime = new cString(sz);
        return replayTime != NULL;      
      }
    }
    return false;
}

void cVFDWatch::Recording(const cDevice *pDevice, const char *szName, const char *szFileName, bool bOn)
{
  cMutexLooker m(mutex);

  unsigned int nCardIndex = pDevice->CardIndex();
  if (nCardIndex > memberof(m_nCardIsRecording) - 1 )
    nCardIndex = memberof(m_nCardIsRecording)-1;

  if (nCardIndex < memberof(m_nCardIsRecording)) {
    if (bOn) {
      ++m_nCardIsRecording[nCardIndex];
    }
    else {
      if (m_nCardIsRecording[nCardIndex] > 0)
        --m_nCardIsRecording[nCardIndex];
    }
  }
  else {
    esyslog("targaVFD: Recording: only up to %d devices are supported by this plugin", memberof(m_nCardIsRecording));
  }
}

void cVFDWatch::Channel(int ChannelNumber)
{
    cMutexLooker m(mutex);
    if(chPresentTitle) { 
        delete chPresentTitle;
        chPresentTitle = NULL;
    }
    if(chPresentShortTitle) { 
        delete chPresentShortTitle;
        chPresentShortTitle = NULL;
    }
    if(chName) { 
        delete chName;
        chName = NULL;
    }

    cChannel * ch = Channels.GetByNumber(ChannelNumber);
    if(ch) {
      chID = ch->GetChannelID();
      chPresentTime = 0;
      chFollowingTime = 0;
      if (!isempty(ch->Name())) {
          chName = new cString(ch->Name());
      }
    }
    m_eWatchMode = eLiveTV;
    m_bUpdateScreen = true;
    m_nScrollOffset = 0;
    m_bScrollBackward = false;
}

bool cVFDWatch::Program() {

    bool bChanged = false;
    const cEvent * p = NULL;
    cSchedulesLock schedulesLock;
    const cSchedules * schedules = cSchedules::Schedules(schedulesLock);
    if (chID.Valid() && schedules)
    {
        const cSchedule * schedule = schedules->GetSchedule(chID);
        if (schedule)
        {
            if ((p = schedule->GetPresentEvent()) != NULL)
            {
                if(chPresentTime && chEventID == p->EventID()) {
                  return false;
                }

                bChanged  = true;
                chEventID = p->EventID();
                chPresentTime = p->StartTime();
                chFollowingTime = p->EndTime();

                if(chPresentTitle) { 
                    delete chPresentTitle;
                    chPresentTitle = NULL;
                }
                if (!isempty(p->Title())) {
                    chPresentTitle = new cString(p->Title());
                }

                if(chPresentShortTitle) { 
                    delete chPresentShortTitle;
                    chPresentShortTitle = NULL;
                }
                if (!isempty(p->ShortText())) {
                    chPresentShortTitle = new cString(p->ShortText());
                }
            }
        }
    }
    return bChanged;
}


void cVFDWatch::Volume(int nVolume, bool bAbsolute)
{
  cMutexLooker m(mutex);

  int nAbsVolume;

  nAbsVolume = m_nLastVolume;
  if (bAbsolute) {
      nAbsVolume = nVolume;
  } else {
      nAbsVolume += nVolume;
  }
  if(nAbsVolume > MAXVOLUME) {
      nAbsVolume = MAXVOLUME;
  }
  else if(nAbsVolume < 0) {
      nAbsVolume = 0;
  }

  if(m_nLastVolume > 0 && 0 == nAbsVolume) {
    m_bVolumeMute = true;
  }
  else if(0 == m_nLastVolume && nAbsVolume > 0) {
    m_bVolumeMute = false;
  }
  m_nLastVolume = nAbsVolume;
  tsVolumeLast = time(NULL);
}


void cVFDWatch::OsdClear() {
    cMutexLooker m(mutex);
    if(osdMessage) { 
        delete osdMessage;
        osdMessage = NULL;
        m_bUpdateScreen = true;
    }
    if(osdTitle) { 
        delete osdTitle;
        osdTitle = NULL;
        m_bUpdateScreen = true;
    }
    if(osdItem) { 
        delete osdItem;
        osdItem = NULL;
        m_bUpdateScreen = true;
    }
}

void cVFDWatch::OsdTitle(const char *sz) {
    char *s = NULL;
    char *sc = NULL;
    if(sz && !isempty(sz)) {
        s = strdup(sz);
        sc = compactspace(strreplace(s,'\t',' '));
    }
    if(sc 
        && osdTitle 
        && 0 == strcmp(sc, *osdTitle)) {
      if(s) {
        free(s);
      }
      return;
    }
    cMutexLooker m(mutex);
    if(osdTitle) { 
        delete osdTitle;
        osdTitle = NULL;
        m_bUpdateScreen = true;
    }
    if(sc) {
          osdTitle = new cString(sc);
          m_bUpdateScreen = true;
    }
    if(s) {
      free(s);
    }
}

void cVFDWatch::OsdCurrentItem(const char *sz)
{
    char *s = NULL;
    char *sc = NULL;
    if(sz && !isempty(sz)) {
        s = strdup(sz);
        sc = compactspace(strreplace(s,'\t',' '));
    }
    if(sc 
        && osdItem 
        && 0 == strcmp(sc, *osdItem)) {
      if(s) {
        free(s);
      }
      return;
    }
    cMutexLooker m(mutex);
    if(osdItem) { 
        delete osdItem;
        osdItem = NULL;
        m_bUpdateScreen = true;
    }
    if(sc) {
          osdItem = new cString(sc);
          m_bUpdateScreen = true;
    }
    if(s) {
      free(s);
    }
}

void cVFDWatch::OsdStatusMessage(const char *sz)
{
    char *s = NULL;
    char *sc = NULL;
    if(sz && !isempty(sz)) {
        s = strdup(sz);
        sc = compactspace(strreplace(s,'\t',' '));
    }
    if(sc 
        && osdMessage 
        && 0 == strcmp(sc, *osdMessage)) {
      if(s) {
        free(s);
      }
      return;
    }
    cMutexLooker m(mutex);
    if(osdMessage) { 
        delete osdMessage;
        osdMessage = NULL;
        m_bUpdateScreen = true;
    }
    if(sc) {
          osdMessage = new cString(sc);
          m_bUpdateScreen = true;
    }
    if(s) {
      free(s);
    }
}

bool cVFDWatch::SetFont(const char *szFont, bool bTwoLineMode, int nBigFontHeight, int nSmallFontHeight) {
    cMutexLooker m(mutex);
    if(cVFD::SetFont(szFont, bTwoLineMode, nBigFontHeight, nSmallFontHeight)) {
      m_bUpdateScreen = true;
      return true;
    }
    return false;
}

eIconState cVFDWatch::ForceIcon(unsigned int nIcon, eIconState nState) {

  unsigned int nIconOff = nIcon;

  switch(nState) {
    case eIconStateAuto:
      m_nIconsForceOn   &= ~(nIconOff);
      m_nIconsForceOff  &= ~(nIconOff);
      m_nIconsForceMask &= ~(nIconOff);
      break;
    case eIconStateOn:
      m_nIconsForceOn   |=   nIcon;
      m_nIconsForceOff  &= ~(nIconOff);
      m_nIconsForceMask |=   nIconOff;
      break;
    case eIconStateOff:
      m_nIconsForceOff  |=   nIcon;
      m_nIconsForceOn   &= ~(nIconOff);
      m_nIconsForceMask |=   nIconOff;
      break;
    default:
      break;
  }
  if(m_nIconsForceOn  & nIcon) return eIconStateOn;
  if(m_nIconsForceOff & nIcon) return eIconStateOff;
  return eIconStateAuto;
}

