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
#define DEFAULT_TWO_LINE_MODE	0

/// The one and only Stored setup data
cVFDSetup theSetup;

/// Ctor, load default values
cVFDSetup::cVFDSetup(void)
:  m_cWidth(DEFAULT_WIDTH)
,  m_cHeight(DEFAULT_HEIGHT)
{
  m_nOnExit = DEFAULT_ON_EXIT;
  m_nBrightness = DEFAULT_BRIGHTNESS;
  m_bTwoLineMode = DEFAULT_TWO_LINE_MODE;

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

  m_bTwoLineMode = x.m_bTwoLineMode;

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
		    esyslog("targaVFD:  OnExit must be between %d and %d; using default %d",
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
		    esyslog("targaVFD:  Brightness must be between 0 and 2; using default %d",
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
    esyslog("targaVFD:  Font '%s' not found; using default %s", 
        szValue, DEFAULT_FONT);
    strncpy(m_szFont,DEFAULT_FONT,sizeof(m_szFont));
    return true;
  }

  // Two Line Mode
  if(!strcasecmp(szName, "TwoLineMode")) {
    int n = atoi(szValue);
    if ((n != 0) && (n != 1))
    {
      esyslog("targaVFD:  TwoLineMode must be 0 or 1. using default %d", 
              DEFAULT_TWO_LINE_MODE );
      n = DEFAULT_TWO_LINE_MODE;
    }

    if (n) {
      m_bTwoLineMode = 1;
    } else {
      m_bTwoLineMode = 0;
    }
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
  SetupStore("TwoLineMode",theSetup.m_bTwoLineMode);
}

cVFDMenuSetup::cVFDMenuSetup(cVFDWatch*    pDev)
: m_tmpSetup(theSetup)
, m_pDev(pDev)
{
  SetSection(tr("targavfd"));

  cFont::GetAvailableFontNames(&fontNames);
  fontNames.Insert(strdup(DEFAULT_FONT));
  fontIndex = max(0, fontNames.Find(m_tmpSetup.m_szFont));

  Add(new cMenuEditIntItem (tr("Brightness"),           
        &m_tmpSetup.m_nBrightness,        
        0, 2));

  Add(new cMenuEditStraItem(tr("Default font"),           
        &fontIndex, fontNames.Size(), &fontNames[0]));

  Add(new cMenuEditBoolItem(tr("Render mode"),                    
        &m_tmpSetup.m_bTwoLineMode,    
        tr("Single line"), tr("Dual lines")));

  static const char * szExitModes[eOnExitMode_LASTITEM];
  szExitModes[eOnExitMode_SHOWMSG]      = tr("Do nothing");
  szExitModes[eOnExitMode_SHOWCLOCK]    = tr("Showing clock");
  szExitModes[eOnExitMode_BLANKSCREEN]  = tr("Turning display off");
  szExitModes[eOnExitMode_NEXTTIMER]    = tr("Show next timer");

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
        || m_tmpSetup.m_bTwoLineMode != theSetup.m_bTwoLineMode) {
        m_pDev->SetFont(m_tmpSetup.m_szFont, m_tmpSetup.m_bTwoLineMode);
    }
	}
  return cMenuSetupPage::ProcessKey(nKey);
}


