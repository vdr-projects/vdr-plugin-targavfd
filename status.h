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

#ifndef __VFD_STATUS__H
#define __VFD_STATUS__H

#include "vfd.h"
#include <vdr/status.h>

class cVFDWatch;

class cVFDStatusMonitor : public cStatus {
  cVFDWatch*    m_pDev;
 public: 
  cVFDStatusMonitor(cVFDWatch*    pDev);
 protected:
#if VDRVERSNUM >= 10726
  virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber, bool LiveView);
#else
  virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber);
#endif
  virtual void Recording(const cDevice *pDevice, const char *szName, const char *szFileName, bool bOn);
  virtual void Replaying(const cControl *pControl, const char *szName, const char *szFileName, bool bOn);
  virtual void SetVolume(int Volume, bool Absolute);
  virtual void OsdClear(void);
  virtual void OsdTitle(const char *Title);
  virtual void OsdStatusMessage(const char *Message);
  virtual void OsdHelpKeys(const char *Red, const char *Green, const char *Yellow, const char *Blue);
  virtual void OsdCurrentItem(const char *Text);
  virtual void OsdTextItem(const char *Text, bool Scroll);
  virtual void OsdChannel(const char *Text);
  virtual void OsdProgramme(time_t PresentTime, const char *PresentTitle, const char *PresentSubtitle, time_t FollowingTime, const char *FollowingTitle, const char *FollowingSubtitle);
};

#endif

