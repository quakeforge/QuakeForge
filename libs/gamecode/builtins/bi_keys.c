/*
	bi_keys.c

	CSQC key-api builtins

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

#include "QF/cbuf.h"
#include "QF/idparse.h"
#include "QF/keys.h"
#include "QF/progs.h"
#include "QF/zone.h"

static cbuf_t *cbuf; //FIXME use a properly allocated cbuf rather than this hack

static inline void
check_cbuf (void)
{
	if (!cbuf)
		cbuf = Cbuf_New (COM_extract_line, COM_parse_line, NULL, NULL);
}

/*
    bi_Key_SetBinding
    
    QC-Function for set a binding
*/
static void
bi_Key_SetBinding (progs_t *pr)
{
	int	        target  = P_INT (pr, 0);
	int         keynum  = P_INT (pr, 1);
	const char *binding = P_STRING (pr, 2);
	cbuf_t     *tcb = cbuf_active;

	if(strlen(binding) == 0 || binding[0] == '\0') {
		binding = NULL;	/* unbind a binding */
	}

	check_cbuf ();
	cbuf_active = cbuf;
	Key_SetBinding (target, keynum, binding);
	cbuf_active = tcb;
}

/*
    bi_Key_LookupBinding
    
    Perform a reverse-binding-lookup
*/
static void
bi_Key_LookupBinding (progs_t *pr)
{
	int	        target  = P_INT (pr, 0);
	int	        bindnum = P_INT (pr, 1);
	const char *binding = P_STRING (pr, 2);
	int i;
	knum_t keynum = -1;
	const char *keybind = NULL;

	for (i = 0; i < QFK_LAST; i++) {
		keybind = keybindings[target][i].str;
		if(keybind == NULL) { continue; }
		if(strcmp(keybind, binding) == 0) {
			bindnum--;
			if(bindnum == 0) {
				keynum = i;
				break;
			}
		}
	}

	R_INT (pr) = keynum;	
};

/*
    bi_Key_CountBinding
    
    Counts how often a binding is assigned to a key
*/
static void
bi_Key_CountBinding (progs_t *pr)
{
	int	        target  = P_INT (pr, 0);
	const char *binding = P_STRING (pr, 1);
	int i, res = 0;
	const char *keybind = NULL;

	for (i = 0; i < QFK_LAST; i++) {
		keybind = keybindings[target][i].str;
		if(keybind == NULL) { continue; }
		if(strcmp(keybind, binding) == 0) {
			res++;
		}
	}

	R_INT (pr) = res;	
};


/*
    bi_Key_LookupBinding
    
    Convertes a keynum to a string
*/
static void
bi_Key_KeynumToString (progs_t *pr)
{
	int	        keynum  = P_INT (pr, 0);

	RETURN_STRING (pr, Key_KeynumToString (keynum));
};


void
Key_Progs_Init (progs_t *pr)
{
	PR_AddBuiltin (pr, "Key_SetBinding", bi_Key_SetBinding, -1);
	PR_AddBuiltin (pr, "Key_LookupBinding", bi_Key_LookupBinding, -1);
	PR_AddBuiltin (pr, "Key_CountBinding", bi_Key_CountBinding, -1);
	PR_AddBuiltin (pr, "Key_KeynumToString", bi_Key_KeynumToString, -1);
// NEED THIS ?//	PR_AddBuiltin (pr, "Key_StringToKeynum", bi_Key_KeynumToString, -1);
}
