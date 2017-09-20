/** Path source.
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

#include <stdbool.h>

#include <villas/list.h>

/* Forward declarations */
struct node;
struct path;

struct path_source {
	struct node *node;
	struct path *path;

	bool masked;
	int index;			/**< Offset of this path_source within the path::mask and path::received bitsets */

	struct list mappings;		/**< List of mappings (struct mapping_entry). */
};

void path_source_mux(struct path_source *ps, struct sample *smps[], unsigned cnt);

struct path_source * path_source_create();

int path_source_init(struct path_source *ps, struct path *p, int index);

int path_source_destroy(struct path_source *ps);

/** @} */
