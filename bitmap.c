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

cVFDBitmap::cVFDBitmap(int w, int h) {
  width = w;
  height = h;

  // lines are byte aligned
  bytesPerLine = (width + 7) / 8;

 	bitmap = MALLOC(uchar, bytesPerLine * height);
  clear();
}

cVFDBitmap::cVFDBitmap() {
  height = 0;
  width = 0;
  bitmap = NULL;
}

cVFDBitmap::~cVFDBitmap() {

  if(bitmap)  
    free(bitmap);
  bitmap = NULL;
}

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

    if(height && width)
    	bitmap = MALLOC(uchar, bytesPerLine * height);
  }
  if(x.bitmap)
  	memcpy(bitmap, x.bitmap, bytesPerLine * height);
  return *this;
}

bool cVFDBitmap::operator == (const cVFDBitmap& x) const {

  if(height != x.height
    || width != x.width
    || bitmap == NULL
    || x.bitmap == NULL)
    return false;
	return ((memcmp(x.bitmap, bitmap, bytesPerLine * height)) == 0);
}


void cVFDBitmap::clear() {
    if (bitmap)
      memset(bitmap, 0x00, bytesPerLine * height);
}

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

