/*
	joy.c

	Joystick input interface

	Copyright (C) 2000 David Jeffery
	Copyright (C) 2000 Jeff Teunissen <deek@dusknet.dhs.org>
	Copyright (C) 2001 Ragnvald `Despair` Maartmann-Moe IV

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

#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/mathlib.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/cmd.h"

#include "compat.h"
#include <string.h>

cvar_t     *joy_device;					// Joystick device name
cvar_t     *joy_enable;					// Joystick enabling flag
cvar_t     *joy_amp;					// Joystick amplification
cvar_t     *joy_pre_amp;				// Joystick pre-amplification

qboolean    joy_found = false;
qboolean    joy_active = false;

typedef struct {
	const char *name;
	const char *string;
} ocvar_t;

ocvar_t joy_axes_cvar_init[JOY_MAX_AXES] = {
	{"joyaxis1", "1"},
	{"joyaxis2", "2"},
	{"joyaxis3", "3"},
	{"joyaxis4", "0"},
	{"joyaxis5", "0"},
	{"joyaxis6", "0"},
	{"joyaxis7", "0"},
	{"joyaxis8", "0"}
};

struct joy_axis joy_axes[JOY_MAX_AXES];
struct joy_button joy_buttons[JOY_MAX_BUTTONS];

static void
joy_check_axis_buttons (struct joy_axis *ja, float value)
{
	struct joy_axis_button *ab;
	int         pressed = -1;
	int         i;

	// the axis button list is sorted in decending order of absolute threshold
	for (i = 0; i < ja->num_buttons; i++) {
		ab = &ja->axis_buttons[i];
		if ((value < 0) == (ab->threshold < 0)
			&& fabsf(value) >= fabsf (ab->threshold)) {
			pressed = i;
			break;

		}
	}
	// make sure any buttons that are no longer active are "released"
	for (i = 0; i < ja->num_buttons; i++) {
		if (i == pressed)
			continue;
		ab = &ja->axis_buttons[i];
		if (ab->state) {
			Key_Event (ab->key, 0, 0);
			ab->state = 0;
		}
	}
	// press the active button if there is one
	if (pressed >= 0) {
		// FIXME support repeat?
		if (!ab->state)
			Key_Event (ab->key, 0, 1);
		ab->state = 1;
	}
}

VISIBLE void
JOY_Command (void)
{
	JOY_Read ();
}

VISIBLE void
JOY_Move (void)
{
	struct joy_axis *ja;
	float       value;
	float       amp = joy_amp->value * in_amp->value;
	float       pre = joy_pre_amp->value * in_pre_amp->value;
	int         i;

	if (!joy_active || !joy_enable->int_val)
		return;

	for (i = 0; i < JOY_MAX_AXES; i++) {
		ja = &joy_axes[i];
		if (abs(ja->offset) + abs(ja->current) < abs(ja->offset) + abs(ja->deadzone))
			ja->current = -ja->offset;

		value = amp * ja->amp * (ja->offset + ja->current * pre * ja->pre_amp) / 120.0f;
		switch (ja->dest) {
			case js_none:
				// ignore axis
				break;
			case js_position:
				if (ja->current)
					viewdelta.position[ja->axis] += value;
				break;
			case js_angles:
				if (ja->current)
					viewdelta.angles[ja->axis] -= value;
				break;
			case js_button:
				joy_check_axis_buttons (ja, value);
				break;
		}
	}
}

VISIBLE void
JOY_Init (void)
{
	int     i;

	if (JOY_Open () == -1) {
		Sys_MaskPrintf (SYS_VID, "JOY: Joystick not found.\n");
		joy_found = false;
		joy_active = false;
		return;
	}

	joy_found = true;

	if (!joy_enable->int_val) {
		Sys_MaskPrintf (SYS_VID, "JOY: Joystick found, but not enabled.\n");
		joy_active = false;
		JOY_Close ();
	}

	Sys_MaskPrintf (SYS_VID, "JOY: Joystick found and activated.\n");

	// Initialize joystick if found and enabled
	for (i = 0; i < JOY_MAX_BUTTONS; i++) {
		joy_buttons[i].old = 0;
		joy_buttons[i].current = 0;
	}
	joy_active = true;
}

static void
joyamp_f (cvar_t *var)
{
	Cvar_Set (var, va ("%g", max (0.0001, var->value)));
}

typedef struct {
	const char	*name;
	js_dest_t	destnum;
} js_dests_t;

typedef struct {
	const char	*name;
	js_dest_t	optnum;
} js_opts_t;

js_dests_t js_dests[] = {
		{"none", 		js_none},								// ignore axis
		{"movement",	js_position},							// linear delta
		{"aim", 		js_angles},								// linear delta
		{"button",		js_button},								// axis button
};


js_opts_t js_opts[] = {
		{"amp",		js_amp},
		{"pre_amp",	js_pre_amp},
		{"deadzone",js_deadzone},
		{"offset",	js_offset},
		{"type",	js_type},
		{"button",	js_axis_button},
};

const char *
JOY_GetOption_c (int i)
{
	js_opts_t *opt;
	for (opt=&js_opts[0]; opt->name; opt++) {
		if ((int)opt->optnum == i)
			return opt->name;
	}

	return NULL;
}

int
JOY_GetOption_i (const char *c)
{
	js_opts_t *opt;
	for (opt=&js_opts[0]; opt->name; opt++) {
		if (!strcmp(opt->name, c))
			return opt->optnum;
	}

	return -1; //Failure code;
}

const char *
JOY_GetDest_c (int i)
{
	js_dests_t *dest;
	for (dest=&js_dests[0]; dest->name; dest++) {
		if ((int)dest->destnum == i)
			return dest->name;
	}

	return NULL;
}

int
JOY_GetDest_i (const char *c)
{
	js_dests_t *dest;
	for (dest=&js_dests[0]; dest->name; dest++) {
		if (!strcmp(dest->name, c))
			return dest->destnum;
	}

	return -1; //Failure code;
}

static void
in_joy_f (void)
{
	int i, ax, c = Cmd_Argc();

	if (c < 3) {
		Sys_Printf ("in_joy <axis#> [<var> <value>]* : Configures the joystick behaviour\n");
	}

	ax = strtol(Cmd_Argv(1), NULL ,0);

	i = 2;
	while(i<c)
	{
		int var = JOY_GetOption_i (Cmd_Argv(i++));

		switch (var) {
		case js_amp:
			joy_axes[ax].amp = strtof (Cmd_Argv(i++), NULL);
			Sys_Printf("Set axis %i's amp to %f\n", ax, joy_axes[ax].amp);
			break;
		case js_pre_amp:
			joy_axes[ax].pre_amp = strtof (Cmd_Argv(i++), NULL);
			break;
		case js_deadzone:
			joy_axes[ax].deadzone = strtol (Cmd_Argv(i++), NULL, 10);
			break;
		case js_offset:
			joy_axes[ax].offset = strtol (Cmd_Argv(i++), NULL, 10);
			break;
		case js_type:
			joy_axes[ax].dest = JOY_GetDest_i (Cmd_Argv(i++));
			joy_axes[ax].axis = strtol (Cmd_Argv(i++), NULL, 10);
			break;
		default:
			Sys_Printf ("Unknown option %s.\n", Cmd_Argv(i-1));
			break;
		}
	}
}

VISIBLE void
JOY_Init_Cvars (void)
{
	joy_device = Cvar_Get ("joy_device", "/dev/input/js0", CVAR_NONE | CVAR_ROM, 0,
						   "Joystick device");
	joy_enable = Cvar_Get ("joy_enable", "1", CVAR_NONE | CVAR_ARCHIVE, 0,
						   "Joystick enable flag");
	joy_amp = Cvar_Get ("joy_amp", "1", CVAR_NONE | CVAR_ARCHIVE, joyamp_f,
						"Joystick amplification");
	joy_pre_amp = Cvar_Get ("joy_pre_amp", "1", CVAR_NONE | CVAR_ARCHIVE,
							joyamp_f, "Joystick pre-amplification");

	Cmd_AddCommand("in_joy", in_joy_f, "Configures the joystick behaviour");
}

VISIBLE void
JOY_Shutdown (void)
{
	if (!joy_active)
		return;

	JOY_Close ();

	joy_active = false;
	joy_found = false;
}
