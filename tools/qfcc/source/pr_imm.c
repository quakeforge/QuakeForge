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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "QF/hash.h"

#include "qfcc.h"

static hashtab_t  *string_imm_defs;
static hashtab_t  *float_imm_defs;
static hashtab_t  *vector_imm_defs;
static hashtab_t  *entity_imm_defs;
static hashtab_t  *field_imm_defs;
static hashtab_t  *func_imm_defs;
static hashtab_t  *pointer_imm_defs;
static hashtab_t  *quaternion_imm_defs;
static hashtab_t  *integer_imm_defs;

static const char *
string_imm_get_key (void *_def, void *unused)
{
	def_t       *def = (def_t*)_def;
	return G_STRING (def->ofs);
}

static const char *
float_imm_get_key (void *_def, void *unused)
{
	def_t       *def = (def_t*)_def;
	static char rep[20];
	sprintf (rep, "\001float:%08X\001", G_INT(def->ofs));
	return rep;
}

static const char *
vector_imm_get_key (void *_def, void *unused)
{
	def_t       *def = (def_t*)_def;
	static char rep[60];
	sprintf (rep, "\001vector:%08X\001%08X\001%08X\001",
			 G_INT(def->ofs), G_INT(def->ofs+1), G_INT(def->ofs+2));
	return rep;
}

static const char *
quaternion_imm_get_key (void *_def, void *unused)
{
	def_t       *def = (def_t*)_def;
	static char rep[60];
	sprintf (rep, "\001quaternion:%08X\001%08X\001%08X\001%08X\001",
			 G_INT(def->ofs), G_INT(def->ofs+1),
			 G_INT(def->ofs+2), G_INT(def->ofs+3));
	return rep;
}

static const char *
int_imm_get_key (void *_def, void *_str)
{
	def_t      *def = (def_t*)_def;
	static char rep[60];
	char       *str = (char*)_str;
	sprintf (rep, "\001%s:%08X\001", str, G_INT(def->ofs));
	return rep;
}

/*
	PR_ParseImmediate

	Looks for a preexisting constant
*/
def_t *
PR_ParseImmediate (def_t *def)
{
	def_t	*cn = 0;
	char rep[60];
	hashtab_t *tab = 0;

	if (!string_imm_defs) {
		string_imm_defs = Hash_NewTable (16381, string_imm_get_key, 0, 0);
		float_imm_defs = Hash_NewTable (16381, float_imm_get_key, 0, 0);
		vector_imm_defs = Hash_NewTable (16381, vector_imm_get_key, 0, 0);
	}
	if (def && def->type != pr_immediate_type) {
		PR_ParseError ("type mismatch for immediate value");
	}
	if (pr_immediate_type == &type_string) {
		cn = (def_t*) Hash_Find (string_imm_defs, pr_immediate_string);
		tab = string_imm_defs;
		//printf ("%s\n",pr_immediate_string);
	} else if (pr_immediate_type == &type_float) {
		sprintf (rep, "\001float:%08X\001", *(int*)&pr_immediate._float);
		cn = (def_t*) Hash_Find (float_imm_defs, rep);
		tab = float_imm_defs;
		//printf ("%f\n",pr_immediate._float);
	} else if (pr_immediate_type == &type_vector) {
		sprintf (rep, "\001vector:%08X\001%08X\001%08X\001",
				 *(int*)&pr_immediate.vector[0],
				 *(int*)&pr_immediate.vector[1],
				 *(int*)&pr_immediate.vector[2]);
		cn = (def_t*) Hash_Find (vector_imm_defs, rep);
		tab = vector_imm_defs;
		//printf ("%f %f %f\n",pr_immediate.vector[0], pr_immediate.vector[1], pr_immediate.vector[2]);
	} else {
		PR_ParseError ("weird immediate type");
	}
	if (cn) {
		if (cn->type != pr_immediate_type)
			printf("urk\n");
		//printf("found\n");
		if (def) {
			PR_FreeLocation (def);
			def->ofs = cn->ofs;
			def->initialized = 1;
			cn = def;
		}
		PR_Lex ();
		return cn;
	}

	// allocate a new one
	// always share immediates
	if (def) {
		cn = def;
	} else {
		cn = PR_NewDef (pr_immediate_type, ".imm", 0);
		cn->ofs = PR_NewLocation (pr_immediate_type);
		pr_global_defs[cn->ofs] = cn;
	}
	cn->initialized = 1;
	// copy the immediate to the global area
	if (pr_immediate_type == &type_string)
		pr_immediate.string = CopyString (pr_immediate_string);

	memcpy (pr_globals + cn->ofs, &pr_immediate,
			4 * type_size[pr_immediate_type->type]);

	Hash_Add (tab, cn);

	PR_Lex ();

	return cn;
}

def_t *
PR_ReuseConstant (expr_t *expr, def_t *def)
{
	def_t	*cn = 0;
	char rep[60], *r = rep;
	hashtab_t *tab = 0;
	type_t *type;
	expr_t e = *expr;

	if (!string_imm_defs) {
		string_imm_defs = Hash_NewTable (16381, string_imm_get_key, 0, 0);
		float_imm_defs = Hash_NewTable (16381, float_imm_get_key, 0, 0);
		vector_imm_defs = Hash_NewTable (16381, vector_imm_get_key, 0, 0);
		entity_imm_defs = Hash_NewTable (16381, int_imm_get_key, 0, "entity");
		field_imm_defs = Hash_NewTable (16381, int_imm_get_key, 0, "field");
		func_imm_defs = Hash_NewTable (16381, int_imm_get_key, 0, "func");
		pointer_imm_defs = Hash_NewTable (16381, int_imm_get_key, 0, "pointer");
		quaternion_imm_defs = Hash_NewTable (16381, quaternion_imm_get_key, 0, 0);
		integer_imm_defs = Hash_NewTable (16381, int_imm_get_key, 0, "integer");

		Hash_Add (string_imm_defs, PR_NewDef (&type_string, ".imm", 0));
		Hash_Add (float_imm_defs, PR_NewDef (&type_float, ".imm", 0));
		Hash_Add (entity_imm_defs, PR_NewDef (&type_entity, ".imm", 0));
		Hash_Add (pointer_imm_defs, PR_NewDef (&type_pointer, ".imm", 0));
		Hash_Add (integer_imm_defs, PR_NewDef (&type_integer, ".imm", 0));
	}
	switch (e.type) {
		case ex_entity:
			sprintf (rep, "\001entity:%08X\001", e.e.integer_val);
			tab = entity_imm_defs;
			type = &type_entity;
			break;
		case ex_field:
			sprintf (rep, "\001field:%08X\001", e.e.integer_val);
			tab = field_imm_defs;
			type = &type_field;
			break;
		case ex_func:
			sprintf (rep, "\001func:%08X\001", e.e.integer_val);
			tab = func_imm_defs;
			type = &type_function;
			break;
		case ex_pointer:
			sprintf (rep, "\001pointer:%08X\001", e.e.integer_val);
			tab = pointer_imm_defs;
			type = &type_pointer;
			break;
		case ex_integer:
			if (!def || def->type != &type_float) {
				sprintf (rep, "\001integer:%08X\001", e.e.integer_val);
				tab = integer_imm_defs;
				type = &type_integer;
				break;
			}
			e.e.float_val = e.e.integer_val;
		case ex_float:
			sprintf (rep, "\001float:%08X\001", e.e.integer_val);
			tab = float_imm_defs;
			type = &type_float;
			break;
		case ex_string:
			r = e.e.string_val ? e.e.string_val : "";
			tab = string_imm_defs;
			type = &type_string;
			break;
		case ex_vector:
			sprintf (rep, "\001vector:%08X\001%08X\001%08X\001",
					 *(int*)&e.e.vector_val[0],
					 *(int*)&e.e.vector_val[1],
					 *(int*)&e.e.vector_val[2]);
			tab = vector_imm_defs;
			type = &type_vector;
			break;
		case ex_quaternion:
			sprintf (rep, "\001quaternion:%08X\001%08X\001%08X\001%08X\001",
					 *(int*)&e.e.quaternion_val[0],
					 *(int*)&e.e.quaternion_val[1],
					 *(int*)&e.e.quaternion_val[2],
					 *(int*)&e.e.quaternion_val[3]);
			tab = vector_imm_defs;
			type = &type_quaternion;
			break;
		default:
			abort ();
	}
	cn = (def_t*) Hash_Find (tab, r);
	if (cn) {
		if (def) {
			PR_FreeLocation (def);
			def->ofs = cn->ofs;
			def->initialized = 1;
			cn = def;
		}
		return cn;
	}
	// allocate a new one
	// always share immediates
	if (def) {
		cn = def;
	} else {
		cn = PR_NewDef (type, ".imm", 0);
		cn->ofs = PR_NewLocation (type);
		pr_global_defs[cn->ofs] = cn;
		if (type == &type_vector || type == &type_quaternion) {
			int i;

			for (i = 0; i< 3 + (type == &type_quaternion); i++)
				PR_NewDef (&type_float, ".imm", 0);
		}
	}
	cn->initialized = 1;
	// copy the immediate to the global area
	if (e.type == ex_string)
		e.e.integer_val = CopyString (r);

	memcpy (pr_globals + cn->ofs, &e.e, 4 * type_size[type->type]);

	Hash_Add (tab, cn);

	return cn;
}
