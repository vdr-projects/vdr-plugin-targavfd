/*
 * targavfd plugin for VDR (C++)
 *
 * (C) 2010-2017 Andreas Brachold <vdr07 AT deltab de>
 *
 * This targavfd plugin is free software: you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as published 
 * by the Free Software Foundation, version 3 of the License.
 *
 * See the files README and COPYING for details.
 *
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "watch.h"
#include "setup.h"

#include <vdr/plugin.h>
#include <vdr/tools.h>
#include <vdr/config.h>
#include <vdr/i18n.h>

#define DEFAULT_ON_EXIT      eOnExitMode_BLANKSCREEN  /**< Blank the device completely */
#define DEFAULT_BRIGHTNESS   1
#define DEFAULT_WIDTH        96
#define DEFAULT_HEIGHT       16
#define DEFAULT_FONT         "Sans:Bold"
#define DEFAULT_TWO_LINE_MODE  eRenderMode_SingleLine
#define DEFAULT_BIG_FONT_HEIGHT   14
#define DEFAULT_SMALL_FONT_HEIGHT 7
#define DEFAULT_VOLUME_MODE   eVolumeMode_ShowEver    /**< Show the volume bar ever */
#define DEFAULT_SUSPEND_MODE   eSuspendMode_Never      /**< Suspend display never */

/// The one and only Stored setup data
cVFDSetup theSetup;

/// Ctor, load default values
cVFDSetup::cVFDSetup(void)
:  m_cWidth(DEFAULT_WIDTH)
,  m_cHeight(DEFAULT_HEIGHT)
{
  m_nOnExit = DEFAULT_ON_EXIT;
  m_nBrightness = DEFAULT_BRIGHTNESS;
  m_nRenderMode = DEFAULT_TWO_LINE_MODE;
  m_nBigFontHeight = DEFAULT_BIG_FONT_HEIGHT;
  m_nSmallFontHeight = DEFAULT_SMALL_FONT_HEIGHT;
  m_nVolumeMode = DEFAULT_VOLUME_MODE;
  m_nSuspendMode = DEFAULT_SUSPEND_MODE;
  m_nSuspendTimeOn = 2200;
  m_nSuspendTimeOff = 800;
  m_bSuspend_Timed = 1;   /**< Suspend display, resume short time */
  m_bSuspend_Icons = 1;   /**< Suspend icons */

  strncpy(m_szFont,DEFAULT_FONT,sizeof(m_szFont));
}

cVFDSetup::cVFDSetup(const cVFDSetup& x)
:  m_cWidth(DEFAULT_WIDTH)
,  m_cHeight(DEFAULT_HEIGHT)
{
  *this = x;
}

cVFDSetup& cVFDSetup::operator = (const cVFDSetup& x)
{
  m_nOnExit = x.m_nOnExit;
  m_nBrightness = x.m_nBrightness;

  m_nRenderMode = x.m_nRenderMode;
  m_nBigFontHeight = x.m_nBigFontHeight;
  m_nSmallFontHeight = x.m_nSmallFontHeight;

  m_nVolumeMode = x.m_nVolumeMode;
  m_nSuspendMode = x.m_nSuspendMode;
  m_nSuspendTimeOn = x.m_nSuspendTimeOn;
  m_nSuspendTimeOff = x.m_nSuspendTimeOff;

  m_bSuspend_Timed = x.m_bSuspend_Timed;
  m_bSuspend_Icons = x.m_bSuspend_Icons;

  strncpy(m_szFont,x.m_szFont,sizeof(m_szFont));

  return *this;
}

bool cVFDSetup::SetupParseInt(const char *szName, const char *szValue,
    const char *szKey,
    const int nMin, const int nMax, const int nDefault,
    int& nValue) 
{
  if(!strcasecmp(szName, szKey)) {
    int n = atoi(szValue);
    if ((n < nMin) || (n >= nMax)) {
        esyslog("targaVFD: %s must be between %d and %d, using default %d",
               szKey, nMin, nMax-1, nDefault);
        n = nDefault;
    }
    nValue = n;
    dsyslog("targaVFD: %s set to %d", szKey, nValue);
    return true;
  }
  return false;
}

/// compare profilenames and load there value
bool cVFDSetup::SetupParse(const char *szName, const char *szValue)
{
  if(SetupParseInt(szName, szValue, "OnExit", eOnExitMode_SHOWMSG, eOnExitMode_LASTITEM, DEFAULT_ON_EXIT, m_nOnExit)) { return true; }
  if(SetupParseInt(szName, szValue, "Brightness", 0, 2, DEFAULT_BRIGHTNESS, m_nBrightness)) { return true; }

  // Font
  if(!strcasecmp(szName, "Font")) {
    if(szValue) {
      cStringList fontNames;
      cFont::GetAvailableFontNames(&fontNames);
      if(fontNames.Find(szValue)>=0) {
        strncpy(m_szFont,szValue,sizeof(m_szFont));
        dsyslog("targaVFD: %s set to %s", szName, m_szFont);
        return true;
      }
    }
    esyslog("targaVFD:  %s '%s' not found, using default %s",
        szName, szValue, DEFAULT_FONT);
    strncpy(m_szFont,DEFAULT_FONT,sizeof(m_szFont));
    return true;
  }

  if(SetupParseInt(szName, szValue, "BigFont", 5, 24, DEFAULT_BIG_FONT_HEIGHT, m_nBigFontHeight)) { return true; }
  if(SetupParseInt(szName, szValue, "SmallFont", 5, 24, DEFAULT_SMALL_FONT_HEIGHT, m_nSmallFontHeight)) { return true; }
  if(SetupParseInt(szName, szValue, "TwoLineMode", eRenderMode_SingleLine, eRenderMode_LASTITEM, DEFAULT_TWO_LINE_MODE, m_nRenderMode)) { return true; }
  if(SetupParseInt(szName, szValue, "VolumeMode", eVolumeMode_ShowNever, eVolumeMode_LASTITEM, DEFAULT_VOLUME_MODE, m_nVolumeMode)) { return true; }
  if(SetupParseInt(szName, szValue, "SuspendMode", eSuspendMode_Never, eSuspendMode_LASTITEM, DEFAULT_SUSPEND_MODE, m_nSuspendMode)) { return true; }
  if(SetupParseInt(szName, szValue, "SuspendTimed", 0, 2, 1, m_bSuspend_Timed)) { return true; }
  if(SetupParseInt(szName, szValue, "SuspendIcons", 0, 2, 1, m_bSuspend_Icons)) { return true; }
  if(SetupParseInt(szName, szValue, "SuspendTimeOn", 0, 2400, 2200, m_nSuspendTimeOn)) { return true; }
  if(SetupParseInt(szName, szValue, "SuspendTimeOff", 0, 2400, 800, m_nSuspendTimeOff)) { return true; }

  //Unknow parameter
  return false;
}


// --- cVFDMenuSetup --------------------------------------------------------
void cVFDMenuSetup::Store(void)
{
  theSetup = m_tmpSetup;

  SetupStore("OnExit",     theSetup.m_nOnExit);
  SetupStore("Brightness", theSetup.m_nBrightness);
  SetupStore("Font",       theSetup.m_szFont);
  SetupStore("BigFont", theSetup.m_nBigFontHeight);
  SetupStore("SmallFont", theSetup.m_nSmallFontHeight);
  SetupStore("TwoLineMode",theSetup.m_nRenderMode);
  SetupStore("VolumeMode", theSetup.m_nVolumeMode);
  SetupStore("SuspendMode", theSetup.m_nSuspendMode);
  SetupStore("SuspendTimed", theSetup.m_bSuspend_Timed);
  SetupStore("SuspendIcons", theSetup.m_bSuspend_Icons);
  SetupStore("SuspendTimeOn", theSetup.m_nSuspendTimeOn);
  SetupStore("SuspendTimeOff", theSetup.m_nSuspendTimeOff);
}

cVFDMenuSetup::cVFDMenuSetup(cVFDWatch* pDev)
: m_tmpSetup(theSetup)
, m_pDev(pDev)
{
  SetSection(tr("targavfd"));

  cFont::GetAvailableFontNames(&fontNames);
  fontNames.Insert(strdup(DEFAULT_FONT));
  fontIndex = max(0, fontNames.Find(m_tmpSetup.m_szFont));

  static const char * szBrightness[3];
  szBrightness[0] = tr("Show nothing");
  szBrightness[1] = tr("Half brightness");
  szBrightness[2] = tr("Full brightness");

  Add(new cMenuEditStraItem (tr("Brightness"),
        &m_tmpSetup.m_nBrightness,
        memberof(szBrightness), szBrightness));

  static const char * szRenderMode[eRenderMode_LASTITEM];
  szRenderMode[eRenderMode_SingleLine] = tr("Single line");
  szRenderMode[eRenderMode_DualLine] = tr("Dual lines");
  szRenderMode[eRenderMode_SingleTopic] = tr("Only topic");
  szRenderMode[eRenderMode_MultiPage] = tr("Multiple pages");

  Add(new cMenuEditStraItem(tr("Render mode"),
        &m_tmpSetup.m_nRenderMode,    
        memberof(szRenderMode), szRenderMode));

  Add(new cMenuEditStraItem(tr("Default font"),
        &fontIndex, fontNames.Size(), &fontNames[0]));
  Add(new cMenuEditIntItem (tr("Height of big font"),
        &m_tmpSetup.m_nBigFontHeight,        
        5, 24));
  Add(new cMenuEditIntItem (tr("Height of small font"),
        &m_tmpSetup.m_nSmallFontHeight,        
        5, 24));

  static const char * szSuspendMode[eSuspendMode_LASTITEM];
  szSuspendMode[eSuspendMode_Never] = tr("Never");
  szSuspendMode[eSuspendMode_Ever]  = tr("Only per time");

  Add(new cMenuEditStraItem (tr("Suspend display at night"),
        &m_tmpSetup.m_nSuspendMode,
        memberof(szSuspendMode), szSuspendMode));
  Add(new cMenuEditBoolItem(tr("Resume on user activities"),
        &m_tmpSetup.m_bSuspend_Timed,
        tr("No"), tr("Yes")));
  Add(new cMenuEditBoolItem(tr("Hide icons"),
        &m_tmpSetup.m_bSuspend_Icons,
        tr("No"), tr("Yes")));
  Add(new cMenuEditTimeItem (tr("Beginning of suspend"),
        &m_tmpSetup.m_nSuspendTimeOn));
  Add(new cMenuEditTimeItem (tr("End time of suspend"),
        &m_tmpSetup.m_nSuspendTimeOff));

  static const char * szVolumeMode[eVolumeMode_LASTITEM];
  szVolumeMode[eVolumeMode_ShowNever] = tr("Never");
  szVolumeMode[eVolumeMode_ShowTimed] = tr("as volume short time");
  szVolumeMode[eVolumeMode_ShowEver]  = tr("as volume");
  szVolumeMode[eVolumeMode_Progress]  = tr("as replay progress");

  Add(new cMenuEditStraItem (tr("Show bargraph"),
        &m_tmpSetup.m_nVolumeMode,
        memberof(szVolumeMode), szVolumeMode));

  static const char * szExitModes[eOnExitMode_LASTITEM];
  szExitModes[eOnExitMode_SHOWMSG]      = tr("Do nothing");
  szExitModes[eOnExitMode_SHOWCLOCK]    = tr("Showing clock");
  szExitModes[eOnExitMode_BLANKSCREEN]  = tr("Turning display off");
  szExitModes[eOnExitMode_NEXTTIMER]    = tr("Show next timer");
  szExitModes[eOnExitMode_NEXTTIMER_BLANKSCR] = tr("Show only present next timer");

  Add(new cMenuEditStraItem (tr("Exit mode"),
        &m_tmpSetup.m_nOnExit,
        memberof(szExitModes), szExitModes));
}

eOSState cVFDMenuSetup::ProcessKey(eKeys nKey)
{
  if(nKey == kOk) {
    // Store edited Values
    Utf8Strn0Cpy(m_tmpSetup.m_szFont, fontNames[fontIndex], sizeof(m_tmpSetup.m_szFont));
    if (0 != strcmp(m_tmpSetup.m_szFont, theSetup.m_szFont)
        || m_tmpSetup.m_nRenderMode != theSetup.m_nRenderMode
        || ( m_tmpSetup.m_nRenderMode != eRenderMode_DualLine && (m_tmpSetup.m_nBigFontHeight != theSetup.m_nBigFontHeight))
        || ( m_tmpSetup.m_nRenderMode == eRenderMode_DualLine && (m_tmpSetup.m_nSmallFontHeight != theSetup.m_nSmallFontHeight))
      ) {
        m_pDev->SetFont(m_tmpSetup.m_szFont, 
                        m_tmpSetup.m_nRenderMode == eRenderMode_DualLine ? true : false, 
                        m_tmpSetup.m_nBigFontHeight, 
                        m_tmpSetup.m_nSmallFontHeight);
    }
  }
  return cMenuSetupPage::ProcessKey(nKey);
}


