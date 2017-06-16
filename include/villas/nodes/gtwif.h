/** Node type: GTWIF - RSCAD protocol for RTDS
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

/**
 * @addtogroup gtwif GTWIF protocol for RTDS node type
 * @ingroup node
 * @{
 */

#pragma once

#include <netinet/in.h>

#include "node.h"
#include "list.h"
#include "rtds/rscad/inf.h"

struct gtwif {
	struct sockaddr_in remote;	/**< The IP / port address of the RTDS racks. */

	struct gtwif_direction {
		uint32_t *addresses;
		int count;
	} in, out;
	
	struct rscad_inf inffile;	/**< The RSCAD case information file. */
	
	int sd;				/**< The socket descriptor. */
	
	double rate;			/**< The polling rate. */
	double timeout;			/**< The recv() timeout. */
	
	struct sample *last;		/**< The last sample which has been sent by this node. */
};

/** @} */