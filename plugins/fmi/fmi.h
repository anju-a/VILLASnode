/** A generic co-simulation FMU which exposes all VILLAS node-types via the FMI
 *
 * @file
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

#pragma once

#include "fmi2Functions.h"

#include <villas/super_node.h>

/* Memory management helpers */
#define fmi2Free(c, p) ((c)->functions.freeMemory(p))
#define fmi2Alloc(c, n, s) ((c)->functions.allocateMemory(n, s))

#define fmi2Assert(c, cond) do { log(LOG_ERROR, "assertion failed: %s at %s:%d", STR(cond), __FILE__, __LINE__); return fmi2Error; } while(0);

/* Value references (see modelDescription.xml) */
#define FMI_VR_CONFIG_PATH	1000000 /* fmi2String */
#define FMI_VR_NODE_NAME	1000001 /* fmi2String */
#define FMI_VR_SYNC_MODE	1000002 /* fmi2Integer */
#define FMI_VR_SEQUENCE		1000003 /* fmi2Integer */
#define FMI_VR_TS_ORIGIN_SEC	1000004 /* fmi2Integer */
#define FMI_VR_TS_ORIGIN_NSEC	1000005 /* fmi2Integer */
#define FMI_VR_TS_RECV_SEC	1000006 /* fmi2Integer */
#define FMI_VR_TS_RECV_NSEC	1000007 /* fmi2Integer */
#define FMI_VR_OFFSET		1000008 /* fmi2Real */
#define FMI_VR_OUTPUTS		2000000
#define FMI_VR_INPUTS		3000000

#define FMI_MAX_SAMPLELEN	 256

#define FMI_GUID "{86adbd39-06c4-45f5-900c-808487672495}"

typedef enum fmi2State {
	FMI2_STATE_INSTANTIATED,
	FMI2_STATE_INITIALIZATION_MODE,
	FMI2_STATE_STEP_COMPLETE,
	FMI2_STATE_STEP_INPROGRESS,
	FMI2_STATE_STEP_CANCELED,
	FMI2_STATE_STEP_FAILED,
	FMI2_STATE_TERMINATED,
	FMI2_STATE_ERROR,
	FMI2_STATE_FATAL
};

typedef enum fmi2SyncMode {
	FMI2_SYNC_MODE_UNSYNCHRONIZED = 0,
	FMI2_SYNC_MODE_SYNCHRONIZED = 1
};

typedef struct fmi2Experiment {
	fmi2Real tolerance;
	fmi2Real startTime;
	fmi2Real stopTime;
	fmi2Boolean toleranceDefined;
	fmi2Boolean stopTimeDefined;
};

struct fmu {
	struct pool pool;
	struct node *node;

	struct sample *lastReceived;
	struct sample *lastSend;
	struct sample *nextSend;

	fmi2Experiment experiment;
	fmi2State state;
	fmi2SyncMode mode;

	fmi2String fmuGUID;
	fmi2String fmuResourceLocation;
	fmi2String instanceName;

	fmi2CallbackFunctions functions;

	/* Parameters */
	fmi2String configPath;
	fmi2String nodeName;
};
