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

#ifndef __VFD_SETUP_H___
#define __VFD_SETUP_H___

#include <vdr/menuitems.h>
#define memberof(x) (sizeof(x)/sizeof(*x))

enum eOnExitMode {
   eOnExitMode_SHOWMSG     /**< Do nothing - just leave the "last" message there */
  ,eOnExitMode_SHOWCLOCK   /**< Show the big clock */
  ,eOnExitMode_BLANKSCREEN /**< Blank the device completely */
  ,eOnExitMode_NEXTTIMER   /**< Show next active timer */
  ,eOnExitMode_NEXTTIMER_BLANKSCR   /**< Show next active timer, or blank if none timer defined */
  ,eOnExitMode_LASTITEM
};

enum eVolumeMode {
   eVolumeMode_ShowNever   /**< Show the volume bar never */
  ,eVolumeMode_ShowTimed   /**< Show the volume bar short time */
  ,eVolumeMode_ShowEver    /**< Show the volume bar ever */
  ,eVolumeMode_Progress    /**< Show the volume bar as time progress */
  ,eVolumeMode_LASTITEM
};

enum eRenderMode {
   eRenderMode_SingleLine   /**< Render screen at single line */
  ,eRenderMode_DualLine     /**< Render screen at dual line */
  ,eRenderMode_SingleTopic  /**< Render screen at single line, only names */
  ,eRenderMode_MultiPage    /**< Render screen at multiple pages */
  ,eRenderMode_LASTITEM
};

enum eSuspendMode {
   eSuspendMode_Never   /**< Suspend display never */
  ,eSuspendMode_Timed   /**< Suspend display, resume short time */
  ,eSuspendMode_Ever    /**< Suspend display ever */
  ,eSuspendMode_LASTITEM
};

struct cVFDSetup 
{
  int          m_nOnExit;
  int          m_nBrightness;

  const int    m_cWidth;
  const int    m_cHeight;

  int          m_nBigFontHeight;
  int          m_nSmallFontHeight;

  char         m_szFont[256];

  int          m_nRenderMode; /** enable two line mode */

  int          m_nVolumeMode;

  int          m_nSuspendMode;
  int          m_nSuspendTimeOn;
  int          m_nSuspendTimeOff;

  cVFDSetup(void);
  cVFDSetup(const cVFDSetup& x);
  cVFDSetup& operator = (const cVFDSetup& x);
  
  /// Parse our own setup parameters and store their values.
  bool SetupParse(const char *szName, const char *szValue);
  
};

class cVFDWatch;
class cVFDMenuSetup
	:public cMenuSetupPage 
{
  cVFDSetup  m_tmpSetup;
  cVFDWatch* m_pDev;
  cStringList fontNames;
  int         fontIndex;
protected:
  virtual void Store(void);
  virtual eOSState ProcessKey(eKeys nKey);
public:
  cVFDMenuSetup(cVFDWatch*    pDev);
};


/// The exported one and only Stored setup data
extern cVFDSetup theSetup;

#endif  //__VFD_SETUP_H___

