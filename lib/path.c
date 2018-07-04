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

#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <poll.h>

#include <villas/config.h>
#include <villas/utils.h>
#include <villas/path.h>
#include <villas/timing.h>
#include <villas/pool.h>
#include <villas/queue.h>
#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/memory.h>
#include <villas/stats.h>
#include <villas/node.h>

PathSource::PathSource(struct node *n) :
	node(n)
{
	int ret;

	ret = pool_init(&pool, MAX(DEFAULT_QUEUELEN, node->in.vectorize), SAMPLE_LEN(node->samplelen), &memtype_hugepage);
	if (ret) {
		// @todo: throw exception
	}
}

PathSource::~PathSource()
{
	int ret;

	ret = pool_destroy(&pool);
	if (ret) {
		// @todo: throw exception
	}

	ret = list_destroy(&mappings, nullptr, true);
	if (ret) {
		// @todo: throw exception
	}
}

void PathSource::read(struct path *p, int i)
{
	int recv, tomux, ready, cnt;

	cnt = node->in.vectorize;

	struct sample *read_smps[cnt];
	struct sample *muxed_smps[cnt];
	struct sample **tomux_smps;

	/* Fill smps[] free sample blocks from the pool */
	ready = sample_alloc_many(&pool, read_smps, cnt);
	if (ready != cnt)
		warn("Pool underrun for path source %s", node_name(node));

	/* Read ready samples and store them to blocks pointed by smps[] */
	recv = node_read(node, read_smps, ready);
	if (recv == 0)
		goto out2;
	else if (recv < 0)
		error("Failed to read samples from node %s", node_name(node));
	else if (recv < ready)
		warn("Partial read for path %s: read=%u, expected=%u", getName(), recv, ready);

	bitset_set(&p->received, i);

	if (p->mode == Path::mode::ANY) { /* Mux all samples */
		tomux_smps = read_smps;
		tomux = recv;
	}
	else { /* Mux only last sample and discard others */
		tomux_smps = read_smps + recv - 1;
		tomux = 1;
	}

	for (int i = 0; i < tomux; i++) {
		muxed_smps[i] = i == 0
			? sample_clone(p->last_sample)
			: sample_clone(muxed_smps[i-1]);

		muxed_smps[i]->sequence = p->last_sequence + 1;

		mapping_remap(&mappings, muxed_smps[i], tomux_smps[i], nullptr);
	}

	sample_copy(p->last_sample, muxed_smps[tomux-1]);

	debug(15, "Path %s received = %s", getName(), bitset_dump(&p->received));

#ifdef WITH_HOOKS
	int toenqueue = hook_process_list(&p->hooks, muxed_smps, tomux);
        if (toenqueue != tomux) {
                int skipped = tomux - toenqueue;

                debug(LOG_NODES | 10, "Hooks skipped %u out of %u samples for path %s", skipped, tomux, p->getName());
        }
#else
	int toenqueue = tomux;
#endif

	if (bitset_test(&p->mask, i)) {
		/* Check if we received an update from all nodes/ */
		if ((p->mode == Path::mode::ANY) ||
		    (p->mode == Path::mode::ALL && !bitset_cmp(&p->mask, &p->received)))
		{
			p->enqueue(muxed_smps, toenqueue);

			/* Reset bitset of updated nodes */
			bitset_clear_all(&p->received);
		}
	}

	sample_put_many(muxed_smps, tomux);
out2:	sample_put_many(read_smps, ready);
}

PathDestination::PathDestination(int queuelen)
{
	int ret;

	ret = queue_init(&queue, queuelen, &memtype_hugepage);
	if (ret) {
		// @todo: throw exception
	}
}

~PathDestination::PathDestination()
{
	ret = queue_destroy(&queue);
	if (ret) {
		// @todo: throw exception
	}
}

void Path::enqueue(struct sample *smps[], unsigned cnt)
{
	unsigned enqueued, cloned;

	struct sample *clones[cnt];

	cloned = sample_clone_many(clones, smps, cnt);
	if (cloned < cnt)
		warn("Pool underrun in path %s", getName());

	for (PathDestination *pd : destinations) {
		enqueued = queue_push_many(&queue, (void **) clones, cloned);
		if (enqueued != cnt)
			warn("Queue overrun for path %s", getName());

		/* Increase reference counter of these samples as they are now also owned by the queue. */
		sample_get_many(clones, cloned);

		debug(LOG_PATH | 15, "Enqueued %u samples to destination %s of path %s", enqueued, node_name(pd->node), getName());
	}

	sample_put_many(clones, cloned);
}

void PathDestination::write()
{
	int cnt = node->out.vectorize;
	int sent;
	int available;
	int released;

	struct sample *smps[cnt];

	/* As long as there are still samples in the queue */
	while (1) {
		available = queue_pull_many(&queue, (void **) smps, cnt);
		if (available == 0)
			break;
		else if (available < cnt)
			debug(LOG_PATH | 5, "Queue underrun for path %s: available=%u expected=%u", getName(), available, cnt);

		debug(LOG_PATH | 15, "Dequeued %u samples from queue of node %s which is part of path %s", available, node_name(pd->node), getName());

		sent = node_write(pd->node, smps, available);
		if (sent < 0)
			error("Failed to sent %u samples to node %s", cnt, node_name(pd->node));
		else if (sent < available)
			warn("Partial write to node %s: written=%d, expected=%d", node_name(pd->node), sent, available);

		released = sample_put_many(smps, sent);

		debug(LOG_PATH | 15, "Released %d samples back to memory pool", released);
	}
}

/** Main thread function per path: read samples -> write samples */
void Path::runSingle()
{
	PathSource *ps = sources.front();

	debug(1, "Started path %s in single mode", getName());

	for (;;) {
		ps->read(p, 0);

		for (PathDestination *pd : destinations)
			pd->write(p);
	}

	return nullptr;
}

/** Main thread function per path: read samples -> write samples */
void Path::runPoll()
{
	int ret;

	debug(1, "Started path %s in polling mode", getName());

	for (;;) {
		ret = poll(reader.pfds, reader.nfds, -1);
		if (ret < 0)
			serror("Failed to poll");

		debug(10, "Path %s returned from poll(2)", getName());

		for (int i = 0; i < reader.nfds; i++) {
			PathSource *ps = sources[i];

			if (reader.pfds[i].revents & POLLIN) {
				/* Timeout: re-enqueue the last sample */
				if (reader.pfds[i].fd == task_fd(&timeout)) {
					task_wait(&timeout);

					last_sample->sequence = last_sequence++;

					enqueue(&last_sample, 1);
				}
				/* A source is ready to receive samples */
				else {
					ps->read(p, i);
				}
			}
		}

		for (PathDestiations *pd: destintaions)
			pd->write(p);
	}

	return nullptr;
}

Path::Path() :
	builtin(1),
	reverse(0),
	enabled(1),
	poll(-1),
	queuelen(DEFAULT_QUEUELEN),
	_name(nullptr),
	mode(Path::mode::ANY),
	rate(0) /* Disabled */
{
#ifdef WITH_HOOKS
	/* Add internal hooks if they are not already in the list */
	list_init(&ooks);
	if (builtin) {
		int ret;

		for (size_t i = 0; i < list_length(&plugins); i++) {
			struct plugin *q = (struct plugin *) list_at(&plugins, i);

			if (q->type != PLUGIN_TYPE_HOOK)
				continue;

			struct hook_type *vt = &q->hook;

			if (!(vt->flags & HOOK_PATH) || !(vt->flags & HOOK_BUILTIN))
				continue;

			struct hook *h = (struct hook *) alloc(sizeof(struct hook));

			ret = hook_init(h, vt, p, nullptr);
			if (ret)
				return ret;

			list_push(&hooks, h);
		}
	}
#endif /* WITH_HOOKS */

	state = STATE_INITIALIZED;

	return 0;
}

int Path::initPoll()
{
	int ret, nfds;

	nfds = sources.size();
	if (rate > 0)
		nfds++;

	reader.nfds = nfds;
	reader.pfds = alloc(sizeof(struct pollfd) * reader.nfds);

	for (PathSource *ps : sources) {
		/* This slot is only used if it is not masked */
		reader.pfds[i].events = POLLIN;
		reader.pfds[i].fd = node_fd(ps->node);

		if (reader.pfds[i].fd < 0)
			error("Failed to get file descriptor for node %s", node_name(ps->node));
	}

	/* We use the last slot for the timeout timer. */
	if (rate > 0) {
		ret = task_init(&timeout, rate, CLOCK_MONOTONIC);
		if (ret)
			return ret;

		reader.pfds[nfds-1].events = POLLIN;
		reader.pfds[nfds-1].fd = task_fd(&timeout);
		if (reader.pfds[nfds-1].fd < 0)
			error("Failed to get file descriptor for timer of path %s", getName());
	}

	return 0;
}

int Path::init()
{
	int ret;

	assert(state == STATE_CHECKED);

#ifdef WITH_HOOKS
	/* We sort the hooks according to their priority before starting the path */
	list_sort(&hooks, hook_cmp_priority);
#endif /* WITH_HOOKS */

	/* Initialize destinations */
	for (PathDestination *pd : destinations) {
		ret = pd->init(queuelen);
		if (ret)
			return ret;
	}

	/* Initialize sources */
	for (PathSource *ps : sources) {
		ret = ps->init();
		if (ret)
			return ret;
	}

	bitset_init(&received, sources.size()));
	bitset_init(&mask, sources.size()));

	/* Calc sample length of path and initialize bitset */
	samplelen = 0;
	for (PathSource *ps : sources) {
		if (ps->masked)
			bitset_set(&mask, i);

		for (size_t i = 0; i < list_length(&ps->mappings); i++) {
			struct mapping_entry *me = (struct mapping_entry *) list_at(&ps->mappings, i);

			int len = me->length;
			int off = me->offset;

			if (off + len > samplelen)
				samplelen = off + len;
		}
	}

	if (!samplelen)
		samplelen = DEFAULT_SAMPLELEN;

	ret = pool_init(&pool, MAX(1, destinations.size()) * queuelen, SAMPLE_LEN(samplelen), &memtype_hugepage);
	if (ret)
		return ret;

	last_sample = sample_alloc(&pool);
	if (!last_sample)
		return -1;

	/* Prepare poll() */
	if (oll) {
		ret = initPoll();
		if (ret)
			return ret;
	}

	return 0;
}

int Path::parseJson(json_t *cfg, struct list *nodes)
{
	int ret;

	json_error_t err;
	json_t *json_in;
	json_t *json_out = nullptr;
	json_t *json_hooks = nullptr;
	json_t *json_mask = nullptr;

	const char *mode = nullptr;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: o, s?: o, s?: o, s?: b, s?: b, s?: b, s?: i, s?: s, s?: b, s?: F, s?: o }",
		"in", &json_in,
		"out", &json_out,
		"hooks", &json_hooks,
		"reverse", &reverse,
		"enabled", &enabled,
		"builtin", &builtin,
		"queuelen", &queuelen,
		"mode", &mode,
		"poll", &poll,
		"rate", &rate,
		"mask", &json_mask
	);
	if (ret)
		jerror(&err, "Failed to parse path configuration");

	/* Input node(s) */
	ret = mapping_parse_list(&sources, json_in, nodes);
	if (ret)
		error("Failed to parse input mapping of path %s", getName());

	/* Optional settings */
	if (mode) {
		if      (!strcmp(mode, "any"))
			mode = Path::mode::ANY;
		else if (!strcmp(mode, "all"))
			mode = Path::mode::ALL;
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

		PathSource *ps = nullptr;

		/* Check if there is already a PathSource for this source */
		for (PathSource *pt : sources)
			if (pt->node == me->node) {
				ps = pt;
				break;
			}
		}

		if (!ps) {
			ps = new PathSource(me->node);

			ps->masked = false;

			ps->mappings.state = STATE_DESTROYED;

			list_init(&ps->mappings);

			sources.push_back(ps);
		}

		list_push(&ps->mappings, me);
	}

	for (size_t i = 0; i < list_length(&destinations); i++) {
		struct node *n = (struct node *) list_at(&destinations, i);

		PathDestination *pd = new PathDestination(n);

		destinations.push_back(pd);
	}

	if (json_mask) {
		json_t *json_entry;
		size_t i;

		if (!json_is_array(json_mask))
			error("The 'mask' setting must be a list of node names");

		json_array_foreach(json_mask, i, json_entry) {
			const char *name;
			struct node *node;
			PathSource *ps = nullptr;

			name = json_string_value(json_entry);
			if (!name)
				error("The 'mask' setting must be a list of node names");

			node = list_lookup(nodes, name);
			if (!node)
				error("The 'mask' entry '%s' is not a valid node name", name);

			/* Search correspondending PathSource to node */
			for (PathSource *pt : sources) {
				if (pt->node == node) {
					ps = pt;
					break;
				}
			}

			if (!ps)
				error("Node %s is not a source of the path %s", node_name(node), getName());

			ps->masked = true;
		}
	}
	else {/* Enable all by default */
		for (PathSource *ps : sources)
			ps->masked = true;
	}

#ifdef WITH_HOOKS
	if (json_hooks) {
		ret = hook_parse_list(&hooks, json_hooks, p, nullptr);
		if (ret)
			return ret;
	}
#endif /* WITH_HOOKS */

	/* Autodetect whether to use poll() for this path or not */
	if (poll == -1) {
		PathSource *ps = sources.front();

		poll = (
			rate > 0 ||
		        sources.size() > 1
		) && node_fd(ps->node) != 1;
	}

	cfg = cfg;
	state = STATE_PARSED;

	return 0;
}

int Path::check()
{
	assert(state != STATE_DESTROYED);

	if (rate < 0)
		error("Setting 'rate' of path %s must be a positive number.", getName());

	for (PathSource *ps : sources) {
		if (!ps->node->_vt->read)
			error("Source node '%s' is not supported as a source for path '%s'", node_name(ps->node), getName());
	}

	for (PathDestination *pd : destinations) {
		if (!pd->node->_vt->write)
			error("Destiation node '%s' is not supported as a sink for path '%s'", node_name(pd->node), getName());
	}

	if (!IS_POW2(queuelen)) {
		queuelen = LOG2_CEIL(queuelen);
		warn("Queue length should always be a power of 2. Adjusting to %d", queuelen);
	}

	state = STATE_CHECKED;

	return 0;
}

int Path::start()
{
	int ret;
	char *mode, *mask;

	assert(state == STATE_CHECKED);

	switch (mode) {
		case Path::mode::ANY: mode = "any";     break;
		case Path::mode::ALL: mode = "all";     break;
		default:              mode = "unknown"; break;
	}

	mask = bitset_dump(&mask);

	info("Starting path %s: mode=%s, poll=%s, mask=%s, rate=%.2f, enabled=%s, reversed=%s, queuelen=%d, samplelen=%d, #hooks=%zu, #sources=%zu, #destinations=%zu",
		getName(),
		mode,
		poll ? "yes" : "no",
		mask,
		rate,
		enabled ? "yes" : "no",
		reverse ? "yes" : "no",
		queuelen, samplelen,
		list_length(&hooks),
		sources.size(),
		destinations.size()
	);

	free(mask);

#ifdef WITH_HOOKS
	for (size_t i = 0; i < list_length(&hooks); i++) {
		struct hook *h = (struct hook *) list_at(&hooks, i);

		ret = hook_start(h);
		if (ret)
			return ret;
	}
#endif /* WITH_HOOKS */

	last_sequence = 0;

	bitset_clear_all(&received);

	/* We initialize the intial sample with zeros */
	for (PathSource *ps : sources) {
		for (size_t j = 0; j < list_length(&ps->mappings); j++) {
			struct mapping_entry *me = (struct mapping_entry *) list_at(&ps->mappings, j);

			int len = me->length;
			int off = me->offset;

			if (len + off > last_sample->length)
				last_sample->length = len + off;

			for (int k = off; k < off + len; k++) {
				last_sample->data[k].f = 0;

				sample_set_data_format(last_sample, k, SAMPLE_DATA_FORMAT_FLOAT);
			}
		}
	}

	/* Start one thread per path for sending to destinations
	 *
	 * Special case: If the path only has a single source and this source
	 * does not offer a file descriptor for polling, we will use a special
	 * thread function.
	 */
	tid = std::thread(&, poll ? &runPoll : &runSingle, this);

	state = STATE_STARTED;

	return 0;
}

int Path::stop()
{
	int ret;

	if (state != STATE_STARTED)
		return 0;

	info("Stopping path: %s", getName());

	state = STATE_STOPPING;

	tid.join();

#ifdef WITH_HOOKS
	for (size_t i = 0; i < list_length(&hooks); i++) {
		struct hook *h = (struct hook *) list_at(&hooks, i);

		ret = hook_stop(h);
		if (ret)
			return ret;
	}
#endif /* WITH_HOOKS */

	state = STATE_STOPPED;

	return 0;
}

Path::~Path()
{
	if (state == STATE_DESTROYED)
		return 0;

#ifdef WITH_HOOKS
	list_destroy(&hooks, (dtor_cb_t) hook_destroy, true);
#endif
	list_destroy(&sources, (dtor_cb_t) path_source_destroy, true);
	list_destroy(&destinations, (dtor_cb_t) path_destination_destroy, true);

	if (reader.pfds)
		free(reader.pfds);

	if (_name)
		free(_name);

	if (rate > 0)
		task_destroy(&timeout);

	pool_destroy(&pool);

	state = STATE_DESTROYED;

	return 0;
}

const char * Path::getName()
{
	if (!_name) {
		strcatf(&_name, "[");

		for (PathSource *ps: sources)
			strcatf(&_name, " %s", node_name_short(ps->node));

		strcatf(&_name, " ] " CLR_MAG("=>") " [");

		for (PathDestiations *pd : destinations) {
			strcatf(&_name, " %s", node_name_short(pd->node));

		strcatf(&_name, " ]");
	}

	return _name;
}

int Path::usesNode(struct node *n)
{
	for (PathSource *ps: sources) {
		if (ps->node == n)
			return 0;
	}

	for (PathDestiations *pd : destinations) {
		if (pd->node == n)
			return 0;
	}

	return -1;
}

Path Path::reverse()
{
	if (destinations.size() != 1 || sources.size() != 1)
		return -1;

	Path r;

	/* General */
	r.enabled = enabled;

	/* Source / Destinations */
	PathDestination *orig_pd = destinations.front();
	PathSource      *orig_ps = sources.front();

	PathDestination *new_pd = new PathDestination(orig_ps->node);
	PathSource      *new_ps = new PathSource(orig_pd->node);

	struct mapping_entry *new_me = alloc(sizeof(struct mapping_entry));
	new_ps->masked = true;

	new_me->node = new_ps->node;
	new_me->type = MAPPING_TYPE_DATA;
	new_me->data.offset = 0;
	new_me->length = 0;

	list_init(&new_ps->mappings);
	list_push(&new_ps->mappings, new_me);

	r.destinations.push_back(new_pd);
	r.sources.push_back(new_ps);

#ifdef WITH_HOOKS
	for (size_t i = 0; i < list_length(&hooks); i++) {
		int ret;

		struct hook *h = (struct hook *) list_at(&hooks, i);
		struct hook *g = (struct hook *) alloc(sizeof(struct hook));

		ret = hook_init(g, h->_vt, r, nullptr);
		if (ret)
			return ret;

		list_push(&r->hooks, g);
	}
#endif /* WITH_HOOKS */

	return r;
}
