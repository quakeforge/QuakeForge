/*
	vid_sgl.c

	Video driver for OpenGL-using versions of SDL

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

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>
#include <SDL.h>

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/vid.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "context_sdl.h"
#include "r_cvar.h"
#include "sbar.h"

#ifdef WIN32	// FIXME: evil hack to get full DirectSound support with SDL
# include <windows.h>
# include <SDL_syswm.h>
HWND 		mainwindow;
#endif

#define	WARP_WIDTH	320
#define	WARP_HEIGHT	200

int			VID_options_items = 1;

SDL_Surface *screen = NULL;


void *
QFGL_GetProcAddress (void *handle, const char *name)
{
	return SDL_GL_GetProcAddress (name);
}

void *
QFGL_LoadLibrary (void)
{
	if (SDL_GL_LoadLibrary (gl_driver->string) != 0)
		Sys_Error ("Couldn't load OpenGL library %s!", gl_driver->string);

	return NULL;
}

static void
GL_Init (void)
{
	GL_Init_Common ();
}

void
GL_EndRendering (void)
{
	qfglFinish ();
	SDL_GL_SwapBuffers ();
	Sbar_Changed ();
}

void
VID_Init (unsigned char *palette)
{
	Uint32      flags = SDL_OPENGL;
	int         i, j;

	// Initialize the SDL library 
	if (SDL_Init (SDL_INIT_VIDEO) < 0)
		Sys_Error ("Couldn't initialize SDL: %s", SDL_GetError ());

	GL_Pre_Init ();

	VID_GetWindowSize (640, 480);

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap8 = vid_colormap;
	vid.fullbright = 256 - LittleLong (*((int *) vid.colormap8 + 2048));

	// Interpret command-line params

	// Set vid parameters
	if ((i = COM_CheckParm ("-conwidth")) != 0)
		vid.conwidth = atoi (com_argv[i + 1]);
	else
		vid.conwidth = scr_width;

	vid.conwidth &= 0xfff8;				// make it a multiple of eight
	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth * 3 / 4;

	i = COM_CheckParm ("-conheight");
	if (i != 0)						// Set console height, no smaller than 200
		vid.conheight = max (atoi (com_argv[i + 1]), 200);

	// Check if we want fullscreen
	if (vid_fullscreen->int_val) {
		flags |= SDL_FULLSCREEN;
#ifndef WIN32		// Don't annoy Mesa/3dfx folks
		// FIXME: Maybe this could be put in a different spot, but I don't
		// know where. Anyway, it's to work around a 3Dfx Glide bug.
//		Cvar_SetValue (in_grab, 1); // Needs #include "QF/input.h"
		putenv ((char *)"MESA_GLX_FX=fullscreen");
	} else {
		putenv ((char *)"MESA_GLX_FX=window");
#endif
	}

	// Setup GL Attributes
	SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
//	SDL_GL_SetAttribute (SDL_GL_STENCIL_SIZE, 0);	// Try for 0, 8
//	SDL_GL_SetAttribute (SDL_GL_STEREO, 1);			// Someday...

	for (i = 0; i < 5; i++) {
		int k;
		int color[5] = {32, 24, 16, 15, 0};
		int rgba[5][4] = {
			{8, 8, 8, 0},
			{8, 8, 8, 8},
			{5, 6, 5, 0},
			{5, 5, 5, 0},
			{5, 5, 5, 1},
		};

		SDL_GL_SetAttribute (SDL_GL_RED_SIZE, rgba[i][0]);
		SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, rgba[i][1]);
		SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, rgba[i][2]);
		SDL_GL_SetAttribute (SDL_GL_ALPHA_SIZE, rgba[i][3]);

		for (j = 0; j < 5; j++) {
			for (k = 32; k >= 16; k -= 8) {
				SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, k);
				if ((screen = SDL_SetVideoMode (scr_width, scr_height,
												color[j], flags)))
					goto success;
			}
		}
	}

	Sys_Error ("Couldn't set video mode: %s", SDL_GetError ());
	SDL_Quit ();

success:
	vid.height = vid.conheight = min (vid.conheight, scr_height);
	vid.width = vid.conwidth = min (vid.conwidth, scr_width);
	Con_CheckResize (); // Now that we have a window size, fix console

	vid.aspect = ((float) vid.height / (float) vid.width) * (4.0 / 3.0);
	vid.numpages = 2;

	GL_Init ();

	VID_SDL_GammaCheck ();
	VID_InitGamma (palette);
	VID_SetPalette (vid.palette);
	VID_Init8bitPalette ();	// Check for 3DFX Extensions and initialize them.

	Con_Printf ("Video mode %dx%d initialized.\n", scr_width, scr_height);

	vid.initialized = true;

	SDL_ShowCursor (0);		// hide the mouse pointer

#ifdef WIN32
// FIXME: EVIL thing - but needed for win32 until
// SDL_sound works better - without this DirectSound fails.

//	SDL_GetWMInfo(&info);
//	mainwindow=info.window;
	mainwindow=GetActiveWindow();
#endif

	vid.recalc_refdef = 1;				// force a surface cache flush
}
