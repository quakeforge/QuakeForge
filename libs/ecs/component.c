/*
	component.c

	Component handling

	Copyright (C) 2022 Bill Currke

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

#include "QF/heapsort.h"
#include "QF/sys.h"

#define IMPLEMENT_COMPONENT_Funcs
#include "QF/ecs/component.h"

VISIBLE ecs_registry_t *
ECS_NewRegistry (void)
{
	ecs_registry_t *reg = calloc (1, sizeof (ecs_registry_t));
	reg->next = Ent_Index (nullent);
	reg->href_comp = nullent;
	return reg;
}

VISIBLE void
ECS_DelRegistry (ecs_registry_t *registry)
{
	free (registry->entities);
	for (uint32_t i = 0; i < registry->num_components; i++) {
		free (registry->comp_pools[i].sparse);
		free (registry->comp_pools[i].dense);
		free (registry->comp_pools[i].data);
	}
	free (registry->comp_pools);
	free (registry);
}

VISIBLE void
ECS_RegisterComponents (ecs_registry_t *registry,
						const component_t *components, uint32_t count)
{
	registry->num_components = count;
	registry->components = components;
	registry->comp_pools = calloc (count, sizeof (ecs_pool_t));
	size_t      size = registry->max_entities * sizeof (uint32_t);
	for (uint32_t i = 0; i < registry->num_components; i++) {
		registry->comp_pools[i].sparse = malloc (size);
		memset (registry->comp_pools[i].sparse, nullent, size);
	}
}

typedef struct {
	__compar_d_fn_t cmp;
	void       *arg;
	ecs_pool_t *pool;
	const component_t *comp;
} ecs_sort_t;

static int
ecs_compare (const void *a, const void *b, void *arg)
{
	ecs_sort_t *sortctx = arg;
	return sortctx->cmp (a, b, sortctx->arg);
}

static void
swap_uint32 (uint32_t *a, uint32_t *b)
{
	uint32_t    t = *a;
	*a = *b;
	*b = t;
}

static void
ecs_swap (void *_a, void *_b, void *arg)
{
	ecs_sort_t *sortctx = arg;
	size_t      size = sortctx->comp->size;
	ecs_pool_t *pool = sortctx->pool;
	uint32_t   *a = _a;
	uint32_t   *b = _b;
	uint32_t    a_ind = a - pool->dense;
	uint32_t    b_ind = b - pool->dense;
	uint32_t    a_ent_ind = Ent_Index (pool->dense[a_ind]);
	uint32_t    b_ent_ind = Ent_Index (pool->dense[b_ind]);
	__auto_type a_data = (byte *) pool->data + a_ind * size;
	__auto_type b_data = (byte *) pool->data + b_ind * size;
	Component_SwapElements (sortctx->comp, a_data, b_data);
	swap_uint32 (a, b);
	swap_uint32 (&pool->sparse[a_ent_ind], &pool->sparse[b_ent_ind]);
}

VISIBLE void
ECS_SortComponentPool (ecs_registry_t *registry, uint32_t component,
					   __compar_d_fn_t cmp, void *arg)
{
	if (component >= registry->num_components) {
		Sys_Error ("ECS_SortComponentPool: invalid component: %u", component);
	}
	ecs_pool_t *pool = &registry->comp_pools[component];
	if (!pool->count) {
		return;
	}
	__auto_type comp = &registry->components[component];
	ecs_sort_t sortctx = { .cmp = cmp, .arg = arg, .pool = pool, .comp = comp };
	heapsort_s (pool->dense, pool->count, sizeof (uint32_t),
				ecs_compare, ecs_swap, &sortctx);
}

VISIBLE void *
Ent_AddComponent (uint32_t ent, uint32_t comp, ecs_registry_t *registry)
{
	ecs_pool_t *pool = &registry->comp_pools[comp];
	uint32_t    id = Ent_Index (ent);
	uint32_t    ind = pool->sparse[id];
	if (ind >= pool->count || pool->dense[ind] != ent) {
		if (pool->count == pool->max_count) {
			pool->max_count += COMP_GROW;
			pool->dense = realloc (pool->dense,
								   pool->max_count * sizeof (uint32_t));
			Component_ResizeArray (&registry->components[comp], &pool->data,
								   pool->max_count);
		}
		uint32_t    ind = pool->count++;
		pool->sparse[id] = ind;
		pool->dense[ind] = ent;
	}
	return Ent_GetComponent (ent, comp, registry);
}

VISIBLE void
Ent_RemoveComponent (uint32_t ent, uint32_t comp, ecs_registry_t *registry)
{
	uint32_t    id = Ent_Index (ent);
	ecs_pool_t *pool = &registry->comp_pools[comp];
	uint32_t    ind = pool->sparse[id];
	if (ind < pool->count && pool->dense[ind] == ent) {
		uint32_t    last = pool->count - 1;
		Component_DestroyElements (&registry->components[comp], pool->data,
								   ind, 1);
		if (last > ind) {
			pool->sparse[Ent_Index (pool->dense[last])] = ind;
			pool->dense[ind] = pool->dense[last];
			Component_MoveElements (&registry->components[comp], pool->data,
									ind, last, 1);
		}
		pool->count--;
		pool->sparse[id] = nullent;
	}
}

VISIBLE uint32_t
ECS_NewEntity (ecs_registry_t *registry)
{
	uint32_t    ent;
	if (registry->available) {
		registry->available--;
		uint32_t    next = registry->next;
		ent = next | Ent_Generation (registry->entities[next]);
		registry->next = Ent_Index (registry->entities[next]);
		registry->entities[next] = ent;
	} else {
		if (registry->num_entities == Ent_Index (nullent)) {
			Sys_Error ("ECS_NewEntity: out of entities");
		}
		if (registry->num_entities == registry->max_entities) {
			registry->max_entities += ENT_GROW;
			size_t      size = registry->max_entities * sizeof (uint32_t);
			registry->entities = realloc (registry->entities, size);
			for (uint32_t i = 0; i < registry->num_components; i++) {
				uint32_t   *sparse = registry->comp_pools[i].sparse;
				sparse = realloc (sparse, size);
				memset (sparse + registry->max_entities - ENT_GROW, nullent,
						ENT_GROW * sizeof (uint32_t));
				registry->comp_pools[i].sparse = sparse;
			}
		}
		ent = registry->num_entities++;
		// ent starts out with generation 0
		registry->entities[ent] = ent;
	}
	return ent;
}

VISIBLE void
ECS_DelEntity (ecs_registry_t *registry, uint32_t ent)
{
	uint32_t    next = registry->next | Ent_NextGen (Ent_Generation (ent));
	uint32_t    id = Ent_Index (ent);
	registry->entities[id] = next;
	registry->next = id;
	registry->available++;

	for (uint32_t i = 0; i < registry->num_components; i++) {
		Ent_RemoveComponent (ent, i, registry);
	}
}
