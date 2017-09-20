/** Path destination.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/utils.h>
#include <villas/node.h>
#include <villas/path.h>
#include <villas/path_destination.h>
#include <villas/path_worker.h>

extern struct path_worker *worker;

struct path_destination * path_destination_create()
{
	int ret;
	struct path_destination *pd;

	pd = (struct path_destination *) alloc(sizeof(struct path_destination));
	if (!pd)
		return NULL;

	ret = path_worker_add_destination(worker, pd);
	if (ret)
		return NULL;

	return pd;
}

int path_destination_init(struct path_destination *pd, struct path *p)
{
	int ret;

	pd->path = p;

	ret = queue_signalled_init(&pd->queue, pd->path->queuelen, &memtype_hugepage, 0);
	if (ret)
		return ret;

	return 0;
}

int path_destination_destroy(struct path_destination *pd)
{
	int ret;

	ret = queue_signalled_destroy(&pd->queue);
	if (ret)
		return ret;

	return 0;
}

int path_destination_write(struct path_destination *pd)
{
	int cnt = pd->node->vectorize;
	int sent;
	int available;
	int released;

	struct sample *smps[cnt];

	/* As long as there are still samples in the queue */
	while (1) {
		available = queue_signalled_pull_many(&pd->queue, (void **) smps, cnt);
		if (available == 0)
			break;
		else if (available < cnt)
			debug(LOG_PATH | 5, "Queue underrun for path %s: available=%u expected=%u", path_name(pd->path), available, cnt);

		debug(LOG_PATH | 15, "Dequeued %u samples from queue of node %s which is part of path %s", available, node_name(pd->node), path_name(pd->path));

		sent = node_write(pd->node, smps, available);
		if (sent < 0)
			error("Failed to sent %u samples to node %s", cnt, node_name(pd->node));
		else if (sent < available)
			warn("Partial write to node %s: written=%d, expected=%d", node_name(pd->node), sent, available);

		released = sample_put_many(smps, sent);

		debug(LOG_PATH | 15, "Released %d samples back to memory pool", released);
	}

	return 0;
}
