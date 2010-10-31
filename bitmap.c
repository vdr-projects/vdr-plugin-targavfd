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

#include <vdr/tools.h>
#include "bitmap.h"

/**
 * Ctor - Create framebuffer
 *
 * \param w        count of horizontal columns.
 * \param h        count of vertical rows.
 */
cVFDBitmap::cVFDBitmap(int w, int h) {
  width = w;
  height = h;

  // lines are byte aligned
  bytesPerLine = (width + 7) / 8;
  if(0<height && 0<bytesPerLine)
 	  bitmap = MALLOC(uchar, bytesPerLine * height);
  clear();
}

/**
 * Ctor - Create empty framebuffer
 */
cVFDBitmap::cVFDBitmap() {
  height = 0;
  width = 0;
  bitmap = NULL;
}

/**
 * Dtor - Destroy framebuffer
 */
cVFDBitmap::~cVFDBitmap() {

  if(bitmap)  
    free(bitmap);
  bitmap = NULL;
}

/**
 * Compare current framebuffer. 
 */
cVFDBitmap& cVFDBitmap::operator = (const cVFDBitmap& x) {

  if(height != x.height
    || width != x.width
    || bitmap == NULL) {
  
    if(bitmap)  
      free(bitmap);
    bitmap = NULL;

    height = x.height;
    width  = x.width;

    bytesPerLine = (width + 7) / 8;

    if(0<height && 0<bytesPerLine)
    	bitmap = MALLOC(uchar, bytesPerLine * height);
  }
  if(bitmap && x.bitmap)
  	memcpy(bitmap, x.bitmap, bytesPerLine * height);
  return *this;
}

/**
 * Copy current framebuffer. 
 */
bool cVFDBitmap::operator == (const cVFDBitmap& x) const {

  if(height != x.height
    || width != x.width
    || bitmap == NULL
    || x.bitmap == NULL)
    return false;
	return ((memcmp(x.bitmap, bitmap, bytesPerLine * height)) == 0);
}

/**
 * Cleanup current framebuffer. 
 */
void cVFDBitmap::clear() {
    if (bitmap)
      memset(bitmap, 0x00, bytesPerLine * height);
}

/**
 * Draw a single pixel framebuffer. 
 *
 * \param x        horizontal column.
 * \param y        vertical row.
 */
bool cVFDBitmap::SetPixel(int x, int y)
{
    unsigned char c;
    unsigned int n;

    if (!bitmap)
        return false;

    if (x >= width || x < 0)
        return false;
    if (y >= height || y < 0)
        return false;

    n = x + ((y / 8) * width);
    c = 0x80 >> (y % 8);

    if(n >= (bytesPerLine * height))
        return false;

    bitmap[n] |= c;
    return true;
}


/**
 * Draw a horizontal line on framebuffer. 
 *
 * \param x1       First horizontal column.
 * \param y        vertical row.
 * \param x2       Second horizontal column.
 */
bool cVFDBitmap::HLine(int x1, int y, int x2) {

    sort(x1,x2);

    for (int x = x1; x <= x2; x++) {
        if(!SetPixel(x, y))
          return false;
    }
    return true;
}

/**
 * Draw a vertical line on framebuffer. 
 *
 * \param x        horizontal column.
 * \param y1       First vertical row.
 * \param y2       Second vertical row.
 */
bool cVFDBitmap::VLine(int x, int y1, int y2) {

    sort(y1,y2);

    for (int y = y1; y <= y2; y++) {
        if(!SetPixel(x, y))
          return false;
    }
    return true;
}

/**
 * Draw a rectangle on framebuffer. 
 *
 * \param x1       First horizontal corner (column).
 * \param y1       First vertical corner (row).
 * \param x2       Second horizontal corner (column).
 * \param y2       Second vertical corner (row).
 * \param filled   drawing of rectangle should be filled.
 */
bool cVFDBitmap::Rectangle(int x1, int y1, int x2, int y2, bool filled) {

    if (!filled) {
        return HLine(x1, y1, x2)
            && VLine(x1, y1, y2)
            && HLine(x1, y2, x2)
            && VLine(x2, y1, y2);
    } else {
        sort(y1,y2);

        for (int y = y1; y <= y2; y++) {
            if(!HLine(x1, y, x2))
              return false;
        }
        return true;
    }
}

