/** Path reader thread.
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

#include "node.h"
#include "path.h"
#include "reader.h"
#include "utils.h"

static void * reader(void *arg)
{
	struct reader *r = arg;
	
	struct sample *smps[r->node->vectorize];
	
	int ready, recv, enqueue, enqueued, notified;
	
	debug(LOG_PATH | 5, "Reader thread started for node: %s", node_name(r->node));

	for (;;) {
		/* Fill smps[] free sample blocks from the pool */
		ready = sample_alloc(&r->pool, smps, r->node->vectorize);
		if (ready != r->node->vectorize)
			warn("Pool underrun for path reader of node %s", node_name(r->node));

		/* Read ready samples and store them to blocks pointed by smps[] */
		recv = node_read(r->node, smps, ready);
		if (recv < 0)
			error("Failed to receive message from node %s", node_name(r->node));
		else if (recv < ready) {
			warn("Partial read for path reader of node %s: read=%u expected=%u", node_name(r->node), recv, ready);
			/* Free samples that weren't written to */
			sample_free(smps + recv, ready - recv);
		}

		debug(LOG_PATH | 15, "Received %u messages from node %s", recv, node_name(r->node));

		/* Run preprocessing hooks for vector of samples */
#if 0
		enqueue = hook_read_list(&p->hooks, smps, recv);
		if (enqueue != recv) {
			info("Hooks skipped %u out of %u samples for path %s", recv - enqueue, recv, path_name(p));

			if (p->stats)
				stats_update(p->stats->delta, STATS_SKIPPED, recv - enqueue);
		}
#else
		enqueue = recv;
#endif

		/* Keep track of the lowest index that wasn't enqueued;
		 * all following samples must be freed here */
		int refd = 0;
		for (size_t i = 0; i < list_length(&r->sources); i++) {
			struct path_source *ps = list_at(&r->sources, i);
	
			enqueued = queue_push_many(&ps->queue, (void **) smps, enqueue);
			if (enqueue != enqueued)
				warn("Queue overrun for source %s", node_name(r->node));
			
			notified = queue_signalled_push(&ps->path->writer.events, ps);
			if (notified < 1)
				warn("Failed to notify path writer");
	
			if (refd < enqueued)
				refd = enqueued;
	
			debug(LOG_PATH | 15, "Enqueued %u samples from node %s to path %s", enqueued, node_name(r->node), path_name(ps->path));
		}
	
		/* Release those samples which have not been pushed into a queue */
		sample_free(smps + refd, ready - refd);
	}

	return NULL;
}

int reader_init(struct reader *r, struct node *n)
{
	int ret;

	r->node = n;
	
	ret = pool_init(&r->pool, 1000, SAMPLE_LEN(r->node->samplelen), &memtype_hugepage); /** @todo calucalate pool length */
	if (ret)
		return ret;
	
	return 0;
}

int reader_start(struct reader *r)
{
	int ret;
	
	if (r->state == STATE_STARTED)
		return -1;
	
	ret = pthread_create(&r->thread, NULL, reader, (void *) r);
	if (ret)
		return ret;
	
	r->state = STATE_STARTED;
	
	return 0;
}

int reader_stop(struct reader *r)
{
	int ret;
	
	if (r->state != STATE_STARTED)
		return -1;
	
	ret = pthread_cancel(r->thread);
	if (ret)
		return ret;
	
	ret = pthread_join(r->thread, NULL);
	if (ret)
		return ret;
	
	r->state = STATE_STOPPED;
	
	return 0;
}

int reader_destroy(struct reader *r)
{
	int ret;
	
	ret = pool_destroy(&r->pool);
	if (ret)
		return ret;
	
	r->state = STATE_DESTROYED;
	
	return 0;
}

int reader_attach_source(struct reader *r, struct queue *q)
{
	if (list_contains(&r->sources, q))
		return 0;
	
	list_push(&r->sources, q);
	
	return 0;
}

int reader_detach_queue(struct reader *r, struct queue *q)
{
	if (!list_contains(&r->sources, q))
		return -1;
	
	list_remove(&r->sources, q);
	
	return 0;
}