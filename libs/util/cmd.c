/*
	cmd.c

	script command processing module

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

#ifdef HAVE_FNMATCH_H
# define model_t sunmodel_t
# include <fnmatch.h>
# undef model_t
#else
# ifdef WIN32
# include "fnmatch.h"
# endif
#endif

#include <ctype.h>
#include <errno.h>

#include <sys/types.h>
#include <dirent.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/sizebuf.h"
#include "QF/sys.h"
#include "QF/vfs.h"
#include "QF/zone.h"
#include "QF/dstring.h"
#include "QF/va.h"
#include "QF/info.h"

#include "compat.h"
#include "exp.h"

#ifndef HAVE_FNMATCH_PROTO
int         fnmatch (const char *__pattern, const char *__string, int __flags);
#endif

typedef struct cmdalias_s {
	struct cmdalias_s *next;
	const char *name;
	const char *value;
	qboolean restricted; // Created from restricted buffer
	qboolean legacy; // Created from a legacy buffer
} cmdalias_t;

cmdalias_t *cmd_alias;
cmd_source_t cmd_source;

cmd_buffer_t *cmd_consolebuffer;	// Console buffer
cmd_buffer_t *cmd_legacybuffer;         // Server stuffcmd buffer with
					// absolute backwards-compatibility
cmd_buffer_t *cmd_privatebuffer;	// Buffer for internal command execution
cmd_buffer_t *cmd_keybindbuffer;        // Buffer for commands from bound keys
cmd_buffer_t *cmd_activebuffer;         // Buffer currently being executed

cmd_buffer_t *cmd_recycled;		// Recycled buffers


cmd_thread_t *cmd_threads;		// Detached buffers running by themselves
long int cmd_threadid;			// The id of the last thread + 1

dstring_t  *cmd_backtrace;

qboolean    cmd_error;

cvar_t     *cmd_warncmd;
cvar_t     *cmd_maxloop;

hashtab_t  *cmd_alias_hash;
hashtab_t  *cmd_hash;

//=============================================================================

/* Structure management */


/* Creates a new local variable */
cmd_localvar_t *
Cmd_NewLocal (const char *key, const char *value)
{
	cmd_localvar_t *new;
	dstring_t  *dkey, *dvalue;

	new = malloc (sizeof (cmd_localvar_t));
	if (!new)
		Sys_Error ("Cmd_NewLocal:  Memory allocation failed!");
	dkey = dstring_newstr ();
	dvalue = dstring_newstr ();
	dstring_appendstr (dkey, key);
	dstring_appendstr (dvalue, value);
	new->key = dkey;
	new->value = dvalue;
	return new;
}

/* Gets a local variable from a command buffer */
cmd_localvar_t	*
Cmd_GetLocal (cmd_buffer_t *buffer, const char *key)
{
	cmd_localvar_t *var;

	var = (cmd_localvar_t *)Hash_Find(buffer->locals, key);
	return var;
}

/* Sets a local variable on a command buffer */
void
Cmd_SetLocal (cmd_buffer_t *buffer, const char *key, const char *value)
{
	cmd_localvar_t *var;

	var = (cmd_localvar_t *)Hash_Find(buffer->locals, key);
	if (!var) {
		var = Cmd_NewLocal (key, value);
		Hash_Add (buffer->locals, (void *) var);
	} else {
		dstring_clearstr (var->value);
		dstring_appendstr (var->value, value);
	}
}

/* Hashtable callbacks... */
const char *
Cmd_LocalGetKey (void *ele, void *ptr)
{
	return ((cmd_localvar_t *) ele)->key->str;
}

void
Cmd_LocalFree (void *ele, void *ptr)
{
	dstring_delete (((cmd_localvar_t *) ele)->key);
	dstring_delete (((cmd_localvar_t *) ele)->value);
	free (ele);
}

/* Token management */

/* Creates a new token */
cmd_token_t	*
Cmd_NewToken (void) {
	cmd_token_t *new;

	new = calloc(1,sizeof(cmd_token_t));
	SYS_CHECKMEM (new);
	new->original = dstring_newstr();
	new->processed = dstring_newstr();
	return new;
}

/* Command buffer management */

/* Creates a command buffer with or without its own local variables */
cmd_buffer_t	*
Cmd_NewBuffer (qboolean ownvars)
{
	cmd_buffer_t *new;

	if (cmd_recycled) { // If we have old buffers lying around
		new = cmd_recycled;
		cmd_recycled = new->next;
	}
	else {
		new = calloc (1, sizeof (cmd_buffer_t));
		new->buffer = dstring_newstr ();
		new->line = dstring_newstr ();
		new->realline = dstring_newstr ();
		new->looptext = dstring_newstr ();
		new->retval = dstring_newstr ();
	}
	if (ownvars)
		new->locals = Hash_NewTable (512, Cmd_LocalGetKey, Cmd_LocalFree, 0);
	new->ownvars = ownvars;
	new->prev = new->next = 0;
	new->position = cmd_ready;
	new->returned = cmd_normal;
	return new;
}

/*
	Cmd_FreeBuffer

	Actually just "recycles" a buffer for
	later use
*/

void
Cmd_FreeBuffer (cmd_buffer_t *free) {
	if (free->ownvars)
		Hash_DelTable(free->locals); // Local variables are always deadbeef
	free->subroutine = false;
	free->again = false;
	free->wait = false;
	free->legacy = false;
	free->ownvars = false;
	free->loop = false;
	free->embedded = false;
	free->restricted = false;

	free->timeleft = 0.0;
	
	dstring_clearstr (free->buffer);
	dstring_clearstr (free->line);
	dstring_clearstr (free->realline);
	dstring_clearstr (free->looptext);
	dstring_clearstr (free->retval);
	free->locals = 0;
	free->next = cmd_recycled;
	cmd_recycled = free;
}

/* Frees an entire linked list (stack) of buffers */
void
Cmd_FreeStack (cmd_buffer_t *stack) {
	cmd_buffer_t *temp;

	for (;stack; stack = temp) {
		temp = stack->next;
		Cmd_FreeBuffer (stack);
	}
}

/* Thread management */

/* Creates a new thread */
cmd_thread_t	*
Cmd_NewThread (long int id) {
	cmd_thread_t *new;

	new = calloc (1, sizeof(cmd_thread_t));
	SYS_CHECKMEM (new);
	new->id = id;
	new->cbuf = Cmd_NewBuffer (true);

	return new;
}

/* Frees a thread */
void
Cmd_FreeThread (cmd_thread_t *thread) {
	Cmd_FreeBuffer (thread->cbuf);
	free (thread);
	return;
}

/* Adds a thread to a linked list */
void
Cmd_AddThread (cmd_thread_t **list, cmd_thread_t *thread) {
	thread->next = *list;
	thread->prev = 0;
	if (*list)
		(*list)->prev = thread;
	*list = thread;
}

/* Removes a thread from a linked list and frees it */
void
Cmd_RemoveThread (cmd_thread_t **list, cmd_thread_t *thread) {
	if (thread == *list)
		*list = thread->next;
	if (thread->next)
		thread->next->prev = thread->prev;
	if (thread->prev)
		thread->prev->next = thread->next;
	Cmd_FreeThread (thread);
}


/* End structure management */

/* Escape character functions used by most of the parser */

/* Determines if a character is escaped */
inline
qboolean
escaped (const char *str, int i)
{
	int         n, c;

	if (!i)
		return 0;
	for (n = i - 1, c = 0; n >= 0 && str[n] == '\\'; n--, c++)
		;
	return c & 1;
}

/* Escapes a list of characters in a dstring */
inline
void
escape (dstring_t * dstr, const char *clist)
{
	int         i;

	for (i = 0; i < strlen (dstr->str); i++) {
		if (strchr(clist, dstr->str[i]) && !escaped(dstr->str,i)) {
				dstring_insertstr (dstr, "\\", i);
				i++;
				continue;
		}
	}
}

/* Unescapes a character and all backslashes preceding it in a dstring */
inline
int
unescape (dstring_t * dstr, int start)
{
	int i;

	for (i = start; i && dstr->str[i-1] == '\\'; i--);
	dstring_snip (dstr, i, (int) ((start - i)/2.0+0.5));
	return (int) ((start - i)/2.0+.05);
}


/*
	Cmd_Wait_f

	Causes execution of the remainder of the
	command buffer and stack to be delayed until
	next frame.  This allows commands like:
	bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
*/


/* Command buffer functions */

void
Cbuf_Init (void)
{
	cmd_consolebuffer = Cmd_NewBuffer (true);
	cmd_legacybuffer = Cmd_NewBuffer (true);
	cmd_legacybuffer->legacy = true;
	cmd_legacybuffer->restricted = true;
	cmd_privatebuffer = Cmd_NewBuffer (true);
	cmd_keybindbuffer = Cmd_NewBuffer (true);

	cmd_activebuffer = cmd_consolebuffer;

	cmd_threads = 0;
	cmd_threadid = 0;
	cmd_recycled = 0;

	cmd_backtrace = dstring_newstr ();
}


void
Cbuf_AddTextTo (cmd_buffer_t *buffer, const char *text)
{
	dstring_appendstr (buffer->buffer, text);
}

void
Cbuf_AddText (const char *text)
{
	Cbuf_AddTextTo (cmd_activebuffer, text);
}

void
Cbuf_InsertTextTo (cmd_buffer_t *buffer, const char *text)
{
	dstring_insertstr (buffer->buffer, "\n", 0);
	dstring_insertstr (buffer->buffer, text, 0);
}

void
Cbuf_InsertText (const char *text)
{
	Cbuf_InsertTextTo (cmd_activebuffer, text);
}


/*
	Cbuf_ExtractLine

	Finds the next \n,\r, or ;-delimeted
	line in the command buffer and copies
	it into a buffer.  Also shifts the rest
	of the command buffer back to the start.
	Won't cut a line off inside braces.
*/

void
Cbuf_ExtractLine (dstring_t * buffer, dstring_t * line, qboolean legacy)
{
	int         i, dquotes = 0, braces = 0, n;
	char		*tmp;

	for (i = 0; buffer->str[i]; i++) {
		if (!escaped (buffer->str, i)) {
			if (buffer->str[i] == '"')
				dquotes ^= 1;
			else if (buffer->str[i] == ';' && !dquotes && !braces)
				break;
			else if (!legacy && buffer->str[i] == '{' && !dquotes)
				braces++;
			else if (!legacy && buffer->str[i] == '}' && !dquotes)
				braces--;
		}
		if (buffer->str[i] == '/' && buffer->str[i + 1] == '/'
		  && !dquotes) {
			// Filter out comments until newline
			if ((tmp = strchr (buffer->str+i, '\n')))
				n = tmp - (buffer->str+i);
			else if ((tmp = strchr (buffer->str+i, '\r')))
				n = tmp - (buffer->str+i);
			else
				n = strlen(buffer->str+i); // snip till \0
			dstring_snip (buffer, i, n);
			i--;
		}
		else if ((buffer->str[i] == '\n' || buffer->str[i] == '\r') && !braces) {
			if (escaped (buffer->str, i)) {
				dstring_snip (buffer, i-1, 2);
				i -= 2;
			} else
				break;
		}
	}
	if (i)
		dstring_insert (line, buffer->str, i, 0);
	if (buffer->str[i]) {
		dstring_snip (buffer, 0, i + 1);
	} else {
		// We've hit the end of the buffer, just clear it
		dstring_clearstr (buffer);
	}
}

/*
	Cbuf_ExecuteBuffer

	Extracts and executes each line in the
	command buffer, until it is empty,
	a wait command is executed, or an
	error occurs.

	Records its position each step of the
	way so that execution can be paused
	and restarted after a command is tokenized.
	This is necessary for embedded commands.
*/

void
Cbuf_ExecuteBuffer (cmd_buffer_t *buffer)
{
	dstring_t  *buf = dstring_newstr ();
	cmd_buffer_t *temp = cmd_activebuffer;	// save old context
	int ret;


	if (buffer->timeleft) {
		double newtime = Sys_DoubleTime ();
		buffer->timeleft -= newtime - buffer->lasttime;
		buffer->lasttime = newtime;
		if (buffer->timeleft < 0.0)
			buffer->timeleft = 0.0;
	}
	if (buffer->timeleft)
		return;
	cmd_activebuffer = buffer;
	buffer->wait = false;
	buffer->again = false;

	while (1) {
		if (!strlen(buffer->buffer->str) && buffer->position == cmd_ready) {
			if (buffer->loop) {
				if (cmd_maxloop->value && buffer->loop > cmd_maxloop->value) {
					Cmd_Error(va("GIB: Loop lasted longer than %i iterations, forcefully terminating.\n",
								(int)cmd_maxloop->value));
					break;
				}
				Cbuf_InsertTextTo(buffer, buffer->looptext->str);
				buffer->loop++;
			}
			else
				break;
		}

		if (buffer->position == cmd_ready) {
			Cbuf_ExtractLine (buffer->buffer, buf, buffer->legacy);
			if (!buf->str[0])
				continue;
			Cmd_TokenizeString (buf->str, cmd_activebuffer->legacy);
			if (cmd_error)
				break;
			buffer->position = cmd_tokenized;
		}
		if (buffer->position == cmd_tokenized) {
			ret = Cmd_Process ();
			if (ret < 0)
				break;
			buffer->position = cmd_processed;
		}
		if (buffer->position == cmd_processed) {
			Cmd_ExecuteParsed (src_command);
			if (cmd_error || buffer->again)
				break;
			buffer->position = cmd_ready;
			if (buffer->wait || buffer->subroutine)
				break;
		}
		dstring_clearstr (buf);
	}
	dstring_delete (buf);
	cmd_activebuffer = temp;			// restore old context
}


/*
	Cbuf_ExecuteStack
	
	Executes an entire stack of buffers,
	starting at the bottom and moving up.
	This is the proper way to start execution
	of a specific buffer.  It will handle
	subroutine and errors properly.
*/

void
Cbuf_ExecuteStack (cmd_buffer_t *buffer)
{
	cmd_buffer_t *cur, *temp;

	cmd_error = false;

	for (cur = buffer; cur->next; cur = cur->next);
	for (;cur; cur = temp) {
		temp = cur->prev;
		Cbuf_ExecuteBuffer (cur);
		if (cmd_error || cur->wait)
			break;
		if (cur->subroutine) { // Something was added to the stack, follow it
			cur->subroutine = false;
			temp = cur->next;
			continue;
		}
		if (cur != buffer) {
			cur->prev->next = 0;
			Cmd_FreeBuffer (cur);
		}
	}
	if (cmd_error) {
		// If an error occured nuke the stack
		Cmd_FreeStack (buffer->next);
		buffer->next = 0;
		buffer->position = cmd_ready;
		dstring_clearstr (buffer->buffer);	// And the root buffer
	}
}

/*
	Cbuf_Execute

	Executes both root buffers
*/

void
Cbuf_Execute (void)
{
	cmd_thread_t *t, *temp;

	for (t = cmd_threads; t; t = temp) {
		temp = t->next;
		if (!t->cbuf->next && !t->cbuf->buffer->str[0]) {
			Cmd_RemoveThread (&cmd_threads, t);
			continue;
		}
		Cbuf_ExecuteStack (t->cbuf);
	}
	if (cmd_keybindbuffer->buffer->str[0] || cmd_keybindbuffer->next)
		Cbuf_ExecuteStack (cmd_keybindbuffer);
	if (cmd_consolebuffer->buffer->str[0] || cmd_consolebuffer->next)
		Cbuf_ExecuteStack (cmd_consolebuffer);
	if (cmd_legacybuffer->buffer->str[0] || cmd_legacybuffer->next)
		Cbuf_ExecuteStack (cmd_legacybuffer);
}

/*
	Cmd_ExecuteSubroutine
	
	Executes a buffer as a subroutine on
	top of the execution stack.  Execute
	a buffer this way and forget about it,
	it will be handled properly.
*/

void
Cbuf_ExecuteSubroutine (cmd_buffer_t *buffer)
{
	if (cmd_activebuffer->next) // Get rid of anything already there
		Cmd_FreeStack (cmd_activebuffer->next);
	cmd_activebuffer->next = buffer; // Link it in
	buffer->prev = cmd_activebuffer;
	cmd_activebuffer->subroutine = true; // Signal that a subroutine is beginning
	if (cmd_activebuffer->restricted)
		buffer->restricted = true;
	return;
}

/*
	Cbuf_Execute_Sets

	Similar to Cbuf_Execute, but only
	executes set and setrom commands,
	and only in the console buffer.
	Used for reading config files
	before the cmd subsystem is
	entirely loaded.
*/

void
Cbuf_Execute_Sets (void)
{
	dstring_t  *buf = dstring_newstr ();

	while (strlen (cmd_consolebuffer->buffer->str)) {
		Cbuf_ExtractLine (cmd_consolebuffer->buffer, buf, cmd_consolebuffer->legacy);
		if (!strncmp (buf->str, "set", 3) && isspace ((int) buf->str[3])) {
			Cmd_ExecuteString (buf->str, src_command);
		} else if (!strncmp (buf->str, "setrom", 6)
				   && isspace ((int) buf->str[6])) {
			Cmd_ExecuteString (buf->str, src_command);
		}
		dstring_clearstr (buf);
	}
	dstring_delete (buf);
}


/* Command execution functions */

/*
	Cmd_StuffCmds_f

	Adds command line parameters as script statements
	Commands lead with a +, and continue until a - or another +
	quake +prog jctest.qp +cmd amlev1
	quake -nosound +cmd amlev1
*/
void
Cmd_StuffCmds_f (void)
{
	int         i, j;
	int         s;
	char       *build, c;

	s = strlen (com_cmdline);
	if (!s)
		return;

	// pull out the commands
	build = malloc (s + 1);
	SYS_CHECKMEM (build);
	build[0] = 0;

	for (i = 0; i < s - 1; i++) {
		if (com_cmdline[i] == '+') {
			i++;

			for (j = i; !((com_cmdline[j] == '+')
						  || (com_cmdline[j] == '-'
							  && (j == 0 || com_cmdline[j - 1] == ' '))
						  || (com_cmdline[j] == 0)); j++)
				;

			c = com_cmdline[j];
			com_cmdline[j] = 0;

			strncat (build, com_cmdline + i, s - strlen (build));
			strncat (build, "\n", s - strlen (build));
			com_cmdline[j] = c;
			i = j - 1;
		}
	}

	if (build[0])
		Cbuf_InsertText (build);

	free (build);
}

/* Executes a file by dumping it into the console buffer */

void
Cmd_Exec_File (const char *path)
{
	char       *f;
	int         len;
	VFile      *file;

	if (!path || !*path)
		return;
	if ((file = Qopen (path, "r")) != NULL) {
		len = COM_filelength (file);
		f = (char *) malloc (len + 1);
		if (f) {
			f[len] = 0;
			Qread (file, f, len);
			Qclose (file);
			// Always insert into console
			Cbuf_InsertTextTo (cmd_consolebuffer, f);
			free (f);
		}
	}
}


/* Generates a GIB error and prepares a
description and backtrace for the programmer */
void
Cmd_Error (const char *message)
{
	cmd_buffer_t *cur;

	Sys_Printf ("GIB:  Error in execution.  "
				"Type backtrace for a description and execution path to "
				"the error\n");
	cmd_error = true;
	dstring_clearstr (cmd_backtrace);
	dstring_appendstr (cmd_backtrace, message);
	dstring_appendstr (cmd_backtrace, "Path of execution:\n");
	for (cur = cmd_activebuffer; cur; cur = cur->prev) {
		dstring_appendstr (cmd_backtrace, va ("--> %s\n", cur->realline->str));
	}
}

/* Returns a value to the previous buffer
in the stack, but only if it wants one */
void
Cmd_Return (const char *value) {
	if (cmd_activebuffer->prev && cmd_activebuffer->prev->returned == cmd_waiting) {
		dstring_clearstr (cmd_activebuffer->prev->retval);
		dstring_appendstr (cmd_activebuffer->prev->retval, value);
		cmd_activebuffer->prev->returned = cmd_returned;
	} else
		Sys_DPrintf ("GIB warning:  Return value \"%s\" was unwanted.\n", value);
}




/* Command parsing functions */

typedef struct cmd_function_s {
	struct cmd_function_s *next;
	const char *name;
	xcommand_t  function;
	const char *description;
	qboolean pure;
} cmd_function_t;

static cmd_function_t *cmd_functions;	// possible commands to execute

int
Cmd_Argc (void)
{
	return cmd_activebuffer->argc;
}

const char *
Cmd_Argv (int arg)
{
	if (arg >= cmd_activebuffer->argc)
		return "";
	return cmd_activebuffer->argv[arg]->state == cmd_done ?
		cmd_activebuffer->argv[arg]->processed->str :
		cmd_activebuffer->argv[arg]->original->str;
}

const char *
Cmd_Argu (int arg)
{
	if (arg >= cmd_activebuffer->argc)
		return "";
	return cmd_activebuffer->argv[arg]->original->str;
}

const char *
Cmd_Args (int start)
{
	if (start >= cmd_activebuffer->argc)
		return "";
	return cmd_activebuffer->argv[start]->state == cmd_done ?
		cmd_activebuffer->line->str + cmd_activebuffer->args[start] :
		cmd_activebuffer->realline->str + cmd_activebuffer->argsu[start];
}

const char *
Cmd_Argsu (int start)
{
	if (start >= cmd_activebuffer->argc)
		return "";
	return cmd_activebuffer->realline->str + cmd_activebuffer->argsu[start];
}

qboolean
Cmd_Restricted (void)
{
	return cmd_activebuffer->restricted;
}

/* Functions to find partner quotes and braces
corecursively */

int
Cmd_EndDoubleQuote (const char *str, qboolean legacy)
{
	int         i;

	for (i = 1; i < strlen (str); i++) {
		if (str[i] == '\"' && (!escaped (str, i) || legacy))
			return i;
	}
	return -1;							// Not found
}


int
Cmd_EndBrace (const char *str)
{
	int         i, n;

	for (i = 1; i < strlen (str); i++) {
		if (!escaped (str, i)) {
			switch (str[i]) {
				case '{':
					n = Cmd_EndBrace (str + i);
					if (n < 0)
						return n;
					else
						i += n;
					break;
				case '\"':
					n = Cmd_EndDoubleQuote (str + i, false);
					if (n < 0)
						return n;
					else
						i += n;
					break;
				case '}':
					return i;
			}
		}
	}
	return -1;							// No matching brace found
}

int
Cmd_EndBracket (const char *str)
{
	int         i, n;

	for (i = 1; i < strlen (str); i++) {
		if (!escaped (str, i)) {
			switch (str[i]) {
				case '{':
					n = Cmd_EndBrace (str + i);
					if (n < 0)
						return n;
					else
						i += n;
					break;
				case '\"':
					n = Cmd_EndDoubleQuote (str + i, false);
					if (n < 0)
						return n;
					else
						i += n;
					break;
				case '[':
					n = Cmd_EndBracket (str + i);
					if (n < 0)
						return n;
					else
						i += n;
					break;
				case ']':
					return i;
			}
		}
	}
	return -1;							// No matching bracket found
}


/* Returns the length of the token beginning at str */
int
Cmd_GetToken (const char *str, qboolean legacy)
{
	int         i, ret;

	if (!legacy) {
		if (*str == '{')
			return Cmd_EndBrace (str);
		if (*str == '}')
			return -1;
	}
	if (*str == '\"')
		return Cmd_EndDoubleQuote (str, legacy);
	for (i = 0; i < strlen (str); i++) {
		if (isspace ((byte)str[i]))
			break;
		if (!escaped (str, i) || legacy) {
			if (str[i] == '{' && !legacy) {
				ret = Cmd_EndBrace (str+i);
				if (ret < 0)
					return ret;
				i += ret;
				continue;
			}
			else if (str[i] == '}' && !legacy)
				return -1;
			else if (str[i] == '\"') {
				ret = Cmd_EndDoubleQuote (str+i, legacy);
				if (ret < 0)
					return ret;
				i += ret;
				continue;
			}
		}
	}
	return i;
}

/* These are only used internally, but need to be declared in advance */
int Cmd_ProcessVariables (dstring_t * dstr);
int Cmd_ProcessEmbedded (cmd_token_t *tok, dstring_t * dstr);

/* HTML-like tag information */
int         tag_shift = 0;
int         tag_special = 0;

/* Special character replacement table */
struct stable_s {
	char        a, b;
} stable1[] = {
	{'f', 0x0D},							// Fake message

	{'[', 0x90},							// Gold braces
	{']', 0x91},

	{'(', 0x80},							// Scroll bar characters
	{'=', 0x81},
	{')', 0x82},
	{'|', 0x83},

	{'<', 0x9D},							// Vertical line characters
	{'-', 0x9E},
	{'>', 0x9F},

	{'.', 0x05},							// White dot

	{'#', 0x0B},							// White block

	{'a', 0x7F},							// White arrow.
	// DO NOT USE <s>a</s> WITH <b> TAG IN ANYTHING SENT TO SERVER.  PERIOD.
	{'A', 0x8D},							// Brown arrow

	{'0', 0x92},							// Golden numbers
	{'1', 0x93},
	{'2', 0x94},
	{'3', 0x95},
	{'4', 0x96},
	{'5', 0x97},
	{'6', 0x98},
	{'7', 0x99},
	{'8', 0x9A},
	{'9', 0x9B},
	{0, 0}
};

/*
	Cmd_ProcessTags

	Looks for html-like tags in a dstring and
	modifies the string accordingly

	FIXME:  This has become messy.  Create tag.[ch]
	and write a more generalized tag parser using
	callbacks`
*/

void
Cmd_ProcessTags (dstring_t * dstr)
{
	int         close = 0, ignore = 0, i, n, c;

	for (i = 0; i < strlen (dstr->str); i++) {
		if (dstr->str[i] == '<' && !escaped (dstr->str, i)) {
			close = 0;
			for (n = 1;
				 dstr->str[i+n] != '>' || escaped (dstr->str, i + n);
				 n++)
				if (dstr->str[i+n] == 0)
					return;
			if (dstr->str[i + 1] == '/')
				close = 1;
			if (!strncmp (dstr->str + i + close + 1, "i", 1)) {
				if (ignore && !close)
					// If we are ignoring, ignore a non close
					continue;
				else if (close && ignore)
					// If we are closing, turn off ignore
					ignore--;
				else if (!close)
					ignore++;			// Otherwise, turn ignore on
			} else if (ignore)
				// If ignore isn't being changed and we are ignore, go on
				continue;
			else if (!strncmp (dstr->str + i + close + 1, "b", 1))
				tag_shift = close ? tag_shift - 1 : tag_shift + 1;
			else if (!strncmp (dstr->str + i + close + 1, "s", 1))
				tag_special = close ? tag_special - 1 : tag_special + 1;
			if (tag_shift < 0)
				tag_shift = 0;
			if (tag_special < 0)
				tag_special = 0;
			dstring_snip (dstr, i, n + 1);
			i--;
			continue;
		}
		c = dstr->str[i];
		/* This ignores escape characters, unless it is itself escaped */
		if (c == '\\' && !escaped (dstr->str, i))
			continue;
		if (tag_special) {
			for (n = 0; stable1[n].a; n++)
				if (c == stable1[n].a)
					c = dstr->str[i] = stable1[n].b;
		}
		if (tag_shift && c < 128)
			c = (dstr->str[i] += 128);
	}
}

/* Looks for statements of the form #{expression}
and feeds them through the EXP math interpreter,
replacing them with the answer in the string */

int
Cmd_ProcessMath (dstring_t * dstr)
{
	dstring_t  *statement;
	int         i, n;
	double       value;
	char       *temp;
	int         ret = 0;

	statement = dstring_newstr ();

	for (i = 0; i < strlen (dstr->str); i++) {
		if (dstr->str[i] == '#' && dstr->str[i + 1] == '{' && !escaped (dstr->str, i)) {
			n = Cmd_EndBrace (dstr->str+i+1)+1;
			if (n < 0) {
				Cmd_Error ("Unmatched brace in math expression.\n");
				ret = -1;
				break;
			}
			/* Copy text between parentheses into a buffer */
			dstring_clearstr (statement);
			dstring_insert (statement, dstr->str + i + 2, n - 2, 0);
			value = EXP_Evaluate (statement->str);
			if (EXP_ERROR == EXP_E_NORMAL) {
				temp = va ("%.10g", value);
				dstring_snip (dstr, i, n + 1);	// Nuke the statement
				dstring_insertstr (dstr, temp, i);	// Stick in the value
				i += strlen (temp) - 1;
			} else {
				ret = -2;
				Cmd_Error (va("Math error: invalid expression %s\n", statement->str));
				break;					// Math evaluation error
			}
		}
	}
	dstring_delete (statement);
	return ret;
}

/* Looks for a bracketted index expression
(i.e [5:29]), evaluates math and variables inside,
removes it from the string, and stores the range
specified in the index into two ints */
int
Cmd_ProcessIndex (dstring_t * dstr, int start, int *val1, int *val2)
{
	dstring_t *index;
	char *sep;
	int i,n, temp;

	i = Cmd_EndBracket (dstr->str + start);
	if (i < 0) {
		Cmd_Error ("Unmatched bracket in index expression.\n");
		return i;
	}
	index = dstring_newstr ();
	dstring_insert (index, dstr->str+start+1, i-1, 0);
	dstring_snip (dstr, start, i+1);
	n = Cmd_ProcessVariables(index);
	if (n < 0) {
		dstring_delete (index);
		return n;
	}
	n = Cmd_ProcessMath (index);
	if (n < 0) {
		dstring_delete (index);
		return n;
	}
	if ((sep = strchr(index->str, ':'))) {
		*val1 = atoi (index->str);
		*val2 = atoi (sep+1);
	} else {
		*val1 = *val2 = atoi(index->str);
	}
	dstring_delete (index);
	if (*val2 < *val1) {
		temp = *val1;
		*val1 = *val2;
		*val2 = temp;
	}
	return 0;
}

/*
	Cmd_ProcessVariablesRecursive

	Looks for occurances of ${varname} and
	replaces them with the contents of the
	variable.  Will first replace occurances
	within itself.
*/

int
Cmd_ProcessVariablesRecursive (dstring_t * dstr, int start)
{
	cmd_localvar_t *lvar;
	cvar_t     *cvar;
	int         i, n, braces = 0;

	if (dstr->str[start+1] == '{')
		braces = 1;
	for (i = start + 1 + braces;; i++) {
		if (!escaped (dstr->str, i)) {
			// If we are within braces or two $ appear next to each other, consider it recursive
			if (dstr->str[i] == '$' && (braces || dstr->str[i-1] == '$')) {
				n = Cmd_ProcessVariablesRecursive (dstr, i);
				if (n < 0) {
					break;
				} else {
					i += n - 1;
					continue;
				}
			} else if ((braces && dstr->str[i] == '}') || (!braces && !isalnum((byte)dstr->str[i]) && dstr->str[i] != '_')) {
				dstring_t *varname = dstring_newstr ();
				dstring_t *copy = dstring_newstr ();
				dstring_insert (varname, dstr->str + start + 1 + braces, i - start - 1 - braces, 0);
				// Nuke it, even if no match is found
				dstring_snip (dstr, start, i - start + braces);
				lvar = (cmd_localvar_t *) Hash_Find (cmd_activebuffer->locals,
													 varname->str);
				if (lvar) {
					// Local variables get precedence
					dstring_appendstr (copy, lvar->value->str);
				} else if ((cvar = Cvar_FindVar (varname->str))) {
					// Then cvars
					dstring_appendstr (copy, cvar->string);
				}
				n = 0;
				dstring_clearstr (varname); // Reuse variable
				if (dstr->str[start] == '[') {
					int val1, val2, res;
					res = Cmd_ProcessIndex (dstr, start, &val1, &val2);
					if (res < 0)
						n = -1;
					else if (val1 >= strlen(copy->str) || val1 < 0)
						n = 0;
					else if (val2 < strlen(copy->str))
						dstring_insert (varname, copy->str+val1, val2-val1+1, 0);
					else
						dstring_insert (varname, copy->str+val1, strlen(copy->str)-val1, 0);
				} else
					dstring_appendstr (varname, copy->str);
				if (n >= 0) {
					escape (varname, "<#\\"); // Preserve backslashes, tags, and math as unprocessed
					dstring_insertstr (dstr, varname->str, start);
					n = strlen(varname->str);
				}
				dstring_delete (copy);
				dstring_delete (varname);
				break;
			}
		}
		if (!dstr->str[i] && braces) {		// No closing brace
			Cmd_Error ("Unmatched brace in variable substitution expression.\n");
			n = -1;
			break;
		}
	}
	return n;
}

/* Scans a dstring for instances of $var or
${var} and calls a recursive function to handle
them */
int
Cmd_ProcessVariables (dstring_t * dstr)
{
	int         i, n;

	for (i = 0; i < strlen (dstr->str); i++) {
		if (dstr->str[i] == '$' && !escaped (dstr->str, i)) {
			n = Cmd_ProcessVariablesRecursive (dstr, i);
			if (n < 0)
				return n;
			else
				i += n - 1;
		}
	}
	return 0;
}

/* Handles a single instance of an embedded function
This is done by creating a subroutine buffer with the
contents of the braces, saving our current position
and returning an error code to drop back to
Cbuf_ExecuteStack.  When the subroutine is executed,
it should return a value to this buffer.  When
this function is called again, it will see that a
value is available and substitute it into the string */
int
Cmd_ProcessEmbeddedSingle (dstring_t * dstr, int start)
{
	int		n;
	dstring_t *command;
	cmd_buffer_t *sub;

	n = Cmd_EndBrace (dstr->str+start+1)+1;
	if (n < 0) {
		Cmd_Error ("GIB:  Unmatched brace in embedded command expression.\n");
		return -1;
	}
	if (cmd_activebuffer->returned == cmd_waiting) {
		cmd_activebuffer->returned = cmd_normal;
		Cmd_Error ("Embedded command expression did not result in a return value.\n");
		return -1;
	}
	if (cmd_activebuffer->returned == cmd_returned) {
		escape (cmd_activebuffer->retval, "<#$\\");
		dstring_snip(dstr, start, n+1);
		dstring_insertstr (dstr, cmd_activebuffer->retval->str, start);
		n = strlen(cmd_activebuffer->retval->str);
		cmd_activebuffer->returned = cmd_normal;
	} else {
		command = dstring_newstr ();
		sub = Cmd_NewBuffer (false);
		sub->embedded = true;
		sub->locals = cmd_activebuffer->locals;
		dstring_insert (command, dstr->str + start + 2, n - 2, 0);
		Cbuf_InsertTextTo (sub, command->str);
		Cbuf_ExecuteSubroutine (sub);
		cmd_activebuffer->returned = cmd_waiting;
		n = -2;
		dstring_delete (command);
	}
	return n;
}

/* Scans for instances of ~{commands} and calls a function
to handle it */
int
Cmd_ProcessEmbedded (cmd_token_t *tok, dstring_t * dstr)
{
	int i, n;

	for (i = tok->pos; i < strlen(dstr->str); i++) {
		if (dstr->str[i] == '~' && dstr->str[i+1] == '{' && !escaped (dstr->str, i)) {
			n = Cmd_ProcessEmbeddedSingle (dstr, i);
			if (n == -2) {
				tok->pos = i;
				return n;
			}
			if (n < 0)
				return n;
			else
				i += n-1;
		}
	}
	return 0;
}




/*
	Cmd_ProcessEscapes

	Looks for the escape character \ and
	removes it.  Special cases exist for
	\n; otherwise, it is simply filtered.
	This should be the last step in the
	parser so that quotes, tags, etc.
	can be escaped
*/

int
Cmd_ProcessEscapes (dstring_t * dstr, const char *noprocess)
{
	int         i;

	if (strlen(dstr->str) == 1)
		return 0;
	for (i = 0; i < strlen (dstr->str); i++)
		if (dstr->str[i] == '\\') {
			dstring_snip (dstr, i, 1);
			if (dstr->str[i] == 'n')
				dstr->str[i] = '\n';
		}
	return 0;
}

/* Applies all filters to a single token */

int
Cmd_ProcessToken (cmd_token_t *token)
{
	int res;

	if (!token->pos) {
		dstring_clearstr (token->processed);
		dstring_appendstr (token->processed, token->original->str);
	}
	res = Cmd_ProcessEmbedded (token, token->processed);
	if (res < 0)
		return res;
	res = Cmd_ProcessVariables (token->processed);
	if (res < 0)
		return res;
	res = Cmd_ProcessMath (token->processed);
	if (res < 0)
		return res;
	Cmd_ProcessTags (token->processed);
	res = Cmd_ProcessEscapes (token->processed, "$#~");
		if (res < 0)
		return res;
	token->state = cmd_done;
	return 0;
}

/*
	Cmd_Process

	Processes all tokens that need to be processed
*/

int
Cmd_Process (void)
{
	int arg, res;
	dstring_t *str;
	dstring_t *org;
	int quotes;
	unsigned int adj = 0;
	cmd_function_t *cmd;

	if (cmd_activebuffer->legacy) // Legacy buffers will never require processing, ever
		return 0;
	if (!Cmd_Argc()) // No tokens, don't bother
		return 0;
	cmd = (cmd_function_t *) Hash_Find (cmd_hash, cmd_activebuffer->argv[0]->original->str);
	if (cmd && cmd->pure) // If the function is pure, don't bother processing
		return 0;

	tag_shift = 0;
	tag_special = 0;

	for (arg = 0; arg < cmd_activebuffer->argc; arg++) {
		if (cmd_activebuffer->argv[arg]->state == cmd_process) {
			res = Cmd_ProcessToken (cmd_activebuffer->argv[arg]);
			if (res < 0)
				return res;

		}
	}

	/* Here we edit the composite command line to reflect
	the changes made to individual tokens. */
	dstring_clearstr (cmd_activebuffer->line);
	dstring_appendstr (cmd_activebuffer->line, cmd_activebuffer->realline->str);
	for (arg = 0; arg < cmd_activebuffer->argc; arg++) {
		if (cmd_activebuffer->argv[arg]->state == cmd_original)
			continue;

		str = cmd_activebuffer->argv[arg]->processed;
		org = cmd_activebuffer->argv[arg]->original;
		if (cmd_activebuffer->argv[arg]->delim == '\"')
			quotes = 1;
		else
			quotes = 0;
		cmd_activebuffer->args[arg] = cmd_activebuffer->argsu[arg] + adj;
		adj += (str->size - 1) - (org->size - 1);
		dstring_replace (cmd_activebuffer->line, str->str, str->size - 1,
						 cmd_activebuffer->args[arg] + quotes + (cmd_activebuffer->argv[arg]->delim == '{'), org->size - 1);
	}
	return 0;
}


/* Breaks a string up into tokens */
void
Cmd_TokenizeString (const char *text, qboolean legacy)
{
	int         i = 0, len = 0, quotes, braces;
	const char *str = text;
	unsigned int cmd_argc = 0;
	qboolean process = true;

	dstring_clearstr (cmd_activebuffer->realline);
	dstring_appendstr (cmd_activebuffer->realline, text);
	dstring_clearstr (cmd_activebuffer->line);
	dstring_appendstr (cmd_activebuffer->line, text);

	if (text[0] == '|') {
		legacy = true;
		str++;
	}
	while (strlen (str + i)) {
		while (isspace ((byte)str[i])) // Get rid of white space
			i++;
		len = Cmd_GetToken (str + i, legacy);
		if (len < 0) {
			Cmd_Error ("Parse error:  Unmatched quotes, braces, or "
					   "double quotes\n");
			cmd_activebuffer->argc = 0;
			return;
		} else if (len == 0)
			break;
		cmd_argc++;
		if (cmd_argc > cmd_activebuffer->maxargc) {
			cmd_activebuffer->argv = realloc (cmd_activebuffer->argv,
											  sizeof (cmd_token_t *) * cmd_argc);
			SYS_CHECKMEM (cmd_activebuffer->argv);
			cmd_activebuffer->argsu = realloc (cmd_activebuffer->argsu,
											  sizeof (int) * cmd_argc);
			SYS_CHECKMEM (cmd_activebuffer->argsu);

			cmd_activebuffer->args = realloc (cmd_activebuffer->args,
											  sizeof (int) * cmd_argc);
			SYS_CHECKMEM (cmd_activebuffer->args);

			cmd_activebuffer->argv[cmd_argc - 1] = Cmd_NewToken ();
			cmd_activebuffer->maxargc++;
		}
		dstring_clearstr (cmd_activebuffer->argv[cmd_argc-1]->original);
		/* Remove surrounding quotes or double quotes or braces */
		quotes = 0;
		braces = 0;
		cmd_activebuffer->argsu[cmd_argc - 1] = i;
		if (str[i] == '"' && str[i + len] == '"') {
			cmd_activebuffer->argv[cmd_argc-1]->delim = str[i];
			i++;
			len -= 1;
			quotes = 1;
		} else if (!legacy && str[i] == '{' && str[i + len] == '}') {
			i++;
			len -= 1;
			braces = 1;
			cmd_activebuffer->argv[cmd_argc-1]->delim = '{';
		} else
			cmd_activebuffer->argv[cmd_argc - 1]->delim = ' ';
		dstring_insert (cmd_activebuffer->argv[cmd_argc-1]->original, str + i, len, 0);
		if (!legacy && !braces && process)
			cmd_activebuffer->argv[cmd_argc-1]->state = cmd_process;
		else
			cmd_activebuffer->argv[cmd_argc-1]->state = cmd_original;
		cmd_activebuffer->argv[cmd_argc-1]->pos = 0;
		i += len + quotes + braces;		/* If we ended on a quote or brace,
										   skip it */
	}
	cmd_activebuffer->argc = cmd_argc;
}

/*
	Cmd_ExecuteParsed

	A complete command line has been parsed, so try to execute it
*/
void
Cmd_ExecuteParsed (cmd_source_t src)
{
	cmd_function_t *cmd;

	cmd_source = src;

	// execute the command line
	if (!Cmd_Argc ())
		return;							// no tokens

	// check functions
	cmd = (cmd_function_t *) Hash_Find (cmd_hash, Cmd_Argv (0));
	if (cmd) {
		if (cmd->function)
			cmd->function ();
		return;
	}
	// Tonik: check cvars
	if (Cvar_Command ())
		return;

	// Check for assignment
	if (Cmd_Argc() == 3 && !strcmp(Cmd_Argv(1), "=")) {
		Cmd_SetLocal (cmd_activebuffer, Cmd_Argv(0), Cmd_Argv(2));
		return;
	}

	// check alias


	if (cmd_warncmd->int_val || developer->int_val)
		Sys_Printf ("Unknown command \"%s\"\n", Cmd_Argv (0));
}

/* Executes a single command in an internal buffer context */
int Cmd_ExecuteString (const char *text, cmd_source_t src)
{
	cmd_buffer_t *old = cmd_activebuffer; // Save context
	int ret;
	cmd_activebuffer = cmd_privatebuffer;
	Cmd_TokenizeString (text, cmd_activebuffer->legacy);
	if (!Cmd_Argc()) {
		cmd_activebuffer = old;
		return -1;
	}
	ret = Cmd_Process ();
	if (ret < 0) {
		cmd_activebuffer = old;
		return ret;
	}
	Cmd_ExecuteParsed (src);
	cmd_activebuffer = old;
	return 0;
}

/* Registers a command and handler function */
int
Cmd_AddCommand (const char *cmd_name, xcommand_t function,
				const char *description)
{
	cmd_function_t *cmd;
	cmd_function_t **c;

	// fail if the command already exists
	cmd = (cmd_function_t *) Hash_Find (cmd_hash, cmd_name);
	if (cmd) {
		Sys_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
		return 0;
	}

	cmd = calloc (1, sizeof (cmd_function_t));
	SYS_CHECKMEM (cmd);
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->description = description;
	Hash_Add (cmd_hash, cmd);
	for (c = &cmd_functions; *c; c = &(*c)->next)
		if (strcmp ((*c)->name, cmd->name) >= 0)
			break;
	cmd->next = *c;
	*c = cmd;
	return 1;
}

/* Unregisters a command */
int
Cmd_RemoveCommand (const char *name)
{
	cmd_function_t *cmd;
	cmd_function_t **c;

	cmd = (cmd_function_t *) Hash_Del (cmd_hash, name);
	if (!cmd)
		return 0;
	for (c = &cmd_functions; *c; c = &(*c)->next)
		if ((*c)->next == cmd)
			break;
	(*c)->next = cmd->next;
	free (cmd);
	return 1;
}

/* Sets a command to receive unprocessed tokens */
void
Cmd_SetPure (const char *name)
{
	cmd_function_t *cmd;

	cmd = (cmd_function_t *) Hash_Find (cmd_hash, name);

	if (cmd)
		cmd->pure = true;
}

/* Checks for the existance of a command */
qboolean
Cmd_Exists (const char *cmd_name)
{
	cmd_function_t *cmd;

	cmd = (cmd_function_t *) Hash_Find (cmd_hash, cmd_name);
	if (cmd) {
		return true;
	}

	return false;
}


/* Command completion functions */

const char *
Cmd_CompleteCommand (const char *partial)
{
	cmd_function_t *cmd;
	int         len;
	cmdalias_t *a;

	len = strlen (partial);

	if (!len)
		return NULL;

	// check for exact match
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strcasecmp (partial, cmd->name))
			return cmd->name;
	for (a = cmd_alias; a; a = a->next)
		if (!strcasecmp (partial, a->name))
			return a->name;

	// check for partial match
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp (partial, cmd->name, len))
			return cmd->name;
	for (a = cmd_alias; a; a = a->next)
		if (!strncasecmp (partial, a->name, len))
			return a->name;

	return NULL;
}

/*
	Cmd_CompleteCountPossible

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha
*/
int
Cmd_CompleteCountPossible (const char *partial)
{
	cmd_function_t *cmd;
	int         len;
	int         h;

	h = 0;
	len = strlen (partial);

	if (!len)
		return 0;

	// Loop through the command list and count all partial matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp (partial, cmd->name, len))
			h++;

	return h;
}

/*
	Cmd_CompleteBuildList

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha
*/
const char **
Cmd_CompleteBuildList (const char *partial)
{
	cmd_function_t *cmd;
	int         len = 0;
	int         bpos = 0;
	int         sizeofbuf;
	const char **buf;

	sizeofbuf = (Cmd_CompleteCountPossible (partial) + 1) * sizeof (char *);
	len = strlen (partial);
	buf = malloc (sizeofbuf + sizeof (char *));

	SYS_CHECKMEM (buf);
	// Loop through the alias list and print all matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp (partial, cmd->name, len))
			buf[bpos++] = cmd->name;

	buf[bpos] = NULL;
	return buf;
}

/*
	Cmd_CheckParm

	Returns the position (1 to argc-1) in the command's argument list
	where the given parameter apears, or 0 if not present
*/
int
Cmd_CheckParm (const char *parm)
{
	int         i;

	if (!parm)
		Sys_Error ("Cmd_CheckParm: NULL");

	for (i = 1; i < Cmd_Argc (); i++)
		if (!strcasecmp (parm, Cmd_Argv (i)))
			return i;

	return 0;
}

/* Basic command handler functions */

/* Executes a script as a subroutine */
void
Cmd_Exec_f (void)
{
	char       *f;
	int         mark;
	cmd_buffer_t *sub;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	mark = Hunk_LowMark ();
	f = (char *) COM_LoadHunkFile (Cmd_Argv (1));
	if (!f) {
		Sys_Printf ("couldn't exec %s\n", Cmd_Argv (1));
		return;
	}
	if (!Cvar_Command ()
		&& (cmd_warncmd->int_val || (developer && developer->int_val)))
		Sys_Printf ("execing %s\n", Cmd_Argv (1));
	sub = Cmd_NewBuffer (true);
	if (!strcasecmp (Cmd_Argv(1), "quake.rc")
	  || !strcasecmp (Cmd_Argv(1), "default.cfg")
	  || !strcasecmp (Cmd_Argv(1), "config.cfg"))
	  	sub->legacy = true;
	Cbuf_AddTextTo (sub, f);
	Hunk_FreeToLowMark (mark);
	Cbuf_ExecuteSubroutine (sub);		// Execute file in it's own buffer
}

/*
	Cmd_Echo_f

	Just prints the rest of the line to the console
*/
void
Cmd_Echo_f (void)
{
	if (Cmd_Argc() == 2)
		Sys_Printf ("%s\n", Cmd_Argv(1));
	else
		Sys_Printf ("%s\n", Cmd_Args (1));
}

/* Hash table functions for aliases and commands */
static void
cmd_alias_free (void *_a, void *unused)
{
	cmdalias_t *a = (cmdalias_t *) _a;

	free ((char *) a->name);
	free ((char *) a->value);
	free (a);
}

static const char *
cmd_alias_get_key (void *_a, void *unused)
{
	cmdalias_t *a = (cmdalias_t *) _a;

	return a->name;
}

static const char *
cmd_get_key (void *c, void *unused)
{
	cmd_function_t *cmd = (cmd_function_t *) c;

	return cmd->name;
}

void Cmd_Runalias_f (void)
{
	cmdalias_t *a;
	
	a = (cmdalias_t *) Hash_Find (cmd_alias_hash, Cmd_Argv (0));
	
	if (a) {
		int i;
		cmd_buffer_t *sub; // Create a new buffer to execute the alias in
		sub = Cmd_NewBuffer (true);
		sub->restricted = a->restricted;
		sub->legacy = a->legacy;
		Cbuf_InsertTextTo (sub, a->value);
		for (i = 0; i < Cmd_Argc (); i++)
			Cmd_SetLocal (sub, va ("%i", i), Cmd_Argv (i));
		Cmd_SetLocal (sub, "argn", va ("%i", Cmd_Argc ()));
		Cbuf_ExecuteSubroutine (sub);
		return;
	} else {
			Sys_Printf ("BUG: No alias found for registered command.  Please report this to the QuakeForge development team.");
			Cmd_Error ("Fatal bug encountered.  Execution aborted.  Please contact the QuakeForge team.");
	}
}

/*
	Cmd_Alias_f

	Creates a new command that executes a command string (possibly ; seperated)
	In GIB, these essentially are functions
*/
void
Cmd_Alias_f (void)
{
	cmdalias_t *alias;
	char       *cmd;
	int         i, c;
	const char *s;

	if (Cmd_Argc () == 1) {
		Sys_Printf ("Current alias commands:\n");
		for (alias = cmd_alias; alias; alias = alias->next)
			Sys_Printf ("alias %s \"%s\"\n", alias->name, alias->value);
		return;
	}

	s = Cmd_Argv (1);
	// if the alias already exists, reuse it
	alias = (cmdalias_t *) Hash_Find (cmd_alias_hash, s);
	if (Cmd_Argc () == 2) {
		if (alias)
			Sys_Printf("alias \"%s\" {%s}\n", alias->name, alias->value);
		return;
	}
	if (alias)
		free ((char *) alias->value);
	else {
		cmdalias_t **a;

		alias = calloc (1, sizeof (cmdalias_t));
		SYS_CHECKMEM (alias);
		alias->name = strdup (s);
		Hash_Add (cmd_alias_hash, alias);
		for (a = &cmd_alias; *a; a = &(*a)->next)
			if (strcmp ((*a)->name, alias->name) >= 0)
				break;
		alias->next = *a;
		*a = alias;
		Cmd_AddCommand (alias->name, Cmd_Runalias_f, "User-created command.");
	}
	// copy the rest of the command line
	cmd = malloc (strlen (Cmd_Args (1)) + 2);	// can never be longer
	SYS_CHECKMEM (cmd);
	cmd[0] = 0;							// start out with a null string
	c = Cmd_Argc ();
	for (i = 2; i < c; i++) {
		strcat (cmd, Cmd_Argv (i));
		if (i != c - 1)
			strcat (cmd, " ");
	}

	alias->value = cmd;
	alias->restricted = Cmd_Restricted ();
	alias->legacy = cmd_activebuffer->legacy;
}

/* Deletes an alias entirely */
void
Cmd_UnAlias_f (void)
{
	cmdalias_t *alias;
	const char *s;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("unalias <alias>: erase an existing alias\n");
		return;
	}

	s = Cmd_Argv (1);
	alias = Hash_Del (cmd_alias_hash, s);

	if (alias) {
		cmdalias_t **a;

		Cmd_RemoveCommand (alias->name);
		
		for (a = &cmd_alias; *a != alias; a = &(*a)->next)
			;
		*a = alias->next;

		free ((char *) alias->name);
		free ((char *) alias->value);
		free (alias);
	} else {
		Sys_Printf ("Unknown alias \"%s\"\n", s);
	}
}

/* Pauses execution of the current stack until
next call of Cmd_Execute (usually next frame) */
void
Cmd_Wait_f (void)
{
	cmd_buffer_t *cur;
	for (cur = cmd_activebuffer; cur; cur = cur->prev)
		cur->wait = true;
}

/* Pauses execution for a certain number of seconds */
void
Cmd_Sleep_f (void)
{
	if (Cmd_Argc() != 2) {
		Cmd_Error ("sleep: invalid number of arguments.\n");
		return;
	}
	cmd_activebuffer->timeleft = (double)atof(Cmd_Argv(1));
	cmd_activebuffer->lasttime = Sys_DoubleTime ();
	Cmd_Wait_f ();
	return;
}

/* Prints a list of available commands */
void
Cmd_CmdList_f (void)
{
	cmd_function_t *cmd;
	int         i;
	int         show_description = 0;
	
	if (Cmd_Argc () > 1)
		show_description = 1;
	for (cmd = cmd_functions, i = 0; cmd; cmd = cmd->next, i++) {
		if (show_description) {
			Sys_Printf ("%-20s :\n%s\n", cmd->name, cmd->description);
		} else {
			Sys_Printf ("%s\n", cmd->name);
		}
	}

	Sys_Printf ("------------\n%d commands\n", i);
}

/* Prints help about a command or cvar */
void
Cmd_Help_f (void)
{
	const char *name;
	cvar_t     *var;
	cmd_function_t *cmd;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("usage: help <cvar/command>\n");
		return;
	}

	name = Cmd_Argv (1);

	for (cmd = cmd_functions; cmd && strcasecmp (name, cmd->name);
		 cmd = cmd->next)
		;
	if (cmd) {
		Sys_Printf ("%s\n", cmd->description);
		return;
	}

	var = Cvar_FindVar (name);
	if (!var)
		var = Cvar_FindAlias (name);
	if (var) {
		Sys_Printf ("%s\n", var->description);
		return;
	}

	Sys_Printf ("variable/command not found\n");
}

/*
	Scripting commands

	The following functions are commands for enhanced scripting

*/

/* Conditionally executes a block of code
Supports if - if else - else */
void
Cmd_If_f (void)
{
	long int    num;
	int ret;

	if ((Cmd_Argc () !=3 && !(Cmd_Argc () >= 5)) || (Cmd_Argc () > 5 && strcmp(Cmd_Argv(3),"else"))) {
		Cmd_Error ("Malformed if statement.\n");
		return;
	}

	/* HACK HACK HACK
	If is set as a pure command, but it needs the first argument
	to be evaluated.  Normally this would mean Cmd_Args is out
	of sync, but since if uses Cmd_Argsu (4) and no other commands
	will need these tokens, it is safe.
	*/
	ret = Cmd_ProcessToken (cmd_activebuffer->argv[1]);
	if (ret < 0) {
		if (ret == -2)
			cmd_activebuffer->again = true; // Embedded command needs a return value first
		return;
	}

	num = strtol (Cmd_Argv(1), 0, 10);

	if (!strcmp(Cmd_Argv(0), "ifnot"))
		num = !num;

	if (num)
		Cbuf_InsertText (Cmd_Argv (2));
	if (!num && Cmd_Argc() == 5)
		Cbuf_InsertText (Cmd_Argv (4));
	if (!num && Cmd_Argc() > 5) {
		Cbuf_InsertText (Cmd_Argsu (4));
	}
	return;
}

/* Executes a block of code while a condition
is met */
void
Cmd_While_f (void) {
	cmd_buffer_t *sub;

	if (Cmd_Argc() < 3) {
		Sys_Printf("Usage: while {condition} {commands}\n");
		return;
	}

	sub = Cmd_NewBuffer (false);
	sub->locals = cmd_activebuffer->locals; // Use current local variables
	sub->loop = true;
	if (cmd_activebuffer->argv[1]->delim == '{')
		dstring_appendstr (sub->looptext, va("ifnot {%s} break\n", Cmd_Argv(1)));
	else if (cmd_activebuffer->argv[1]->delim == '\"')
		dstring_appendstr (sub->looptext, va("ifnot \"%s\" break\n", Cmd_Argv(1)));
	else
		dstring_appendstr (sub->looptext, va("ifnot %s break\n", Cmd_Argv(1)));
	dstring_appendstr (sub->looptext, Cmd_Argv(2));
	Cbuf_ExecuteSubroutine (sub);
	return;
}

/* Works exactly like for in C:
for {initializer; condition; iterator} {code} */
void
Cmd_For_f (void) {
	cmd_buffer_t *sub;
	dstring_t *arg1, *init, *cond, *inc;

	if (Cmd_Argc() < 2 || Cmd_Argc() > 3 || cmd_activebuffer->argv[1]->delim != '{') {
		Cmd_Error("Malformed for statement.\n");
		return;
	}

	arg1 = dstring_newstr ();
	init = dstring_newstr ();
	cond = dstring_newstr ();
	inc = dstring_newstr ();

	dstring_appendstr (arg1, Cmd_Argv(1));
	Cbuf_ExtractLine (arg1, init, true);
	Cbuf_ExtractLine (arg1, cond, true);
	Cbuf_ExtractLine (arg1, inc, true);
	if (!strlen(arg1->str)) {
		sub = Cmd_NewBuffer (false);
		sub->locals = cmd_activebuffer->locals; // Use current local variables
		sub->loop = true;
		dstring_appendstr (sub->looptext, va("ifnot %s break\n", cond->str));
		dstring_appendstr (sub->looptext, va("%s\n", Cmd_Argv(2)));
		dstring_appendstr (sub->looptext, va("%s", inc->str));
		Cbuf_InsertTextTo (sub, init->str);
		Cbuf_ExecuteSubroutine (sub);
	} else
		Cmd_Error("Malformed for statement.\n");
	dstring_delete (arg1);
	dstring_delete (init);
	dstring_delete (cond);
	dstring_delete (inc);
	return;
}

/* Breaks out of a loop buffer */
void
Cmd_Break_f (void) {
	if (cmd_activebuffer->loop) {
		cmd_activebuffer->loop = false;
		dstring_clearstr(cmd_activebuffer->buffer);
		return;
	} else {
		Cmd_Error("Break command used outside of loop!\n");
		return;
	}
}

/* Returns a value to the buffer that requested it */
void
Cmd_Return_f (void) {
	int argc = Cmd_Argc();
	const char *val = Cmd_Argv(1);
	cmd_buffer_t *old = cmd_activebuffer; // save context
	if (argc > 2) {
		Cmd_Error("GIB:  Invalid return statement.  Return takes either one argument or none.\n");
		return;
	}
	while (cmd_activebuffer->loop) { // We need to get out of any loops
		dstring_clearstr(cmd_activebuffer->buffer);
		cmd_activebuffer->loop = false;
		cmd_activebuffer = cmd_activebuffer->prev;
	}
	if (!cmd_activebuffer->prev) {
		Cmd_Error("GIB:  Return attempted in a root buffer\n");
		cmd_activebuffer = old;
		return;
	}
	dstring_clearstr (cmd_activebuffer->buffer); // Clear the buffer out no matter what
	if (!cmd_activebuffer->embedded) // If this isn't an embedded command buffer
		cmd_activebuffer = cmd_activebuffer->prev; // The buffer we want must be two places up on the stack
	if (argc == 2)
		Cmd_Return (val);
	cmd_activebuffer = old;
}

/* Sets the value of a local variable
Obsoleted by var = value, kept anyway */
void
Cmd_Lset_f (void)
{
	if (Cmd_Argc () != 3) {
		Cmd_Error ("lset: invalid number of arguments.\n");
		return;
	}
	Cmd_SetLocal (cmd_activebuffer, Cmd_Argv (1), Cmd_Argv (2));
}

/* Executes a command in a new thread */
void
Cmd_Detach_f (void)
{
	cmd_thread_t *thread;
	if (Cmd_Restricted()) {
		Cmd_Error ("detach: access to restricted command denied");
		return;
	}

	if (Cmd_Argc () != 2) {
		Cmd_Error ("detach: invalid number of arguments.\n");
		return;
	}

	thread = Cmd_NewThread (cmd_threadid++);
	Cmd_AddThread (&cmd_threads, thread);
	Cbuf_AddTextTo (thread->cbuf, Cmd_Argv(1));
	Cbuf_ExecuteStack (thread->cbuf); // Execute it now
	cmd_error = false; // Don't let errors cross into this stack
	Cmd_Return (va("%li", thread->id));
}

/* Kills a thread based on its thread id */
void
Cmd_Killthread_f (void)
{
	long int id;
	cmd_thread_t *t;
	if (Cmd_Argc() != 2)
		Cmd_Error ("killthread: invalid number of arguments.\n");
	id = atol (Cmd_Argv(1));
	for (t = cmd_threads; t; t = t->next)
		if (id == t->id) {
			if (t->cbuf->next)
				Cmd_FreeStack (t->cbuf->next);
			t->cbuf->next = 0;
			dstring_clearstr (t->cbuf->buffer);
			return;
		}
	Cmd_Error ("kill: invalid thread id\n");
}

/* Useful builtin functions */

/* Generates a random integer between two values */
void
Cmd_Randint_f (void) {
	int low, high;
	if (Cmd_Argc () != 3) {
		Cmd_Error ("randint: invalid number of arguments.\n");
		return;
	}
	low = atoi(Cmd_Argv(1));
	high = atoi(Cmd_Argv(2));
	Cmd_Return (va("%i", (int)(low+(float)rand()/(float)RAND_MAX*(float)(high-low+1))));
}

/* Returns 1 if two strings are equal, zero otherwise */
void
Cmd_Streq_f (void)
{
	if (Cmd_Argc () != 3) {
		Cmd_Error ("streq: invalid number of arguments.\n");
		return;
	}
	Cmd_Return (va("%i",!strcmp (Cmd_Argv(1), Cmd_Argv(2))));
}

/* Returns the length of a string */
void
Cmd_Strlen_f (void)
{
	if (Cmd_Argc () != 2) {
		Cmd_Error ("strlen: invalid number of arguments.\n");
		return;
	}
	Cmd_Return (va("%ld", (long) strlen (Cmd_Argv(1))));
}


/* File access */

/*
	Cmd_CollapsePath

	Collapses .. and . in a path
	and returns 1 if permission
	to access it is granted.
	Thanks to taniwha for this
	mystic routine.
*/

int
Cmd_CollapsePath (char *str)
{
	char *d, *p, *path;
	p = path = str;
	while (*p) {
		if (p[0] == '.') {
			if (p[1] == '.') {
				if (p[2] == '/' || p[2] == 0) {
					d = p;
					if (d > path)
						d--;
					while (d > path && d[-1] != '/')
						d--;
					if (d == path
						&& d[0] == '.' && d[1] == '.'
						&& (d[2] == '/' || d[2] == '0')) {
						p += 2 + (p[2] == '/');
						continue;
					}
					strcpy (d, p + 2 + (p[2] == '/'));
					p = d + (d != path);
				}
			} else if (p[1] == '/') {
				strcpy (p, p + 2);
				continue;
			} else if (p[1] == 0) {
				p[0] = 0;
			}
		}
		while (*p && *p != '/')
			p++;
		if (*p == '/')
			p++;
	}
	if ( (!path[0])
	  || (path[0] == '.' && path[1] == '.' && (path[2] == '/' || path [2] == 0))
	  || (path[strlen (path) - 1] =='/') 
	  || path[0] == '~') {
		return 0;
	}
	return 1;
}

void
Cmd_File_read_f (void)
{
	char *path, *contents;
	int mark;

	if (Cmd_Restricted ()) {
		Cmd_Error ("file_read: access to restricted command denied.\n");
		return;
	}
	if (Cmd_Argc() != 2) {
		Cmd_Error ("file_read: invalid number of arguments.\n");
		return;
	}
	path = strdup (Cmd_Argv(1));
	SYS_CHECKMEM (path);
	if (!Cmd_CollapsePath (path)) {
			free (path);
			Cmd_Error ("file_read: access to restricted directory/file denied.\n");
			return;
	}
	free(path);
	Sys_DPrintf ("file_read: opening %s/%s\n", com_gamedir, path);
	mark = Hunk_LowMark ();
	contents = (char *) COM_LoadHunkFile (Cmd_Argv(1));
	if (!contents) {
		Cmd_Error (va ("file_read: could not open file for reading: %s\n", strerror (errno)));
		return;
	}
	Cmd_Return (contents);
	Hunk_FreeToLowMark (mark);
}

void
Cmd_File_write_f (void)
{
	VFile *file;
	char *path;

	if (Cmd_Restricted ()) {
		Cmd_Error ("file_write: access to restricted command denied.\n");
		return;
	}
	if (Cmd_Argc() != 3) {
		Cmd_Error ("file_write: invalid number of arguments.\n");
		return;
	}
	path = strdup (Cmd_Argv(1));
	SYS_CHECKMEM (path);
	if (!Cmd_CollapsePath (path)) {
			free (path);
			Cmd_Error ("file_write: access to restricted directory/file denied.\n");
			return;
	}
	free(path);
	Sys_DPrintf ("file_write: opening %s/%s\n", com_gamedir, path);
	if (!(file = Qopen (va("%s/%s", com_gamedir, Cmd_Argv(1)), "w"))) {
		Cmd_Error (va ("file_write: could not open file for writing: %s\n", strerror (errno)));
		return;
	}
	Qprintf (file, "%s", Cmd_Argv(2));
	Qclose (file);
}

void
Cmd_File_find_f (void)
{
	DIR *directory;
	struct dirent *entry;
	char *path;
	dstring_t *list;
	
	if (Cmd_Restricted ()) {
		Cmd_Error ("file_find: access to restricted command denied.\n");
		return;
	}
	if (Cmd_Argc() < 2 || Cmd_Argc() > 3) {
		Cmd_Error ("file_find: invalid number of arguments.\n");
		return;
	}

	if (Cmd_Argc() == 3) {
		path = strdup (Cmd_Argv(2));
		if (!Cmd_CollapsePath (path)) {
			free (path);
			Cmd_Error ("file_find: access to restricted directory/file denied.\n");
			return;
		}
		free (path);
	}
	directory = opendir (va ("%s/%s", com_gamedir, Cmd_Argv(2)));
	if (!directory) {
			Cmd_Error (va ("file_find: could not open directory: %s\n", strerror (errno)));
			return;
	}
	list = dstring_newstr ();
	while ((entry = readdir (directory))) {
		if (!fnmatch (Cmd_Argv(1), entry->d_name, 0)) {
			dstring_appendstr (list, "\n");
			dstring_appendstr (list, entry->d_name);
		}
	}
	if (list->str[0])
		Cmd_Return (list->str+1);
	else
		Cmd_Return ("");
	dstring_delete (list);
}
			
	
void
Cmd_Eval_f (void)
{
	if (Cmd_Argc() < 2) {
		Cmd_Error("eval: invalid number of arguments.\n");
		return;
	}
	Cbuf_InsertText (Cmd_Args (1));
}

void
Cmd_Legacy_f (void)
{
	if (Cmd_Argc() < 2) {
		Cmd_Error ("legacy: invalid number of arguments.\n");
		return;
	}
	Cbuf_AddTextTo (cmd_legacybuffer, Cmd_Argsu (1));
}

/* Stats and information */

/* Prints the last backtrace recorded */
void
Cmd_Backtrace_f (void)
{
	Sys_Printf ("%s", cmd_backtrace->str);
}

/* Prints a list of running threads */
void
Cmd_Threadstats_f (void)
{
	cmd_thread_t *t;

	Sys_Printf ("Currently running threads:\n");
	for (t = cmd_threads; t; t = t->next)
		Sys_Printf("%li\n", t->id);
}

/*
	Cmd_Init_Hash

	initialise the command and alias hash tables
*/

void
Cmd_Init_Hash (void)
{
	cmd_hash = Hash_NewTable (1021, cmd_get_key, 0, 0);
	cmd_alias_hash = Hash_NewTable (1021, cmd_alias_get_key, cmd_alias_free, 0);
}

void
Cmd_Init (void)
{
	// register our commands
	Cmd_AddCommand ("stuffcmds", Cmd_StuffCmds_f, "Execute the commands given "
					"at startup again");
	Cmd_AddCommand ("exec", Cmd_Exec_f, "Execute a script file");
	Cmd_AddCommand ("echo", Cmd_Echo_f, "Print text to console");
	Cmd_AddCommand ("alias", Cmd_Alias_f, "Used to create a reference to a "
					"command or list of commands.\n"
					"When used without parameters, displays all current "
					"aliases.\n"
					"Note: Enclose multiple commands within quotes and "
					"seperate each command with a semi-colon.");
	Cmd_AddCommand ("unalias", Cmd_UnAlias_f, "Remove the selected alias");
	Cmd_AddCommand ("wait", Cmd_Wait_f, "Wait a game tic");
	Cmd_AddCommand ("sleep", Cmd_Sleep_f, "Sleep for $1 seconds");
	Cmd_AddCommand ("cmdlist", Cmd_CmdList_f, "List all commands");
	Cmd_AddCommand ("help", Cmd_Help_f, "Display help for a command or "
					"variable");

	Cmd_AddCommand ("if", Cmd_If_f, "Conditionally execute a set of commands.");
	Cmd_SetPure ("if");
	Cmd_AddCommand ("ifnot", Cmd_If_f, "Conditionally execute a set of commands if the condition is false.");
	Cmd_AddCommand ("while", Cmd_While_f, "Execute a set of commands while a condition is true.");
	Cmd_SetPure ("while");
	Cmd_AddCommand ("for", Cmd_For_f, "A while loop with initialization and iteration commands.");
	Cmd_SetPure ("for");
	Cmd_AddCommand ("break", Cmd_Break_f, "Break out of a loop.");
	Cmd_AddCommand ("return", Cmd_Return_f, "Return a value to calling buffer.");
	Cmd_AddCommand ("lset", Cmd_Lset_f, "Sets the value of a local variable (not cvar).");
	Cmd_AddCommand ("backtrace", Cmd_Backtrace_f, "Show a description of the last GIB error and a backtrace.");

	Cmd_AddCommand ("randint", Cmd_Randint_f, "Returns a random integer between $1 and $2");
	Cmd_AddCommand ("streq", Cmd_Streq_f, "Returns 1 if $1 and $2 are the same string, 0 otherwise");
	Cmd_AddCommand ("strlen", Cmd_Strlen_f, "Returns the length of $1");
	Cmd_AddCommand ("file_read", Cmd_File_read_f, "Reads file $1 in the current gamedir and returns its contents.");
	Cmd_AddCommand ("file_write", Cmd_File_write_f, "Write $2 to the file $1 in the current gamedir");
	Cmd_AddCommand ("file_find", Cmd_File_find_f, "Finds a file matching pattern $1 in subdirectory $2 of the gamedir.");
	Cmd_AddCommand ("eval", Cmd_Eval_f, "Evaluates a command.  Useful for callbacks or dynamically generated commands.");
	Cmd_AddCommand ("legacy", Cmd_Legacy_f, "Adds a command to the legacy buffer");
	Cmd_SetPure ("legacy");
	Cmd_AddCommand ("detach", Cmd_Detach_f, "Starts a thread with an initial program of $1");
	Cmd_AddCommand ("killthread", Cmd_Killthread_f, "Kills thread with id $1");
	Cmd_AddCommand ("threadstats", Cmd_Threadstats_f, "Shows statistics about threads");

	cmd_warncmd = Cvar_Get ("cmd_warncmd", "0", CVAR_NONE, NULL, "Toggles the "
							"display of error messages for unknown commands");
	cmd_maxloop = Cvar_Get ("cmd_maxloop", "0", CVAR_NONE, NULL, "Controls the "
							"maximum number of iterations a loop in GIB can do "
							"before being forcefully terminated.  0 is infinite.");
	// Constants for the math interpreter
	// We don't need to assign the return values to anything because these are never used elsewhere
	Cvar_Get ("M_PI", "3.1415926535897932384626433832795029", CVAR_ROM, NULL, "Pi");
}


/* Legacy junk.  Get rid of it sometime */
char       *com_token;
static size_t com_token_size;

static inline void
write_com_token (size_t pos, char c)
{
	if (pos + 1 <= com_token_size) {
	  write:
		com_token[pos] = c;
		return;
	}
	com_token_size = (pos + 1024) & ~1023;
	com_token = realloc (com_token, com_token_size);
	if (!com_token)
		Sys_Error ("COM_Parse: could not allocate %ld bytes",
				   (long) com_token_size);
	goto write;
}

/*
	COM_Parse

	Parse a token out of a string
	FIXME:  Does anything still need this crap?
*/
const char *
COM_Parse (const char *_data)
{
	const byte *data = (const byte *) _data;
	unsigned int c;
	size_t      len = 0;

	write_com_token (len, 0);

	if (!data)
		return NULL;

	// skip whitespace
  skipwhite:
	while ((c = *data) <= ' ') {
		if (c == 0)
			return NULL;				// end of file;
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/') {
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	// handle quoted strings specially
	if (c == '\"') {
		data++;
		while (1) {
			c = *data++;
			if (c == '\"' || !c) {
				write_com_token (len, 0);
				return c ? data : data - 1;
			}
			write_com_token (len, c);
			len++;
		}
	}
	// parse a regular word
	do {
		write_com_token (len, c);
		data++;
		len++;

		c = *data;
	} while (c > 32);

	write_com_token (len, 0);
	return data;
}
