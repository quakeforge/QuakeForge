/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <QF/va.h>

#include "qfcc.h"
#include "expr.h"

void
PrecacheSound (def_t *e, int ch)
{
	char       *n;
	int         i;

	if (!e->ofs)
		return;

	n = G_STRING (e->ofs);

	for (i = 0; i < numsounds; i++) {
		if (!strcmp (n, precache_sounds[i])) {
			return;
		}
	}

	if (numsounds == MAX_SOUNDS) {
		error (0, "PrecacheSound: numsounds == MAX_SOUNDS");
		return;
	}

	strcpy (precache_sounds[i], n);
	if (ch >= '1' && ch <= '9')
		precache_sounds_block[i] = ch - '0';
	else
		precache_sounds_block[i] = 1;

	numsounds++;
}

void
PrecacheModel (def_t *e, int ch)
{
	char       *n;
	int         i;

	if (!e->ofs)
		return;

	n = G_STRING (e->ofs);

	for (i = 0; i < nummodels; i++) {
		if (!strcmp (n, precache_models[i])) {
			return;
		}
	}

	if (nummodels == MAX_MODELS) {
		error (0, "PrecacheModels: nummodels == MAX_MODELS");
		return;
	}

	strcpy (precache_models[i], n);
	if (ch >= '1' && ch <= '9')
		precache_models_block[i] = ch - '0';
	else
		precache_models_block[i] = 1;

	nummodels++;
}

void
PrecacheFile (def_t *e, int ch)
{
	char       *n;
	int         i;

	if (!e->ofs)
		return;

	n = G_STRING (e->ofs);

	for (i = 0; i < numfiles; i++) {
		if (!strcmp (n, precache_files[i])) {
			return;
		}
	}

	if (numfiles == MAX_FILES) {
		error (0, "PrecacheFile: numfiles == MAX_FILES");
		return;
	}

	strcpy (precache_files[i], n);
	if (ch >= '1' && ch <= '9')
		precache_files_block[i] = ch - '0';
	else
		precache_files_block[i] = 1;

	numfiles++;
}
