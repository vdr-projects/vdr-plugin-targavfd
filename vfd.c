/*
 * targavfd plugin for VDR (C++)
 *
 * (C) 2010 Andreas Brachold <vdr07 AT deltab de>

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

static const byte ICON_PLAY       = 0x00; //Play
static const byte ICON_PAUSE      = 0x01; //Pause
static const byte ICON_RECORD     = 0x02; //Record
static const byte ICON_MESSAGE    = 0x03; //Message symbol (without the inner @)
static const byte ICON_MSGAT      = 0x04; //Message @
static const byte ICON_MUTE       = 0x05; //Mute
static const byte ICON_WLAN1      = 0x06; //WLAN (tower base)
static const byte ICON_WLAN2      = 0x07; //WLAN strength (1 of 3)
static const byte ICON_WLAN3      = 0x08; //WLAN strength (2 of 3)
static const byte ICON_WLAN4      = 0x09; //WLAN strength (3 of 3)
static const byte ICON_VOLUME     = 0x0A; //Volume (the word)
static const byte ICON_VOL1       = 0x0B; //Volume level 1 of 14
static const byte ICON_VOL2       = 0x0C; //Volume level 2 of 14
static const byte ICON_VOL3       = 0x0D; //Volume level 3 of 14
static const byte ICON_VOL4       = 0x0E; //Volume level 4 of 14
static const byte ICON_VOL5       = 0x0F; //Volume level 5 of 14
static const byte ICON_VOL6       = 0x10; //Volume level 6 of 14
static const byte ICON_VOL7       = 0x11; //Volume level 7 of 14
static const byte ICON_VOL8       = 0x12; //Volume level 8 of 14
static const byte ICON_VOL9       = 0x13; //Volume level 9 of 14
static const byte ICON_VOL10      = 0x14; //Volume level 10 of 14
static const byte ICON_VOL11      = 0x15; //Volume level 11 of 14
static const byte ICON_VOL12      = 0x16; //Volume level 12 of 14
static const byte ICON_VOL13      = 0x17; //Volume level 13 of 14
static const byte ICON_VOL14      = 0x18; //Volume level 14 of 14

static const byte STATE_OFF       = 0x00; //Symbol off
static const byte STATE_ON        = 0x01; //Symbol on
static const byte STATE_ONHIGH    = 0x02; //Symbol on, high intensity, can only be used with the volume symbols

static const byte CMD_PREFIX      = 0x1b;
static const byte CMD_SETCLOCK    = 0x00; //Actualize the time of the display
static const byte CMD_SMALLCLOCK  = 0x01; //Display small clock on display
static const byte CMD_BIGCLOCK    = 0x02; //Display big clock on display
static const byte CMD_SETSYMBOL   = 0x30; //Enable or disable symbol
static const byte CMD_SETDIMM     = 0x40; //Set the dimming level of the display
static const byte CMD_RESET       = 0x50; //Reset all configuration data to default and clear
static const byte CMD_SETRAM      = 0x60; //Set the actual graphics RAM offset for next data write
static const byte CMD_SETPIXEL    = 0x70; //Write pixel data to RAM of the display
static const byte CMD_TEST1       = 0xf0; //Show vertical test pattern
static const byte CMD_TEST2       = 0xf1; //Show horizontal test pattern

static const byte TIME_12         = 0x00; //12 hours format
static const byte TIME_24         = 0x01; //24 hours format

static const byte BRIGHT_OFF      = 0x00; //Display off
static const byte BRIGHT_DIMM     = 0x01; //Display dimmed
static const byte BRIGHT_FULL     = 0x02; //Display full brightness

cVFDQueue::cVFDQueue() {
	hid = NULL;
  bInit = false;
}

cVFDQueue::~cVFDQueue() {
  cVFDQueue::close();
}

bool cVFDQueue::open()
{
  HIDInterfaceMatcher matcher = { 0x19c2, 0x6a11, NULL, NULL, 0 };
  hid_return ret;

  /* see include/debug.h for possible values */
  hid_set_debug(HID_DEBUG_NONE);
  hid_set_debug_stream(0);
  /* passed directly to libusb */
  hid_set_usb_debug(0);
  
  ret = hid_init();
  if (ret != HID_RET_SUCCESS) {
    esyslog("targaVFD: init - %s (%d)", hiderror(ret), ret);
    return false;
  }
  bInit = true;

  hid = hid_new_HIDInterface();
  if (hid == 0) {
    esyslog("targaVFD: hid_new_HIDInterface() failed, out of memory?\n");
    return false;
  }

  ret = hid_force_open(hid, 0, &matcher, 3);
  if (ret != HID_RET_SUCCESS) {
    esyslog("targaVFD: open - %s (%d)", hiderror(ret), ret);
    hid_close(hid);
    hid_delete_HIDInterface(&hid);
    hid = 0;
    return false;
  }

  while (!empty()) {
    pop();
  }
  //ret = hid_write_identification(stdout, hid);
  //if (ret != HID_RET_SUCCESS) {
  //  esyslog("targaVFD: write_identification %s (%d)", hiderror(ret), ret);
  //  return false;
  //}
  return true;
}

void cVFDQueue::close() {
  hid_return ret;
  if (hid != 0) {
    ret = hid_close(hid);
    if (ret != HID_RET_SUCCESS) {
      esyslog("targaVFD: close - %s (%d)", hiderror(ret), ret);
    }

    hid_delete_HIDInterface(&hid);
    hid = 0;
  }
  if(bInit) {
    ret = hid_cleanup();
    if (ret != HID_RET_SUCCESS) {
      esyslog("targaVFD: cleanup - %s (%d)", hiderror(ret), ret);
    }
    bInit = false;
  }
}

void cVFDQueue::QueueCmd(const byte & cmd) {
  this->push(CMD_PREFIX);
  this->push(cmd);
}

void cVFDQueue::QueueData(const byte & data) {
  this->push(data);
}

bool cVFDQueue::QueueFlush() {

  if(empty())
    return true;
  if(!isopen()) {
    return false;
  }

  int const PATH_OUT[1] = { 0xff7f0004 };
  char buf[64];
  hid_return ret;

  while (!empty()) {
    buf[0] = (char) std::min((size_t)63,size());
    for(unsigned int i = 0;i < 63 && !empty();++i) {
      buf[i+1] = (char) front(); //the first element in the queue
      pop();              //remove the first element of the queue
    }
    ret = hid_set_output_report(hid, PATH_OUT, sizeof(PATH_OUT), buf, (buf[0] + 1));
    if (ret != HID_RET_SUCCESS) {
      esyslog("targaVFD: set_output_report - %s (%d)", hiderror(ret), ret);
      while (!empty()) {
        pop();
      }
      cVFDQueue::close();
      return false;
    }
  }
  return true;
}

const char *cVFDQueue::hiderror(hid_return ret) const
{
  switch(ret) {
    case HID_RET_SUCCESS:
      return "success";
    case HID_RET_INVALID_PARAMETER:
      return "invalid parameter";
    case HID_RET_NOT_INITIALISED:
      return "not initialized";
    case HID_RET_ALREADY_INITIALISED:
      return "hid_init() already called";
    case HID_RET_FAIL_FIND_BUSSES:
      return "failed to find any USB busses";
    case HID_RET_FAIL_FIND_DEVICES:
      return "failed to find any USB devices";
    case HID_RET_FAIL_OPEN_DEVICE:
      return "failed to open device";
    case HID_RET_DEVICE_NOT_FOUND:
      return "device not found";
    case HID_RET_DEVICE_NOT_OPENED:
      return "device not yet opened";
    case HID_RET_DEVICE_ALREADY_OPENED:
      return "device already opened";
    case HID_RET_FAIL_CLOSE_DEVICE:
      return "could not close device";
    case HID_RET_FAIL_CLAIM_IFACE:
      return "failed to claim interface; is another driver using it?";
    case HID_RET_FAIL_DETACH_DRIVER:
      return "failed to detach kernel driver";
    case HID_RET_NOT_HID_DEVICE:
      return "not recognized as a HID device";
    case HID_RET_HID_DESC_SHORT:
      return "HID interface descriptor too short";
    case HID_RET_REPORT_DESC_SHORT:
      return "HID report descriptor too short";
    case HID_RET_REPORT_DESC_LONG:
      return "HID report descriptor too long";
    case HID_RET_FAIL_ALLOC:
      return "failed to allocate memory";
    case HID_RET_OUT_OF_SPACE:
      return "no space left in buffer";
    case HID_RET_FAIL_SET_REPORT:
      return "failed to set report";
    case HID_RET_FAIL_GET_REPORT:
      return "failed to get report";
    case HID_RET_FAIL_INT_READ:
      return "interrupt read failed";
    case HID_RET_NOT_FOUND:
      return "not found";
#ifdef HID_RET_TIMEOUT
    case HID_RET_TIMEOUT:
      return "timeout";
#endif
  }
  return "unknown error";
}

cVFD::cVFD() 
{
  this->pFont = NULL;
	this->lastIconState = 0;
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
  if(!SetFont(theSetup.m_szFont,theSetup.m_bTwoLineMode)) {
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

	/* Make sure the framebuffer backing store is there... */
	this->backingstore = new cVFDBitmap(theSetup.m_cWidth,theSetup.m_cHeight);
	if (this->backingstore == NULL) {
		esyslog("targaVFD: unable to create framebuffer backing store");
		return false;
	}
  backingstore->SetPixel(0,0);//make dirty

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

inline byte toBCD(int x){ 
 return (byte)(((x) / 10 * 16) + ((x) % 10));
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
    delete backingstore;
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
 * Flush data on screen to the LCD.
 */
bool cVFD::flush()
{
	/*
	 * The display only provides for a complete screen refresh. If
	 * nothing has changed, don't refresh.
	 */
  if (!((*backingstore) == (*framebuf))) {
	  /* send buffer for one command or display data */
    const uchar* fb = framebuf->getBitmap();
	  int bytes = framebuf->Width() * framebuf->Height() / 8;
	  int width = framebuf->Width();
    QueueCmd(CMD_SETRAM);
    QueueData(0);
    QueueCmd(CMD_SETPIXEL);
    QueueData(bytes);
    for (int i=0;i<width;i+=1)
    {
       QueueData(*(fb + i));
       QueueData(*(fb + (i + width)));
	  }
	  /* Update the backing store. */
    (*backingstore) = (*framebuf);
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
int cVFD::DrawText(int x, int y, const char* string)
{
  if(pFont && framebuf)
    return pFont->DrawText(framebuf, x, y, string, 1024);
  return -1;
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
  this->QueueData((byte) (nBrightness));
}

bool cVFD::SetFont(const char *szFont, int bTwoLineMode) {

  cVFDFont* tmpFont = NULL;

  cString sFileName = cFont::GetFontFileName(szFont);
  if(!isempty(sFileName))
  {
    if (bTwoLineMode)
    {
      tmpFont = new cVFDFont(sFileName,6,8);
    } else {
      tmpFont = new cVFDFont(sFileName,12,11);
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
