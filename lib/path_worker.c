/** Path reader.
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

#include <string.h>
#include <poll.h>
#include <pthread.h>

#include <villas/utils.h>
#include <villas/node.h>
#include <villas/path.h>
#include <villas/path_source.h>
#include <villas/path_destination.h>
#include <villas/path_worker.h>
#include <villas/queue_signalled.h>

static int path_job_cmp(const struct path_job *a, const struct path_job *b, void *ctx)
{
	if (a->type == b->type) {
		switch (a->type) {
			case PATH_SOURCE: return a->source == b->source;
			case PATH_DESTINATION: return a->destination == b->destination;
			default: return -1;
		}
	}
	else
		return -1;
}

static int path_worker_add_job(struct path_worker *pw, struct path_job *j)
{
	void *p;

	pthread_mutex_lock(&pw->mtx);

	p = list_search(&pw->jobs, (cmp_cb_t) path_job_cmp, j);
	if (p)
		return -1; /* The same job is already in the list */

	list_push(&pw->jobs, memdup(j, sizeof(*j)));

	int i = pw->nfds++;
	pw->pfds = realloc(pw->pfds, sizeof(struct pollfd) * pw->nfds);
	pw->pfds[i] = j->pfd;

	pthread_mutex_unlock(&pw->mtx);

	return 0;
}

static int path_worker_remove_job(struct path_worker *pw, struct path_job *j)
{
	void *p;
	ssize_t i;

	pthread_mutex_lock(&pw->mtx);

	p = list_search(&pw->jobs, (cmp_cb_t) path_job_cmp, j);
	if (!p)
		return -1; /* The job is already not in the list */

	i = list_index(&pw->jobs, p);
	if (i < 0)
		return -1; /* Something went wrong here */

	memmove(&pw->pfds[i], &pw->pfds[i+1], pw->nfds - i - 1);

	pw->nfds--;

	list_remove(&pw->jobs, p);

	pthread_mutex_unlock(&pw->mtx);

	return 0;
}

static void path_worker_read(struct path_worker *pw, struct path_source *ps)
{
	int received, ready, cnt = ps->node->vectorize;

	struct sample *smps[cnt];

	/* Fill smps[] free sample blocks from the pool */
	ready = sample_alloc_many(&pw->pool, smps, cnt);
	if (ready != cnt)
		warn("Pool underrun for path source %s", node_name(ps->node));

	/* Read ready samples and store them to blocks pointed by smps[] */
	received = node_read(ps->node, smps, ready);
	if (received == 0)
		goto out;
	else if (received < 0)
		error("Failed to receive message from node %s", node_name(ps->node));
	else if (received < ready)
		warn("Partial read for node %s: read=%u, expected=%u", node_name(ps->node), received, ready);

	/* Enqueue the new samples into each path_source */
	for (size_t i = 0; i < list_length(&pw->jobs); i++) {
		struct path_job *j = (struct path_job *) list_at(&pw->jobs, i);

		if (j->type != PATH_SOURCE)
			continue;

		if (j->source->node != ps->node)
			continue;

		path_source_mux(ps, smps, received);
	}

out:	sample_put_many(smps, ready);
}

static void path_worker_write(struct path_worker *pw, struct path_destination *pd)
{
	for (size_t i = 0; i < list_length(&pw->jobs); i++) {
		struct path_job *j = (struct path_job *) list_at(&pw->jobs, i);

		if (j->type != PATH_DESTINATION)
			continue;

		path_destination_write(j->destination);
	}
}

static void path_worker_poll(struct path_worker *pw)
{
	int ret;

	ret = poll(pw->pfds, pw->nfds, -1);
	if (ret < 0)
		serror("Failed to poll");

	for (int i = 0; i < pw->nfds; i++) {
		struct path_job *j = list_at(&pw->jobs, i);

		if (pw->pfds[i].revents & POLLIN) {
			switch (j->type) {
				case PATH_SOURCE:
					path_worker_read(pw, j->source);
					break;

				case PATH_DESTINATION:
					path_worker_write(pw, j->destination);
					break;
			}
		}
	}
}

static void * path_worker_run(void *arg)
{
	struct path_worker *pw = (struct path_worker *) arg;

	while (1) {
		path_worker_poll(pw);
	}

	return NULL;
}

struct path_worker *path_worker_create()
{
	struct path_worker *pw;

	pw = alloc(sizeof(struct path_worker));
	if (!pw)
		return NULL;

	return pw;
}

int path_worker_init(struct path_worker *pw)
{
	int ret;

	ret = pthread_mutex_init(&pw->mtx, NULL);
	if (ret)
		return ret;

	ret = list_init(&pw->jobs);
	if (ret)
		return ret;

	pw->nfds = 0;
	pw->pfds = NULL;

	return 0;
}

int path_worker_add_source(struct path_worker *pw, struct path_source *ps)
{
	struct path_job j = {
		.type = PATH_SOURCE,
		.source = ps,
		.pfd = {
			.fd = node_fd(ps->node),
			.events = POLLIN
		}
	};

	return path_worker_add_job(pw, &j);
}

int path_worker_remove_source(struct path_worker *pw, struct path_source *ps)
{
	struct path_job j = {
		.type = PATH_SOURCE,
		.source = ps
	};

	return path_worker_remove_job(pw, &j);
}

int path_worker_add_destination(struct path_worker *pw, struct path_destination *pd)
{
	struct path_job j = {
		.type = PATH_DESTINATION,
		.destination = pd,
		.pfd = {
			.fd = queue_signalled_fd(&pd->queue),
			.events = POLLIN
		}
	};

	return path_worker_add_job(pw, &j);
}

int path_worker_remove_destination(struct path_worker *pw, struct path_destination *pd)
{
	struct path_job j = {
		.type = PATH_DESTINATION,
		.destination = pd
	};

	return path_worker_remove_job(pw, &j);
}

int path_worker_destroy(struct path_worker *pw)
{
	int ret;

	ret = list_destroy(&pw->jobs, NULL, true);
	if (ret)
		return ret;

	ret = pthread_mutex_destroy(&pw->mtx);
	if (ret)
		return ret;

	if (pw->pfds)
		free(pw->pfds);

	return 0;
}

int path_worker_start(struct path_worker *pw)
{
	int ret;

	/* Start one thread per path for sending to destinations */
	ret = pthread_create(&pw->tid, NULL, &path_worker_run, pw);
	if (ret)
		return ret;

	return 0;
}

int path_worker_stop(struct path_worker *pw)
{
	int ret;

	ret = pthread_cancel(pw->tid);
	if (ret)
		return ret;

	ret = pthread_join(pw->tid, NULL);
	if (ret)
		return ret;

	return 0;
}
