/*
	cl_ents.c

	entity parsing and management

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
#include "QF/locs.h"
#include "QF/msg.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sys.h"

#include "qw/msg_ucmd.h"

#include "qw/bothdefs.h"
#include "cl_cam.h"
#include "cl_ents.h"
#include "cl_main.h"
#include "cl_parse.h"
#include "cl_pred.h"
#include "cl_tent.h"
#include "compat.h"
#include "d_iface.h"
#include "host.h"
#include "qw/pmove.h"
#include "clview.h"

static struct predicted_player {
	int         flags;
	qboolean    active;
	vec3_t      origin;					// predicted origin
} predicted_players[MAX_CLIENTS];


// PACKET ENTITY PARSING / LINKING ============================================

/*
	CL_ParseDelta

	Can go from either a baseline or a previous packet_entity
*/
static void
CL_ParseDelta (entity_state_t *from, entity_state_t *to, int bits)
{
	int		i;

	// set everything to the state we are delta'ing from
	*to = *from;

	to->number = bits & 511;
	bits &= ~511;

	if (bits & U_MOREBITS) {			// read in the low order bits
		i = MSG_ReadByte (net_message);
		bits |= i;
	}

	// LordHavoc: Endy neglected to mark this as being part of the QSG
	// version 2 stuff...
	if (bits & U_EXTEND1) {
		bits |= MSG_ReadByte (net_message) << 16;
		if (bits & U_EXTEND2)
			bits |= MSG_ReadByte (net_message) << 24;
	}

	to->flags = bits;

	if (bits & U_MODEL)
		to->modelindex = MSG_ReadByte (net_message);

	if (bits & U_FRAME)
		to->frame = MSG_ReadByte (net_message);

	if (bits & U_COLORMAP) {
		byte        cmap = MSG_ReadByte (net_message);
		if (cmap != to->colormap)
			to->skin = mod_funcs->Skin_SetColormap (to->skin, cmap);
		to->colormap = cmap;
	}

	if (bits & U_SKIN)
		to->skinnum = MSG_ReadByte (net_message);

	if (bits & U_EFFECTS)
		to->effects = MSG_ReadByte (net_message);

	if (bits & U_ORIGIN1)
		to->origin[0] = MSG_ReadCoord (net_message);

	if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle (net_message);

	if (bits & U_ORIGIN2)
		to->origin[1] = MSG_ReadCoord (net_message);

	if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle (net_message);

	if (bits & U_ORIGIN3)
		to->origin[2] = MSG_ReadCoord (net_message);

	if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle (net_message);

	if (bits & U_SOLID) {
		// FIXME
	}

	if (!(bits & U_EXTEND1))
		return;

	// LordHavoc: Endy neglected to mark this as being part of the QSG
	// version 2 stuff... rearranged it and implemented missing effects
// Ender (QSG - Begin)
	if (bits & U_ALPHA)
		to->alpha = MSG_ReadByte (net_message);
	if (bits & U_SCALE)
		to->scale = MSG_ReadByte (net_message);
	if (bits & U_EFFECTS2)
		to->effects = (to->effects & 0xFF) | (MSG_ReadByte (net_message) << 8);
	if (bits & U_GLOWSIZE)
		to->glow_size = MSG_ReadByte (net_message);
	if (bits & U_GLOWCOLOR)
		to->glow_color = MSG_ReadByte (net_message);
	if (bits & U_COLORMOD)
		to->colormod = MSG_ReadByte (net_message);

	if (!(bits & U_EXTEND2))
		return;

	if (bits & U_FRAME2)
		to->frame = (to->frame & 0xFF) | (MSG_ReadByte (net_message) << 8);
// Ender (QSG - End)
}

static void
FlushEntityPacket (void)
{
	entity_state_t	olde, newe;
	int				word;

	Sys_MaskPrintf (SYS_DEV, "FlushEntityPacket\n");

	memset (&olde, 0, sizeof (olde));

	cl.validsequence = 0;				// can't render a frame
	cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].invalid = true;

	// read it all, but ignore it
	while (1) {
		word = MSG_ReadShort (net_message);
		if (net_message->badread) {		// something didn't parse right...
			Host_Error ("msg_badread in packetentities");
			return;
		}

		if (!word)
			break;						// done

		CL_ParseDelta (&olde, &newe, word);
	}
}

void
CL_ParsePacketEntities (qboolean delta)
{
	byte		from;
	int			oldindex, newindex, newnum, oldnum, oldpacket, newpacket, word;
	packet_entities_t *oldp, *newp, dummy;
	qboolean	full;

	newpacket = cls.netchan.incoming_sequence & UPDATE_MASK;
	newp = &cl.frames[newpacket].packet_entities;
	cl.frames[newpacket].invalid = false;

	if (delta) {
		from = MSG_ReadByte (net_message);

		oldpacket = cl.frames[newpacket].delta_sequence;
		if (cls.demoplayback2)
			from = oldpacket = (cls.netchan.incoming_sequence - 1);
		if ((from & UPDATE_MASK) != (oldpacket & UPDATE_MASK))
			Sys_MaskPrintf (SYS_DEV, "WARNING: from mismatch\n");
	} else
		oldpacket = -1;

	full = false;
	if (oldpacket != -1) {
		if (cls.netchan.outgoing_sequence - oldpacket >= UPDATE_BACKUP - 1) {
			// we can't use this, it is too old
			FlushEntityPacket ();
			return;
		}
		cl.validsequence = cls.netchan.incoming_sequence;
		oldp = &cl.frames[oldpacket & UPDATE_MASK].packet_entities;
	} else {	// a full update that we can start delta compressing from now
		oldp = &dummy;
		dummy.num_entities = 0;
		cl.validsequence = cls.netchan.incoming_sequence;
		full = true;
	}

	oldindex = 0;
	newindex = 0;
	newp->num_entities = 0;

	while (1) {
		word = MSG_ReadShort (net_message);
		if (net_message->badread) {		// something didn't parse right...
			Host_Error ("msg_badread in packetentities");
			return;
		}

		if (!word) {					// copy rest of ents from old packet
			while (oldindex < oldp->num_entities) {
				if (newindex >= MAX_DEMO_PACKET_ENTITIES)
					Host_Error ("CL_ParsePacketEntities: newindex == "
								"MAX_DEMO_PACKET_ENTITIES");
				newp->entities[newindex] = oldp->entities[oldindex];
				newindex++;
				oldindex++;
			}
			break;
		}
		newnum = word & 511;
		oldnum = oldindex >= oldp->num_entities ? 9999 :
			oldp->entities[oldindex].number;

		while (newnum > oldnum) {
			if (full) {
				Sys_Printf ("WARNING: oldcopy on full update");
				FlushEntityPacket ();
				return;
			}
			// copy one of the old entities over to the new packet unchanged
			if (newindex >= MAX_DEMO_PACKET_ENTITIES)
				Host_Error ("CL_ParsePacketEntities: newindex == "
							"MAX_DEMO_PACKET_ENTITIES");
			newp->entities[newindex] = oldp->entities[oldindex];
			newindex++;
			oldindex++;
			oldnum = oldindex >= oldp->num_entities ? 9999 :
				oldp->entities[oldindex].number;
		}

		if (newnum < oldnum) {						// new from baseline
			if (word & U_REMOVE) {
				if (full) {
					cl.validsequence = 0;
					Sys_Printf ("WARNING: U_REMOVE on full update\n");
					FlushEntityPacket ();
					return;
				}
				continue;
			}

			if (newindex >= MAX_DEMO_PACKET_ENTITIES)
				Host_Error ("CL_ParsePacketEntities: newindex == "
							"MAX_DEMO_PACKET_ENTITIES");
			CL_ParseDelta (&qw_entstates.baseline[newnum],
						   &newp->entities[newindex],
						   word);
			newindex++;
			continue;
		}

		if (newnum == oldnum) {					// delta from previous
			if (full) {
				cl.validsequence = 0;
				Sys_Printf ("WARNING: delta on full update");
			}
			if (word & U_REMOVE) {				// Clear the entity
				entity_t	*ent = &cl_entities[newnum];
				if (ent->efrag)
					r_funcs->R_RemoveEfrags (ent);
				memset (ent, 0, sizeof (entity_t));
				oldindex++;
				continue;
			}
			CL_ParseDelta (&oldp->entities[oldindex],
						   &newp->entities[newindex], word);
			newindex++;
			oldindex++;
		}
	}

	newp->num_entities = newindex;
}

static int
TranslateFlags (int src)
{
	int         dst = 0;

	if (src & DF_EFFECTS)
		dst |= PF_EFFECTS;
	if (src & DF_SKINNUM)
		dst |= PF_SKINNUM;
	if (src & DF_DEAD)
		dst |= PF_DEAD;
	if (src & DF_GIB)
		dst |= PF_GIB;
	if (src & DF_WEAPONFRAME)
		dst |= PF_WEAPONFRAME;
	if (src & DF_MODEL)
		dst |= PF_MODEL;

	return dst;
}

static void
CL_ParseDemoPlayerinfo (int num)
{
	int            flags, i;
	player_info_t *info;
	player_state_t *state, *prevstate;
	static player_state_t dummy;

	info = &cl.players[num];
	state = &cl.frames[parsecountmod].playerstate[num];

	state->pls.number = num;

	if (info->prevcount > cl.parsecount || !cl.parsecount) {
		prevstate = &dummy;
	} else {
		if (cl.parsecount - info->prevcount >= UPDATE_BACKUP-1)
			prevstate = &dummy;
		else
			prevstate = &cl.frames[info->prevcount
								   & UPDATE_MASK].playerstate[num];
	}
	info->prevcount = cl.parsecount;

	if (cls.findtrack && info->stats[STAT_HEALTH] != 0) {
		autocam = CAM_TRACK;
		Cam_Lock (num);
		ideal_track = num;
		cls.findtrack = false;
	}

	memcpy (state, prevstate, sizeof (player_state_t));

	flags = MSG_ReadShort (net_message);
	state->pls.flags = TranslateFlags (flags);
	state->messagenum = cl.parsecount;
	state->pls.cmd.msec = 0;
	state->pls.frame = MSG_ReadByte (net_message);
	state->state_time = parsecounttime;
	for (i=0; i <3; i++)
		if (flags & (DF_ORIGIN << i))
			state->pls.origin[i] = MSG_ReadCoord (net_message);
	for (i=0; i <3; i++)
		if (flags & (DF_ANGLES << i))
			state->pls.cmd.angles[i] = MSG_ReadAngle16 (net_message);
	if (flags & DF_MODEL)
		state->pls.modelindex = MSG_ReadByte (net_message);
	if (flags & DF_SKINNUM)
		state->pls.skinnum = MSG_ReadByte (net_message);
	if (flags & DF_EFFECTS)
		state->pls.effects = MSG_ReadByte (net_message);
	if (flags & DF_WEAPONFRAME)
		state->pls.weaponframe = MSG_ReadByte (net_message);
	VectorCopy (state->pls.cmd.angles, state->viewangles);
}

void
CL_ParsePlayerinfo (void)
{
	int            flags, msec, num, i;
	player_state_t *state;

	num = MSG_ReadByte (net_message);
	if (num > MAX_CLIENTS)
		Host_Error ("CL_ParsePlayerinfo: bad num");

	if (cls.demoplayback2) {
		CL_ParseDemoPlayerinfo (num);
		return;
	}

	state = &cl.frames[parsecountmod].playerstate[num];

	state->pls.number = num;

	flags = state->pls.flags = MSG_ReadShort (net_message);

	state->messagenum = cl.parsecount;
	MSG_ReadCoordV (net_message, state->pls.origin);

	state->pls.frame = MSG_ReadByte (net_message);

	// the other player's last move was likely some time
	// before the packet was sent out, so accurately track
	// the exact time it was valid at
	if (flags & PF_MSEC) {
		msec = MSG_ReadByte (net_message);
		state->state_time = parsecounttime - msec * 0.001;
	} else
		state->state_time = parsecounttime;

	if (flags & PF_COMMAND)
		MSG_ReadDeltaUsercmd (net_message, &nullcmd, &state->pls.cmd);

	for (i = 0; i < 3; i++) {
		if (flags & (PF_VELOCITY1 << i))
			state->pls.velocity[i] = (short) MSG_ReadShort (net_message);
		else
			state->pls.velocity[i] = 0;
	}
	if (flags & PF_MODEL)
		i = MSG_ReadByte (net_message);
	else
		i = cl_playerindex;
	state->pls.modelindex = i;

	if (flags & PF_SKINNUM)
		state->pls.skinnum = MSG_ReadByte (net_message);
	else
		state->pls.skinnum = 0;

	if (flags & PF_EFFECTS)
		state->pls.effects = MSG_ReadByte (net_message);
	else
		state->pls.effects = 0;

	if (flags & PF_WEAPONFRAME)
		state->pls.weaponframe = MSG_ReadByte (net_message);
	else
		state->pls.weaponframe = 0;

	VectorCopy (state->pls.cmd.angles, state->viewangles);

	if (cl.stdver >= 2.0 && (flags & PF_QF)) {
		// QSG2
		int         bits;
		byte        val;
		entity_t   *ent;

		ent = &cl_player_ents[num];
		bits = MSG_ReadByte (net_message);
		if (bits & PF_ALPHA) {
			val = MSG_ReadByte (net_message);
			ent->colormod[3] = val / 255.0;
		}
		if (bits & PF_SCALE) {
			val = MSG_ReadByte (net_message);
			ent->scale = val / 16.0;
		}
		if (bits & PF_EFFECTS2) {
			state->pls.effects |= MSG_ReadByte (net_message) << 8;
		}
		if (bits & PF_GLOWSIZE) {
			state->pls.glow_size = MSG_ReadByte (net_message);
		}
		if (bits & PF_GLOWCOLOR) {
			state->pls.glow_color = MSG_ReadByte (net_message);
		}
		if (bits & PF_COLORMOD) {
			float       r = 1.0, g = 1.0, b = 1.0;
			val = MSG_ReadByte (net_message);
			if (val != 255) {
				r = (float) ((val >> 5) & 7) * (1.0 / 7.0);
				g = (float) ((val >> 2) & 7) * (1.0 / 7.0);
				b = (float) (val & 3) * (1.0 / 3.0);
			}
			VectorSet (r, g, b, ent->colormod);
		}
		if (bits & PF_FRAME2) {
			state->pls.frame |= MSG_ReadByte (net_message) << 8;
		}
	}
}

/*
	CL_SetSolid

	Builds all the pmove physents for the current frame
*/
void
CL_SetSolidEntities (void)
{
	int					i;
	entity_state_t	   *state;
	frame_t			   *frame;
	packet_entities_t  *pak;

	pmove.physents[0].model = cl.worldmodel;
	VectorZero (pmove.physents[0].origin);
	VectorZero (pmove.physents[0].angles);
	pmove.physents[0].info = 0;
	pmove.numphysent = 1;

	frame = &cl.frames[parsecountmod];
	pak = &frame->packet_entities;

	for (i = 0; i < pak->num_entities; i++) {
		state = &pak->entities[i];

		if (!state->modelindex)
			continue;
		if (!cl.model_precache[state->modelindex])
			continue;
		if (cl.model_precache[state->modelindex]->hulls[1].firstclipnode
			|| cl.model_precache[state->modelindex]->clipbox) {
			if (pmove.numphysent == MAX_PHYSENTS) {
				Sys_Printf ("WARNING: entity physent overflow, email "
							"quakeforge-devel@lists.quakeforge.net\n");
				break;
			}
			pmove.physents[pmove.numphysent].model =
				cl.model_precache[state->modelindex];
			VectorCopy (state->origin,
						pmove.physents[pmove.numphysent].origin);
			VectorCopy (state->angles,
						pmove.physents[pmove.numphysent].angles);
			pmove.numphysent++;
		}
	}
}

void
CL_ClearPredict (void)
{
	memset (predicted_players, 0, sizeof (predicted_players));
	//fixangle = 0;
}

/*
	Calculate the new position of players, without other player clipping

	We do this to set up real player prediction.
	Players are predicted twice, first without clipping other players,
	then with clipping against them.
	This sets up the first phase.
*/
void
CL_SetUpPlayerPrediction (qboolean dopred)
{
	double			playertime;
	frame_t		   *frame;
	int				msec, j;
	player_state_t	exact;
	player_state_t *state;
	struct predicted_player *pplayer;

	playertime = realtime - cls.latency + 0.02;
	if (playertime > realtime)
		playertime = realtime;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];

	for (j = 0, pplayer = predicted_players, state = frame->playerstate;
		 j < MAX_CLIENTS; j++, pplayer++, state++) {

		pplayer->active = false;

		if (state->messagenum != cl.parsecount)
			continue;					// not present this frame

		if (!state->pls.modelindex)
			continue;

		pplayer->active = true;
		pplayer->flags = state->pls.flags;

		// note that the local player is special, since he moves locally
		// we use his last predicted postition
		if (j == cl.playernum) {
			VectorCopy (cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].
						playerstate[cl.playernum].pls.origin, pplayer->origin);
		} else {
			// predict only half the move to minimize overruns
			msec = 500 * (playertime - state->state_time);
			if (msec <= 0 || !dopred) {
				VectorCopy (state->pls.origin, pplayer->origin);
//				Sys_MaskPrintf (SYS_DEV, "nopredict\n");
			} else {
				// predict players movement
				state->pls.cmd.msec = msec = min (msec, 255);
//				Sys_MaskPrintf (SYS_DEV, "predict: %i\n", msec);

				CL_PredictUsercmd (state, &exact, &state->pls.cmd, false);
				VectorCopy (exact.pls.origin, pplayer->origin);
			}
		}
	}
}

/*
	CL_SetSolid

	Builds all the pmove physents for the current frame
	Note that CL_SetUpPlayerPrediction () must be called first!
	pmove must be setup with world and solid entity hulls before calling
	(via CL_PredictMove)
*/
void
CL_SetSolidPlayers (int playernum)
{
	int			j;
	physent_t  *pent;
	struct predicted_player *pplayer;

	if (!cl_solid_players->int_val)
		return;

	pent = pmove.physents + pmove.numphysent;

	for (j = 0, pplayer = predicted_players; j < MAX_CLIENTS; j++, pplayer++) {
		if (!pplayer->active)
			continue;					// not present this frame

		// the player object never gets added
		if (j == playernum)
			continue;

		if (pplayer->flags & PF_DEAD)
			continue;					// dead players aren't solid

		if (pmove.numphysent == MAX_PHYSENTS) {
			Sys_Printf ("WARNING: player physent overflow, email "
						"quakeforge-devel@lists.quakeforge.net\n");
			break;
		}

		pent->model = 0;
		VectorCopy (pplayer->origin, pent->origin);
		VectorCopy (player_mins, pent->mins);
		VectorCopy (player_maxs, pent->maxs);
		pmove.numphysent++;
		pent++;
	}
}
