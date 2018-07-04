/** Message paths
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

#include <list>

#include <pthread.h>
#include <jansson.h>

#include <villas/list.h>
#include <villas/queue.h>
#include <villas/pool.h>
#include <villas/bitset.h>
#include <villas/common.h>
#include <villas/hook.h>
#include <villas/mapping.h>
#include <villas/task.h>

/* Forward declarations */
struct stats;
struct node;

class PathSource {

public:
	struct node *node;

	bool masked;

	struct pool pool;
	struct list mappings;			/**< List of mappings (struct mapping_entry). */

	PathSource();
	~PathSource();
};

class PathDestination {

public:
	struct node *node;

	struct queue queue;

	void write();

	PathDestination(int queuelen);
	~PathDestination();
};

/** The datastructure for a path. */
class Path {

protected:
	int initPoll();
	void enqueue(struct sample *smps[], unsigned cnt);

	/* Thread functions */
	void runSingle();
	void runPoll();

	enum state state;		/**< Path state. */

	/** The register mode determines under which condition the path is triggered. */
	enum mode {
		ANY,			/**< The path is triggered whenever one of the sources receives samples. */
		ALL			/**< The path is triggered only after all sources have received at least 1 sample. */
	} mode;				/**< Determines when this path is triggered. */

	struct {
		int nfds;
		struct pollfd *pfds;
	} reader;

	struct pool pool;
	struct sample *last_sample;
	int last_sequence;

	std::list<PathSource *> sources;	/**< List of all incoming nodes (struct path_source). */
	std::list<PathDestination *> destinations; /**< List of all outgoing nodes (struct path_destination). */

	struct list hooks;		/**< List of processing hooks (struct hook). */

	struct task timeout;

	double rate;			/**< A timeout for */
	int enabled;			/**< Is this path enabled. */
	int poll;			/**< Weather or not to use poll(2). */
	int reverse;			/**< This path as a matching reverse path. */
	int builtin;			/**< This path should use built-in hooks by default. */
	int queuelen;			/**< The queue length for each path_destination::queue */
	int samplelen;			/**< Will be calculated based on path::sources.mappings */

	char *_name;			/**< Singleton: A string which is used to print this path to screen. */

	struct bitset mask;		/**< A mask of path_sources which are enabled for poll(). */
	struct bitset received;		/**< A mask of path_sources for which we already received samples. */

	pthread_t tid;			/**< The thread id for this path. */
	json_t *cfg;			/**< A JSON object containing the configuration of the path. */

public:
	Path();

	/** Initialize internal data structures. */
	int init();

	/** Check if path configuration is proper. */
	int check();

	/** Start a path.
	 *
	 * Start a new pthread for receiving/sending messages over this path.
	 *
	 * @retval 0 Success. Everything went well.
	 * @retval <0 Error. Something went wrong.
	 */
	int start();

	/** Stop a path.
	 *
	 * @retval 0 Success. Everything went well.
	 * @retval <0 Error. Something went wrong.
	 */
	int stop();

	/** Destroy path by freeing dynamically allocated memory. */
	~Path();

	/** Show some basic statistics for a path. */
	void printStats();

	/** Fills the provided buffer with a string representation of the path.
	 *
	 * Format: source => [ dest1 dest2 dest3 ]
	 *
	 * @return A pointer to a string containing a textual representation of the path.
	 */
	const char * getName();

	/** Reverse a path */
	Path reverse();

	/** Check if node is used as source or destination of a path. */
	int usesNode(struct node *n);

	/** Parse a single path and add it to the global configuration.
	 *
	 * @param cfg A JSON object containing the configuration of the path.
	 * @param nodes A linked list of all existing nodes
	 * @retval 0 Success. Everything went well.
	 * @retval <0 Error. Something went wrong.
	 */
	int parseJson(json_t *cfg, struct list *nodes);
};

/** @} */
