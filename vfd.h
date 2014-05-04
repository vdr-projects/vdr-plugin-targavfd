/*
 * targavfd plugin for VDR (C++)
 *
 * (C) 2010-2011 Andreas Brachold <vdr07 AT deltab de>
 *
 * This targavfd plugin is free software: you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as published 
 * by the Free Software Foundation, version 3 of the License.
 *
 * See the files README and COPYING for details.
 *
 */

#ifndef __VFD_H_
#define __VFD_H_

#include <queue>
#include <libusb-1.0/libusb.h>
#include "bitmap.h"

enum eIcons {
  eIconOff = 0,
  eIconPLAY         = 1 << 0x00, // Play
  eIconPAUSE        = 1 << 0x01, // Pause
  eIconRECORD       = 1 << 0x02, // Record
  eIconMESSAGE      = 1 << 0x03, // Message symbol (without the inner @)
  eIconMSGAT        = 1 << 0x04, // Message @
  eIconMUTE         = 1 << 0x05, // Mute
  eIconWLAN1        = 1 << 0x06, // WLAN (tower base)
  eIconWLAN2        = 1 << 0x07, // WLAN strength (1 of 3)
  eIconWLAN3        = 1 << 0x08, // WLAN strength (2 of 3)
  eIconWLAN4        = 1 << 0x09, // WLAN strength (3 of 3)
  eIconVOLUME       = 1 << 0x0A, // Volume (the word)
  eIconVOL1         = 1 << 0x0B, // Volume level 1 of 14
  eIconVOL2         = 1 << 0x0C, // Volume level 2 of 14
  eIconVOL3         = 1 << 0x0D, // Volume level 3 of 14
  eIconVOL4         = 1 << 0x0E, // Volume level 4 of 14
  eIconVOL5         = 1 << 0x0F, // Volume level 5 of 14
  eIconVOL6         = 1 << 0x10, // Volume level 6 of 14
  eIconVOL7         = 1 << 0x11, // Volume level 7 of 14
  eIconVOL8         = 1 << 0x12, // Volume level 8 of 14
  eIconVOL9         = 1 << 0x13, // Volume level 9 of 14
  eIconVOL10        = 1 << 0x14, // Volume level 10 of 14
  eIconVOL11        = 1 << 0x15, // Volume level 11 of 14
  eIconVOL12        = 1 << 0x16, // Volume level 12 of 14
  eIconVOL13        = 1 << 0x17, // Volume level 13 of 14
  eIconVOL14        = 1 << 0x18  // Volume level 14 of 14
};

class cVFDFont;

class cVFDQueue : public std::queue<unsigned char> {
  struct libusb_device_handle* devh;
    bool bInit;
public:
  cVFDQueue();
  virtual ~cVFDQueue();
protected:
  virtual bool open();
  virtual void close();
  virtual bool isopen() const { return devh != NULL; }
  void QueueCmd(const unsigned char & cmd);
  void QueueData(const unsigned char & data);
  bool QueueFlush();
private:
  const char *usberror(int ret) const;
};

class cVFD : public cVFDQueue {

	/* framebuffer and backingstore for current contents */
	cVFDBitmap* framebuf;
	unsigned char * backingstore;
	unsigned int lastIconState;
	unsigned int m_iSizeYb;

  int   m_nScrollOffset;
  bool  m_bScrollBackward;
  bool  m_bScrollNeeded;

protected:
  cVFDFont*   pFont;

  bool SendCmdClock();
  bool SendCmdShutdown();
  void Brightness(int nBrightness);
public:
  cVFD();
  virtual ~cVFD();

  virtual bool open();
  virtual void close();


  void clear ();
  int DrawText(int x, int y, const char* string, int nMaxWidth = 1024);
  int DrawTextEclipsed(int x, int y, const char* string, int nMaxWidth = 1024);

  int DrawTextScrolled(int x, int y, const char* string, bool bCenter);
  inline bool NeedScrolled() const { return m_nScrollOffset > 0 || m_bScrollBackward; };
  void RestartScrolled();

  int Height() const;
  int Width() const;
  bool Rectangle(int x1, int y1, int x2, int y2, bool filled);
  bool flush (bool refreshAll = true);

  void icons(unsigned int state);
  virtual bool SetFont(const char *szFont, bool bTwoLineMode, int nBigFontHeight, int nSmallFontHeight);
};


#endif



