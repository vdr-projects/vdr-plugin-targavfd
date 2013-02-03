/*
 * targavfd plugin for VDR (C++)
 *
 * (C) 2010 Andreas Brachold <vdr07 AT deltab de>
 *     Glyph handling based on <vdr/font.c>
 *
 * This targavfd plugin is free software: you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as published 
 * by the Free Software Foundation, version 3 of the License.
 *
 * See the files README and COPYING for details.
 *
 */

#ifndef __VFD_FONT_H___
#define __VFD_FONT_H___

#include <vdr/config.h>
#include <vdr/font.h>
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "bitmap.h"

struct cVFDKerning {
  uint prevSym;
  int kerning;
  cVFDKerning(uint PrevSym, int Kerning = 0) { prevSym = PrevSym; kerning = Kerning; }
  };

class cVFDGlyph : public cListObject {
private:
  uint charCode;
  uchar *bitmap;
  int advanceX;
  int advanceY;
  int left;  ///< The bitmap's left bearing expressed in integer pixels.
  int top;   ///< The bitmap's top bearing expressed in integer pixels.
  int width; ///< The number of pixels per bitmap row.
  int rows;  ///< The number of bitmap rows.
  int pitch; ///< The pitch's absolute value is the number of bytes taken by one bitmap row, including padding.
  cVector<cVFDKerning> kerningCache;
public:
  cVFDGlyph(uint CharCode, FT_GlyphSlotRec_ *GlyphData);
  virtual ~cVFDGlyph();
  uint CharCode(void) const { return charCode; }
  uchar *Bitmap(void) const { return bitmap; }
  int AdvanceX(void) const { return advanceX; }
  int AdvanceY(void) const { return advanceY; }
  int Left(void) const { return left; }
  int Top(void) const { return top; }
  int Width(void) const { return width; }
  int Rows(void) const { return rows; }
  int Pitch(void) const { return pitch; }
  int GecVFDKerningCache(uint PrevSym) const;
  void SecVFDKerningCache(uint PrevSym, int Kerning);
  };


class cVFDFont : public cFont {
private:
  int height;
  int bottom;
  FT_Library library; ///< Handle to library
  FT_Face face; ///< Handle to face object
  mutable cList<cVFDGlyph> glyphCacheMonochrome;
  int Bottom(void) const { return bottom; }
  int Kerning(cVFDGlyph *Glyph, uint PrevSym) const;
  cVFDGlyph* Glyph(uint CharCode) const;
  virtual void DrawText(cBitmap*, int, int, const char*, tColor, tColor, int) const {};
#if APIVERSNUM >= 10717
  virtual void DrawText(cPixmap*, int, int, const char*, tColor, tColor, int) const {};
#endif
public:
  cVFDFont(const char *Name, int CharHeight, int CharWidth = 0);
  virtual ~cVFDFont();
  virtual int Width(uint c) const;
  virtual int Width(const char *s) const;
  virtual int Height(void) const { return height; }

  int DrawText(cVFDBitmap *Bitmap, int x, int y, const char *s, int Width) const;
};


#endif

