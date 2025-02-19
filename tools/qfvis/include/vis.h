/*
	vis.h

	PVS/PHS generation tool

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
		Boston, MA  02111-1307, USA

*/

#ifndef __vis_h
#define __vis_h

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#if defined (HAVE_PTHREAD_H) && defined (HAVE_PTHREAD)
#define USE_PTHREADS
#endif

#ifdef USE_PTHREADS
#include <pthread.h>
extern pthread_rwlock_t *global_lock;
extern pthread_rwlock_t *portal_locks;
extern pthread_rwlock_t *stats_lock;

#define	WRLOCK(l) \
	do { \
		if (options.threads > 1) \
			pthread_rwlock_wrlock (l); \
	} while (0)

#define	RDLOCK(l) \
	do { \
		if (options.threads > 1) \
			pthread_rwlock_rdlock (l); \
	} while (0)

#define	UNLOCK(l)	\
	do { \
		if (options.threads > 1)  \
			pthread_rwlock_unlock (l); \
	} while (0)

#define WRLOCK_PORTAL(p) WRLOCK (&portal_locks[p - portals])
#define RDLOCK_PORTAL(p) RDLOCK (&portal_locks[p - portals])
#define UNLOCK_PORTAL(p) UNLOCK (&portal_locks[p - portals])

#else
#define	LOCK(l)
#define	UNLOCK(l)
#define WRLOCK_PORTAL(p)
#define RDLOCK_PORTAL(p)
#define UNLOCK_PORTAL(p)
#endif

#include "QF/cmem.h"
#include "QF/dstring.h"
#include "QF/set.h"
#include "QF/zone.h"
#include "QF/simd/vec4f.h"

#define	PORTALFILE				"PRT1"
#define	PORTALFILE_AM			"PRT1-AM"
#define	PORTALFILE2				"PRT2"
#define	ON_EPSILON				0.1

typedef struct winding_s {
	struct winding_s *next;
	bool        original;	// don't free, it's part of the portal
	unsigned    numpoints;
	int         id;
	int         thread;
	vec4f_t     points[1];	// variable sized
} winding_t;

typedef enum {
	stat_none,
	stat_selected,
	stat_working,
	stat_done
} vstatus_t;

typedef struct {
	vec4f_t     plane;		// normal pointing into neighbor
	vspheref_t  sphere;		// bounding sphere
	int         cluster;	// neighbor
	winding_t  *winding;
	vstatus_t   status;
	set_t      *visbits;
	set_t      *mightsee;
	int         nummightsee;
	int         numcansee;
} portal_t;

typedef struct seperating_plane_s {
	vec4f_t     plane;		// from portal is on positive side
	struct seperating_plane_s *next;
} sep_t;

typedef struct passage_s {
	struct passage_s *next;
	int         from, to;	// cluster numbers
	sep_t      *planes;
} passage_t;

typedef struct cluster_s {
	int         numportals;
	int         visofs;
	vspheref_t  sphere;
	passage_t  *passages;
	portal_t   *portals;
} cluster_t;

typedef struct pstack_s {
	struct pstack_s *next;		///< linked list of active stack objects
	cluster_t  *cluster;		///< the cluster being sub-vised
	winding_t  *source_winding;	///< clipped source portal winding
	portal_t   *pass_portal;	///< the portal exiting from the cluster
	vec4f_t     pass_plane;		///< plane of the pass portal
	winding_t  *pass_winding;	///< clipped pass portal winding
	set_t      *mightsee;
	sep_t      *separators[2];
	bool        did_targetchecks; ///< mightsee has been reduced with source/pass
	unsigned    num_expected_targetchecks; ///< estimated cost of doing target checks,
	                            /// including cost of level above
} pstack_t;

typedef struct {
	unsigned long portaltest;	///< number of portals tested via separators
	unsigned long portalpass;	///< number of portals through which vis passes
	unsigned long portalcheck;	///< number of portal checks
	unsigned long targettested;	///< number of times target portal tested
	unsigned long targettrimmed;///< number of times target portal trimmed
	unsigned long targetclipped;///< number of times target portal clipped away
	unsigned long sourcetested;	///< number of times source portal tested
	unsigned long sourcetrimmed;///< number of times source portal trimmed
	unsigned long sourceclipped;///< number of times source portal clipped away
	unsigned long chains;		///< number of visits to clusters
	unsigned long mighttest;	///< amount mightsee is used for masked tests
	unsigned long vistest;		///< amount visbits is used for masked tests
	unsigned long mightseeupdate;///< amount of updates to waiting portals
	unsigned    sep_alloc;		///< how many separators were allocated
	unsigned    sep_free;		///< how many separators were freed
	unsigned    sep_highwater;	///< most separators in flight
	unsigned    sep_maxbulk;	///< most separators freed at once
	size_t      winding_mark;	///< most memory allocated to windings
	unsigned    stack_alloc;	///< how many stack blocks were allocated
	unsigned    stack_free;		///< how many stack blocks were freed
} visstat_t;

typedef struct threaddata_s {
	visstat_t   stats;			///< per-thread statistics merged on completion
	set_t      *clustervis;		///< clusters base portal can see
	portal_t   *base;			///< portal for which this thread is being run
	pstack_t    pstack_head;
	sep_t      *sep_freelist;	///< per-thread list of free separators
	memsuper_t *memsuper;		///< per-thread memory pool
	memhunk_t  *hunk;
	dstring_t  *str;
	set_pool_t  set_pool;
	int         id;
	int         winding_id;
	unsigned    numsteps;
	unsigned    numtargetchecks;
	set_t      *scratchbits;
} threaddata_t;

typedef struct {
	set_t      *portalsee;
	unsigned long selfcull;		///< number of protals culled by self
	unsigned long clustercull;	///< number of portals culled by cluster sphere
	unsigned long clustertest;	///< number of portals tested by cluster
	unsigned long spheretest;	///< number of portal sphere tests done
	unsigned long spherecull;	///< number of portals culled by sphere tests
	unsigned long spherepass;	///< number of portals passed by sphere tests
	unsigned long windingtest;	///< number of portal pairs tested by winding
	unsigned long windingcull;	///< number of portals culled by winding tests
	unsigned long windingpass;	///< number of portals passed by winding tests
	unsigned long clustersee;
	int         id;
} basethread_t;

extern unsigned numportals;
extern unsigned portalclusters;
extern unsigned numrealleafs;
extern int bitbytes;
extern int bitbytes_l;
extern int bitlongs;
extern struct bsp_s *bsp;

extern portal_t *portals;
extern cluster_t *clusters;
extern uint32_t *leafcluster;
extern byte *uncompressed;

winding_t *NewWinding (threaddata_t *thread, int points);
winding_t *ClipWinding (threaddata_t *thread, winding_t *in, vec4f_t split,
						bool keepon);
winding_t *CopyWinding (threaddata_t *thread, const winding_t *w);

void ClusterFlow (int clusternum);
void PortalBase (basethread_t *thread, portal_t *portal);
void PortalFlow (threaddata_t *data, portal_t *portal);
void CalcAmbientSounds (void);

struct sizebuf_s;
int CompressRow (struct sizebuf_s *dest, const byte *vis, unsigned num_leafs,
				 int utf8);

void CalcFatPVS (void);

void RunThreads (const char *heading, void *(*thread_func) (void *),
				 int (*calc_progress)(void));

extern const char spinner[];
extern const char progress[];
extern int *working;
extern int progress_tick;

#endif// __vis_h
