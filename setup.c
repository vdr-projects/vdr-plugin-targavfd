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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "watch.h"
#include "setup.h"

#include <vdr/plugin.h>
#include <vdr/tools.h>
#include <vdr/config.h>
#include <vdr/i18n.h>

#define DEFAULT_ON_EXIT      eOnExitMode_BLANKSCREEN	/**< Blank the device completely */
#define DEFAULT_BRIGHTNESS   1
#define DEFAULT_WIDTH        96
#define DEFAULT_HEIGHT       16
#define DEFAULT_FONT         "Sans:Bold"
#define DEFAULT_TWO_LINE_MODE	eRenderMode_SingleLine
#define DEFAULT_BIG_FONT_HEIGHT   14
#define DEFAULT_SMALL_FONT_HEIGHT 7
#define DEFAULT_VOLUME_MODE 	eVolumeMode_ShowEver    /**< Show the volume bar ever */
#define DEFAULT_SUSPEND_MODE 	eSuspendMode_Never      /**< Suspend display never */

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

  strncpy(m_szFont,x.m_szFont,sizeof(m_szFont));

  return *this;
}

/// compare profilenames and load there value 
bool cVFDSetup::SetupParse(const char *szName, const char *szValue)
{
  // OnExit
  if(!strcasecmp(szName, "OnExit")) {
    int n = atoi(szValue);
    if ((n < eOnExitMode_SHOWMSG) || (n >= eOnExitMode_LASTITEM)) {
		    esyslog("targaVFD:  OnExit must be between %d and %d, using default %d",
		           eOnExitMode_SHOWMSG, eOnExitMode_LASTITEM, DEFAULT_ON_EXIT);
		    n = DEFAULT_ON_EXIT;
    }
    m_nOnExit = n;
    return true;
  }

  // Brightness
  if(!strcasecmp(szName, "Brightness")) {
    int n = atoi(szValue);
    if ((n < 0) || (n > 2)) {
		    esyslog("targaVFD:  Brightness must be between 0 and 2, using default %d",
		           DEFAULT_BRIGHTNESS);
		    n = DEFAULT_BRIGHTNESS;
    }
    m_nBrightness = n;
    return true;
  }
  // Font
  if(!strcasecmp(szName, "Font")) {
    if(szValue) {
      cStringList fontNames;
      cFont::GetAvailableFontNames(&fontNames);
      if(fontNames.Find(szValue)>=0) {
        strncpy(m_szFont,szValue,sizeof(m_szFont));
        return true;
      }
    }
    esyslog("targaVFD:  Font '%s' not found, using default %s", 
        szValue, DEFAULT_FONT);
    strncpy(m_szFont,DEFAULT_FONT,sizeof(m_szFont));
    return true;
  }
  // BigFont
  if(!strcasecmp(szName, "BigFont")) {
    int n = atoi(szValue);
    if ((n < 5) || (n > 24)) {
		    esyslog("targaVFD:  BigFont must be between 5 and 24, using default %d",
		           DEFAULT_BIG_FONT_HEIGHT);
		    n = DEFAULT_BIG_FONT_HEIGHT;
    }
    m_nBigFontHeight = n;
    return true;
  }
  // SmallFont
  if(!strcasecmp(szName, "SmallFont")) {
    int n = atoi(szValue);
    if ((n < 5) || (n > 24)) {
		    esyslog("targaVFD:  SmallFont must be between 5 and 24, using default %d",
		           DEFAULT_SMALL_FONT_HEIGHT);
		    n = DEFAULT_SMALL_FONT_HEIGHT;
    }
    m_nSmallFontHeight = n;
    return true;
  }

  // Two Line Mode
  if(!strcasecmp(szName, "TwoLineMode")) {
    int n = atoi(szValue);
    if ((n < eRenderMode_SingleLine) || (n >= eRenderMode_LASTITEM)) {
      esyslog("targaVFD:  TwoLineMode must be between %d and %d, using default %d", 
              eRenderMode_SingleLine, eRenderMode_LASTITEM, DEFAULT_TWO_LINE_MODE );
      n = DEFAULT_TWO_LINE_MODE;
    }
    m_nRenderMode = n;
    return true;
  }

  // VolumeMode
  if(!strcasecmp(szName, "VolumeMode")) {
    int n = atoi(szValue);
    if ((n < eVolumeMode_ShowNever) || (n >= eVolumeMode_LASTITEM)) {
		    esyslog("targaVFD:  VolumeMode must be between %d and %d, using default %d",
		           eVolumeMode_ShowNever, eVolumeMode_LASTITEM, DEFAULT_VOLUME_MODE);
		    n = DEFAULT_VOLUME_MODE;
    }
    m_nVolumeMode = n;
    return true;
  }

  // SuspendMode
  if(!strcasecmp(szName, "SuspendMode")) {
    int n = atoi(szValue);
    if ((n < eSuspendMode_Never) || (n >= eSuspendMode_LASTITEM)) {
		    esyslog("targaVFD:  SuspendMode must be between %d and %d, using default %d",
		           eSuspendMode_Never, eSuspendMode_LASTITEM, DEFAULT_SUSPEND_MODE);
		    n = DEFAULT_SUSPEND_MODE;
    }
    m_nSuspendMode = n;
    return true;
  }
  // SuspendTimeOn
  if(!strcasecmp(szName, "SuspendTimeOn")) {
    int n = atoi(szValue);
    if ((n < 0) || (n >= 2400)) {
		    esyslog("targaVFD:  SuspendTimeOn must be between %d and %d, using default %d",
		           0, 2359, 0);
		    n = 0;
    }
    m_nSuspendTimeOn = n;
    return true;
  }
  // SuspendTimeOff
  if(!strcasecmp(szName, "SuspendTimeOff")) {
    int n = atoi(szValue);
    if ((n < 0) || (n >= 2400)) {
		    esyslog("targaVFD:  SuspendTimeOff must be between %d and %d, using default %d",
		           0, 2359, 0);
		    n = 0;
    }
    m_nSuspendTimeOff = n;
    return true;
  }

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
  SetupStore("SuspendTimeOn", theSetup.m_nSuspendTimeOn);
  SetupStore("SuspendTimeOff", theSetup.m_nSuspendTimeOff);
}

cVFDMenuSetup::cVFDMenuSetup(cVFDWatch*    pDev)
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

  static const char * szExitModes[eOnExitMode_LASTITEM];
  szExitModes[eOnExitMode_SHOWMSG]      = tr("Do nothing");
  szExitModes[eOnExitMode_SHOWCLOCK]    = tr("Showing clock");
  szExitModes[eOnExitMode_BLANKSCREEN]  = tr("Turning display off");
  szExitModes[eOnExitMode_NEXTTIMER]    = tr("Show next timer");
  szExitModes[eOnExitMode_NEXTTIMER_BLANKSCR] = tr("Show only present next timer");

  Add(new cMenuEditStraItem (tr("Exit mode"),           
        &m_tmpSetup.m_nOnExit,
        memberof(szExitModes), szExitModes));


  static const char * szVolumeMode[eVolumeMode_LASTITEM];
  szVolumeMode[eVolumeMode_ShowNever] = tr("Never");
  szVolumeMode[eVolumeMode_ShowTimed] = tr("as volume short time");
  szVolumeMode[eVolumeMode_ShowEver]  = tr("as volume");
  szVolumeMode[eVolumeMode_Progress]  = tr("as replay progress");

  Add(new cMenuEditStraItem (tr("Show bargraph"),           
        &m_tmpSetup.m_nVolumeMode,
        memberof(szVolumeMode), szVolumeMode));

  static const char * szSuspendMode[eSuspendMode_LASTITEM];
  szSuspendMode[eSuspendMode_Never] = tr("Never");
  szSuspendMode[eSuspendMode_Timed] = tr("Resume on activities");
  szSuspendMode[eSuspendMode_Ever]  = tr("Only per time");

  Add(new cMenuEditStraItem (tr("Suspend display at night"),           
        &m_tmpSetup.m_nSuspendMode,
        memberof(szSuspendMode), szSuspendMode));
  Add(new cMenuEditTimeItem (tr("Beginning of suspend"),           
        &m_tmpSetup.m_nSuspendTimeOn));
  Add(new cMenuEditTimeItem (tr("End time of suspend"),           
        &m_tmpSetup.m_nSuspendTimeOff));
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


