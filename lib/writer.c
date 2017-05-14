/** Path writer thread.
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

#include "writer.h"
#include "sample.h"
#include "log.h"
#include "node.h"
#include "hook.h"
#include "path.h"

/** Main thread function per path: receive -> sent messages */
static void * writer(void *arg)
{
	int ret;

	struct writer *w = arg;
	
	debug(LOG_PATH | 5, "Writer thread started for path");
	
	for (;;) {
		struct path_source *ps;
		
		/* Wait for a sample from any of the incoming nodes */
		ret = queue_signalled_pull(&w->events, (void *) &ps);
		if (ret < 0)
			error("Failed to wait for event");
		
		path_source_process(ps);
		
		/* Send new sample to destinations */
		for (size_t i = 0; i < list_length(&ps->path->destinations); i++) {
			struct path_destination *pd = list_at(&ps->path->destinations, i);
			
			past_destination_process(pd)
		}
	}

	return NULL;
}

int writer_init(struct writer *w)
{
	return 0;
}

int writer_start(struct writer *w)
{
	int ret;
	
	ret = pthread_create(&w->thread, NULL, writer, w);
	if (ret)
		return ret;
	
	return 0;
}

int writer_stop(struct writer *w)
{
	int ret;
	
	ret = pthread_cancel(w->thread);
	if (ret)
		return ret;
	
	ret = pthread_join(w->thread, NULL);
	if (ret)
		return ret;

	return 0;
}

int writer_destroy(struct writer *pr)
{
	return 0;
}