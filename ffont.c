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

#include <vdr/tools.h>
#include "ffont.h"

// --- cVFDFont ---------------------------------------------------------

#define KERNING_UNKNOWN  (-10000)

cVFDGlyph::cVFDGlyph(uint CharCode, FT_GlyphSlotRec_ *GlyphData)
{
  charCode = CharCode;
  advanceX = GlyphData->advance.x >> 6;
  advanceY = GlyphData->advance.y >> 6;
  left = GlyphData->bitmap_left;
  top = GlyphData->bitmap_top;
  width = GlyphData->bitmap.width;
  rows = GlyphData->bitmap.rows;
  pitch = GlyphData->bitmap.pitch;
  bitmap = MALLOC(uchar, rows * pitch);
  memcpy(bitmap, GlyphData->bitmap.buffer, rows * pitch);
}

cVFDGlyph::~cVFDGlyph()
{
  free(bitmap);
}

int cVFDGlyph::GecVFDKerningCache(uint PrevSym) const
{
  for (int i = kerningCache.Size(); --i > 0; ) {
      if (kerningCache[i].prevSym == PrevSym)
         return kerningCache[i].kerning;
      }
  return KERNING_UNKNOWN;
}

void cVFDGlyph::SecVFDKerningCache(uint PrevSym, int Kerning)
{
  kerningCache.Append(cVFDKerning(PrevSym, Kerning));
}



cVFDFont::cVFDFont(const char *Name, int CharHeight, int CharWidth)
{
  height = 0;
  bottom = 0;
  int error = FT_Init_FreeType(&library);
  if (!error) {
     error = FT_New_Face(library, Name, 0, &face);
     if (!error) {
        if (face->num_fixed_sizes && face->available_sizes) { // fixed font
           // TODO what exactly does all this mean?
           height = face->available_sizes->height;
           for (uint sym ='A'; sym < 'z'; sym++) { // search for descender for fixed font FIXME
               FT_UInt glyph_index = FT_Get_Char_Index(face, sym);
               error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
               if (!error) {
                  error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
                  if (!error) {
                     if (face->glyph->bitmap.rows-face->glyph->bitmap_top > bottom)
                        bottom = face->glyph->bitmap.rows-face->glyph->bitmap_top;
                     }
                  else
                     esyslog("targaVFD: FreeType: error %d in FT_Render_Glyph", error);
                  }
               else
                  esyslog("targaVFD: FreeType: error %d in FT_Load_Glyph", error);
               }
           }
        else {
           error = FT_Set_Char_Size(face, // handle to face object
                                    CharWidth  << 6, // CharWidth in 1/64th of points
                                    CharHeight << 6, // CharHeight in 1/64th of points
                                    CharWidth > 8 ? 64 : 80,    // horizontal device resolution
                                    72);   // vertical device resolution
           if (!error) {
              height = ((face->size->metrics.ascender-face->size->metrics.descender) + 63) / 64;
              bottom = abs((face->size->metrics.descender - 63) / 64);
              }
           else
              esyslog("targaVFD: FreeType: error %d during FT_Set_Char_Size (font = %s)\n", error, Name);
           }
        }
     else
        esyslog("targaVFD: FreeType: load error %d (font = %s)", error, Name);
     }
  else
     esyslog("targaVFD: FreeType: initialization error %d (font = %s)", error, Name);
}

cVFDFont::~cVFDFont()
{
  FT_Done_Face(face);
  FT_Done_FreeType(library);
}

int cVFDFont::Kerning(cVFDGlyph *Glyph, uint PrevSym) const
{
  int kerning = 0;
  if (Glyph && PrevSym) {
     kerning = Glyph->GecVFDKerningCache(PrevSym);
     if (kerning == KERNING_UNKNOWN) {
        FT_Vector delta;
        FT_UInt glyph_index = FT_Get_Char_Index(face, Glyph->CharCode());
        FT_UInt glyph_index_prev = FT_Get_Char_Index(face, PrevSym);
        FT_Get_Kerning(face, glyph_index_prev, glyph_index, FT_KERNING_DEFAULT, &delta);
        kerning = delta.x / 64;
        Glyph->SecVFDKerningCache(PrevSym, kerning);
        }
     }
  return kerning;
}

cVFDGlyph* cVFDFont::Glyph(uint CharCode) const
{
  // Non-breaking space:
  if (CharCode == 0xA0)
     CharCode = 0x20;

  // Lookup in cache:
  cList<cVFDGlyph> *glyphCache = &glyphCacheMonochrome;
  for (cVFDGlyph *g = glyphCache->First(); g; g = glyphCache->Next(g)) {
      if (g->CharCode() == CharCode)
         return g;
      }

  FT_UInt glyph_index = FT_Get_Char_Index(face, CharCode);

  // Load glyph image into the slot (erase previous one):
  int error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
  if (error)
     esyslog("targaVFD: FreeType: error during FT_Load_Glyph");
  else {
#if ((FREETYPE_MAJOR == 2 && FREETYPE_MINOR == 1 && FREETYPE_PATCH >= 7) \
  || (FREETYPE_MAJOR == 2 && FREETYPE_MINOR == 2 && FREETYPE_PATCH <= 1))
     if (CharCode == 32) // workaround for libfreetype bug
        error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
     else
#endif
     error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO);
     if (error)
        esyslog("targaVFD: FreeType: error during FT_Render_Glyph %d, %d\n", CharCode, glyph_index);
     else { //new bitmap
        cVFDGlyph *Glyph = new cVFDGlyph(CharCode, face->glyph);
        glyphCache->Add(Glyph);
        return Glyph;
        }
     }
#define UNKNOWN_GLYPH_INDICATOR '?'
  if (CharCode != UNKNOWN_GLYPH_INDICATOR)
     return Glyph(UNKNOWN_GLYPH_INDICATOR);
  return NULL;
}

int cVFDFont::Width(uint c) const
{
  cVFDGlyph *g = Glyph(c);
  return g ? g->AdvanceX() : 0;
}

int cVFDFont::Width(const char *s) const
{
  int w = 0;
  if (s) {
     uint prevSym = 0;
     while (*s) {
           int sl = Utf8CharLen(s);
           uint sym = Utf8CharGet(s, sl);
           s += sl;
           cVFDGlyph *g = Glyph(sym);
           if (g)
              w += g->AdvanceX() + Kerning(g, prevSym);
           prevSym = sym;
           }
     }
  return w;
}

int cVFDFont::DrawText(cVFDBitmap *Bitmap, int x, int y, const char *s, int Width) const
{
  if (s && height) { // checking height to make sure we actually have a valid font
     uint prevSym = 0;
     while (*s) {
           int sl = Utf8CharLen(s);
           uint sym = Utf8CharGet(s, sl);
           s += sl;
           cVFDGlyph *g = Glyph(sym);
           if (!g)
              continue;
           int kerning = Kerning(g, prevSym);
           prevSym = sym;
           uchar *buffer = g->Bitmap();
           int symWidth = g->Width();
           if (Width && x + symWidth + g->Left() + kerning - 1 > Width)
              return x; // we don't draw partial characters
           if (x + symWidth + g->Left() + kerning > 0) {
              for (int row = 0; row < g->Rows(); row++) {
                  for (int pitch = 0; pitch < g->Pitch(); pitch++) {
                      uchar bt = *(buffer + (row * g->Pitch() + pitch));
                      for (int col = 0; col < 8 && col + pitch * 8 <= symWidth; col++) {
                         if (bt & 0x80)
                            Bitmap->SetPixel(x + col + pitch * 8 + g->Left() + kerning,
                                             y + row + (height - Bottom() - g->Top()));
                         bt <<= 1;
                         }
                      }
                  }
              }
           x += g->AdvanceX() + kerning;
           if (x > Bitmap->Width() - 1)
              return x - (g->AdvanceX() + kerning);
           }
        return x;
     }
  return 0;
}


