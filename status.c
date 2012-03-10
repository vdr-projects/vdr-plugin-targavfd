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

#include <stdint.h>
#include <time.h>
#include <vdr/eitscan.h>

#include "watch.h"
#include "status.h"

//#define MOREDEBUGMSG

// ---
cVFDStatusMonitor::cVFDStatusMonitor(cVFDWatch*    pDev)
: m_pDev(pDev)
{

}

#if VDRVERSNUM >= 10726
void cVFDStatusMonitor::ChannelSwitch(const cDevice *pDevice, int nChannelNumber, bool bLiveView) {
#else
void cVFDStatusMonitor::ChannelSwitch(const cDevice *pDevice, int nChannelNumber) {
    bool bLiveView = pDevice && pDevice->IsPrimaryDevice()  && false == EITScanner.UsesDevice(pDevice);
#endif

    if (nChannelNumber > 0 
        && bLiveView
        && (nChannelNumber == cDevice::CurrentChannel()))
    {
#ifdef MOREDEBUGMSG
        dsyslog("targaVFD: channel switched to %d on DVB %d", nChannelNumber, pDevice->CardIndex());
#endif
        m_pDev->Channel(nChannelNumber);
    }
}

void cVFDStatusMonitor::SetVolume(int Volume, bool Absolute)
{
#ifdef MOREDEBUGMSG
  dsyslog("targaVFD: SetVolume  %d %d", Volume, Absolute);
#endif
  m_pDev->Volume(Volume,Absolute);
}

void cVFDStatusMonitor::Recording(const cDevice *pDevice, const char *szName, const char *szFileName, bool bOn)
{
#ifdef MOREDEBUGMSG
  dsyslog("targaVFD: Recording  %d %s", pDevice->CardIndex(), szName);
#endif
  m_pDev->Recording(pDevice,szName,szFileName,bOn);
}

void cVFDStatusMonitor::Replaying(const cControl *pControl, const char *szName, const char *szFileName, bool bOn)
{
#ifdef MOREDEBUGMSG
  dsyslog("targaVFD: Replaying  %s", szName);
#endif
  m_pDev->Replaying(pControl,szName,szFileName,bOn);
}

void cVFDStatusMonitor::OsdClear(void)
{
#ifdef MOREDEBUGMSG
  dsyslog("targaVFD: OsdClear");
#endif
  m_pDev->OsdClear();
}

void cVFDStatusMonitor::OsdTitle(const char *Title)
{
#ifdef MOREDEBUGMSG
  dsyslog("targaVFD: OsdTitle '%s'", Title);
#endif
  m_pDev->OsdTitle(Title);
}

void cVFDStatusMonitor::OsdStatusMessage(const char *szMessage)
{
#ifdef MOREDEBUGMSG
  dsyslog("targaVFD: OsdStatusMessage '%s'", szMessage ? szMessage : "NULL");
#endif
  m_pDev->OsdStatusMessage(szMessage);
}

void cVFDStatusMonitor::OsdHelpKeys(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
#ifdef unusedMOREDEBUGMSG
  dsyslog("targaVFD: OsdHelpKeys %s - %s - %s - %s", Red, Green, Yellow, Blue);
#endif
}

void cVFDStatusMonitor::OsdCurrentItem(const char *szText)
{
#ifdef MOREDEBUGMSG
  dsyslog("targaVFD: OsdCurrentItem %s", szText);
#endif
  m_pDev->OsdCurrentItem(szText);
}

void cVFDStatusMonitor::OsdTextItem(const char *Text, bool Scroll)
{
#ifdef MOREDEBUGMSG
  dsyslog("targaVFD: OsdTextItem %s %d", Text, Scroll);
#endif
}

void cVFDStatusMonitor::OsdChannel(const char *Text)
{
#ifdef MOREDEBUGMSG
  dsyslog("targaVFD: OsdChannel %s", Text);
#endif
}

void cVFDStatusMonitor::OsdProgramme(time_t PresentTime, const char *PresentTitle, const char *PresentSubtitle, time_t FollowingTime, const char *FollowingTitle, const char *FollowingSubtitle)
{
#ifdef unusedMOREDEBUGMSG
  char buffer[25];
  struct tm tm_r;
  dsyslog("targaVFD: OsdProgramme");
  strftime(buffer, sizeof(buffer), "%R", localtime_r(&PresentTime, &tm_r));
  dsyslog("%5s %s", buffer, PresentTitle);
  dsyslog("%5s %s", "", PresentSubtitle);
  strftime(buffer, sizeof(buffer), "%R", localtime_r(&FollowingTime, &tm_r));
  dsyslog("%5s %s", buffer, FollowingTitle);
  dsyslog("%5s %s", "", FollowingSubtitle);
#endif
}

