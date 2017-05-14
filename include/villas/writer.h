/** Path writer thread.
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

#include <pthread.h>

#include "queue_signalled.h"
#include "list.h"

/* Forward declarations */
struct path_destination;
struct sample;

/* Arguments for writer thread. */
struct writer {
	pthread_t thread;			/**< The thread id for this path. */

	/** The event queue is used to signallize the arrival new samples from one
	 * of the source nodes to the path thread. A reader thread which has read new
	 * samples will enqueue a pointer to the path_source struct to this queue.
	 * Synchronization is done interally with pthread condition variables. */
	struct queue_signalled events;
};

/** Initialize a new path reader. */
int writer_init(struct writer *w);

/** Destroy the reader. */
int writer_destroy(struct writer *w);

/** Start the thread for this reader. */
int writer_start(struct writer *w);

/** Stop the thread for this reader. */
int writer_stop(struct writer *w);