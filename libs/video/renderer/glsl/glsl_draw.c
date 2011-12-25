/*
	glsl_draw.c

	2D drawing support for GLSL

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/12/23

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

#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/sys.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_textures.h"

#include "gl_draw.h"

static const char quaketext_vert[] =
#include "quaketxt.vc"
;

static const char quaketext_frag[] =
#include "quaketxt.fc"
;

VISIBLE byte *draw_chars;
static dstring_t *char_queue;
static int  char_texture;
static int  qtxt_vert;
static int  qtxt_frag;
//static int  qtxt_prog;

VISIBLE qpic_t *
Draw_PicFromWad (const char *name)
{
	return 0;
}

VISIBLE qpic_t *
Draw_CachePic (const char *path, qboolean alpha)
{
	return 0;
}

VISIBLE void
Draw_TextBox (int x, int y, int width, int lines, byte alpha)
{
}

static int
compile_shader (const char *name, const char *shader_src, int type)
{
	const char *src[1];
	int         shader;
	int         compiled;

	src[0] = shader_src;
	shader = qfglCreateShader (type);
	qfglShaderSource (shader, 1, src, 0);
	qfglCompileShader (shader);
	qfglGetShaderiv (shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		dstring_t  *log = dstring_new ();
		int         size;
		qfglGetShaderiv (shader, GL_INFO_LOG_LENGTH, &size);
		log->size = size + 1;	// for terminating null
		dstring_adjust (log);
		qfglGetShaderInfoLog (shader, log->size, 0, log->str);
		qfglDeleteShader (shader);
		Sys_Printf ("Shader (%s) compile error:\n----8<----\n%s----8<----\n",
					name, log->str);
		dstring_delete (log);
		return 0;
	}
	return shader;
}

VISIBLE void
Draw_Init (void)
{
	GLuint      tnum;
	int         i;

	char_queue = dstring_new ();
	qtxt_vert = compile_shader ("quaketxt.vert", quaketext_vert,
								GL_VERTEX_SHADER);
	qtxt_frag = compile_shader ("quaketxt.frag", quaketext_frag,
								GL_FRAGMENT_SHADER);

	draw_chars = W_GetLumpName ("conchars");
	for (i = 0; i < 256 * 64; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;		// proper transparent color

	qfglGenTextures (1, &tnum);
	char_texture = tnum;
	qfglBindTexture (GL_TEXTURE_2D, char_texture);
	qfglTexImage2D (GL_TEXTURE_2D, 0, GL_LUMINANCE,
					128, 128, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, draw_chars);
}

static inline void
queue_character (int x, int y, byte chr)
{
	byte       *v;
	int         i;

	char_queue->size += 5;
	dstring_adjust (char_queue);
	v = (byte *) char_queue->str + char_queue->size - 5;
	for (i = 0; i < 4; i++) {
		*v++ = x;
		*v++ = y;
		*v++ = i & 1;
		*v++ = (i >> 1) & 1;
		*v++ = chr;
	}
}

VISIBLE void
Draw_Character (int x, int y, unsigned int chr)
{
}

VISIBLE void
Draw_String (int x, int y, const char *str)
{
}

VISIBLE void
Draw_nString (int x, int y, const char *str, int count)
{
}

void
Draw_AltString (int x, int y, const char *str)
{
}

VISIBLE void
Draw_Crosshair (void)
{
}

void
Draw_CrosshairAt (int ch, int x, int y)
{
}

VISIBLE void
Draw_Pic (int x, int y, qpic_t *pic)
{
}

VISIBLE void
Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width,
			 int height)
{
}

VISIBLE void
Draw_ConsoleBackground (int lines, byte alpha)
{
}

VISIBLE void
Draw_TileClear (int x, int y, int w, int h)
{
}

VISIBLE void
Draw_Fill (int x, int y, int w, int h, int c)
{
}

VISIBLE void
Draw_FadeScreen (void)
{
}

void
GL_FlushText (void)
{
	char_queue->size = 0;
}
