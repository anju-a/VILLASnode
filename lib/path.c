/** Message paths.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <villas/common.h>
#include <villas/config.h>
#include <villas/utils.h>
#include <villas/node.h>
#include <villas/path.h>
#include <villas/path_destination.h>
#include <villas/path_source.h>
#include <villas/path_worker.h>
#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/mapping.h>

struct path_worker *worker = NULL;

void path_process(struct path *p, struct sample *smps[], unsigned cnt)
{
	unsigned enqueued, cloned, processed;

	struct sample *clones[cnt];

	cloned = sample_clone_many(clones, smps, cnt);
	if (cloned < cnt)
		warn("Pool underrun in path %s", path_name(p));

	/* Run processing hooks */
	processed = hook_process_list(&p->hooks, clones, cloned);
	if (processed == 0)
		return;

	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = (struct path_destination *) list_at(&p->destinations, i);

		enqueued = queue_signalled_push_many(&pd->queue, (void **) clones, cloned);
		if (enqueued != cnt)
			warn("Queue overrun for path %s", path_name(p));

		/* Increase reference counter of these samples as they are now also owned by the queue. */
		sample_get_many(clones, cloned);

		debug(LOG_PATH | 15, "Enqueued %u samples to destination %s of path %s", enqueued, node_name(pd->node), path_name(p));
	}

	sample_put_many(clones, cloned);
}

struct path * path_create()
{
	struct path *p;

	p = alloc(sizeof(struct path));
	if (!p)
		return NULL;

	list_init(&p->destinations);
	list_init(&p->sources);

	p->_name = NULL;

	/* Default values */
	p->mode = PATH_MODE_ANY;
	p->rate = 0; /* Disabled */

	p->reverse = 0;
	p->enabled = 1;
	p->queuelen = DEFAULT_QUEUELEN;

#ifdef WITH_HOOKS
	/* Add internal hooks if they are not already in the list */
	list_init(&p->hooks);
	for (size_t i = 0; i < list_length(&plugins); i++) {
		struct plugin *q = (struct plugin *) list_at(&plugins, i);

		if (q->type != PLUGIN_TYPE_HOOK)
			continue;

		struct hook_type *vt = &q->hook;

		if ((vt->flags & HOOK_PATH) && (vt->flags & HOOK_BUILTIN)) {
			int ret;

			struct hook *h = (struct hook *) alloc(sizeof(struct hook));

			ret = hook_init(h, vt, p, NULL);
			if (ret)
				return NULL;

			list_push(&p->hooks, h);
		}
	}
#endif /* WITH_HOOKS */

	p->state = STATE_INITIALIZED;

	return p;
}

int path_init(struct path *p)
{
	int ret;
	struct sample *previous;

	assert(p->state == STATE_CHECKED);

	/* Create worker if not existant */
	if (!worker) {
		worker = path_worker_create();
		if (!worker)
			error("Failed to create path worker");
	}

	bitset_init(&p->received, list_length(&p->sources));
	bitset_init(&p->mask, list_length(&p->sources));

#ifdef WITH_HOOKS
	hook_sort(&p->hooks);
#endif

	/* Initialize destinations */
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = (struct path_destination *) list_at(&p->destinations, i);

		ret = path_destination_init(pd, p);
		if (ret)
			return ret;
	}

	/* Initialize sources */
	p->samplelen = 0;
	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = (struct path_source *) list_at(&p->sources, i);

		ret = path_source_init(ps, p, i);
		if (ret)
			return ret;
	}

	if (!p->samplelen)
		p->samplelen = DEFAULT_SAMPLELEN;

	ret = pool_init(&p->pool, MAX(1, list_length(&p->destinations)) * p->queuelen, SAMPLE_LEN(p->samplelen), &memtype_hugepage);
	if (ret)
		return ret;

	previous = sample_alloc(&p->pool);
	if (!previous)
		return -1;

	previous->sequence = 0;
	previous->length = p->samplelen;

	atomic_init(&p->previous, previous);

	return 0;
}

int path_parse(struct path *p, json_t *cfg, struct list *nodes)
{
	int ret;

	json_error_t err;
	json_t *json_in;
	json_t *json_out = NULL;
	json_t *json_hooks = NULL;
	json_t *json_mask = NULL;

	const char *mode = NULL;

	struct list sources = { .state = STATE_DESTROYED };
	struct list destinations = { .state = STATE_DESTROYED };

	list_init(&sources);
	list_init(&destinations);

	ret = json_unpack_ex(cfg, &err, 0, "{ s: o, s?: o, s?: o, s?: b, s?: b, s?: i, s?: s, s?: F, s?: o }",
		"in", &json_in,
		"out", &json_out,
		"hooks", &json_hooks,
		"reverse", &p->reverse,
		"enabled", &p->enabled,
		"queuelen", &p->queuelen,
		"mode", &mode,
		"rate", &p->rate,
		"mask", &json_mask
	);
	if (ret)
		jerror(&err, "Failed to parse path configuration");

	/* Input node(s) */
	ret = mapping_parse_list(&sources, json_in, nodes);
	if (ret)
		error("Failed to parse input mapping of path %s", path_name(p));

	/* Optional settings */
	if (mode) {
		if      (!strcmp(mode, "any"))
			p->mode = PATH_MODE_ANY;
		else if (!strcmp(mode, "all"))
			p->mode = PATH_MODE_ALL;
		else
			error("Invalid path mode '%s'", mode);
	}

	/* Output node(s) */
	if (json_out) {
		ret = node_parse_list(&destinations, json_out, nodes);
		if (ret)
			jerror(&err, "Failed to parse output nodes");
	}

	for (size_t i = 0; i < list_length(&sources); i++) {
		struct mapping_entry *me = (struct mapping_entry *) list_at(&sources, i);

		struct path_source *ps = NULL;

		/* Check if there is already a path_source for this source */
		for (size_t j = 0; j < list_length(&p->sources); j++) {
			struct path_source *pt = (struct path_source *) list_at(&p->sources, j);

			if (pt->node == me->node) {
				ps = pt;
				break;
			}
		}

		if (!ps) {
			ps = path_source_create();
			if (!ps)
				error("Failed to create path source");

			ps->node = me->node;

			list_push(&p->sources, ps);
		}

		list_push(&ps->mappings, me);
	}

	for (size_t i = 0; i < list_length(&destinations); i++) {
		struct node *n = (struct node *) list_at(&destinations, i);

		struct path_destination *pd;

		pd = path_destination_create();
		if (!pd)
			error("Failed to create path destination");

		pd->node = n;

		list_push(&p->destinations, pd);
	}

	if (json_mask) {
		json_t *json_entry;
		size_t i;

		if (!json_is_array(json_mask))
			error("The 'mask' setting must be a list of node names");

		json_array_foreach(json_mask, i, json_entry) {
			const char *name;
			struct node *node;
			struct path_source *ps = NULL;

			name = json_string_value(json_entry);
			if (!name)
				error("The 'mask' setting must be a list of node names");

			node = list_lookup(nodes, name);
			if (!node)
				error("The 'mask' entry '%s' is not a valid node name", name);

			/* Search correspondending path_source to node */
			for (size_t i = 0; i < list_length(&p->sources); i++) {
				struct path_source *pt = (struct path_source *) list_at(&p->sources, i);

				if (pt->node == node) {
					ps = pt;
					break;
				}
			}

			if (!ps)
				error("Node %s is not a source of the path %s", node_name(node), path_name(p));

			ps->masked = true;
		}
	}
	else {/* Enable all by default */
		for (size_t i = 0; i < list_length(&p->sources); i++) {
			struct path_source *ps = (struct path_source *) list_at(&p->sources, i);

			ps->masked = true;
		}
	}

#if WITH_HOOKS
	if (json_hooks) {
		ret = hook_parse_list(&p->hooks, json_hooks, p, NULL);
		if (ret)
			return ret;
	}
#endif /* WITH_HOOKS */

	list_destroy(&sources, NULL, false);
	list_destroy(&destinations, NULL, false);

	p->cfg = cfg;
	p->state = STATE_PARSED;

	return 0;
}

int path_check(struct path *p)
{
	assert(p->state != STATE_DESTROYED);

	if (p->rate < 0)
		error("Setting 'rate' of path %s must be a positive number.", path_name(p));

	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = (struct path_source *) list_at(&p->sources, i);

		if (!ps->node->_vt->read)
			error("Source node '%s' is not supported as a source for path '%s'", node_name(ps->node), path_name(p));
	}

	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = (struct path_destination *) list_at(&p->destinations, i);

		if (!pd->node->_vt->write)
			error("Destiation node '%s' is not supported as a sink for path '%s'", node_name(pd->node), path_name(p));
	}

	if (!IS_POW2(p->queuelen)) {
		p->queuelen = LOG2_CEIL(p->queuelen);
		warn("Queue length should always be a power of 2. Adjusting to %d", p->queuelen);
	}

	p->state = STATE_CHECKED;

	return 0;
}

int path_start(struct path *p)
{
	int ret;
	char *mode, *mask;

	assert(p->state == STATE_CHECKED);

	switch (p->mode) {
		case PATH_MODE_ANY: mode = "any";     break;
		case PATH_MODE_ALL: mode = "all";     break;
		default:            mode = "unknown"; break;
	}

	mask = bitset_dump(&p->mask);

	info("Starting path %s: mode=%s, mask=%s, rate=%.2f, enabled=%s, reversed=%s, queuelen=%d, samplelen=%d, #hooks=%zu, #sources=%zu, #destinations=%zu",
		path_name(p),
		mode,
		mask,
		p->rate,
		p->enabled ? "yes": "no",
		p->reverse ? "yes": "no",
		p->queuelen, p->samplelen,
		list_length(&p->hooks),
		list_length(&p->sources),
		list_length(&p->destinations)
	);

	free(mask);

#ifdef WITH_HOOKS
	for (size_t i = 0; i < list_length(&p->hooks); i++) {
		struct hook *h = (struct hook *) list_at(&p->hooks, i);

		ret = hook_start(h);
		if (ret)
			return ret;
	}
#endif /* WITH_HOOKS */

	bitset_clear_all(&p->received);

	p->state = STATE_STARTED;

	return 0;
}

int path_stop(struct path *p)
{
	int ret;

	if (p->state != STATE_STARTED)
		return 0;

	info("Stopping path: %s", path_name(p));

#ifdef WITH_HOOKS
	for (size_t i = 0; i < list_length(&p->hooks); i++) {
		struct hook *h = (struct hook *) list_at(&p->hooks, i);

		ret = hook_stop(h);
		if (ret)
			return ret;
	}
#endif /* WITH_HOOKS */

	p->state = STATE_STOPPED;

	return 0;
}

int path_destroy(struct path *p)
{
	if (p->state == STATE_DESTROYED)
		return 0;

#ifdef WITH_HOOKS
	list_destroy(&p->hooks, (dtor_cb_t) hook_destroy, true);
#endif
	list_destroy(&p->sources, (dtor_cb_t) path_source_destroy, true);
	list_destroy(&p->destinations, (dtor_cb_t) path_destination_destroy, true);

	if (p->_name)
		free(p->_name);

	if (p->rate > 0)
		task_destroy(&p->timeout);

	p->state = STATE_DESTROYED;

	return 0;
}

const char * path_name(struct path *p)
{
	if (!p->_name) {
		strcatf(&p->_name, "[");

		for (size_t i = 0; i < list_length(&p->sources); i++) {
			struct path_source *ps = (struct path_source *) list_at(&p->sources, i);

			strcatf(&p->_name, " %s", node_name_short(ps->node));
		}

		strcatf(&p->_name, " ] " CLR_MAG("=>") " [");

		for (size_t i = 0; i < list_length(&p->destinations); i++) {
			struct path_destination *pd = (struct path_destination *) list_at(&p->destinations, i);

			strcatf(&p->_name, " %s", node_name_short(pd->node));
		}

		strcatf(&p->_name, " ]");
	}

	return p->_name;
}

int path_uses_node(struct path *p, struct node *n)
{
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = (struct path_destination *) list_at(&p->destinations, i);

		if (pd->node == n)
			return 0;
	}

	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = (struct path_source *) list_at(&p->sources, i);

		if (ps->node == n)
			return 0;
	}

	return -1;
}

int path_reverse(struct path *p, struct path *r)
{
	if (list_length(&p->destinations) != 1 || list_length(&p->sources) != 1)
		return -1;

	/* General */
	r->enabled = p->enabled;

	/* Source / Destinations */
	struct path_destination *orig_pd = list_first(&p->destinations);
	struct path_source      *orig_ps = list_first(&p->sources);

	struct path_destination *new_pd = (struct path_destination *) alloc(sizeof(struct path_destination));
	struct path_source      *new_ps = (struct path_source *) alloc(sizeof(struct path_source));
	struct mapping_entry    *new_me = alloc(sizeof(struct mapping_entry));
	new_pd->node = orig_ps->node;
	new_ps->node = orig_pd->node;
	new_ps->masked = true;

	new_me->node = new_ps->node;
	new_me->type = MAPPING_TYPE_DATA;
	new_me->data.offset = 0;
	new_me->length = 0;

	list_init(&new_ps->mappings);
	list_push(&new_ps->mappings, new_me);

	list_push(&r->destinations, new_pd);
	list_push(&r->sources, new_ps);

#ifdef WITH_HOOKS
	for (size_t i = 0; i < list_length(&p->hooks); i++) {
		int ret;

		struct hook *h = (struct hook *) list_at(&p->hooks, i);
		struct hook *g = (struct hook *) alloc(sizeof(struct hook));

		ret = hook_init(g, h->_vt, r, NULL);
		if (ret)
			return ret;

		list_push(&r->hooks, g);
	}
#endif /* WITH_HOOKS */
	return 0;
}
