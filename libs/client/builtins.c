/*
	builtins.c

	CSQC builtins

	Copyright (C) 2011 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/06/15

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

#include <string.h>

#include "QF/mathlib.h"
#include "QF/sys.h"

#include "csqc.h"

#define CSQC_API_VERSION 1.0f

static struct {
	vec3_t     *v_forward;
	vec3_t     *v_up;
	vec3_t     *v_right;
} csqc_globals;

static struct {
	int         origin;
} csqc_fields;

static struct {
	func_t      Init;
	func_t      Shutdown;
	func_t      UpdateView;
	func_t      WorldLoaded;
	func_t      Parse_StuffCmd;
	func_t      Parse_CenterPrint;
	func_t      Parse_Print;
	func_t      InputEvent;
	func_t      ConsoleCommand;
	func_t      Ent_Update;
	func_t      Event_Sound;
	func_t      Remove;
} csqc_funcs;

static struct {
	const char *name;
	func_t     *field;
} csqc_func_list[] = {
	{"CSQC_Init",				&csqc_funcs.Init},
	{"CSQC_Shutdown",			&csqc_funcs.Shutdown},
	{"CSQC_UpdateView",			&csqc_funcs.UpdateView},
	{"CSQC_WorldLoaded",		&csqc_funcs.WorldLoaded},
	{"CSQC_Parse_StuffCmd",		&csqc_funcs.Parse_StuffCmd},
	{"CSQC_Parse_CenterPrint",	&csqc_funcs.Parse_CenterPrint},
	{"CSQC_Parse_Print",		&csqc_funcs.Parse_Print},
	{"CSQC_InputEvent",			&csqc_funcs.InputEvent},
	{"CSQC_ConsoleCommand",		&csqc_funcs.ConsoleCommand},
	{"CSQC_Ent_Update",			&csqc_funcs.Ent_Update},
	{"CSQC_Event_Sound",		&csqc_funcs.Event_Sound},
	{"CSQC_Remove",				&csqc_funcs.Remove},
};

static progs_t csqc_pr_state;

#if TYPECHECK_PROGS
#define CSQCFIELD(e,f,t) E_var (e, PR_AccessField (&csqc_pr_state, #f, ev_##t, __FILE__, __LINE__), t)
#else
#define CSQCFIELD(e,f,t) E_var (e, csqc_fields.f, t)
#endif

#define CSQCfloat(e,f)      CSQCFIELD (e, f, float)
#define CSQCstring(e,f)     CSQCFIELD (e, f, string)
#define CSQCfunc(e,f)       CSQCFIELD (e, f, func)
#define CSQCentity(e,f)     CSQCFIELD (e, f, entity)
#define CSQCvector(e,f)     CSQCFIELD (e, f, vector)
#define CSQCinteger(e,f)    CSQCFIELD (e, f, integer)


static void
CSQC_makevectors (progs_t *pr)
{
	AngleVectors (P_VECTOR (pr, 0), *csqc_globals.v_forward,
				  *csqc_globals.v_right, *csqc_globals.v_up);
}

static void
CSQC_setorigin (progs_t *pr)
{
	edict_t    *e;
	vec_t      *org;

	e = P_EDICT (pr, 0);
	org = P_VECTOR (pr, 1);
	VectorCopy (org, CSQCvector (e, origin));

	//XXX CS_LinkEdict (e, false);
}

static void
CSQC_setmodel (progs_t *pr)
{
}

static void
CSQC_setsize (progs_t *pr)
{
}

static void
CSQC_sound (progs_t *pr)
{
}

static void
CSQC_error (progs_t *pr)
{
}

static void
CSQC_objerror (progs_t *pr)
{
}

static void
CSQC_spawn (progs_t *pr)
{
}

static void
CSQC_remove (progs_t *pr)
{
}

static void
CSQC_traceline (progs_t *pr)
{
}

static void
CSQC_precache_sound (progs_t *pr)
{
}

static void
CSQC_precache_model (progs_t *pr)
{
}

static void
CSQC_findradius (progs_t *pr)
{
}

static void
CSQC_walkmove (progs_t *pr)
{
}

static void
CSQC_droptofloor (progs_t *pr)
{
}

static void
CSQC_lightstyle (progs_t *pr)
{
}

static void
CSQC_checkbottom (progs_t *pr)
{
}

static void
CSQC_pointcontents (progs_t *pr)
{
}

static void
CSQC_localcmd (progs_t *pr)
{
}

static void
CSQC_changeyaw (progs_t *pr)
{
}

static void
CSQC_makestatic (progs_t *pr)
{
}

static void
CSQC_ambientsound (progs_t *pr)
{
}

static void
CSQC_clearscene (progs_t *pr)
{
}

static void
CSQC_addentities (progs_t *pr)
{
}

static void
CSQC_addentity (progs_t *pr)
{
}

static void
CSQC_setviewprop (progs_t *pr)
{
}

static void
CSQC_getviewprop (progs_t *pr)
{
}

static void
CSQC_adddynamiclight (progs_t *pr)
{
}

static void
CSQC_renderscene (progs_t *pr)
{
}

static void
CSQC_unproject (progs_t *pr)
{
}

static void
CSQC_project (progs_t *pr)
{
}

static void
CSQC_drawfillpal (progs_t *pr)
{
}

static void
CSQC_is_cached_pic (progs_t *pr)
{
}

static void
CSQC_precache_pic (progs_t *pr)
{
}

static void
CSQC_drawgetimagesize (progs_t *pr)
{
}

static void
CSQC_free_pic (progs_t *pr)
{
}

static void
CSQC_drawcharacter (progs_t *pr)
{
}

static void
CSQC_drawrawstring (progs_t *pr)
{
}

static void
CSQC_drawpic (progs_t *pr)
{
}

static void
CSQC_drawfillrgb (progs_t *pr)
{
}

static void
CSQC_drawcolorocodedstring (progs_t *pr)
{
}

static void
CSQC_setmodelindex (progs_t *pr)
{
}

static void
CSQC_modelnameforindex (progs_t *pr)
{
}

static void
CSQC_setsensitivityscaler (progs_t *pr)
{
}

static void
CSQC_cprint (progs_t *pr)
{
}

static void
CSQC_print (progs_t *pr)
{
}

static void
CSQC_pointparticles (progs_t *pr)
{
}

static void
CSQC_trailparticles (progs_t *pr)
{
}

static void
CSQC_particleeffectnum (progs_t *pr)
{
}

static void
CSQC_getinputstate (progs_t *pr)
{
}

static void
CSQC_runplayerphysics (progs_t *pr)
{
}

static void
CSQC_isdemo (progs_t *pr)
{
}

static void
CSQC_isserver (progs_t *pr)
{
}

static void
CSQC_keynumtostring (progs_t *pr)
{
}

static void
CSQC_stringtokeynum (progs_t *pr)
{
}

static void
CSQC_getkeybind (progs_t *pr)
{
}

static void
CSQC_setlistener (progs_t *pr)
{
}

static void
CSQC_deltalisten (progs_t *pr)
{
}

static void
CSQC_readbyte (progs_t *pr)
{
}

static void
CSQC_readchar (progs_t *pr)
{
}

static void
CSQC_readshort (progs_t *pr)
{
}

static void
CSQC_readlong (progs_t *pr)
{
}

static void
CSQC_readcoord (progs_t *pr)
{
}

static void
CSQC_readangle (progs_t *pr)
{
}

static void
CSQC_readstring (progs_t *pr)
{
}

static void
CSQC_readfloat (progs_t *pr)
{
}

static void
CSQC_readentitynum (progs_t *pr)
{
}

static void
CSQC_getstatf (progs_t *pr)
{
}

static void
CSQC_getstati (progs_t *pr)
{
}

static void
CSQC_getstats (progs_t *pr)
{
}

static void
CSQC_getplayerkey (progs_t *pr)
{
}

static void
CSQC_serverkey (progs_t *pr)
{
}

static void
CSQC_getentitytoken (progs_t *pr)
{
}

static void
CSQC_registercommand (progs_t *pr)
{
}

static void
CSQC_wasfreed (progs_t *pr)
{
	edict_t    *ed = P_EDICT (pr, 0);
	R_FLOAT (pr) = ed->free;
}

static void
CSQC_sendevent (progs_t *pr)
{
}

#define CSQC_BUILTIN(name, number) {#name, CSQC_##name, number}
static builtin_t builtins[] = {
	CSQC_BUILTIN (makevectors, 1),
	CSQC_BUILTIN (setorigin, 2),
	CSQC_BUILTIN (setmodel, 3),
	CSQC_BUILTIN (setsize, 4),

	CSQC_BUILTIN (sound, 8),
	CSQC_BUILTIN (error, 10),
	CSQC_BUILTIN (objerror, 11),
	CSQC_BUILTIN (spawn, 14),
	CSQC_BUILTIN (remove, 15),
	CSQC_BUILTIN (traceline, 16),
//	CSQC_BUILTIN (checkclient, 17),

	CSQC_BUILTIN (precache_sound, 19),
	CSQC_BUILTIN (precache_model, 20),
//	CSQC_BUILTIN (stuffcmd, 21),
	CSQC_BUILTIN (findradius, 22),
//	CSQC_BUILTIN (bprint, 23),
//	CSQC_BUILTIN (sprint, 24),

	CSQC_BUILTIN (walkmove, 32),

	CSQC_BUILTIN (droptofloor, 34),
	CSQC_BUILTIN (lightstyle, 35),

	CSQC_BUILTIN (checkbottom, 40),
	CSQC_BUILTIN (pointcontents, 41),

//	CSQC_BUILTIN (aim, 44),

	CSQC_BUILTIN (localcmd, 46),
	CSQC_BUILTIN (changeyaw, 49),

//	CSQC_BUILTIN (writebyte, 52),
//	{"WriteBytes",			CSQC_no,				-1},
//	CSQC_BUILTIN (writechar, 53),
//	CSQC_BUILTIN (writeshort, 54),
//	CSQC_BUILTIN (writelong, 55),
//	CSQC_BUILTIN (writecoord, 56),
//	CSQC_BUILTIN (writeangle, 57),
//	{"WriteCoordV",			CSQC_no,				-1},
//	{"WriteAngleV",			CSQC_no,				-1},
//	CSQC_BUILTIN (writestring, 58),
//	CSQC_BUILTIN (writeentity, 59),

//	CSQC_BUILTIN (movetogoal, 67),
//	CSQC_BUILTIN (precache_file, 68),
	CSQC_BUILTIN (makestatic, 69),
//	CSQC_BUILTIN (changelevel, 70),

//	CSQC_BUILTIN (centerprint, 73),XXX
	CSQC_BUILTIN (ambientsound, 74),
	{"precache_model2", CSQC_precache_model, 75},
	{"precache_sound2", CSQC_precache_sound, 76},
//	CSQC_BUILTIN (precache_file2, 77),
//	CSQC_BUILTIN (setspawnparms, 78),XXX

//	CSQC_BUILTIN (logfrag, 79),
//	CSQC_BUILTIN (infokey, 80),
//	CSQC_BUILTIN (multicast, 82),

	// 3D scene management
	CSQC_BUILTIN (clearscene, 300),
	CSQC_BUILTIN (addentities, 301),
	CSQC_BUILTIN (addentity, 302),
	CSQC_BUILTIN (setviewprop, 303),
	CSQC_BUILTIN (getviewprop, 309),
	CSQC_BUILTIN (adddynamiclight, 305),
	CSQC_BUILTIN (renderscene, 304),
	CSQC_BUILTIN (unproject, 310),
	CSQC_BUILTIN (project, 311),

	// 2D display
	CSQC_BUILTIN (drawfillpal, 314),
	CSQC_BUILTIN (is_cached_pic, 316),
	CSQC_BUILTIN (precache_pic, 317),
	CSQC_BUILTIN (drawgetimagesize, 318),
	CSQC_BUILTIN (free_pic, 319),
	CSQC_BUILTIN (drawcharacter, 320),
	CSQC_BUILTIN (drawrawstring, 321),
	CSQC_BUILTIN (drawpic, 322),
	CSQC_BUILTIN (drawfillrgb, 323),
	CSQC_BUILTIN (drawcolorocodedstring, 326),

	CSQC_BUILTIN (setmodelindex, 333),
	CSQC_BUILTIN (modelnameforindex, 334),
	CSQC_BUILTIN (setsensitivityscaler, 346),
	CSQC_BUILTIN (cprint, 338),
	CSQC_BUILTIN (print, 339),
	CSQC_BUILTIN (pointparticles, 337),
	CSQC_BUILTIN (trailparticles, 336),
	CSQC_BUILTIN (particleeffectnum, 335),

	CSQC_BUILTIN (getinputstate, 345),
	CSQC_BUILTIN (runplayerphysics, 347),
	CSQC_BUILTIN (isdemo, 349),
	CSQC_BUILTIN (isserver, 350),
	CSQC_BUILTIN (keynumtostring, 340),
	CSQC_BUILTIN (stringtokeynum, 341),
	CSQC_BUILTIN (getkeybind, 342),

	CSQC_BUILTIN (setlistener, 351),

	CSQC_BUILTIN (deltalisten, 371),

	CSQC_BUILTIN (readbyte, 360),
	CSQC_BUILTIN (readchar, 361),
	CSQC_BUILTIN (readshort, 362),
	CSQC_BUILTIN (readlong, 363),
	CSQC_BUILTIN (readcoord, 364),
	CSQC_BUILTIN (readangle, 365),
	CSQC_BUILTIN (readstring, 366),
	CSQC_BUILTIN (readfloat, 367),
	CSQC_BUILTIN (readentitynum, 368),

	CSQC_BUILTIN (getstatf, 330),
	CSQC_BUILTIN (getstati, 331),
	CSQC_BUILTIN (getstats, 332),
	CSQC_BUILTIN (getplayerkey, 348),
	CSQC_BUILTIN (serverkey, 354),
	CSQC_BUILTIN (getentitytoken, 355),
	CSQC_BUILTIN (registercommand, 352),

	CSQC_BUILTIN (wasfreed, 353),
	CSQC_BUILTIN (sendevent, 359),
};

static float
version_number (void)
{
	union {
		float       f;
		int         i;
	}           version;
	int         ver[4];

	memset (ver, 0, sizeof (ver));
	sscanf (VERSION, "%d.%d.%d.%d", &ver[0], &ver[1], &ver[2], &ver[3]);
	// allow 1 digit for patch, 2 for minor and N for major.
	version.f = ver[2] + ver[1] * 10 + ver[0] * 1000;
	// hack in the git revision count. for the same count, the fraction of the
	// version will move around depending on the last release version, but
	// for the same last release version, newer revisions are guaranteed to
	// compare greater than older revisions.
	//
	// Of course, this will break if the bits needed to represent the last
	// release version ever overlap with the bits needed for the git revision
	// count, but that's not likely any time soon as major is still 0 and minor
	// is still single digit thus only 7 bits are needed for the version
	// leaving 17 for the revision count. There better not be 128k revisions
	// between releases :P
	version.i |= ver[3];
	Sys_MaskPrintf (SYS_CSQC, "version: %.8e\n", version.f);
	return version.f;
}

static int
csqc_load (progs_t *pr)
{
	size_t      i;

	for (i = 0;
		 i < sizeof (csqc_func_list) / sizeof (csqc_func_list[0]); i++) {
		dfunction_t *f = PR_FindFunction (pr, csqc_func_list[i].name);

		*csqc_func_list[i].field = 0;
		if (f)
			*csqc_func_list[i].field = (func_t) (f - pr->pr_functions);
	}
	if (csqc_funcs.Init) {
		PR_RESET_PARAMS (&csqc_pr_state);
		P_FLOAT (&csqc_pr_state, 0) = CSQC_API_VERSION;
		P_STRING (&csqc_pr_state, 1) = PR_SetTempString (&csqc_pr_state,
														 PACKAGE_NAME);
		P_FLOAT (&csqc_pr_state, 2) = version_number ();
		PR_ExecuteProgram (&csqc_pr_state, csqc_funcs.Init);
	}
	return 1;
}

void
CSQC_Cmds_Init (void)
{
	version_number();
	PR_Cmds_Init (&csqc_pr_state);
	PR_RegisterBuiltins (&csqc_pr_state, builtins);
	PR_AddLoadFunc (&csqc_pr_state, csqc_load);
}
