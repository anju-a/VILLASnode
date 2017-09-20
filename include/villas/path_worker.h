/** Path reader.
 *
 * @file
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

/** A path connects one input node to multiple output nodes (1-to-n).
 *
 * @addtogroup path Path
 * @{
 */

#pragma once

#include <poll.h>

#include <villas/common.h>
#include <villas/list.h>
#include <villas/pool.h>

/* Forward declarations */
struct path_destination;
struct path_source;

struct path_job {
	enum {
		PATH_SOURCE,
		PATH_DESTINATION
	} type;

	struct pollfd pfd;

	union {
		struct path_source *source;
		struct path_destination *destination;
	};
};

struct path_worker {
	enum state state;

	struct list jobs;		/**< List of jobs this worker is handling. List of struct path_job */
	struct pollfd *pfds;		/**< Array of struct pollfd. Matches 1-to-1 to entries in path_worker_jobs. */
	int nfds;

	struct pool pool;

	pthread_t tid;
	pthread_mutex_t mtx;		/**< Mutex protecting access to path_worker::pfds and path_worker::nfds */
};

struct path_worker *path_worker_create();

int path_worker_init(struct path_worker *pw);

int path_worker_add_source(struct path_worker *pw, struct path_source *ps);

int path_worker_remove_source(struct path_worker *pw, struct path_source *ps);

int path_worker_add_destination(struct path_worker *pw, struct path_destination *pd);

int path_worker_remove_destination(struct path_worker *pw, struct path_destination *pd);

int path_worker_destroy(struct path_worker *pw);

int path_worker_start(struct path_worker *pw);

int path_worker_stop(struct path_worker *pw);

/** @} */
