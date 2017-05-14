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

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <libconfig.h>


#include "path.h"
#include "reader.h"
#include "writer.h"
#include "config.h"
#include "utils.h"
#include "timing.h"
#include "pool.h"
#include "queue.h"
#include "hook.h"
#include "plugin.h"
#include "super_node.h"
#include "memory.h"
#include "stats.h"

int path_source_init(struct path_source *ps)
{	
	return 0;
}

int path_source_destroy(struct path_source *ps)
{
	return 0;
}

int path_destination_init(struct path_destination *pd)
{
	int ret;

	ret = queue_init(&pd->queue, 1000, &memtype_hugepage); /** @todo queue len should be calculcated accordingly */
	if (ret)
		return ret;
	
	return 0;
}

int path_destination_destroy(struct path_destination *pd)
{
	queue_destroy(&pd->queue);

	return 0;
}


#if 0
int path_fastpath(struct path *p)
{
	reader_read();
	
	path_process();
	
	writer_write();
}
#endif

int path_init(struct path *p, struct super_node *sn)
{
	assert(p->state == STATE_DESTROYED);

	list_init(&p->hooks);
	list_init(&p->sources);
	list_init(&p->destinations);

	/* Default values */
	p->reverse = 0;
	p->enabled = 1;

	p->queuelen = DEFAULT_QUEUELEN;

	p->super_node = sn;

	p->state = STATE_INITIALIZED;

	return 0;
}

int path_parse(struct path *p, config_setting_t *cfg, struct list *nodes)
{
	config_setting_t *cfg_in, *cfg_out, *cfg_hooks;
	int ret;

	struct list sources = { .state = STATE_DESTROYED };
	struct list destinations = { .state = STATE_DESTROYED };

	list_init(&sources);
	list_init(&destinations);

	/* Input node */
	cfg_in = config_setting_get_member(cfg, "in");
	if (!cfg_in)
		cerror(cfg, "Missing input nodes for path");

	ret = node_parse_list(&sources, cfg_in, nodes);
	if (ret || list_length(&sources) == 0)
		cerror(cfg_in, "Invalid output nodes");

	/* Output node(s) */
	cfg_out = config_setting_get_member(cfg, "out");
	if (!cfg_out)
		cerror(cfg, "Missing output nodes for path");

	ret = node_parse_list(&destinations, cfg_out, nodes);
	if (ret || list_length(&destinations) == 0)
		cerror(cfg_out, "Invalid output nodes");

	/* Optional settings */
	cfg_hooks = config_setting_get_member(cfg, "hooks");
	if (cfg_hooks) {
		ret = hook_parse_list(&p->hooks, cfg_hooks, p);
		if (ret)
			return ret;
	}

	config_setting_lookup_bool(cfg, "reverse", &p->reverse);
	config_setting_lookup_bool(cfg, "enabled", &p->enabled);
	config_setting_lookup_int(cfg, "queuelen", &p->queuelen);

	if (!IS_POW2(p->queuelen)) {
		p->queuelen = LOG2_CEIL(p->queuelen);
		warn("Queue length should always be a power of 2. Adjusting to %d", p->queuelen);
	}

	p->cfg = cfg;

	for (size_t i = 0; i < list_length(&sources); i++) {
		struct node *n = list_at(&sources, i);

		struct path_source ps = {
			.node = n
		};

		list_push(&p->sources, memdup(&ps, sizeof(ps)));
	}

	for (size_t i = 0; i < list_length(&destinations); i++) {
		struct node *n = list_at(&destinations, i);

		struct path_destination pd = {
			.node = n
		};

		list_push(&p->destinations, memdup(&pd, sizeof(pd)));
	}

	list_destroy(&sources, NULL, false);
	list_destroy(&destinations, NULL, false);

	return 0;
}

int path_check(struct path *p)
{
	assert(p->state != STATE_DESTROYED);

	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = list_at(&p->sources, i);

		if (!ps->node->_vt->read)
			error("Source node '%s' is not supported as a source for path '%s'", node_name(ps->node), path_name(p));
	}
	
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = list_at(&p->destinations, i);

		if (!pd->node->_vt->write)
			error("Destiation node '%s' is not supported as a sink for path '%s'", node_name(pd->node), path_name(p));
	}

	p->state = STATE_CHECKED;

	return 0;
}

int path_init2(struct path *p)
{
	int ret;

	assert(p->state == STATE_CHECKED);

	/* Add internal hooks if they are not already in the list*/
	for (size_t i = 0; i < list_length(&plugins); i++) {
		struct plugin *q = list_at(&plugins, i);

		if (q->type == PLUGIN_TYPE_HOOK) {
			struct hook h = { .state = STATE_DESTROYED };
			struct hook_type *vt = &q->hook;

			if (vt->builtin) {
				ret = hook_init(&h, vt, p);
				if (ret)
					return ret;

				list_push(&p->hooks, memdup(&h, sizeof(h)));
			}
		}
	}

	/* We sort the hooks according to their priority before starting the path */
	list_sort(&p->hooks, hook_cmp_priority);

	/* Initialize path sources */
	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = list_at(&p->sources, i);

		ret = path_source_init(ps);
		if (ret)
			error("Failed to initialize path source");
	}

	/* Initialize path destinations */
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = list_at(&p->destinations, i);

		ret = path_destination_init(pd);
		if (ret)
			error("Failed to initialize path destination");
	}

	return 0;
}

int path_start(struct path *p)
{
	int ret;

	assert(p->state == STATE_CHECKED);

	info("Starting path %s: #hooks=%zu, #destinations=%zu", path_name(p), list_length(&p->hooks), list_length(&p->destinations));

	for (size_t i = 0; i < list_length(&p->hooks); i++) {
		struct hook *h = list_at(&p->hooks, i);

		ret = hook_start(h);
		if (ret)
			return ret;
	}

	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = list_at(&p->sources, i);
		struct reader *r = node_get_reader(ps->node);

		ret = reader_start(r);
		if (ret)
			return ret;
	}
	
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = list_at(&p->destinations, i);
		struct writer *w = node_get_writer(pd->node);

		ret = writer_start(w);
		if (ret)
			return ret;
	}

	p->state = STATE_STARTED;

	return 0;
}

int path_stop(struct path *p)
{
	int ret;

	if (p->state != STATE_STARTED)
		return 0;

	info("Stopping path: %s", path_name(p));

	for (size_t i = 0; i < list_length(&p->hooks); i++) {
		struct hook *h = list_at(&p->hooks, i);

		ret = hook_stop(h);
		if (ret)
			return ret;
	}
	
	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = list_at(&p->sources, i);
		struct reader *r = node_get_reader(ps->node);
		
		if (!r)
			continue;

		ret = reader_stop(r);
		if (ret)
			return ret;
	}
	
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = list_at(&p->destinations, i);
		struct writer *w = node_get_writer(pd->node);
		
		if (!w)
			continue;

		ret = writer_stop(w);
		if (ret)
			return ret;
	}

	p->state = STATE_STOPPED;

	return 0;
}

int path_destroy(struct path *p)
{
	if (p->state == STATE_DESTROYED)
		return 0;

	list_destroy(&p->hooks, (dtor_cb_t) hook_destroy, true);
	list_destroy(&p->sources, (dtor_cb_t) path_source_destroy, true);
	list_destroy(&p->destinations, (dtor_cb_t) path_destination_destroy, true);

	if (p->_name)
		free(p->_name);

	p->state = STATE_DESTROYED;

	return 0;
}

const char * path_name(struct path *p)
{
	if (!p->_name) {
		strcatf(&p->_name, "[");
		
		for (size_t i = 0; i < list_length(&p->sources); i++) {
			struct path_source *ps = list_at(&p->sources, i);

			strcatf(&p->_name, " %s", node_name_short(ps->node));
		}

		strcatf(&p->_name, " ] " MAG("=>") " [");

		for (size_t i = 0; i < list_length(&p->destinations); i++) {
			struct path_destination *pd = list_at(&p->destinations, i);

			strcatf(&p->_name, " %s", node_name_short(pd->node));
		}

		strcatf(&p->_name, " ]");
	}

	return p->_name;
}

int path_uses_node(struct path *p, struct node *n)
{
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = list_at(&p->destinations, i);

		if (pd->node == n)
			return 0;
	}
	
	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = list_at(&p->sources, i);

		if (ps->node == n)
			return 0;
	}

	return -1;
}

int path_reverse(struct path *p, struct path *r)
{
	int ret;

	if (list_length(&p->destinations) != 1 || list_length(&p->sources) != 1)
		return -1;

	struct path_destination *first_pd = list_first(&p->destinations);
	struct path_source *first_ps = list_first(&p->sources);

	/* General */
	r->enabled = p->enabled;
	r->cfg = p->cfg;

	struct path_destination *pd = alloc(sizeof(struct path_destination));
	struct path_source      *ps = alloc(sizeof(struct path_source));

	pd->node = first_ps->node;
	ps->node = first_pd->node;

	list_push(&r->destinations, pd);
	list_push(&r->sources, ps);

	/* Copy hooks */
	for (size_t i = 0; i < list_length(&p->hooks); i++) {
		struct hook *h = list_at(&p->hooks, i);
		struct hook hc = { .state = STATE_DESTROYED };

		ret = hook_init(&hc, h->_vt, p);
		if (ret)
			return ret;

		list_push(&r->hooks, memdup(&hc, sizeof(hc)));
	}

	return 0;
}

int path_source_process(struct path_source *ps)
{
	int ret;
	
	info("Received new samples from %s", node_name(ps->node));

	while (1) {
		struct sample *smp;
		
		/* Dequeue a sample */
		ret = queue_pull(&ps->queue, (void **) &smp);
		if (ret < 1)
			break;
		
		/* Construct new composite sample */
		mapping_construct(&ps->mapping, ps->path->last, smp);
		
		/* Call hook functions for new sample */
		
		
		/* Enqueue sample to destinations */
	}
	
	return 0;
}

int path_destination_process(struct path_destination *pd)
{
	int cnt = pd->node->vectorize;
	int sent;
	int tosend;
	int available;
	int released;

	struct sample *smps[cnt];

	/* As long as there are still samples in the queue */
	while (1) {
		available = queue_pull_many(&pd->queue, (void **) smps, cnt);
		if (available == 0)
			break;
		else if (available < cnt) 
			debug(LOG_PATH | 5, "Queue underrun for path %s: available=%u expected=%u", path_name(pd->path), available, cnt);
	
		debug(LOG_PATH | 15, "Dequeued %u samples from queue of node %s which is part of path %s", available, node_name(pd->node), path_name(pd->path));

		tosend = hook_write_list(&pd->path->hooks, smps, available);
		if (tosend == 0)
			continue;

		sent = node_write(pd->node, smps, tosend);
		if (sent < 0)
			error("Failed to sent %u samples to node %s", cnt, node_name(pd->node));
		else if (sent < tosend)
			warn("Partial write to node %s", node_name(pd->node));

		debug(LOG_PATH | 15, "Sent %u messages to node %s", sent, node_name(pd->node));

		released = sample_put_many(smps, sent);

		debug(LOG_PATH | 15, "Released %d samples back to memory pool", released);
	}
	
	return 0;
}