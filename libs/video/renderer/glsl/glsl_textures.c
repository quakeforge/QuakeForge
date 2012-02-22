/*
	glsl_textures.c

	Texture format setup.

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2001 Ragnvald Maartmann-Moe IV

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

#define NH_DEFINE
#include "namehack.h"

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/mathlib.h"
#include "QF/model.h"
#include "QF/sys.h"
#include "QF/vrect.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_textures.h"

struct scrap_s {
	GLuint      tnum;
	int         size;	// in pixels, for now, always square, power of 2
	int         format;
	int         bpp;
	vrect_t    *free_rects;
	vrect_t    *rects;
	subpic_t   *subpics;
	struct scrap_s *next;
};

static scrap_t *scrap_list;

static int max_tex_size;

int
GLSL_LoadQuakeTexture (const char *identifier, int width, int height,
					   byte *data)
{
	GLuint      tnum;

	qfeglGenTextures (1, &tnum);
	qfeglBindTexture (GL_TEXTURE_2D, tnum);
	qfeglTexImage2D (GL_TEXTURE_2D, 0, GL_LUMINANCE,
					width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qfeglGenerateMipmap (GL_TEXTURE_2D);

	return tnum;
}

static void
GLSL_Resample8BitTexture (unsigned char *in, int inwidth, int inheight,
						unsigned char *out, int outwidth, int outheight)
{
	// Improvements here should be mirrored in build_skin_8 in gl_skin.c
	unsigned char *inrow;
	int            i, j;
	unsigned int   frac, fracstep;

	if (!outwidth || !outheight)
		return;
	fracstep = inwidth * 0x10000 / outwidth;
	for (i = 0; i < outheight; i++, out += outwidth) {
		inrow = in + inwidth * (i * inheight / outheight);
		frac = fracstep >> 1;
		for (j = 0; j < outwidth; j ++) {
			out[j] = inrow[frac >> 16];
			frac += fracstep;
		}
	}
}

static void
GLSL_Mipmap8BitTexture (const byte *src, unsigned width, unsigned height,
					  byte *mip)
{
	unsigned    mw = width >> 1;
	unsigned    mh = height >> 1;
	unsigned    i, j;

	mw = max (mw, 1);
	mh = max (mh, 1);

	for (j = 0; j < mh; j++) {
		for (i = 0; i < mw; i++) {
			*mip++ = src [j * 2 * width + i * 2];
		}
	}
}

int
GLSL_LoadQuakeMipTex (const texture_t *tex)
{
	unsigned    swidth, sheight;
	GLuint      tnum;
	byte       *data = (byte *) tex;
	byte       *buffer = 0;
	byte       *scaled;
	int         lod;

	for (swidth = 1; swidth < tex->width; swidth <<= 1);
	for (sheight = 1; sheight < tex->height; sheight <<= 1);

	qfeglGenTextures (1, &tnum);
	qfeglBindTexture (GL_TEXTURE_2D, tnum);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
					   GL_NEAREST_MIPMAP_NEAREST);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	if (swidth != tex->width || sheight != tex->height)
		buffer = malloc (swidth * sheight);

	// preshift so swidth and sheight are the correct sizes end the end of
	// the loop
	swidth <<= 1;
	sheight <<= 1;
	for (lod = 0; lod < MIPLEVELS; lod++) {
		// preshift so swidth and sheight are the correct sizes end the end of
		// the loop
		swidth >>= 1;
		sheight >>= 1;
		swidth = max (swidth, 1);
		sheight = max (sheight, 1);
		if (buffer) {
			unsigned    w = tex->width;
			unsigned    h = tex->height;

			w = max (w >> lod, 1);
			h = max (h >> lod, 1);
			GLSL_Resample8BitTexture (data + tex->offsets[lod], w, h,
									buffer, swidth, sheight);
			scaled = buffer;
		} else {
			scaled = data + tex->offsets[lod];
		}
		qfeglTexImage2D (GL_TEXTURE_2D, lod, GL_LUMINANCE, swidth, sheight,
						0, GL_LUMINANCE, GL_UNSIGNED_BYTE, scaled);
	}
	if (swidth > 1 || sheight > 1) {
		// mipmap will hold the reduced image, so this is more than enough
		byte       *mipmap = malloc (swidth * sheight);
		byte       *mip = mipmap;
		while (swidth > 1 || sheight > 1) {
			// scaled holds the source of the last lod uploaded
			GLSL_Mipmap8BitTexture (scaled, swidth, sheight, mip);
			swidth >>= 1;
			sheight >>= 1;
			swidth = max (swidth, 1);
			sheight = max (sheight, 1);
			qfeglTexImage2D (GL_TEXTURE_2D, lod, GL_LUMINANCE, swidth, sheight,
							0, GL_LUMINANCE, GL_UNSIGNED_BYTE, mip);
			scaled = mip;
			mip += swidth * sheight;
			lod++;
		}
		free (mipmap);
	}
	if (buffer)
		free (buffer);
	return tnum;
}

int
GLSL_LoadRGBTexture (const char *identifier, int width, int height, byte *data)
{
	GLuint      tnum;

	qfeglGenTextures (1, &tnum);
	qfeglBindTexture (GL_TEXTURE_2D, tnum);
	qfeglTexImage2D (GL_TEXTURE_2D, 0, GL_RGB,
					width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qfeglGenerateMipmap (GL_TEXTURE_2D);

	return tnum;
}

void
GLSL_ReleaseTexture (int tex)
{
	GLuint      tnum = tex;
	qfeglDeleteTextures (1, &tnum);
}

static void
glsl_scraps_f (void)
{
	scrap_t    *scrap;
	vrect_t    *rect;
	int         area;

	if (!scrap_list) {
		Sys_Printf ("No scraps\n");
		return;
	}
	for (scrap = scrap_list; scrap; scrap = scrap->next) {
		for (rect = scrap->free_rects, area = 0; rect; rect = rect->next)
			area += rect->width * rect->height;
		Sys_Printf ("tnum=%u size=%d format=%04x bpp=%d free=%d%%\n",
					scrap->tnum, scrap->size, scrap->format, scrap->bpp,
					area * 100 / (scrap->size * scrap->size));
		if (Cmd_Argc () > 1) {
			for (rect = scrap->rects, area = 0; rect; rect = rect->next)
				Sys_Printf ("%d %d %d %d\n", rect->x, rect->y,
							rect->width, rect->height);
		}
	}
}

void
GLSL_TextureInit (void)
{
	qfeglGetIntegerv (GL_MAX_TEXTURE_SIZE, &max_tex_size);
	Sys_MaskPrintf (SYS_GLSL, "max texture size: %d\n", max_tex_size);

	Cmd_AddCommand ("glsl_scraps", glsl_scraps_f, "Dump GLSL scrap stats");
}

scrap_t *
GLSL_CreateScrap (int size, int format)
{
	int         i;
	int         bpp;
	scrap_t    *scrap;
	byte       *data;

	for (i = 0; i < 16; i++)
		if (size <= 1 << i)
			break;
	size = 1 << i;
	size = min (size, max_tex_size);
	switch (format) {
		case GL_ALPHA:
		case GL_LUMINANCE:
			bpp = 1;
			break;
		case GL_LUMINANCE_ALPHA:
			bpp = 2;
			break;
		case GL_RGB:
			bpp = 3;
			break;
		case GL_RGBA:
			bpp = 4;
			break;
		default:
			Sys_Error ("GL_CreateScrap: Invalid texture format");
	}
	scrap = malloc (sizeof (scrap_t));
	qfeglGenTextures (1, &scrap->tnum);
	scrap->size = size;
	scrap->format = format;
	scrap->bpp = bpp;
	scrap->free_rects = VRect_New (0, 0, size, size);
	scrap->rects = 0;
	scrap->subpics = 0;
	scrap->next = scrap_list;
	scrap_list = scrap;

	data = calloc (1, size * size * bpp);

	qfeglBindTexture (GL_TEXTURE_2D, scrap->tnum);
	qfeglTexImage2D (GL_TEXTURE_2D, 0, format,
					size, size, 0, format, GL_UNSIGNED_BYTE, data);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	//FIXME parameterize (linear for lightmaps)
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qfeglGenerateMipmap (GL_TEXTURE_2D);
	free (data);

	return scrap;
}

void
GLSL_ScrapClear (scrap_t *scrap)
{
	vrect_t    *t;
	subpic_t   *sp;

	while (scrap->free_rects) {
		t = scrap->free_rects;
		scrap->free_rects = t->next;
		VRect_Delete (t);
	}
	while (scrap->rects) {
		t = scrap->rects;
		scrap->rects = t->next;
		VRect_Delete (t);
	}
	while (scrap->subpics) {
		sp = scrap->subpics;
		scrap->subpics = (subpic_t *) sp->next;
		free (sp);
	}

	scrap->free_rects = VRect_New (0, 0, scrap->size, scrap->size);
}

void
GLSL_DestroyScrap (scrap_t *scrap)
{
	scrap_t   **s;

	for (s = &scrap_list; *s; s = &(*s)->next) {
		if (*s == scrap) {
			*s = scrap->next;
			break;
		}
	}
	GLSL_ScrapClear (scrap);
	VRect_Delete (scrap->free_rects);
	GLSL_ReleaseTexture (scrap->tnum);
	free (scrap);
}

int
GLSL_ScrapTexture (scrap_t *scrap)
{
	return scrap->tnum;
}

subpic_t *
GLSL_ScrapSubpic (scrap_t *scrap, int width, int height)
{
	int         i, w, h;
	vrect_t   **t, **best;
	vrect_t    *old, *frags, *rect;
	subpic_t   *subpic;

	for (i = 0; i < 16; i++)
		if (width <= (1 << i))
			break;
	w = 1 << i;
	for (i = 0; i < 16; i++)
		if (height <= (1 << i))
			break;
	h = 1 << i;

	best = 0;
	for (t = &scrap->free_rects; *t; t = &(*t)->next) {
		if ((*t)->width < w || (*t)->height < h)
			continue;						// won't fit
		if (!best) {
			best = t;
			continue;
		}
		if ((*t)->width <= (*best)->width || (*t)->height <= (*best)->height)
			best = t;
	}
	if (!best)
		return 0;							// couldn't find a spot
	old = *best;
	*best = old->next;
	rect = VRect_New (old->x, old->y, w, h);
	frags = VRect_Difference (old, rect);
	VRect_Delete (old);
	if (frags) {
		// old was bigger than the requested size
		for (old = frags; old->next; old = old->next)
			;
		old->next = scrap->free_rects;
		scrap->free_rects = frags;
	}
	rect->next = scrap->rects;
	scrap->rects = rect;

	subpic = malloc (sizeof (subpic_t));
	*((subpic_t **) &subpic->next) = scrap->subpics;
	scrap->subpics = subpic;
	*((scrap_t **) &subpic->scrap) = scrap;
	*((vrect_t **) &subpic->rect) = rect;
	*((int *) &subpic->tnum) = scrap->tnum;
	*((int *) &subpic->width) = width;
	*((int *) &subpic->height) = height;
	*((float *) &subpic->size) = 1.0 / scrap->size;
	return subpic;
}

void
GLSL_SubpicDelete (subpic_t *subpic)
{
	scrap_t    *scrap = (scrap_t *) subpic->scrap;
	vrect_t    *rect = (vrect_t *) subpic->rect;
	vrect_t    *old, *merge;
	vrect_t   **t;
	subpic_t  **sp;

	for (sp = &scrap->subpics; *sp; sp = (subpic_t **) &(*sp)->next)
		if (*sp == subpic)
			break;
	if (*sp != subpic)
		Sys_Error ("GLSL_ScrapDelSubpic: broken subpic");
	*sp = (subpic_t *) subpic->next;
	free (subpic);
	for (t = &scrap->rects; *t; t = &(*t)->next)
		if (*t == rect)
			break;
	if (*t != rect)
		Sys_Error ("GLSL_ScrapDelSubpic: broken subpic");
	*t = rect->next;

	do {
		merge = 0;
		for (t = &scrap->free_rects; *t; t = &(*t)->next) {
			merge = VRect_Merge (*t, rect);
			if (merge) {
				old = *t;
				*t = (*t)->next;
				VRect_Delete (old);
				VRect_Delete (rect);
				rect = merge;
				break;
			}
		}
	} while (merge);
	rect->next = scrap->free_rects;
	scrap->free_rects = rect;
}

void
GLSL_SubpicUpdate (subpic_t *subpic, byte *data)
{
	scrap_t    *scrap = (scrap_t *) subpic->scrap;
	vrect_t    *rect = (vrect_t *) subpic->rect;

	qfeglBindTexture (GL_TEXTURE_2D, scrap->tnum);
	qfeglTexSubImage2D (GL_TEXTURE_2D, 0, rect->x, rect->y,
					   subpic->width, subpic->height, scrap->format,
					   GL_UNSIGNED_BYTE, data);
}
