/*
	cl_chase.c

	chase camera support

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/keys.h"
#include "QF/input.h"
#include "QF/mathlib.h"

#include "QF/plugin/vid_render.h"
#include "QF/scene/transform.h"

#include "world.h"

#include "client/chase.h"
#include "client/input.h"
#include "client/view.h"


vec4f_t     camera_origin = {0,0,0,1};
vec4f_t     player_origin = {0,0,0,1};
vec4f_t     player_angles = {0,0,0,1};

vec3_t      camera_angles = {0,0,0};

vec3_t      chase_angles;
vec3_t      chase_dest;
vec3_t      chase_dest_angles;
vec3_t      chase_pos;

cvar_t     *chase_back;
cvar_t     *chase_up;
cvar_t     *chase_right;
cvar_t     *chase_active;


void
Chase_Init_Cvars (void)
{
	chase_back = Cvar_Get ("chase_back", "100", CVAR_NONE, NULL, "None");
	chase_up = Cvar_Get ("chase_up", "16", CVAR_NONE, NULL, "None");
	chase_right = Cvar_Get ("chase_right", "0", CVAR_NONE, NULL, "None");
	chase_active = Cvar_Get ("chase_active", "0", CVAR_NONE, NULL, "None");
}

void
Chase_Reset (void)
{
	// for respawning and teleporting
	// start position 12 units behind head
}

static inline vec4f_t
TraceLine (chasestate_t *cs, vec4f_t start, vec4f_t end)
{
	trace_t     trace;

	memset (&trace, 0, sizeof (trace));
	trace.fraction = 1;
	MOD_TraceLine (cs->worldmodel->brush.hulls, 0, &start[0], &end[0], &trace);

	return (vec4f_t) {trace.endpos[0], trace.endpos[1], trace.endpos[2], 1};
}

void
Chase_Update (chasestate_t *cs)
{
	viewstate_t *vs = cs->viewstate;
	float       pitch, yaw, fwd;
	vec4f_t     move = {};
	vec4f_t	    forward = {}, up = {}, right = {}, stop = {}, dir = {};

	// lazy camera, look toward player entity

	if (chase_active->int_val == 2 || chase_active->int_val == 3) {
		// control camera angles with key/mouse/joy-look
		vec3_t      d;
		VectorSubtract (vs->player_angles, player_angles, d);
		VectorAdd (camera_angles, d, camera_angles);

		if (chase_active->int_val == 2) {
			camera_angles[PITCH] = bound (-60, camera_angles[PITCH], 60);
		}

		// move camera, it's not enough to just change the angles because
		// the angles are automatically changed to look toward the player

		if (chase_active->int_val == 3) {
			player_origin = vs->player_origin;
		}

		AngleVectors (camera_angles, &forward[0], &right[0], &up[0]);
		camera_origin = player_origin - chase_back->value * forward;

		if (chase_active->int_val == 2) {
			player_origin = vs->player_origin;

			// don't let camera get too low
			if (camera_origin[2] < player_origin[2] + chase_up->value) {
				camera_origin[2] = player_origin[2] + chase_up->value;
			}
		}

		// don't let camera get too far from player

		dir = camera_origin - player_origin;
		forward = normalf (dir);

		if (magnitudef (dir)[0] > chase_back->value) {
			camera_origin = player_origin + forward * chase_back->value;
		}

		// check for walls between player and camera

		camera_origin += 8 * forward;
		stop = TraceLine (cs, player_origin, camera_origin);
		if (magnitude3f (stop)[0] != 0) {
			camera_origin = stop - forward;
		}

		dir = camera_origin - vs->player_origin;
		forward = normalf (dir);

		if (chase_active->int_val == 2) {
			if (dir[1] == 0 && dir[0] == 0) {
				// look straight up or down
//				camera_angles[YAW] = r_data->refdef->viewstate->angles[YAW];
				if (dir[2] > 0)
					camera_angles[PITCH] = 90;
				else
					camera_angles[PITCH] = 270;
			} else {
				yaw = (atan2 (dir[1], dir[0]) * 180 / M_PI);
				if (yaw < 0)
					yaw += 360;
				if (yaw < 180)
					yaw += 180;
				else
					yaw -= 180;
				camera_angles[YAW] = yaw;

				fwd = sqrt (dir[0] * dir[0] + dir[1] * dir[1]);
				pitch = (atan2 (dir[2], fwd) * 180 / M_PI);
				if (pitch < 0)
					pitch += 360;
				camera_angles[PITCH] = pitch;
			}
		}

		vec4f_t     rotation;
		AngleQuat (camera_angles, &rotation[0]);//FIXME
		Transform_SetWorldRotation (vs->camera_transform, rotation);
		Transform_SetWorldPosition (vs->camera_transform, camera_origin);

		// get basic movement from keyboard

//		VectorCopy (vs->player_angles, cmd.angles);

		if (in_strafe.state & 1) {
			move[SIDE] += cl_sidespeed->value * IN_ButtonState (&in_right);
			move[SIDE] -= cl_sidespeed->value * IN_ButtonState (&in_left);
		}
		move[SIDE] += cl_sidespeed->value * IN_ButtonState (&in_moveright);
		move[SIDE] -= cl_sidespeed->value * IN_ButtonState (&in_moveleft);

		if (!(in_klook.state & 1)) {
			move[FORWARD] += cl_forwardspeed->value
				* IN_ButtonState (&in_forward);
			move[FORWARD] -= cl_backspeed->value * IN_ButtonState (&in_back);
		}
		if (in_speed.state & 1) {
			move *= cl_movespeedkey->value;
		}

		// mouse and joystick controllers add to movement
		VectorSet (0, vs->player_angles[1] - camera_angles[1], 0, dir);
		AngleVectors (&dir[0], &forward[0], &right[0], &up[0]); //FIXME
		//forward *= viewdelta.position[2] * m_forward->value; FIXME
		//right *= viewdelta.position[0] * m_side->value; FIXME
		dir = forward + right;
		move[FORWARD] += dir[0];
		move[SIDE]    -= dir[1];

		VectorSet (0, camera_angles[1], 0, dir);
		AngleVectors (&dir[0], &forward[0], &right[0], &up[0]); //FIXME

		VectorScale (forward, move[FORWARD], forward);
		VectorScale (right,   move[SIDE],    right);
		VectorAdd   (forward, right, dir);

		if (dir[1] || dir[0]) {
			vs->player_angles[YAW] = (atan2 (dir[1], dir[0]) * 180 / M_PI);
			if (vs->player_angles[YAW] <   0) {
				vs->player_angles[YAW] += 360;
			}
		}

		vs->player_angles[PITCH] = 0;

		// remember the new angle to calculate the difference next frame
		VectorCopy (vs->player_angles, player_angles);

		return;
	}

	// regular camera, faces same direction as player

	//FIXME
	AngleVectors (vs->player_angles, &forward[0], &right[0], &up[0]);

	// calc exact destination
	camera_origin = vs->player_origin
		- forward * chase_back->value - right * chase_right->value;
	// chase_up is world up
	camera_origin[2] += chase_up->value;

	// check for walls between player and camera
	stop = TraceLine (cs, vs->player_origin, camera_origin);
	if (magnitude3f (stop)[0] != 0) {
		camera_origin = stop + forward * 8;
	}

	Transform_SetWorldPosition (vs->camera_transform, camera_origin);
}
