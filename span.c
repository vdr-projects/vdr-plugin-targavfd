/*
 * targavfd plugin for VDR (C++)
 *
 * (C) 2010 Andreas Brachold <vdr07 AT deltab de>
 *     Span handling suggest by span-plugin
 *
 * This targavfd plugin is free software: you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as published 
 * by the Free Software Foundation, version 3 of the License.
 *
 * See the files README and COPYING for details.
 *
 */

#include "targavfd.h"
#include "span.h"

/*
 * Spectrum Analyzer visualization
 * need as middleware span-plugin
 */
bool cVFDWatch::RenderSpectrumAnalyzer()
{
  bool bDraw = false;
  /*if ( theSetup.m_bEnableSpectrumAnalyzer )*/ {
    if (cPluginManager::CallFirstService(SPAN_GET_BAR_HEIGHTS_ID, NULL)) {
      Span_GetBarHeights_v1_0 gbh;

      int bands = 19;
      int falloff = 8;
      int i,x,y;

      unsigned int *barHeights = new unsigned int[bands];
      unsigned int *barHeightsLeftChannel = new unsigned int[bands];
      unsigned int *barHeightsRightChannel = new unsigned int[bands];
      unsigned int volumeLeftChannel;
      unsigned int volumeRightChannel;
      unsigned int volumeBothChannels;
      unsigned int *barPeaksBothChannels = new unsigned int[bands];
      unsigned int *barPeaksLeftChannel  = new unsigned int[bands];
      unsigned int *barPeaksRightChannel = new unsigned int[bands];

      gbh.bands                     = bands;
      gbh.barHeights                = barHeights;
      gbh.barHeightsLeftChannel     = barHeightsLeftChannel;
      gbh.barHeightsRightChannel    = barHeightsRightChannel;
      gbh.volumeLeftChannel         = &volumeLeftChannel;
      gbh.volumeRightChannel        = &volumeRightChannel;
      gbh.volumeBothChannels        = &volumeBothChannels;
      gbh.name                      = "targavfd";
      gbh.falloff                   = falloff;
      gbh.barPeaksBothChannels      = barPeaksBothChannels;
      gbh.barPeaksLeftChannel       = barPeaksLeftChannel;
      gbh.barPeaksRightChannel      = barPeaksRightChannel;

      if ( cPluginManager::CallFirstService(SPAN_GET_BAR_HEIGHTS_ID, &gbh )) {

        int barWidth = (this->Width() - bands)/bands;
        int saEndY = this->Height();
  
        this->clear(); 

        for ( i=0; i < bands; i++ ) {
          // draw bar
          y = (barHeights[i]*(saEndY))/SPAN_HEIGHT;
          x = (barWidth*i) + i;
          this->Rectangle(x,
              saEndY,
              x + barWidth - 1,
              saEndY - y,
              true);

          // draw peak
          y = (barPeaksBothChannels[i]*(saEndY))/SPAN_HEIGHT;
          if ( y   > 0) {
            this->Rectangle(x,
                saEndY - y,
                x + barWidth - 1,
                saEndY - y,
                true);
          }
        }
        bDraw = true;
      }
      delete [] barHeights;
      delete [] barHeightsLeftChannel;
      delete [] barHeightsRightChannel;
      delete [] barPeaksBothChannels;
      delete [] barPeaksLeftChannel;
      delete [] barPeaksRightChannel;
    }
  }
  return bDraw;
}

/*
 * Report presence of Spectrum Analyzer visualization
 * from span-plugin requested 
 */
bool cPluginTargaVFD::Service(const char *Id, void *Data) {
  if (Id && strcmp(Id, SPAN_CLIENT_CHECK_ID) == 0) {
    if ( /*theSetup.m_bEnableSpectrumAnalyzer &&*/ (Data != NULL) ) {
      *((Span_Client_Check_1_0*)Data)->isActive = true;
    }
    return true;
  }
  return false;
}


