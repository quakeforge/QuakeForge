/*
	sv_sys_win.c

	(description)

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

#include "winquake.h"

#include <winsock.h>

#include "QF/cvar.h"
#include "QF/info.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "compat.h"
#include "server.h"

qboolean    is_server = true;
qboolean    WinNT;
server_static_t svs;
info_t    **svs_info = &svs.info;


static void
startup (void)
{
	OSVERSIONINFO vinfo;

	// make sure the timer is high precision, otherwise NT gets 18ms resolution
	timeBeginPeriod (1);

	vinfo.dwOSVersionInfoSize = sizeof (vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error ("Couldn't get OS info");

	if ((vinfo.dwMajorVersion < 4) ||
		(vinfo.dwPlatformId == VER_PLATFORM_WIN32s)) {
		Sys_Error (PROGRAM " requires at least Win95 or NT 4.0");
	}

	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
		WinNT = true;
	else
		WinNT = false;
}

char       *newargv[256];

int
main (int argc, const char **argv)
{
	double      newtime, time, oldtime;

	startup ();

	COM_InitArgv (argc, argv);

	host_parms.argc = com_argc;
	host_parms.argv = com_argv;

	SV_Init ();

	if (COM_CheckParm ("-nopriority")) {
		Cvar_Set (sys_sleep, "0");
	} else {
		if (!SetPriorityClass (GetCurrentProcess (), HIGH_PRIORITY_CLASS))
			SV_Printf ("SetPriorityClass() failed\n");
		else
			SV_Printf ("Process priority class set to HIGH\n");
	}

	// sys_sleep > 0 seems to cause packet loss on WinNT (why?)
	if (WinNT)
		Cvar_Set (sys_sleep, "0");

	Sys_RegisterShutdown (Net_LogStop);

	// run one frame immediately for first heartbeat
	SV_Frame (0.1);

	// main loop
	oldtime = Sys_DoubleTime () - 0.1;
	while (1) {
		if (!Sys_CheckInput (!svs.num_clients, net_socket))
			continue;

		// find time passed since last cycle
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		oldtime = newtime;

		SV_Frame (time);
	}

	return true;
}
