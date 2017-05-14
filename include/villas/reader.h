/** Path reader thread.
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

#pragma once

#include "pool.h"
#include "list.h"
#include "common.h"

/* Forward declarations */
struct node;

/** A reader reads samples from one or more nodes and enqueues the samples. */
struct reader {
	enum state state;

	struct node *node;		/**< The node from which this thread reads samples. */

	struct pool pool;		/**< Received samples are drawn from this pool. */
	struct list sources;		/**< This reader will enqueue read samples to these queues. */
	
	pthread_t thread;
};

/** Initialize reader. */
int reader_init(struct reader *r, struct node *n);

/** Start the thread for this reader. */
int reader_start(struct reader *r);

/** Stop the thread for this reader. */
int reader_stop(struct reader *r);

/** Destroy the reader. */
int reader_destroy(struct reader *r);

/** Add samples directed to node \p n to queue \p q. */
int reader_attach_queue(struct reader *r, struct queue *q);

int reader_detach_queue(struct reader *r, struct queue *q);
