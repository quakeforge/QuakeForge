/*
	flow.c

	PVS PHS generator tool

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2002 Colin Thompson

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
		Boston, MA	02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <getopt.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "QF/bspfile.h"
#include "QF/cmem.h"
#include "QF/cmd.h"
#include "QF/mathlib.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "tools/qfvis/include/vis.h"
#include "tools/qfvis/include/options.h"

static int
CheckStack (cluster_t *cluster, threaddata_t *thread)
{
	pstack_t   *portal;

	for (portal = thread->pstack_head.next; portal; portal = portal->next) {
		if (portal->cluster == cluster) {
			printf ("CheckStack: cluster recursion\n");
			return 1;
		}
		if (!portal->cluster)
			break;
	}
	return 0;
}

static pstack_t *
new_stack (threaddata_t *td)
{
	pstack_t   *stack;

	stack = cmemalloc (td->memsuper, sizeof (pstack_t));
	stack->next = 0;
	stack->mightsee = set_new_size_r (&td->set_pool, portalclusters);
	td->stats.stack_alloc++;
	return stack;
}

static sep_t *
new_separator (threaddata_t *thread)
{
	sep_t      *sep;

	sep = CMEMALLOC (32, sep_t, thread->sep, thread->memsuper);
	thread->stats.sep_alloc++;
	return sep;
}

static void
delete_separator (threaddata_t *thread, sep_t *sep)
{
	thread->stats.sep_free++;
	CMEMFREE (thread->sep, sep);
}

static void
free_separators (threaddata_t *thread, sep_t *sep_list)
{
	unsigned count = thread->stats.sep_alloc - thread->stats.sep_free;
	if (count > thread->stats.sep_highwater) {
		thread->stats.sep_highwater = count;
	}
	count = 0;
	while (sep_list) {
		sep_t      *sep = sep_list;
		sep_list = sep->next;
		delete_separator (thread, sep);
		count++;
	}
	if (count > thread->stats.sep_maxbulk) {
		thread->stats.sep_maxbulk = count;
	}
}

static inline int
test_zero (float d)
{
	if (d < -ON_EPSILON)
		return -1;
	else if (d > ON_EPSILON)
		return 1;
	return 0;
}

static int
calc_plane (vec4f_t v1, vec4f_t v2, int flip, vec4f_t p, vec4f_t *plane)
{
	vec4f_t     length;

	if (flip < 0) {
		//CrossProduct (v2, v1, plane.normal);
		*plane = crossf (v2, v1);
	} else {
		//CrossProduct (v1, v2, plane.normal);
		*plane = crossf (v1, v2);
	}

	length = dotf (*plane, *plane);

	// if points don't make a valid plane, skip it
	if (length[0] < ON_EPSILON)
		return 0;

	(*plane)[3] = -dotf (p, *plane)[0];
	return 1;
}

static inline int
test_plane (vec4f_t plane, const winding_t *pass, unsigned index)
{
	int         s1, s2;
	int         k;
	vec4f_t     d;

	k = index + 1 == pass->numpoints ? 0 : index + 1;
	d = dotf (pass->points[k], plane);
	s1 = test_zero (d[0]);
	k = (index == 0 ? pass->numpoints : index) - 1;
	d = dotf (pass->points[k], plane);
	s2 = test_zero (d[0]);
	if (s1 == 0 && s2 == 0)
		return 0;
	if (s1 < 0 || s2 < 0)
		return 0;
	return 1;
}

static inline sep_t *
create_separator (threaddata_t *thread, vec4f_t src_pl, vec4f_t p1, vec4f_t v1,
				  const winding_t *pass, int index, int flip)
{
	int         fliptest;
	vec4f_t     d;
	vec4f_t     v2;
	vec4f_t     plane;
	sep_t      *sep;

	d = dotf (pass->points[index], src_pl);
	if ((fliptest = test_zero (d[0])) == 0)
		return 0;	// The point lies in the source plane

	v2 = pass->points[index] - p1;
	if (!calc_plane (v1, v2, fliptest, pass->points[index], &plane))
		return 0;	// point does not form a valid plane

	if (!test_plane (plane, pass, index))
		return 0;	// not the right point

	sep = new_separator (thread);
	// flip the normal if we want the back side
	if (flip) {
		sep->plane = -plane;
	} else {
		sep->plane = plane;
	}
	return sep;
}

/*
	Find the planes separating source from pass. The planes form a double
	pyramid with source as the base (ie, source's edges will all be in one
	plane each) and the vertex of the pyramid is between source and pass.
	Edges from pass may or may not be in a plane, but each vertex will be in
	at least one plane.

	If flip is false, the planes will be such that the space enclosed by the
	planes and on the pass side of the vertex are on the front sides of the
	planes. If flip is true, then the space on the source side of the vertex
	and enclosed by the planes is on the front side of the planes.

		// find a vertex of pass that makes a plane that puts all of the
		// vertexes of pass on the front side and all of the vertexes of
		// source on the back side
*/
static sep_t *
FindSeparators (threaddata_t *thread,
				const winding_t *source, vec4f_t src_pl,
				const winding_t *pass, int flip)
{
	unsigned    i, j, l;
	vec4f_t     v1;

	sep_t      *separators = 0, *sep;

	for (i = 0; i < source->numpoints; i++) {
		l = (i + 1) == source->numpoints ? 0 : i + 1;
		v1 = source->points[l] - source->points[i];

		for (j = 0; j < pass->numpoints; j++) {
			sep = create_separator (thread, src_pl, source->points[i], v1,
									pass, j, flip);
			if (sep) {
				sep->next = separators;
				separators = sep;
				break;
			}
		}
	}
	return separators;
}

static winding_t *
ClipToSeparators (threaddata_t *thread, const sep_t *separators,
				  winding_t *target)
{
	const sep_t *sep;

	for (sep = separators; target && sep; sep = sep->next) {
		target = ClipWinding (thread, target, sep->plane, false);
	}
	return target;
}

static inline set_t *
select_test_set (portal_t *portal, threaddata_t *thread)
{
	set_t      *test;

	if (portal->status == stat_done) {
		thread->stats.vistest++;
		test = portal->visbits;
	} else {
		thread->stats.mighttest++;
		test = portal->mightsee;
	}
	return test;
}

static inline int
mightsee_more (set_t *might, const set_t *prev_might, const set_t *test,
			   const set_t *vis)
{
	unsigned    i;
	set_bits_t  more = 0;

	// might = intersection (prev_might, test)
	// more = (might is not a subset of vis)
	for (i = 0; i < SET_WORDS (might); i++) {
		might->map[i] = prev_might->map[i] & test->map[i];
		more |= might->map[i] & ~vis->map[i];
	}
	return more != 0;
}

static void
free_winding_memory (threaddata_t *thread, size_t winding_mark)
{
	thread->stats.winding_mark = max (thread->stats.winding_mark,
									  Hunk_LowMark (thread->hunk));
	Hunk_RawFreeToLowMark (thread->hunk, winding_mark);
}

/*
	TargetChecks

	Filter mightsee by clipping against all portals
*/
static unsigned
TargetChecks(threaddata_t *thread,
			 pstack_t *prevstack,
			 set_t **mainportalbits,
			 set_t **tempportalbits)
{
	unsigned    i, initial_portalcheck;
	portal_t   *target_portal;
	vec4f_t     backplane;
	vec4f_t     source_plane, pass_plane;
	const winding_t *pass_winding;
	winding_t  *source_winding, *target_winding;

	if (prevstack->pass_winding == NULL)
		return 0;
	
	thread->stats.chains++;

	// initialize counter
	initial_portalcheck = thread->stats.portalcheck;

	// reuse separators
	sep_t *separators[2] = {NULL, NULL};
	if (prevstack->next) {
		separators[0] = prevstack->next->separators[0];
		separators[1] = prevstack->next->separators[1];
	}

	// empty cluster set
	set_t *mightsee = thread->scratchbits;
	set_empty(mightsee);

	// empty portal set
	set_t *prevportalbits = *mainportalbits;
	set_t *portalbits = *tempportalbits;
	set_empty(portalbits);
	*mainportalbits = portalbits;
	*tempportalbits = prevportalbits;

	source_plane = thread->pstack_head.pass_plane;
	pass_winding = prevstack->pass_winding;
	pass_plane = prevstack->pass_plane;

	// lower than 2 won't have a benefit. higher than 2 refines the source
	// portal even though it isn't propagated forward right now.
	int level = 2; // = options.level

	size_t      winding_mark = Hunk_LowMark (thread->hunk);

	// check all portals for flowing into other leafs
	for (i = 0, target_portal = portals; i < numportals * 2; i++, target_portal++) {

		if (!set_is_member(prevstack->mightsee, target_portal->cluster))
            continue;           // can't possibly see it
		if (!set_is_member(prevportalbits, i))
            continue;           // can't possibly see it
		if (set_is_member(mightsee, target_portal->cluster)) {
			// target check already done and passed
			set_add(portalbits, i); // let the portal survive to the next level
            continue;           
		}

		// get plane of target_portal, point normal into the neighbor cluster
		backplane = -target_portal->plane;

		vec4f_t     diff = vabs4f (pass_plane - backplane);
		vec4i_t     cmp = diff > (vec4f_t) {0.001, 0.001, 0.001, 0.001};
		if (!(cmp[0] || cmp[1] || cmp[2])) { // dist isn't interesting
			continue;		// can't go out a coplanar face
		}

		free_winding_memory (thread, winding_mark);

		thread->stats.portalcheck++;

		target_winding = target_portal->winding;
		target_winding = ClipWinding (thread, target_winding, source_plane, false);
		if (!target_winding)
			continue;

		if (!pass_winding) {
			// unreachable
		}

		target_winding = ClipWinding (thread, target_winding, pass_plane, false);
		if (!target_winding)
			continue;

		// copy source_winding because it likely is already a copy and thus
		// if it gets clipped away, earlier stack levels will get corrupted
		source_winding = CopyWinding (thread, prevstack->source_winding);

		source_winding = ClipWinding (thread, source_winding, backplane, false);
		if (!source_winding) {
			continue;
		}

		thread->stats.portaltest++;
		thread->stats.targettested++;

		if (level > 0) {
			winding_t  *old = target_winding;
			if (!separators[0])
				separators[0] = FindSeparators (thread,
												source_winding,
												source_plane,
												pass_winding, 0);
			target_winding = ClipToSeparators (thread,
											   separators[0],
											   target_winding);
			if (!target_winding) {
				thread->stats.targetclipped++;
				continue;
			}
			if (target_winding != old)
				thread->stats.targettrimmed++;
		}

		if (level > 1) {
			winding_t  *old = target_winding;
			if (!separators[1])
				separators[1] = FindSeparators (thread,
												pass_winding,
												pass_plane,
												source_winding, 1);
			target_winding = ClipToSeparators (thread,
											   separators[1],
											   target_winding);
			if (!target_winding) {
				thread->stats.targetclipped++;
				continue;
			}
			if (target_winding != old)
				thread->stats.targettrimmed++;
		}

		thread->stats.sourcetested++;
		if (level > 2) {
			winding_t  *old = source_winding;
			sep_t      *sep;
			sep = FindSeparators (thread,
								  target_winding, target_portal->plane,
								  pass_winding, 0);
			source_winding = ClipToSeparators (thread, sep, source_winding);
			free_separators (thread, sep);
			if (!source_winding) {
				thread->stats.sourceclipped++;
				continue;
			}
			if (source_winding != old)
				thread->stats.sourcetrimmed++;
		}

		if (level > 3) {
			winding_t  *old = source_winding;
			sep_t      *sep;
			sep = FindSeparators (thread, pass_winding, pass_plane,
								  target_winding, 1);
			source_winding = ClipToSeparators (thread, sep, source_winding);
			free_separators (thread, sep);
			if (!source_winding) {
				thread->stats.sourceclipped++;
				continue;
			}
			if (source_winding != old)
				thread->stats.sourcetrimmed++;
		}

		thread->stats.portalpass++;

		set_add(mightsee, target_portal->cluster);
		set_add(portalbits, i);
	}

	free_winding_memory (thread, winding_mark);
	if (prevstack->next == NULL) {
		// only free if they aren't borrowed
		free_separators (thread, separators[1]);
		free_separators (thread, separators[0]);
	}

	// replace with reduced version
	thread->scratchbits = prevstack->mightsee;
	prevstack->mightsee = mightsee;

	return thread->stats.portalcheck - initial_portalcheck;
}

/*
	IterativeTargetChecks

	Retrace the path and reduce mightsee by clipping the targets directly
*/
static unsigned
IterativeTargetChecks(threaddata_t *thread, pstack_t *head)
{
	unsigned numchecks = 0;

	set_t *portalbits = set_new_size_r(&thread->set_pool, numportals * 2);
	set_t *tempportalbits = set_new_size_r(&thread->set_pool, numportals * 2);
	set_invert(portalbits);

	for (pstack_t* stack = head; stack && stack->cluster; stack = stack->next)
	{
		if (stack->did_targetchecks)
			continue;

		numchecks += TargetChecks(thread, stack, &portalbits, &tempportalbits);

		if (stack->next)
			set_intersection(stack->next->mightsee, stack->mightsee);

		// mark as done
		stack->did_targetchecks = true;
		stack->num_expected_targetchecks = 0;
	}

	set_delete_r(&thread->set_pool, portalbits);
	set_delete_r(&thread->set_pool, tempportalbits);

	return numchecks;
}

/*
	RecursiveClusterFlow

	Flood fill through the clusters
*/
static void
RecursiveClusterFlow (int clusternum, threaddata_t *thread, pstack_t *prevstack)
{
	int         i;
	set_t      *might;
	const set_t *test, *vis;
	cluster_t  *cluster;
	pstack_t   *stack;
	portal_t   *target_portal;
	vec4f_t     backplane;
	vec4f_t     source_plane, pass_plane;
	const winding_t *pass_winding;
	winding_t  *source_winding, *target_winding;

	thread->stats.chains++;

	if (!prevstack->next)
		prevstack->next = new_stack (thread);
	stack = prevstack->next;
	stack->cluster = 0;

	cluster = &clusters[clusternum];
	if (CheckStack(cluster, thread))
		return;

	// mark the cluster as visible
	if (!set_is_member (thread->clustervis, clusternum)) {
		set_add (thread->clustervis, clusternum);
		thread->base->numcansee++;
	}

	// check all target portals instead of just neighbor portals, if the time is right
	if (options.targetratio > 0.0 &&
		prevstack->num_expected_targetchecks > 0 &&
		thread->numsteps * options.targetratio >=
		thread->numtargetchecks + prevstack->num_expected_targetchecks)
	{
		unsigned num_actual_targetchecks = IterativeTargetChecks(thread, thread->pstack_head.next);
		// thread->stats.c_targetcheck += num_actual_targetchecks;
		thread->numtargetchecks += num_actual_targetchecks;
		// prevstack.num_expected_targetchecks is zero now
	}

	stack->cluster = cluster;
	stack->pass_portal = NULL;
	stack->separators[0] = 0;
	stack->separators[1] = 0;
	might = stack->mightsee;
	vis = thread->clustervis;

	source_plane = thread->pstack_head.pass_plane;
	pass_winding = prevstack->pass_winding;
	pass_plane = prevstack->pass_plane;

	size_t      winding_mark = Hunk_LowMark (thread->hunk);

	// check all portals for flowing into other clusters
	for (i = 0; i < cluster->numportals; i++) {
		target_portal = &cluster->portals[i];
		if (!set_is_member (prevstack->mightsee, target_portal->cluster))
			continue;		// can't possibly see it

		// if target_portal can't see anything we haven't already seen, skip it
		test = select_test_set (target_portal, thread);
		might = stack->mightsee; // can change between iterations
		if (!mightsee_more (might, prevstack->mightsee, test, vis)) {
			// can't see anything new
			continue;
		}

		stack->did_targetchecks = false;
		stack->num_expected_targetchecks = 0;
		// calculate num_expected_targetchecks only if we're using it, since it's somewhat expensive to compute
		if (options.targetratio > 0.0)
			stack->num_expected_targetchecks =
				prevstack->num_expected_targetchecks +
				set_count(might);

		// get plane of target_portal, point normal into the neighbor cluster
		backplane = -target_portal->plane;

		vec4f_t     diff = vabs4f (pass_plane - backplane);
		vec4i_t     cmp = diff > (vec4f_t) {0.001, 0.001, 0.001, 0.001};
		if (!(cmp[0] || cmp[1] || cmp[2])) { // dist isn't interesting
			continue;		// can't go out a coplanar face
		}

		free_winding_memory (thread, winding_mark);

		thread->numsteps++;
		thread->stats.portalcheck++;

		target_winding = target_portal->winding;
		target_winding = ClipWinding (thread, target_winding, source_plane, false);
		if (!target_winding)
			continue;

		if (!pass_winding) {
			// the second cluster can be blocked only if coplanar

			stack->source_winding = prevstack->source_winding;

			stack->pass_winding = target_winding;
			stack->pass_plane = target_portal->plane;
			stack->pass_portal = target_portal;

			RecursiveClusterFlow (target_portal->cluster, thread, stack);
			continue;
		}

		target_winding = ClipWinding (thread, target_winding, pass_plane, false);
		if (!target_winding)
			continue;

		// copy source_winding because it likely is already a copy and thus
		// if it gets clipped away, earlier stack levels will get corrupted
		source_winding = CopyWinding (thread, prevstack->source_winding);

		source_winding = ClipWinding (thread, source_winding, backplane, false);
		if (!source_winding) {
			continue;
		}

		thread->stats.portaltest++;
		thread->stats.targettested++;

		if (options.level > 0) {
			winding_t  *old = target_winding;
			if (!stack->separators[0])
				stack->separators[0] = FindSeparators (thread,
													   source_winding,
													   source_plane,
													   pass_winding, 0);
			target_winding = ClipToSeparators (thread,
											   stack->separators[0],
											   target_winding);
			if (!target_winding) {
				thread->stats.targetclipped++;
				continue;
			}
			if (target_winding != old)
				thread->stats.targettrimmed++;
		}

		if (options.level > 1) {
			winding_t  *old = target_winding;
			if (!stack->separators[1])
				stack->separators[1] = FindSeparators (thread,
													   pass_winding,
													   pass_plane,
													   source_winding, 1);
			target_winding = ClipToSeparators (thread,
											   stack->separators[1],
											   target_winding);
			if (!target_winding) {
				thread->stats.targetclipped++;
				continue;
			}
			if (target_winding != old)
				thread->stats.targettrimmed++;
		}

		thread->stats.sourcetested++;
		if (options.level > 2) {
			winding_t  *old = source_winding;
			sep_t      *sep;
			sep = FindSeparators (thread,
								  target_winding, target_portal->plane,
								  pass_winding, 0);
			source_winding = ClipToSeparators (thread, sep, source_winding);
			free_separators (thread, sep);
			if (!source_winding) {
				thread->stats.sourceclipped++;
				continue;
			}
			if (source_winding != old)
				thread->stats.sourcetrimmed++;
		}

		if (options.level > 3) {
			winding_t  *old = source_winding;
			sep_t      *sep;
			sep = FindSeparators (thread, pass_winding, pass_plane,
								  target_winding, 1);
			source_winding = ClipToSeparators (thread, sep, source_winding);
			free_separators (thread, sep);
			if (!source_winding) {
				thread->stats.sourceclipped++;
				continue;
			}
			if (source_winding != old)
				thread->stats.sourcetrimmed++;
		}

		stack->source_winding = source_winding;
		stack->pass_winding = target_winding;

		stack->pass_plane = target_portal->plane;
		stack->pass_portal = target_portal;

		thread->stats.portalpass++;

		// flow through it for real
		RecursiveClusterFlow (target_portal->cluster, thread, stack);
	}
	free_winding_memory (thread, winding_mark);
	free_separators (thread, stack->separators[1]);
	free_separators (thread, stack->separators[0]);
}

void
PortalFlow (threaddata_t *data, portal_t *portal)
{
	WRLOCK_PORTAL (portal);
	if (portal->status != stat_selected)
		Sys_Error ("PortalFlow: reflowed");
	portal->status = stat_working;
	UNLOCK_PORTAL (portal);

	portal->visbits = set_new_size_r (&data->set_pool, portalclusters);

	data->scratchbits = set_new_size_r (&data->set_pool, portalclusters);
	data->clustervis = portal->visbits;
	data->base = portal;

	data->pstack_head.cluster = 0;
	data->pstack_head.pass_portal = portal;
	data->pstack_head.source_winding = portal->winding;
	data->pstack_head.pass_winding = 0;
	data->pstack_head.pass_plane = portal->plane;
	data->pstack_head.mightsee = portal->mightsee;
	data->pstack_head.separators[0] = 0;
	data->pstack_head.separators[1] = 0;

	RecursiveClusterFlow (portal->cluster, data, &data->pstack_head);

	set_delete_r(&data->set_pool, data->scratchbits);
}
