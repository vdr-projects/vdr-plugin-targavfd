/*
 * targavfd plugin for VDR (C++)
 *
 * (C) 2010-2013 Andreas Brachold <vdr07 AT deltab de>
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
#include <ctype.h>

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
  replayFolder = NULL;
  replayTitle = NULL;
  replayTitleLast = NULL;
  replayTime = NULL;

  currentTime = NULL;
  m_eWatchMode = eLiveTV;
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
  if(replayFolder) {
    delete replayFolder;
    replayFolder = NULL;
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
  unsigned int nPage = 0;
  unsigned int nMaxPages = 0;
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
      cMutexLooker m(m_Mutex);
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
          // every 300ms the clock need updates.
          if((0 == (nCnt % 3))) {
            bReDraw = ( theSetup.m_nRenderMode == eRenderMode_MultiPage )
                       ? CurrentTimeHMS(ts)
                       : CurrentTimeHM(ts);
            if(m_eWatchMode != eLiveTV) {
               bReDraw |= ReplayTime();
            } else {
               m_nReplayCurrent = ts - chPresentTime;
               m_nReplayTotal = chFollowingTime - chPresentTime;
            }
        }

        switch(theSetup.m_nRenderMode) {
          case eRenderMode_SingleLine:
          case eRenderMode_DualLine:
          case eRenderMode_SingleTopic:
            bFlush = RenderScreenSinglePage(bReDraw);
            break;
          case eRenderMode_MultiPage:
            // every 15s the Pages should rotated.
            if(nMaxPages && (0 == (nCnt % 150))) {
              nPage ++;
              nPage %= nMaxPages;
              m_bUpdateScreen = true;
            }
            bFlush = RenderScreenPages(bReDraw, nPage, nMaxPages);
            break;
        }

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
        if(m_bVolumeMute) {
          nIcons |= eIconMUTE;
        }
        switch(theSetup.m_nVolumeMode)
        {
            case eVolumeMode_ShowTimed: {
              if((ts - tsVolumeLast) > 15 )
                break;
            }
            case eVolumeMode_ShowEver: {
              const int nVolSteps = (MAXVOLUME/14);
              nIcons |= eIconVOLUME;
              nIcons |= (((1 << (m_nLastVolume / nVolSteps)) - 1) << 0x0B);
              break;
            }
            case eVolumeMode_Progress: {
              const int nBarSteps = (m_nReplayTotal/14);
              if(nBarSteps > 0) {
                nIcons |= (((1 << (m_nReplayCurrent / nBarSteps)) - 1) << 0x0B);
              }
              break;
            }
            case eVolumeMode_ShowNever:
            default :
             break;
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

bool cVFDWatch::RenderScreenSinglePage(bool bReDraw) {
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
        if(RenderSpectrumAnalyzer())
          return true;

        if(Replay()) {
          bForce = true;
        }
        scHeader = replayTime;
        scRender = replayTitle;
        bAllowCurrentTime = true;
    }


    if(bForce) {
      this->RestartScrolled();
    }
    if(bForce || bReDraw || this->NeedScrolled()) {
      this->clear();
      if(scRender) {
        if(theSetup.m_nRenderMode == eRenderMode_DualLine) {
          this->DrawTextScrolled(0,pFont->Height(), *scRender, false);
        } else {
          int nTop = (theSetup.m_cHeight - pFont->Height())/2;
          this->DrawTextScrolled(0,nTop<0?0:nTop, *scRender, false);
        }
      }

      if(scHeader && theSetup.m_nRenderMode == eRenderMode_DualLine) {
        int t = 0;
        if(bAllowCurrentTime && currentTime) {
          t = pFont->Width(*currentTime);
          this->DrawText(theSetup.m_cWidth - t, 0, *currentTime);
          t += 1;
        }
        this->DrawTextEclipsed(0, 0, *scHeader, theSetup.m_cWidth - t);
      }

      m_bUpdateScreen = false;
      return true;
    }
    return false;
}

bool cVFDWatch::RenderScreenPages(bool bReDraw, unsigned int& nPage, unsigned int& nMaxPages) {

    bool bForce = m_bUpdateScreen;

    if(osdMessage) {
      nMaxPages = 1;
      return RenderText(bForce, bReDraw, osdMessage);
    } else if(osdItem) {
      nMaxPages = 2;
      switch(nPage % nMaxPages) {
        case 0: return RenderText(bForce, bReDraw, osdItem);
        case 1: return RenderText(bForce, bReDraw, osdTitle);
      }
    } else if(m_eWatchMode == eLiveTV) {
      nMaxPages = 4;
      if(Program()) {
        // New program
        nPage = 3;
        bForce = true;
      }
      // Skip none present items
      if((nPage % nMaxPages) == 0 && !chPresentTitle) nPage++;
      if((nPage % nMaxPages) == 1 && !chPresentShortTitle) nPage++;

      switch(nPage % nMaxPages) {
        case 0: return RenderText(bForce, bReDraw, chPresentTitle);
        case 1: return RenderText(bForce, bReDraw, chPresentShortTitle);
        case 2: return RenderText(bForce, bReDraw, currentTime);
        case 3: return RenderText(bForce, bReDraw, chName);
      }

    } else {
      nMaxPages = 4;
      if(Replay()) {
        // New replay
        nPage = 0; 
        bForce = true;
      }
      // Skip none present items
      if((nPage % nMaxPages) == 0 && !replayFolder) nPage++;
      if((nPage % nMaxPages) == 1 && !replayTitle) nPage++;

      switch(nPage % nMaxPages) {
        case 0: return RenderText(bForce, bReDraw, replayFolder);
        case 1: return RenderText(bForce, bReDraw, replayTitle);
        case 2: return RenderText(bForce, bReDraw, replayTime ? replayTime : currentTime);
        case 3: 
            if(!RenderSpectrumAnalyzer())
                nPage++; //no span service present
            return true;
      }
    }
    return false;
}

bool cVFDWatch::RenderText(bool bForce, bool bReDraw, cString* scText) {

    if(bForce) {
      this->RestartScrolled();
    }
    if(bForce || bReDraw || this->NeedScrolled()) {
      this->clear();
      if(scText) {
        int nTop = (theSetup.m_cHeight - pFont->Height())/2;
        this->DrawTextScrolled(0,nTop<0?0:nTop,*scText,true);
      }

      m_bUpdateScreen = false;
      return true;
    }
    return false;
}

bool cVFDWatch::CurrentTimeHM(time_t ts) {

  if((ts / 60) != (tsCurrentLast / 60)) {

    if(currentTime)
      delete currentTime;

    tsCurrentLast = ts;
    char buf[25];
    struct tm tm_r;
    strftime(buf, sizeof(buf), "%R", localtime_r(&ts, &tm_r));
    currentTime = new cString(buf);

    return currentTime != NULL;
  } 
  return false;
}

bool cVFDWatch::CurrentTimeHMS(time_t ts) {

  if(ts != tsCurrentLast) {

    if(currentTime)
      delete currentTime;

    tsCurrentLast = ts;
    char buf[25];
    struct tm tm_r;
    strftime(buf, sizeof(buf), "%T", localtime_r(&ts, &tm_r));
    currentTime = new cString(buf);

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

char *striptitle(char *s)
{
  if (s && *s) {
     for (char *p = s + strlen(s) - 1; p >= s; p--) {
         if (isspace(*p) || *p == '~' || *p == '\\')
           *p = 0;
         else
            break;
         }
     }
  return s;
}

void cVFDWatch::Replaying(const cControl * Control, const char * szName, const char *FileName, bool On)
{
    cMutexLooker m(m_Mutex);
    m_bUpdateScreen = true;
    if (On)
    {
        m_pControl = (cControl *) Control;
        m_eWatchMode = eReplay;
        if(replayFolder) {
          delete replayFolder;
          replayFolder = NULL;
        }
        if(replayTitle) {
          delete replayTitle;
          replayTitle = NULL;
        }
        if (szName && !isempty(szName))
        {
            char* Title = NULL;
            char* Name = strdup(skipspace(szName));
            striptitle(Name); // remove space at end
            int slen = strlen(Name);
            ///////////////////////////////////////////////////////////////////////
            //Looking for mp3/muggle-plugin replay : [LS] (444/666) title
            //
            if (slen > 6 &&
                *(Name+0)=='[' &&
                *(Name+3)==']' &&
                *(Name+5)=='(') {
                int i;
                for (i=6; *(Name + i) != '\0'; ++i) { //search for [xx] (xxxx) title
                    if (*(Name+i)==' ' && *(Name+i-1)==')') {
                        if (slen > i) { //if name isn't empty, then copy
                            Title = (Name + i);
                        }
                        break;
                    }
                }
            }
            ///////////////////////////////////////////////////////////////////////
            //Looking for DVD-Plugin replay : 1/8 4/28,  de 2/5 ac3, no 0/7,  16:9, VOLUMENAME
            // cDvdPlayerControl::GetDisplayHeaderLine : titleinfo, audiolang, spulang, aspect, title
            if (!Title && slen>7)
            {
                int i,n;
                for (n=0,i=0;*(Name+i) != '\0';++i)
                { //search volumelabel after 4*", " => xxx, xxx, xxx, xxx, title
                    if (*(Name+i)==' ' && *(Name+i-1)==',') {
                        if (++n == 4) {
                            if (slen > i) {  // if name isn't empty, then copy
                                Title = (Name + i);
                            }
                            break;
                        }
                    }
                }
            }
            if (!Title) {
                // look for file extentsion like .xxx or .xxxx
                bool bIsFile = (slen>5 && ((*(Name+slen-4) == '.') || (*(Name+slen-5) == '.'))) ? true : false;

                for (int i=slen-1;i>0;--i) {
                    //search reverse last Subtitle
                    // - filename contains '~' => subdirectory
                    // or filename contains '/' => subdirectory
                    switch (*(Name+i)) {
                        case '/':
                            if (!bIsFile) {
                                break;
                            }
                        case '~': {
                            Title = (Name + i + 1);
                            Name[i] = '\0';
                            i = 0;
                        }
                        default:
                            break;
                    }
                }
            }

            if (!Title && 0 == strncmp(Name,"[image] ",8)) {
                Title = (Name + 8);
            } else if (!Title && 0 == strncmp(Name,"[audiocd] ",10)) {
                Title = (Name + 10);
            }
            if (Title) {
                if(Name && strcmp(Title, Name))
                    replayFolder = new cString(skipspace(Name));
                replayTitle = new cString(skipspace(Title));
            } else {
                replayTitle = new cString(skipspace(Name));
            }
            free(Name);
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

bool cVFDWatch::ReplayPosition(int &current, int &total, double& dFrameRate) const
{
  if (m_pControl && ((cControl *)m_pControl)->GetIndex(current, total, false)) {
    total = (total == 0) ? 1 : total;
#if VDRVERSNUM >= 10703
    dFrameRate = ((cControl *)m_pControl)->FramesPerSecond();
#endif
    return true;
  }
  return false;
}

const char * cVFDWatch::FormatReplayTime(int current, int total, double dFrameRate) const
{
    static char s[32];

    int cs = (int)((double)current / dFrameRate);
    int ts = (int)((double)total / dFrameRate);
    bool g = (cs > 3600) || (ts > 3600);

    int cm = cs / 60;
    cs %= 60;
    int tm = ts / 60;
    ts %= 60;

    if (total > 1 && theSetup.m_nRenderMode != eRenderMode_MultiPage) {
      if(g) {
#if VDRVERSNUM >= 10703
        snprintf(s, sizeof(s), "%s (%s)", (const char*)IndexToHMSF(current,false,dFrameRate), (const char*)IndexToHMSF(total,false,dFrameRate));
#else
        snprintf(s, sizeof(s), "%s (%s)", (const char*)IndexToHMSF(current), (const char*)IndexToHMSF(total));
#endif
      } else {
        snprintf(s, sizeof(s), "%02d:%02d (%02d:%02d)", cm, cs, tm, ts);
      } 
    }
    else {
      if(g) {
#if VDRVERSNUM >= 10703
        snprintf(s, sizeof(s), "%s", (const char*)IndexToHMSF(current,false,dFrameRate));
#else
        snprintf(s, sizeof(s), "%s", (const char*)IndexToHMSF(current));
#endif
      } else {
        snprintf(s, sizeof(s), "%02d:%02d", cm, cs);
      }
    }
    return s;
}

bool cVFDWatch::ReplayTime() {
    double dFrameRate;
#if VDRVERSNUM >= 10701
    dFrameRate = DEFAULTFRAMESPERSECOND;
#else
    dFrameRate = FRAMESPERSEC;
#endif
    m_nReplayCurrent = 0;
    m_nReplayTotal = 0;
    if(ReplayPosition(m_nReplayCurrent,m_nReplayTotal,dFrameRate)) {
      const char * sz = FormatReplayTime(m_nReplayCurrent,m_nReplayTotal,dFrameRate);
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
  cMutexLooker m(m_Mutex);

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
    esyslog("targaVFD: Recording: only up to %lu devices are supported by this plugin", (unsigned long)memberof(m_nCardIsRecording));
  }
}

void cVFDWatch::Channel(int ChannelNumber)
{
    cMutexLooker m(m_Mutex);
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
    this->RestartScrolled();
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
  cMutexLooker m(m_Mutex);

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
    cMutexLooker m(m_Mutex);
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
    cMutexLooker m(m_Mutex);
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
    cMutexLooker m(m_Mutex);
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
    cMutexLooker m(m_Mutex);
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
    cMutexLooker m(m_Mutex);
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

