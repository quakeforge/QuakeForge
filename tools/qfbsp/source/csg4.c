/*
	Copyright (C) 1996-1997  Id Software, Inc.

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

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include "string.h"
#endif

#include "QF/sys.h"

#include "bsp5.h"

/*
	NOTES

	Brushes that touch still need to be split at the cut point to make a
	tjunction
*/

face_t     *validfaces[MAX_MAP_PLANES];
face_t     *inside, *outside;
int         brushfaces;
int         csgfaces;
int         csgmergefaces;


/*
	NewFaceFromFace

	Duplicates the non point information of a face, used by SplitFace and
	MergeFace.
*/
face_t     *
NewFaceFromFace (face_t *in)
{
	face_t     *newf;

	newf = AllocFace ();

	newf->planenum = in->planenum;
	newf->texturenum = in->texturenum;
	newf->planeside = in->planeside;
	newf->original = in->original;
	newf->contents[0] = in->contents[0];
	newf->contents[1] = in->contents[1];
	newf->detail = in->detail;

	return newf;
}

void
SplitFace (face_t *in, plane_t *split, face_t **front, face_t **back)
{
	face_t     *newf, *new2;
	int         i, j;
	int         sides[MAXEDGES + 1];
	int         counts[3];
	vec_t       dot;
	vec_t       dists[MAXEDGES + 1];
	vec_t      *p1, *p2;
	vec3_t      mid;

	if (in->numpoints < 0)
		Sys_Error ("SplitFace: freed face");
	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for (i = 0; i < in->numpoints; i++) {
		dot = DotProduct (in->pts[i], split->normal);
		dot -= split->dist;
		dists[i] = dot;
		if (dot > ON_EPSILON)
			sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON)
			sides[i] = SIDE_BACK;
		else
			sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if (!counts[0]) {
		*front = NULL;
		*back = in;
		return;
	}
	if (!counts[1]) {
		*front = in;
		*back = NULL;
		return;
	}

	*back = newf = NewFaceFromFace (in);
	*front = new2 = NewFaceFromFace (in);

	// distribute the points and generate splits
	for (i = 0; i < in->numpoints; i++) {
		if (newf->numpoints > MAXEDGES || new2->numpoints > MAXEDGES)
			Sys_Error ("SplitFace: numpoints > MAXEDGES");

		p1 = in->pts[i];

		if (sides[i] == SIDE_ON) {
			VectorCopy (p1, newf->pts[newf->numpoints]);
			newf->numpoints++;
			VectorCopy (p1, new2->pts[new2->numpoints]);
			new2->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			VectorCopy (p1, new2->pts[new2->numpoints]);
			new2->numpoints++;
		} else {
			VectorCopy (p1, newf->pts[newf->numpoints]);
			newf->numpoints++;
		}

		if (sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		// generate a split point
		p2 = in->pts[(i + 1) % in->numpoints];

		dot = dists[i] / (dists[i] - dists[i + 1]);
		for (j = 0; j < 3; j++) {		// avoid round off error when possible
			if (split->normal[j] == 1)
				mid[j] = split->dist;
			else if (split->normal[j] == -1)
				mid[j] = -split->dist;
			else
				mid[j] = p1[j] + dot * (p2[j] - p1[j]);
		}

		VectorCopy (mid, newf->pts[newf->numpoints]);
		newf->numpoints++;
		VectorCopy (mid, new2->pts[new2->numpoints]);
		new2->numpoints++;
	}

	if (newf->numpoints > MAXEDGES || new2->numpoints > MAXEDGES)
		Sys_Error ("SplitFace: numpoints > MAXEDGES");

	// free the original face now that is is represented by the fragments
	FreeFace (in);
}

/*
	ClipInside

	Clips all of the faces in the inside list, possibly moving them to the
	outside list or spliting it into a piece in each list.

	Faces exactly on the plane will stay inside unless overdrawn by later brush

	frontside is the side of the plane that holds the outside list
*/
static void
ClipInside (int splitplane, int frontside, qboolean precedence)
{
	face_t     *insidelist, *next, *f;
	face_t     *frags[2];
	plane_t    *split;

	split = &planes[splitplane];

	insidelist = NULL;
	for (f = inside; f; f = next) {
		next = f->next;

		if (f->planenum == splitplane) {	// exactly on, handle special
			if (frontside != f->planeside || precedence) {	// always clip off
															// opposite faceing
				frags[frontside] = NULL;
				frags[!frontside] = f;
			} else {					// leave it on the outside
				frags[frontside] = f;
				frags[!frontside] = NULL;
			}
		} else {						// proper split
			SplitFace (f, split, &frags[0], &frags[1]);
		}

		if (frags[frontside]) {
			frags[frontside]->next = outside;
			outside = frags[frontside];
		}
		if (frags[!frontside]) {
			frags[!frontside]->next = insidelist;
			insidelist = frags[!frontside];
		}
	}

	inside = insidelist;
}

/*
	SaveOutside

	Saves all of the faces in the outside list to the bsp plane list
*/
static void
SaveOutside (qboolean mirror)
{
	face_t     *f, *next, *newf;
	int         planenum, i;

	for (f = outside; f; f = next) {
		next = f->next;
		csgfaces++;
		Draw_DrawFace (f);
		planenum = f->planenum;

		if (mirror) {
			newf = NewFaceFromFace (f);

			newf->numpoints = f->numpoints;
			newf->planeside = f->planeside ^ 1;	// reverse side
			newf->contents[0] = f->contents[1];
			newf->contents[1] = f->contents[0];

			for (i = 0; i < f->numpoints; i++)	// add points backwards
			{
				VectorCopy (f->pts[f->numpoints - 1 - i], newf->pts[i]);
			}
		} else
			newf = NULL;

		validfaces[planenum] = MergeFaceToList (f, validfaces[planenum]);
		if (newf)
			validfaces[planenum] = MergeFaceToList (newf,
													validfaces[planenum]);

		validfaces[planenum] = FreeMergeListScraps (validfaces[planenum]);
	}
}

/*
	FreeInside

	Free all the faces that got clipped out
*/
static void
FreeInside (int contents)
{
	face_t     *next, *f;

	for (f = inside; f; f = next) {
		next = f->next;

		if (contents != CONTENTS_SOLID) {
			f->contents[0] = contents;
			f->next = outside;
			outside = f;
		} else
			FreeFace (f);
	}
}

/*
	BuildSurfaces

	Returns a chain of all the external surfaces with one or more visible
	faces.
*/
surface_t  *
BuildSurfaces (void)
{
	face_t     *count;
	face_t    **f;
	int         i;
	surface_t  *surfhead, *s;

	surfhead = NULL;

	f = validfaces;
	for (i = 0; i < numbrushplanes; i++, f++) {
		if (!*f)
			continue;					// nothing left on this plane

		// create a new surface to hold the faces on this plane
		s = AllocSurface ();
		s->planenum = i;
		s->next = surfhead;
		surfhead = s;
		s->faces = *f;
		for (count = s->faces; count; count = count->next) {
			csgmergefaces++;
			if (count->detail)
				s->has_detail = 1;
			else
				s->has_struct = 1;
		}
		CalcSurfaceInfo (s);			// bounding box and flags
	}

	return surfhead;
}

static void
CopyFacesToOutside (brush_t *b)
{
	face_t     *newf, *f;

	outside = NULL;

	for (f = b->faces; f; f = f->next) {
		if (f->texturenum == TEX_SKIP)
			continue;

		brushfaces++;
		newf = AllocFace ();
		*newf = *f;
		newf->next = outside;
		newf->contents[0] = CONTENTS_EMPTY;
		newf->contents[1] = b->contents;
		outside = newf;
	}
}

/*
	CSGFaces

	Returns a list of surfaces containing aall of the faces
*/
surface_t  *
CSGFaces (brushset_t *bs)
{
	brush_t    *b1, *b2;
	face_t     *f;
	int         i;
	qboolean    overwrite;
	surface_t  *surfhead;

	qprintf ("---- CSGFaces ----\n");

	memset (validfaces, 0, sizeof (validfaces));

	csgfaces = brushfaces = csgmergefaces = 0;

	Draw_ClearWindow ();

	// do the solid faces
	for (b1 = bs->brushes; b1; b1 = b1->next) {
		// set outside to a copy of the brush's faces
		CopyFacesToOutside (b1);

		if (b1->faces->texturenum < 0) {
			// Don't split HINT and SKIP brushes.
			SaveOutside (false);
			continue;
		}

		overwrite = false;

		for (b2 = bs->brushes; b2; b2 = b2->next) {
			if (b2->faces->texturenum < 0)
				continue;
			// see if b2 needs to clip a chunk out of b1

			if (b1 == b2) {
				overwrite = true;		// later brushes now overwrite
				continue;
			}
			// check bounding box first
			for (i = 0; i < 3; i++)
				if (b1->mins[i] > b2->maxs[i] || b1->maxs[i] < b2->mins[i])
					break;
			if (i < 3)
				continue;

			// divide faces by the planes of the new brush

			inside = outside;
			outside = NULL;

			for (f = b2->faces; f; f = f->next)
				ClipInside (f->planenum, f->planeside, overwrite);

			// these faces are continued in another brush, so get rid of them
			if (b1->contents == CONTENTS_SOLID
				&& b2->contents <= CONTENTS_WATER)
				FreeInside (b2->contents);
			else
				FreeInside (CONTENTS_SOLID);
		}

		// all of the faces left in outside are real surface faces
		if (b1->contents != CONTENTS_SOLID)
			SaveOutside (true);			// mirror faces for inside view
		else
			SaveOutside (false);
	}

#if 0
	if (!csgfaces)
		Sys_Error ("No faces");
#endif

	surfhead = BuildSurfaces ();

	qprintf ("%5i brushfaces\n", brushfaces);
	qprintf ("%5i csgfaces\n", csgfaces);
	qprintf ("%5i mergedfaces\n", csgmergefaces);

	return surfhead;
}
