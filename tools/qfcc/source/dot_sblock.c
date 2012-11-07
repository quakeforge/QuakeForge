/*
	dot_sblock.c

	"emit" sblock graphs to dot (graphvis).

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/01/21

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
#include <stdlib.h>

#include <QF/dstring.h>
#include <QF/quakeio.h>
#include <QF/va.h>

#include "dags.h"
#include "flow.h"
#include "expr.h"
#include "qfcc.h"
#include "function.h"
#include "statements.h"
#include "strpool.h"
#include "symtab.h"
#include "type.h"

static void
flow_statement (dstring_t *dstr, statement_t *s)
{
	dasprintf (dstr, "        <tr>");
	dasprintf (dstr, "<td>%d</td>", s->number);
	dasprintf (dstr, "<td>%s</td>", html_string(quote_string (s->opcode)));
	dasprintf (dstr, "<td>%s</td>", html_string(operand_string (s->opa)));
	dasprintf (dstr, "<td>%s</td>", html_string(operand_string (s->opb)));
	dasprintf (dstr, "<td>%s</td>", html_string(operand_string (s->opc)));
	dasprintf (dstr, "</tr>\n");
}

static void
flow_sblock (dstring_t *dstr, sblock_t *sblock, int blockno)
{
	statement_t *s;
	sblock_t  **target;
	sblock_t  **target_list;
	ex_label_t *l;

	if (sblock->dag) {
		dasprintf (dstr, "  sb_%p -> dagnode_%p;", sblock, sblock->dag);
		print_dag (dstr, sblock->dag);
	}
	dasprintf (dstr, "  sb_%p [shape=none,label=<\n", sblock);
	dasprintf (dstr, "    <table border=\"0\" cellborder=\"1\" "
					 "cellspacing=\"0\">\n");
	dasprintf (dstr, "      <tr>\n");
	dasprintf (dstr, "        <td colspan=\"2\" >%p(%d)</td>\n", sblock,
			   blockno);
	dasprintf (dstr, "        <td height=\"0\" colspan=\"2\" port=\"s\">\n");
	for (l = sblock->labels; l; l = l->next)
		dasprintf (dstr, "            %s(%d)\n", l->name, l->used);
	dasprintf (dstr, "        </td>\n");
	dasprintf (dstr, "        <td></td>\n");
	dasprintf (dstr, "      </tr>\n");
	for (s = sblock->statements; s; s = s->next)
		flow_statement (dstr, s);
	dasprintf (dstr, "      <tr>\n");
	dasprintf (dstr, "        <td colspan=\"2\"></td>\n");
	dasprintf (dstr, "        <td height=\"0\" colspan=\"2\" "
					 "port=\"e\"></td>\n");
	dasprintf (dstr, "        <td></td>\n");
	dasprintf (dstr, "      </tr>\n");
	dasprintf (dstr, "    </table>>];\n");
	if (sblock->next && !flow_is_goto ((statement_t *) sblock->tail)
		&& !flow_is_jumpb ((statement_t *) sblock->tail)
		&& !flow_is_return ((statement_t *) sblock->tail))
		dasprintf (dstr, "  sb_%p:e -> sb_%p:s;\n", sblock, sblock->next);
	if ((target_list = flow_get_targetlist ((statement_t *) sblock->tail))) {
		for (target = target_list; *target; target++)
			dasprintf (dstr, "  sb_%p:e -> sb_%p:s [label=\"%s\"];\n", sblock,
					   *target, ((statement_t *) sblock->tail)->opcode);
		free (target_list);
	}
	dasprintf (dstr, "\n");
}

void
print_sblock (sblock_t *sblock, const char *filename)
{
	int         i;
	dstring_t  *dstr = dstring_newstr();

	dasprintf (dstr, "digraph sblock_%p {\n", sblock);
	dasprintf (dstr, "  layout=dot; rankdir=TB;\n");
	for (i = 0; sblock; sblock = sblock->next, i++)
		flow_sblock (dstr, sblock, i);
	dasprintf (dstr, "}\n");

	if (filename) {
		QFile      *file;

		file = Qopen (filename, "wt");
		Qwrite (file, dstr->str, dstr->size - 1);
		Qclose (file);
	} else {
		fputs (dstr->str, stdout);
	}
	dstring_delete (dstr);
}
