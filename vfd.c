/*
 * targavfd plugin for VDR (C++)
 *
 * (C) 2010-2011 Andreas Brachold <vdr07 AT deltab de>

 * This targavfd plugin is free software: you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as published 
 * by the Free Software Foundation, version 3 of the License.
 *
 * See the files README and COPYING for details.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>

#include <vdr/tools.h>

#include "setup.h"
#include "ffont.h"
#include "vfd.h"

// Values for transaction's data packet.
static const int CONTROL_REQUEST_TYPE_OUT = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;

// From the HID spec:
static const int HID_SET_REPORT = 0x09;
static const int HID_REPORT_TYPE_OUTPUT = 0x02;

static const int unsigned MAX_CONTROL_OUT_TRANSFER_SIZE = 63;

static const int INTERFACE_NUMBER = 0;
static const int TIMEOUT_MS = 5000;

// Defines from display datasheet
static const int VENDOR_ID = 0x019c2;
static const int PRODUCT_ID = 0x06a11;

static const unsigned char ICON_PLAY       = 0x00; //Play
static const unsigned char ICON_PAUSE      = 0x01; //Pause
static const unsigned char ICON_RECORD     = 0x02; //Record
static const unsigned char ICON_MESSAGE    = 0x03; //Message symbol (without the inner @)
static const unsigned char ICON_MSGAT      = 0x04; //Message @
static const unsigned char ICON_MUTE       = 0x05; //Mute
static const unsigned char ICON_WLAN1      = 0x06; //WLAN (tower base)
static const unsigned char ICON_WLAN2      = 0x07; //WLAN strength (1 of 3)
static const unsigned char ICON_WLAN3      = 0x08; //WLAN strength (2 of 3)
static const unsigned char ICON_WLAN4      = 0x09; //WLAN strength (3 of 3)
static const unsigned char ICON_VOLUME     = 0x0A; //Volume (the word)
static const unsigned char ICON_VOL1       = 0x0B; //Volume level 1 of 14
static const unsigned char ICON_VOL2       = 0x0C; //Volume level 2 of 14
static const unsigned char ICON_VOL3       = 0x0D; //Volume level 3 of 14
static const unsigned char ICON_VOL4       = 0x0E; //Volume level 4 of 14
static const unsigned char ICON_VOL5       = 0x0F; //Volume level 5 of 14
static const unsigned char ICON_VOL6       = 0x10; //Volume level 6 of 14
static const unsigned char ICON_VOL7       = 0x11; //Volume level 7 of 14
static const unsigned char ICON_VOL8       = 0x12; //Volume level 8 of 14
static const unsigned char ICON_VOL9       = 0x13; //Volume level 9 of 14
static const unsigned char ICON_VOL10      = 0x14; //Volume level 10 of 14
static const unsigned char ICON_VOL11      = 0x15; //Volume level 11 of 14
static const unsigned char ICON_VOL12      = 0x16; //Volume level 12 of 14
static const unsigned char ICON_VOL13      = 0x17; //Volume level 13 of 14
static const unsigned char ICON_VOL14      = 0x18; //Volume level 14 of 14

static const unsigned char STATE_OFF       = 0x00; //Symbol off
static const unsigned char STATE_ON        = 0x01; //Symbol on
static const unsigned char STATE_ONHIGH    = 0x02; //Symbol on, high intensity, can only be used with the volume symbols

static const unsigned char CMD_PREFIX      = 0x1b;
static const unsigned char CMD_SETCLOCK    = 0x00; //Actualize the time of the display
static const unsigned char CMD_SMALLCLOCK  = 0x01; //Display small clock on display
static const unsigned char CMD_BIGCLOCK    = 0x02; //Display big clock on display
static const unsigned char CMD_SETSYMBOL   = 0x30; //Enable or disable symbol
static const unsigned char CMD_SETDIMM     = 0x40; //Set the dimming level of the display
static const unsigned char CMD_RESET       = 0x50; //Reset all configuration data to default and clear
static const unsigned char CMD_SETRAM      = 0x60; //Set the actual graphics RAM offset for next data write
static const unsigned char CMD_SETPIXEL    = 0x70; //Write pixel data to RAM of the display
static const unsigned char CMD_TEST1       = 0xf0; //Show vertical test pattern
static const unsigned char CMD_TEST2       = 0xf1; //Show horizontal test pattern

static const unsigned char TIME_12         = 0x00; //12 hours format
static const unsigned char TIME_24         = 0x01; //24 hours format

static const unsigned char BRIGHT_OFF      = 0x00; //Display off
static const unsigned char BRIGHT_DIMM     = 0x01; //Display dimmed
static const unsigned char BRIGHT_FULL     = 0x02; //Display full brightness

cVFDQueue::cVFDQueue() {
	devh = NULL;
    bInit = false;
}

cVFDQueue::~cVFDQueue() {
  cVFDQueue::close();
}

bool cVFDQueue::open()
{
	int result;
	bool ready = false;

  dsyslog("targaVFD: scanning for Futaba MDM166A...");
  bInit = true;
  //Initialize libusb
	result = libusb_init(NULL);
	if (result >= 0)
	{
		devh = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);
		if (devh != NULL)
		{
			// a targavfd has been detected.
			// Detach the hidusb driver from the HID to enable using libusb.
			libusb_detach_kernel_driver(devh, INTERFACE_NUMBER);
			{
				result = libusb_claim_interface(devh, INTERFACE_NUMBER);
				if (result >= 0)
				{
					ready = true;
				}
				else
				{
    			esyslog("targaVFD: libusb_claim_interface error! %s (%d)",usberror(result),result);
				}
			}
		}
		else
		{
			esyslog("targaVFD: Unable to find the device!");
		}
	}
	else
	{
		esyslog("targaVFD: Unable to initialize libusb! %s (%d)",usberror(result),result);
	}
  if(!ready) {
    if(devh)  
		  libusb_release_interface(devh, 0);
    devh = NULL;
  }

  while (!empty()) {
    pop();
  }

  return ready;
}

void cVFDQueue::close() {

  if (devh != NULL) {
      int result = libusb_release_interface(devh, 0);
			if (result < 0)
			{
  			esyslog("targaVFD: libusb_release_interface failed! %s (%d)",usberror(result),result);
			}
      libusb_close(devh);
      devh = NULL;
  }
  if(bInit) {
      // Deinitialize libusb 
      libusb_exit(NULL);
      bInit = false;
  }
}

void cVFDQueue::QueueCmd(const unsigned char & cmd) {
  this->push(CMD_PREFIX);
  this->push(cmd);
}

void cVFDQueue::QueueData(const unsigned char & data) {
  this->push(data);
}

bool cVFDQueue::QueueFlush() {

  if(empty())
    return true;
  if(!isopen()) {
    return false;
  }

	int bytes;
	unsigned char buf[MAX_CONTROL_OUT_TRANSFER_SIZE+1];	
  
  while (!empty()) {
    buf[0] = (unsigned char) std::min((size_t)MAX_CONTROL_OUT_TRANSFER_SIZE,size());
    for(unsigned int i = 0;i < MAX_CONTROL_OUT_TRANSFER_SIZE && !empty();++i) {
      buf[i+1] = (char) front(); //the first element in the queue
      pop();              //remove the first element of the queue
    }

	  bytes = libusb_control_transfer(
			  devh,
			  CONTROL_REQUEST_TYPE_OUT ,
			  HID_SET_REPORT,
			  (HID_REPORT_TYPE_OUTPUT<<8)|0x00,
			  INTERFACE_NUMBER,
			  buf,
			  (((int)buf[0]) + 1),
			  TIMEOUT_MS);

	  if (bytes <= 0)
	  {
      esyslog("targaVFD: libusb_control_transfer failed : %s (%d)",usberror(bytes),bytes);
      while (!empty()) {
        pop();
      }
      cVFDQueue::close();
      return false;
	  }
  }
  return true;
}

const char *cVFDQueue::usberror(int ret) const
{
  switch(ret) {
    case LIBUSB_SUCCESS:
      return "Success (no error).";

    case LIBUSB_ERROR_IO:
      return "Input/output error.";

    case LIBUSB_ERROR_INVALID_PARAM: 	
      return "Invalid parameter.";

    case LIBUSB_ERROR_ACCESS:
      return "Access denied (insufficient permissions).";

    case LIBUSB_ERROR_NO_DEVICE:
      return "No such device (it may have been disconnected).";

    case LIBUSB_ERROR_NOT_FOUND:
      return "Entity not found.";

    case LIBUSB_ERROR_BUSY:
      return "Resource busy.";

    case LIBUSB_ERROR_TIMEOUT:
      return "Operation timed out.";

    case LIBUSB_ERROR_OVERFLOW:
      return "Overflow.";

    case LIBUSB_ERROR_PIPE: 	
      return "Pipe error.";

    case LIBUSB_ERROR_INTERRUPTED:
      return "System call interrupted (perhaps due to signal).";

    case LIBUSB_ERROR_NO_MEM:
      return "Insufficient memory.";

    case LIBUSB_ERROR_NOT_SUPPORTED:
      return "Operation not supported or unimplemented on this platform.";

    case LIBUSB_ERROR_OTHER:
      return "Other error. ";
    }
    return "unknown error";
}

cVFD::cVFD() 
{
  pFont = NULL;
  lastIconState = 0;
  framebuf = NULL;
  backingstore = NULL;

  m_nScrollOffset = -1;
  m_bScrollBackward = false;
  m_bScrollNeeded = false;
}

cVFD::~cVFD() {
  this->close();
}


/**
 * Initialize the driver.
 * \retval 0	   Success.
 * \retval <0	  Error.
 */
bool cVFD::open()
{
  if(!SetFont(theSetup.m_szFont, 
              theSetup.m_nRenderMode == eRenderMode_DualLine, 
              theSetup.m_nBigFontHeight, 
              theSetup.m_nSmallFontHeight)) {
		return false;
  }
  if(!cVFDQueue::open()) {
		return false;
  }

	isyslog("targaVFD: open Device successful");

	/* Make sure the frame buffer is there... */
	this->framebuf = new cVFDBitmap(theSetup.m_cWidth,theSetup.m_cHeight);
	if (this->framebuf == NULL) {
		esyslog("targaVFD: unable to allocate framebuffer");
		return false;
	}
    m_iSizeYb = ((theSetup.m_cHeight + 7) / 8);

	/* Make sure the framebuffer backing store is there... */
	this->backingstore = new unsigned char[theSetup.m_cWidth * m_iSizeYb];
	if (this->backingstore == NULL) {
		esyslog("targaVFD: unable to create framebuffer backing store");
		return false;
	}

	this->lastIconState = 0;

  QueueCmd(CMD_RESET);
	//Brightness(theSetup.m_nBrightness);
  if(QueueFlush()) {
  	dsyslog("targaVFD: init() done");
  	return true;
  }
  return false;
}

/*
 * turning display off
 */
bool cVFD::SendCmdShutdown() {
  QueueCmd(CMD_RESET);
	return QueueFlush();
}

inline unsigned char toBCD(int x){ 
 return (unsigned char)(((x) / 10 * 16) + ((x) % 10));
}

/*
 * Show the big clock. We need to set it to the current time, then it just
 * keeps counting automatically.
 */
bool cVFD::SendCmdClock() {

  time_t tt;
  struct tm l;

  tt = time(NULL);
  localtime_r(&tt, &l);

  // Set time
  QueueCmd(CMD_SETCLOCK);
  QueueData(toBCD(l.tm_min));
  QueueData(toBCD(l.tm_hour));

  // Show it
  QueueCmd(CMD_BIGCLOCK);
  QueueData(TIME_24);
  return QueueFlush();
}

/**
 * Close the driver (do necessary clean-up).
 */
void cVFD::close()
{
  cVFDQueue::close();

  if(pFont) {
    delete pFont;
    pFont = NULL;
  }
  if(framebuf) {
    delete framebuf;
    framebuf = NULL;
  }
  if(backingstore) {
    delete[] backingstore;
    backingstore = NULL;
  }

	dsyslog("targaVFD: close() done");
}


/**
 * Clear the screen.
 */
void cVFD::clear()
{
  if(framebuf)
    framebuf->clear();
}


/**
 * Flush cached bitmap data and submit changes rows to the Display.
 */

bool cVFD::flush(bool refreshAll)
{
  unsigned int n, x, yb;

  if (!backingstore || !framebuf)
      return false;

  const uchar* fb = framebuf->getBitmap();
  const unsigned int width = framebuf->Width();

  bool doRefresh = false;
  unsigned int minX = width;
  unsigned int maxX = 0;

  for (yb = 0; yb < m_iSizeYb; ++yb)
      for (x = 0; x < width; ++x)
      {
          n = x + (yb * width);
          if (*(fb + n) != *(backingstore + n))
          {
              *(backingstore + n) = *(fb + n);
              minX = min(minX, x);
              maxX = max(maxX, x + 1);
              doRefresh = true;
          }
      }

  if (refreshAll || doRefresh) {
    if (refreshAll) {
      minX = 0;
      maxX = width;
    }

    maxX = min(maxX, width);

	  unsigned int nData = (maxX-minX) * m_iSizeYb;
	  if(nData) {
	    // send data to display, controller
		QueueCmd(CMD_SETRAM);
		QueueData(minX*m_iSizeYb);
		QueueCmd(CMD_SETPIXEL);
		QueueData(nData);

    for (x = minX; x < maxX; ++x)
	    for (yb = 0; yb < m_iSizeYb; ++yb)
        {
            n = x + (yb * width);
            QueueData((*(backingstore + n)));
        }
		}
  }
  return QueueFlush();
}

/**
 * Print a string on the screen at position (x,y).
 * The upper-left corner is (1,1), the lower-right corner is (this->width, this->height).
 * \param x        Horizontal character position (column).
 * \param y        Vertical character position (row).
 * \param string   String that gets written.
 */
int cVFD::DrawText(int x, int y, const char* string, int nMaxWidth /* = 1024*/)
{
  if(pFont && framebuf)
    return pFont->DrawText(framebuf, x, y, string, nMaxWidth);
  return -1;
}

int cVFD::DrawTextEclipsed(int x, int y, const char* string, int nMaxWidth /* = 1024*/)
{
  static const char* szEclipse = "..";
  int d = pFont->Width(szEclipse);
  int w = this->DrawText(x, y, string, nMaxWidth - d);
  if(w > 0 && ((w-x) != pFont->Width(string))) {
     this->DrawText(w, y, szEclipse);
  }
  return w;
}

void cVFD::RestartScrolled() {
  m_nScrollOffset = 0;
  m_bScrollBackward = false;
  m_bScrollNeeded = true;
}

int cVFD::DrawTextScrolled(int x, int y, const char* string, bool bCenter)
{
  int w = pFont->Width(string);
  int nAlign = 0;
  if(bCenter) {
    nAlign = (this->Width() - w) / 2;
    if(nAlign < 0) {
      nAlign = 0;
    }
  }
  nAlign += x;
  int iRet = this->DrawText(nAlign - m_nScrollOffset,y, string);
  if((nAlign + (w - m_nScrollOffset)) == iRet)
    iRet = 0; // Text fits into screen
  else 
    iRet = 1; // Text large then screen

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
  return iRet;
}


/**
 * Height of framebuffer from current device.
 */
int cVFD::Height() const
{
  if(framebuf)
    return framebuf->Height();
  return 0;
}

/**
 * Width of framebuffer from current device.
 */
int cVFD::Width() const
{
  if(framebuf)
    return framebuf->Width();
  return 0;
}

/**
 * Draw a rectangle on framebuffer on device. 
 *
 * \param x1       First horizontal corner (column).
 * \param y1       First vertical corner (row).
 * \param x2       Second horizontal corner (column).
 * \param y2       Second vertical corner (row).
 * \param filled   drawing of rectangle should be filled.
 */
bool cVFD::Rectangle(int x1, int y1, int x2, int y2, bool filled)
{
  if(framebuf)
    return framebuf->Rectangle(x1, y1, x2, y2, filled);
  return false;
}


/**
 * Sets the "icons state" for the device. We use this to control the icons
   * around the outside the display. 
 *
 * \param state    This symbols to display.
 */
void cVFD::icons(unsigned int state)
{
  for(unsigned i = 0; i <= 0x18; i++) {
    if((state & (1 << i)) != (lastIconState & (1 << i))) 
    { 
      QueueCmd(CMD_SETSYMBOL);
      QueueData(i);
      QueueData((state & (1 << i)) ? STATE_ON : STATE_OFF );
    }
  }  
  lastIconState = state;
}

/**
 * Sets the brightness of the display.
 *
 * \param nBrightness The value the brightness (0 = off
 *                  1 = half brightness; 2 = highest brightness).
 */
void cVFD::Brightness(int nBrightness)
{
	if (nBrightness < 0) {
		nBrightness = 0;
	} else if (nBrightness > 2) {
		nBrightness = 2;
	}
  this->QueueCmd(CMD_SETDIMM);
  this->QueueData((unsigned char) (nBrightness));
}

bool cVFD::SetFont(const char *szFont, bool bTwoLineMode, int nBigFontHeight, int nSmallFontHeight) {

  cVFDFont* tmpFont = NULL;

  cString sFileName = cFont::GetFontFileName(szFont);
  if(!isempty(sFileName))
  {
    if (bTwoLineMode) {
      tmpFont = new cVFDFont(sFileName,nSmallFontHeight);
    } else {
      tmpFont = new cVFDFont(sFileName,nBigFontHeight);
    }
  } else {
		esyslog("targaVFD: unable to find font '%s'",szFont);
  }
  if(tmpFont) {
    if(pFont) {
      delete pFont;
    }
    pFont = tmpFont;
    return true;
  }
  return false;
}
