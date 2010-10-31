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

#ifndef __SPAN_DEFINES_H
#define __SPAN_DEFINES_H


#define SPAN_CLIENT_CHECK_ID    "Span-ClientCheck-v1.0"
#define SPAN_GET_BAR_HEIGHTS_ID "Span-GetBarHeights-v1.0"

struct Span_Client_Check_1_0 {
    bool *isActive;
	bool *isRunning;
};		

struct Span_GetBarHeights_v1_0 {
// all heights are normalized to 100(%)
	unsigned int bands;						// number of bands to compute
	unsigned int *barHeights;				// the heights of the bars of the two channels combined
	unsigned int *barHeightsLeftChannel;	// the heights of the bars of the left channel
	unsigned int *barHeightsRightChannel;	// the heights of the bars of the right channel
	unsigned int *volumeLeftChannel;		// the volume of the left channels
	unsigned int *volumeRightChannel;		// the volume of the right channels
	unsigned int *volumeBothChannels;		// the combined volume of the two channels
	const char *name;						// name of the plugin that wants to get the data
											// (must be unique for each client!)
	unsigned int falloff;                   // bar falloff value
	unsigned int *barPeaksBothChannels;     // bar peaks of the two channels combined
	unsigned int *barPeaksLeftChannel;      // bar peaks of the left channel
	unsigned int *barPeaksRightChannel;     // bar peaks of the right channel
};

// Define a "nice" (human understandable) height of the bars.
// So each client may scale the heights with the guarantee that it is represented as a percentage of 100%.
#define SPAN_HEIGHT 100

#endif
