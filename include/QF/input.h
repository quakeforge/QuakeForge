/*
	input.h

	External (non-keyboard) input devices

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

#ifndef __QF_input_h
#define __QF_input_h

#include "QF/keys.h"

typedef struct in_axisinfo_s {
	int         deviceid;
	int         axis;
	int         value;
	int         min;
	int         max;
} in_axisinfo_t;

typedef struct in_buttoninfo_s {
	int         deviceid;
	int         button;
	int         state;
} in_buttoninfo_t;

#include "QF/input/binding.h"
#include "QF/input/imt.h"

#ifndef __QFCC__

typedef struct {
	vec3_t angles;
	vec3_t position;
} viewdelta_t;

struct qf_fd_set;

typedef struct in_driver_s {
	void (*init) (void *data);
	void (*shutdown) (void *data);

	void (*set_device_event_data) (void *device, void *event_data, void *data);
	void *(*get_device_event_data) (void *device, void *data);

	// The driver must provide either both or none of add_select and
	// chec_select.
	void (*add_select) (struct qf_fd_set *fdset, int *maxfd, void *data);
	void (*check_select) (struct qf_fd_set *fdset, void *data);
	// Generally musually exclusive with add_select/check_select
	void (*process_events) (void *data);

	void (*clear_states) (void *data);
	void (*grab_input) (void *data, int grab);

	void (*axis_info) (void *data, void *device, in_axisinfo_t *axes,
					   int *numaxes);
	void (*button_info) (void *data, void *device, in_buttoninfo_t *buttons,
						 int *numbuttons);
	// null means either invalid number or the name is not known
	const char *(*get_axis_name) (void *data, void *device, int axis_num);
	const char *(*get_button_name) (void *data, void *device, int button_num);
	// -1 for invalid name
	int (*get_axis_num) (void *data, void *device, const char *axis_name);
	int (*get_button_num) (void *data, void *device, const char *button_name);
} in_driver_t;

typedef struct in_device_s {
	int         driverid;
	void       *device;
	const char *name;
	const char *id;
	void       *event_data;
} in_device_t;

extern viewdelta_t viewdelta;

#define freelook (in_mlook.state & 1 || in_freelook->int_val)

struct cvar_s;

int IN_RegisterDriver (in_driver_t *driver, void *data);
void IN_DriverData (int handlle, void *data);
void IN_Init (struct cbuf_s *cbuf);
void IN_Init_Cvars (void);
struct plitem_s;
void IN_SaveConfig (struct plitem_s *config);
void IN_LoadConfig (struct plitem_s *config);

int IN_AddDevice (int driver, void *device, const char *name, const char *id);
void IN_RemoveDevice (int devid);

void IN_SendConnectedDevices (void);
const char *IN_GetDeviceName (int devid) __attribute__((pure));
const char *IN_GetDeviceId (int devid) __attribute__((pure));
void IN_SetDeviceEventData (int devid, void *data);
void *IN_GetDeviceEventData (int devid);
int IN_AxisInfo (int devid, in_axisinfo_t *axes, int *numaxes);
int IN_ButtonInfo (int devid, in_buttoninfo_t *button, int *numbuttons);
const char *IN_GetAxisName (int devid, int axis_num);
const char *IN_GetButtonName (int devid, int button_num);
int IN_GetAxisNumber (int devid, const char *axis_name);
int IN_GetButtonNumber (int devid, const char *button_name);

void IN_ProcessEvents (void);

void IN_UpdateGrab (struct cvar_s *);

void IN_ClearStates (void);

void IN_Move (void); // FIXME: was cmduser_t?
// add additional movement on top of the keyboard move cmd

extern struct cvar_s		*in_grab;
extern struct cvar_s		*in_amp;
extern struct cvar_s		*in_pre_amp;
extern struct cvar_s		*m_filter;
extern struct cvar_s		*in_mouse_accel;
extern struct cvar_s		*in_freelook;
extern struct cvar_s		*lookstrafe;

extern qboolean 	in_mouse_avail;
extern float		in_mouse_x, in_mouse_y;


#endif

#endif//__QF_input_h
