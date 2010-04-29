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

#ifndef __VFD_BITMAP_H___
#define __VFD_BITMAP_H___

class cVFDBitmap  {
  int height;
  int width;
  unsigned int bytesPerLine;
  uchar *bitmap;
protected:
  cVFDBitmap();
public:
  cVFDBitmap( int w, int h );
  
  virtual ~cVFDBitmap();
  cVFDBitmap& operator = (const cVFDBitmap& x);
  bool operator == (const cVFDBitmap& x) const;

  void clear();
  int Height() const { return height; }
  int Width() const { return width; }
  bool SetPixel(int x, int y);

  uchar * getBitmap() const { return bitmap; };
};


#endif

