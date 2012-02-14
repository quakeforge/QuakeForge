/*
	crosshair.c

	Crosshair static data.

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#include <stdlib.h>

#include "QF/qtypes.h"
#include "QF/wadfile.h"

#include "r_internal.h"

// NOTE: this array is INCORRECT for direct uploading in GL
// but is optimal for SW
byte crosshair_data[CROSSHAIR_WIDTH * CROSSHAIR_HEIGHT * CROSSHAIR_COUNT] = {
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

		0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
		0xff, 0xfe, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xfe, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff,
		0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

		//From FitzQuake
		255,255,255,255,255,255,255,255,
		255,255,255,  8,  9,255,255,255,
		255,255,255,  6,  8,  2,255,255,
		255,  6,  8,  8,  6,  8,  8,255,
		255,255,  2,  8,  8,  2,  2,  2,
		255,255,255,  7,  8,  2,255,255,
		255,255,255,255,  2,  2,255,255,
		255,255,255,255,255,255,255,255,
};

qpic_t *
Draw_CrosshairPic (void)
{
	qpic_t     *pic;
	byte       *data;
	int         i, j, x, y, ind;

	pic = malloc (field_offset (qpic_t, data[sizeof (crosshair_data)]));
	pic->width = CROSSHAIR_TILEX * CROSSHAIR_WIDTH;
	pic->height = CROSSHAIR_TILEY * CROSSHAIR_HEIGHT;
	// re-arrange the crosshair_data bytes so they're layed out properly for
	// subimaging
	data = crosshair_data;
	for (j = 0; j < CROSSHAIR_TILEY; j++) {
		for (i = 0; i < CROSSHAIR_TILEX; i++) {
			for (y = 0; y < CROSSHAIR_HEIGHT; y++) {
				for (x = 0; x < CROSSHAIR_WIDTH; x++) {
					ind = j * CROSSHAIR_HEIGHT + y;
					ind *= CROSSHAIR_WIDTH * CROSSHAIR_TILEX;
					ind += i * CROSSHAIR_WIDTH + x;
					pic->data[ind] = *data++;
				}
			}
		}
	}
	return pic;
}
