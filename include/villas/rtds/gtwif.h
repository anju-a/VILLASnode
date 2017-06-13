/** Parsers for RSCAD file formats
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

#include <stdlib.h>
#include <stdint.h>

#define GTWIF_CMD_MODIFY	0x004D
#define GTWIF_CMD_EXAMINE	0x0058	
#define GTWIF_CMD_READLIST2	0x0143
#define GTWIF_CMD_PING		0x0136

/** Send command CMD_READLIST2 to a GTWIF card.
 *
 * This command reads a list of signals from the GTWIF card.
 */
int gtwif_cmd_readlist2(int sd, uint32_t cnt, uint32_t addrs[], uint32_t vals[]);

/** Send command CMD_EXAMINE to a GTWIF card.
 *
 * This command modifies a memory location in the GTWIF memory.
 */
int gtwif_cmd_examine(int sd, uint32_t addr, uint32_t *val);

int gtwif_cmd_modify(int sd, uint32_t addr, uint32_t val);

int gtwif_cmd_modify_many(int sd, uint32_t cnt, uint32_t addrs[], uint32_t vals[], uint32_t old_vals[]);

int gtwif_cmd_ping(int sd, char *name, size_t namelen, char *casename, size_t casenamelen);
