/*
	set.c

	Set manipulation.

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/8/4

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

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/set.h"

static set_pool_t static_set_pool = {
	0, 0,
	DARRAY_STATIC_INIT (8),
	DARRAY_STATIC_INIT (8),
};

static set_iter_t *
new_setiter (set_pool_t *set_pool)
{
	set_iter_t *set_iter;
	ALLOC (16, set_iter_t, set_pool->set_iter, set_iter);
	return set_iter;
}

static void
delete_setiter (set_pool_t *set_pool, set_iter_t *set_iter)
{
	FREE (set_pool->set_iter, set_iter);
}

void
set_del_iter (set_iter_t *set_iter)
{
	delete_setiter (&static_set_pool, set_iter);
}

void
set_del_iter_r (set_pool_t *set_pool, set_iter_t *set_iter)
{
	delete_setiter (set_pool, set_iter);
}

void
set_pool_init (set_pool_t *set_pool)
{
	*set_pool = (set_pool_t) {
		.set_blocks = DARRAY_STATIC_INIT (8),
		.set_iter_blocks = DARRAY_STATIC_INIT (8),
	};
}

void
set_pool_clear (set_pool_t *set_pool)
{
	ALLOC_FREE_BLOCKS (set_pool->set);
	ALLOC_FREE_BLOCKS (set_pool->set_iter);
}

inline set_t *
set_new_r (set_pool_t *set_pool)
{
	set_t      *set;

	ALLOC (16, set_t, set_pool->set, set);
	set->size = sizeof (set->defmap) * 8;
	set->map = set->defmap;
	return set;
}

set_t *
set_new (void)
{
	return set_new_r (&static_set_pool);
}

void
set_delete_r (set_pool_t *set_pool, set_t *set)
{
	if (set->map != set->defmap)
		free (set->map);
	FREE (set_pool->set, set);
}

void
set_delete (set_t *set)
{
	set_delete_r (&static_set_pool, set);
}

void
set_expand (set_t *set, unsigned size)
{
	set_bits_t *map = set->map;

	if (size <= set->size)
		return;

	size = SET_SIZE (size - 1);
	set->map = malloc (size / 8);
	memcpy (set->map, map, set->size / 8);
	memset (set->map + SET_WORDS (set), 0, (size - set->size) / 8);
	set->size = size;
	if (map != set->defmap)
		free (map);
}

void
set_trim (set_t *set)
{
	if (set->map == set->defmap) {
		return;
	}
	unsigned    words = SET_WORDS (set);

	while (words > SET_DEFMAP_SIZE && !set->map[words - 1]) {
		words--;
		set->size -= SET_BITS;
	}
	if (words > SET_DEFMAP_SIZE) {
		set_bits_t *map = realloc (set->map, words * sizeof (set_bits_t));
		if (map && map != set->map) {
			set->map = map;
		}
	} else {
		memcpy (set->defmap, set->map, sizeof (set->defmap));
		free (set->map);
		set->map = set->defmap;
	}
}

inline set_t *
set_new_size_r (set_pool_t *set_pool, unsigned size)
{
	set_t      *set;

	set = set_new_r (set_pool);
	set_expand (set, size);

	return set;
}

set_t *
set_new_size (unsigned size)
{
	return set_new_size_r (&static_set_pool, size);
}

static inline void
_set_add (set_t *set, unsigned x)
{
	if (x >= set->size)
		set_expand (set, x + 1);
	SET_ADD(set, x);
}

static inline void
_set_remove (set_t *set, unsigned x)
{
	if (x >= set->size)
		return;
	SET_REMOVE(set, x);
}

#define START_MASK(start) \
	((~SET_ZERO) << (start % SET_BITS))
#define END_MASK(end) \
	((~SET_ZERO) >> ((SET_BITS - ((end + 1) % SET_BITS)) % SET_BITS))

static inline void
_set_add_range (set_t *set, unsigned start, unsigned count)
{
	if (!count) {
		return;
	}
	if (start + count > set->size) {
		set_expand (set, start + count);
	}
	unsigned    end = start + count - 1;
	set_bits_t  start_mask = START_MASK (start);
	set_bits_t  end_mask = END_MASK (end);
	unsigned    start_ind = start / SET_BITS;
	unsigned    end_ind = end / SET_BITS;
	if (start_ind == end_ind) {
		set->map[start_ind] |= start_mask & end_mask;
	} else {
		set->map[start_ind] |= start_mask;
		for (unsigned i = start_ind + 1; i < end_ind; i++) {
			set->map[i] = ~SET_ZERO;
		}
		set->map[end_ind] |= end_mask;
	}
}

static inline void
_set_remove_range (set_t *set, unsigned start, unsigned count)
{
	if (!count) {
		return;
	}
	if (start >= set->size) {
		return;
	}
	if (start + count > set->size) {
		count = set->size - start;
	}
	unsigned    end = start + count - 1;
	set_bits_t  start_mask = START_MASK (start);
	set_bits_t  end_mask = END_MASK (end);
	unsigned    start_ind = start / SET_BITS;
	unsigned    end_ind = end / SET_BITS;
	if (start_ind == end_ind) {
		set->map[start_ind] &= ~(start_mask & end_mask);
	} else {
		set->map[start_ind] &= ~start_mask;
		for (unsigned i = start_ind + 1; i < end_ind; i++) {
			set->map[i] = SET_ZERO;
		}
		set->map[end_ind] &= ~end_mask;
	}
}

set_t *
set_add (set_t *set, unsigned x)
{
	if (set->inverted)
		_set_remove (set, x);
	else
		_set_add (set, x);
	return set;
}

set_t *
set_add_range (set_t *set, unsigned start, unsigned count)
{
	if (set->inverted)
		_set_remove_range (set, start, count);
	else
		_set_add_range (set, start, count);
	return set;
}

set_t *
set_remove (set_t *set, unsigned x)
{
	if (set->inverted)
		_set_add (set, x);
	else
		_set_remove (set, x);
	return set;
}

set_t *
set_remove_range (set_t *set, unsigned start, unsigned count)
{
	if (set->inverted)
		_set_add_range (set, start, count);
	else
		_set_remove_range (set, start, count);
	return set;
}

set_t *
set_invert (set_t *set)
{
	set->inverted = !set->inverted;
	return set;
}

static set_t *
_set_union (set_t *dst, const set_t *src)
{
	unsigned    size;
	unsigned    i;

	size = max (dst->size, src->size);
	set_expand (dst, size);
	for (i = 0; i < SET_WORDS (src); i++)
		dst->map[i] |= src->map[i];
	return dst;
}

static set_t *
_set_intersection (set_t *dst, const set_t *src)
{
	unsigned    words;
	unsigned    i;

	words = min (SET_WORDS (dst), SET_WORDS (src));
	for (i = 0; i < words; i++)
		dst->map[i] &= src->map[i];
	// if dst is larger than src, then none of the excess elements in dst
	// can be in the intersection
	for ( ; i < SET_WORDS (dst); i++)
		dst->map[i] = 0;
	return dst;
}

static set_t *
_set_difference (set_t *dst, const set_t *src)
{
	unsigned    words;
	unsigned    i;

	words = min (SET_WORDS (dst), SET_WORDS (src));
	for (i = 0; i < words; i++)
		dst->map[i] &= ~src->map[i];
	// if src is larger than dst, excess elements in src cannot be in dst thus
	// there is nothing to remove
	// if dst is larger than src, there is nothing to remove regardless of what
	// is in src
	return dst;
}

static set_t *
_set_reverse_difference (set_t *dst, const set_t *src)
{
	unsigned    words;
	unsigned    i;

	words = min (SET_WORDS (dst), SET_WORDS (src));
	set_expand (dst, src->size);
	for (i = 0; i < words; i++)
		dst->map[i] = ~dst->map[i] & src->map[i];
	// if src is larger than dst, then dst cannot remove the excess elements
	// from src and thus the src elements must be copied
	for ( ; i < SET_WORDS (src); i++)
		dst->map[i] = src->map[i];
	// if dst is larger than src, then the excess elements in dst must be
	// removed
	for ( ; i < SET_WORDS (dst); i++)
		dst->map[i] = 0;
	return dst;
}

set_t *
set_union (set_t *dst, const set_t *src)
{
	if (dst->inverted && src->inverted) {
		return _set_intersection (dst, src);
	} else if (src->inverted) {
		dst->inverted = 1;
		return _set_reverse_difference (dst, src);
	} else if (dst->inverted) {
		return _set_difference (dst, src);
	} else {
		return _set_union (dst, src);
	}
}

set_t *
set_intersection (set_t *dst, const set_t *src)
{
	if (dst->inverted && src->inverted) {
		return _set_union (dst, src);
	} else if (src->inverted) {
		return _set_difference (dst, src);
	} else if (dst->inverted) {
		dst->inverted = 0;
		return _set_reverse_difference (dst, src);
	} else {
		return _set_intersection (dst, src);
	}
}

set_t *
set_difference (set_t *dst, const set_t *src)
{
	if (dst->inverted && src->inverted) {
		dst->inverted = 0;
		return _set_reverse_difference (dst, src);
	} else if (src->inverted) {
		return _set_intersection (dst, src);
	} else if (dst->inverted) {
		return _set_union (dst, src);
	} else {
		return _set_difference (dst, src);
	}
}

set_t *
set_reverse_difference (set_t *dst, const set_t *src)
{
	if (dst->inverted && src->inverted) {
		dst->inverted = 0;
		return _set_difference (dst, src);
	} else if (src->inverted) {
		dst->inverted = 1;
		return _set_union (dst, src);
	} else if (dst->inverted) {
		dst->inverted = 0;
		return _set_intersection (dst, src);
	} else {
		return _set_reverse_difference (dst, src);
	}
}

set_t *
set_assign (set_t *dst, const set_t *src)
{
	unsigned    size;
	unsigned    i;

	size = max (dst->size, src->size);
	set_expand (dst, size);
	dst->inverted = src->inverted;
	for (i = 0; i < SET_WORDS (src); i++)
		dst->map[i] = src->map[i];
	for ( ; i < SET_WORDS (dst); i++)
		dst->map[i] = 0;
	return dst;
}

set_t *
set_empty (set_t *set)
{
	unsigned    i;

	set->inverted = 0;
	for (i = 0; i < SET_WORDS (set); i++)
		set->map[i] = 0;
	return set;
}

set_t *
set_everything (set_t *set)
{
	unsigned    i;

	set->inverted = 1;
	for (i = 0; i < SET_WORDS (set); i++)
		set->map[i] = 0;
	return set;
}

static inline __attribute__((pure)) int
_set_is_empty (const set_t *set)
{
	unsigned    i;

	for (i = 0; i < SET_WORDS (set); i++)
		if (set->map[i])
			return 0;
	return 1;
}

int
set_is_empty (const set_t *set)
{
	if (set->inverted)
		return 0;
	return _set_is_empty (set);
}

int
set_is_everything (const set_t *set)
{
	if (!set->inverted)
		return 0;
	return _set_is_empty (set);
}

static __attribute__((pure)) int
set_test_n_n (const set_t *s1, const set_t *s2)
{
	unsigned    i, end;
	set_bits_t  intersection = 0;
	set_bits_t  difference = 0;

	end = min (s1->size, s2->size) / SET_BITS;
	for (i = 0; i < end; i++) {
		set_bits_t  m1 = s1->map[i];
		set_bits_t  m2 = s2->map[i];

		intersection |= m1 & m2;
		difference |= m1 ^ m2;
	}
	for ( ; i < SET_WORDS (s1); i++) {
		difference |= s1->map[i];
	}
	for ( ; i < SET_WORDS (s2); i++) {
		difference |= s2->map[i];
	}
	return (difference != 0) | ((intersection != 0) << 1);
}

static __attribute__((pure)) int
set_test_n_i (const set_t *s1, const set_t *s2)
{
	unsigned    i, end;
	set_bits_t  intersection = 0;
	set_bits_t  difference = 0;

	end = min (s1->size, s2->size) / SET_BITS;
	for (i = 0; i < end; i++) {
		set_bits_t  m1 = s1->map[i];
		set_bits_t  m2 = ~s2->map[i];

		intersection |= m1 & m2;
		difference |= m1 ^ m2;
	}
	for ( ; i < SET_WORDS (s1); i++) {
		intersection |= s1->map[i];
		difference |= ~s1->map[i];
	}
	for ( ; i < SET_WORDS (s2); i++) {
		difference |= ~s2->map[i];
	}
	return (difference != 0) | ((intersection != 0) << 1);
}

static __attribute__((pure)) int
set_test_i_n (const set_t *s1, const set_t *s2)
{
	unsigned    i, end;
	set_bits_t  intersection = 0;
	set_bits_t  difference = 0;

	end = min (s1->size, s2->size) / SET_BITS;
	for (i = 0; i < end; i++) {
		set_bits_t  m1 = ~s1->map[i];
		set_bits_t  m2 = s2->map[i];

		intersection |= m1 & m2;
		difference |= m1 ^ m2;
	}
	for ( ; i < SET_WORDS (s1); i++) {
		difference |= ~s1->map[i];
	}
	for ( ; i < SET_WORDS (s2); i++) {
		intersection |= s2->map[i];
		difference |= ~s2->map[i];
	}
	return (difference != 0) | ((intersection != 0) << 1);
}

static __attribute__((pure)) int
set_test_i_i (const set_t *s1, const set_t *s2)
{
	unsigned    i, end;
	set_bits_t  intersection = 0;
	set_bits_t  difference = 0;

	end = min (s1->size, s2->size) / SET_BITS;
	for (i = 0; i < end; i++) {
		set_bits_t  m1 = ~s1->map[i];
		set_bits_t  m2 = ~s2->map[i];

		intersection |= m1 & m2;
		difference |= m1 ^ m2;
	}
	for ( ; i < SET_WORDS (s1); i++) {
		difference |= ~s1->map[i];
	}
	for ( ; i < SET_WORDS (s2); i++) {
		intersection |= s2->map[i];
		difference |= ~s2->map[i];
	}
	intersection |= ~0;		// two inverted sets can never be disjoint
	return (difference != 0) | ((intersection != 0) << 1);
}

static __attribute__((pure)) int
set_test (const set_t *s1, const set_t *s2)
{
	if (s1->inverted && s2->inverted)
		return set_test_i_i (s1, s2);
	else if (s2->inverted)
		return set_test_n_i (s1, s2);
	else if (s1->inverted)
		return set_test_i_n (s1, s2);
	else
		return set_test_n_n (s1, s2);
}

int
set_is_disjoint (const set_t *s1, const set_t *s2)
{
	return !(set_test (s1, s2) & 2);
}

int
set_is_intersecting (const set_t *s1, const set_t *s2)
{
	return !!(set_test (s1, s2) & 2);
}

int
set_is_equivalent (const set_t *s1, const set_t *s2)
{
	return !(set_test (s1, s2) & 1);
}

int
set_is_subset (const set_t *set, const set_t *sub)
{
	unsigned    i, end;

	end = min (set->size, sub->size) / SET_BITS;
	if (set->inverted && sub->inverted) {
		for (i = 0; i < end; i++) {
			if (~sub->map[i] & set->map[i])
				return 0;
		}
		for ( ; i < SET_WORDS (set); i++)
			if (set->map[i])
				return 0;
	} else if (set->inverted) {
		for (i = 0; i < end; i++) {
			if (sub->map[i] & set->map[i])
				return 0;
		}
	} else if (sub->inverted) {
		// an inverted set cannot be a subset of a set that is not inverted
		return 0;
	} else {
		for (i = 0; i < end; i++) {
			if (sub->map[i] & ~set->map[i])
				return 0;
		}
		for ( ; i < SET_WORDS (sub); i++)
			if (sub->map[i])
				return 0;
	}
	return 1;
}

static inline int
_set_is_member (const set_t *set, unsigned x)
{
	if (x >= set->size)
		return 0;
	return SET_TEST_MEMBER(set, x) != 0;
}

int
set_is_member (const set_t *set, unsigned x)
{
	if (set->inverted)
		return !_set_is_member (set, x);
	return _set_is_member (set, x);
}

unsigned
set_count (const set_t *set)
{
	unsigned    count = 0;
	set_bits_t *m = set->map;
	unsigned    i = SET_WORDS (set);

	while (i-- > 0) {
		count += __builtin_popcount(*m++);
	}
	return count;
}

set_iter_t *
set_first_r (set_pool_t *set_pool, const set_t *set)
{
	unsigned    x;
	set_iter_t *set_iter;

	for (x = 0; x < set->size; x++) {
		if (_set_is_member (set, x)) {
			set_iter = new_setiter (set_pool);
			set_iter->set = set;
			set_iter->element = x;
			return set_iter;
		}
	}
	return 0;
}

set_iter_t *
set_first (const set_t *set)
{
	return set_first_r (&static_set_pool, set);
}

set_iter_t *
set_next_r (set_pool_t *set_pool, set_iter_t *set_iter)
{
	unsigned    x;

	for (x = set_iter->element + 1; x < set_iter->set->size; x++) {
		if (_set_is_member (set_iter->set, x)) {
			set_iter->element = x;
			return set_iter;
		}
	}
	delete_setiter (set_pool, set_iter);
	return 0;
}

set_iter_t *
set_next (set_iter_t *set_iter)
{
	return set_next_r (&static_set_pool, set_iter);
}

set_iter_t *
set_while_r (set_pool_t *set_pool, set_iter_t *set_iter)
{
	unsigned    x;

	if (_set_is_member (set_iter->set, set_iter->element)) {
		for (x = set_iter->element + 1; x < set_iter->set->size; x++) {
			if (!_set_is_member (set_iter->set, x)) {
				set_iter->element = x;
				return set_iter;
			}
		}
	} else {
		for (x = set_iter->element + 1; x < set_iter->set->size; x++) {
			if (_set_is_member (set_iter->set, x)) {
				set_iter->element = x;
				return set_iter;
			}
		}
	}
	delete_setiter (set_pool, set_iter);
	return 0;
}

set_iter_t *
set_while (set_iter_t *set_iter)
{
	return set_while_r (&static_set_pool, set_iter);
}

const char *
set_to_dstring_r (set_pool_t *set_pool, dstring_t *str, const set_t *set)
{
	set_iter_t *iter;
	int         first = 1;

	if (set_is_empty (set)) {
		dstring_appendstr (str, "{}");
		return str->str;
	}
	if (set_is_everything (set)) {
		dstring_appendstr (str, "{...}");
		return str->str;
	}
	dstring_appendstr (str, "{");
	if (set->inverted) {
		unsigned    start = 0;

		for (iter = set_first_r (set_pool, set); iter;
			 iter = set_next_r (set_pool, iter)) {
			unsigned    end = iter->element;
			while (start < end) {
				dasprintf (str, "%s%d", first ? "" : " ", start++);
				first = 0;
			}
			start = end + 1;
		}
		dasprintf (str, "%s%d ...", first ? "" : " ", start);
	} else {
		for (iter = set_first_r (set_pool, set); iter;
			 iter = set_next_r (set_pool, iter)) {
			dasprintf (str, "%s%d", first ? "" : " ", iter->element);
			first = 0;
		}
	}
	dstring_appendstr (str, "}");
	return str->str;
}

const char *
set_to_dstring (dstring_t *str, const set_t *set)
{
	return set_to_dstring_r (&static_set_pool, str, set);
}

const char *
set_as_string (const set_t *set)
{
	static dstring_t *str;

	if (!str) {
		str = dstring_new ();
	}
	dstring_clearstr (str);
	return set_to_dstring_r (&static_set_pool, str, set);
}

static void
set_shutdown (void *data)
{
	set_pool_clear (&static_set_pool);
}

static void __attribute__((constructor))
set_init (void)
{
	Sys_RegisterShutdown (set_shutdown, 0);
}
