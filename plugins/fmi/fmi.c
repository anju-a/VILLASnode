/** A generic co-simulation FMU which exposes all VILLAS node-types via the FMI.
 *
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

#include "fmi.h"

/* A few global variables which are shared by all FMU instances */
static struct super_node sn = { .state = STATE_DESTROYED };
static struct list fmus = { .state = STATE_DESTROYED };

char * fmi2StrDup(fmi2Component c, char *s)
{
	size_t len = strlen(s);

	char *d = fmi2Alloc(c, 1, len + 1);

	strncpy(d, s, len);

	return d;
}

static void * recv_thread(void *ctx)
{
	int ret;

	struct fmu *f = (struct fmu *) ctx;
	struct node *n = f->node;

	struct sample smps[n->vectorize];

	while (1) {
		ret = node_read(n, smps, n->vectorize);
	}

	return NULL;
}

static void log_callback(struct log *l, void *c, const char *fmt, ...)
{
	struct fmu *f = (struct fmu *) c;

	//f->functions.logger(f->functions.componentEnvironment, f->instanceName, );
}

fmi2Component fmi2Instantiate(fmi2String instanceName,
	fmi2Type fmuType,
	fmi2String fmuGUID,
	fmi2String fmuResourceLocation,
	const fmi2CallbackFunctions *functions,
	fmi2Boolean visible,
	fmi2Boolean loggingOn)
{
	struct fmu *f;

	if (fmuType != fmi2CoSimulation)
		return NULL;

	if (strcmp(fmuGUID, FMU_GUID))
		return NULL;

	/* Fallback to calloc() in case the FMI master does not provide an allocator */
	fmi2CallbackAllocateMemory alloc = functions->allocateMemory
										? functions->allocateMemory
										: calloc;

	f = alloc(1, sizeof(struct fmu));
	if (!f)
		return NULL;

	if (loggingOn) {
		int ret;

		ret = log_init(&sn.log, level, LOG_ALL);
		if (ret)
			error("Failed to initialize log");

		ret = log_register_callback(&sn.log, log_callback, c);
		if (ret)
			error("Failed to register log callbacks");

		ret = log_start(&sn.log);
		if (ret)
			error("Failed to start log");
	}

	f->nodeName = NULL;
	f->configPath = NULL;

	f->node = NULL;
	f->functions = functions;
	f->fmuGUID = fmi2StrDup(c, fmuGUID);
	f->instanceName = fmi2StrDup(c, instanceName);
	f->fmuResourceLocation = fmi2StrDup(c, fmuResourceLocation);

	return (fmi2Component) f;
}

void fmi2FreeInstance(fmi2Component c)
{
	struct fmu *f = (struct fmu *) c;

	if (f == NULL)
		return;

	assert(f->state != FMI2_STATE_FATAL && f->state != FMI2_STATE_STEP_INPROGRESS);

	fmi2Free(c, f->fmuGUID);
	fmi2Free(c, f->instanceName);
	fmi2Free(c, f->fmuResourceLocation);

	super_node_destroy();

	if (f->configPath)
		fmi2Free(c, f->configPath);

	if (f->nodeName)
		fmi2Free(c, f->nodeName);

	fmi2Free(c, f);
}

fmi2Status fmi2SetDebugLogging(fmi2Component c, fmi2Boolean loggingOn,
	size_t nCategories,
	const fmi2String categories[])
{
	struct fmu *f = (struct fmu *) c;

	return fmi2OK;
}

fmi2Status fmi2SetupExperiment(fmi2Component c,
	fmi2Boolean toleranceDefined,
	fmi2Real tolerance,
	fmi2Real startTime,
	fmi2Boolean stopTimeDefined,
	fmi2Real stopTime)
{
	struct fmu *f = (struct fmu *) c;

	assert(f->state != FMI2_STATE_INSTANTIATED);

	f->experiment.toleranceDefined = toleranceDefined;
	f->experiment.stopTimeDefined = stopTimeDefined;

	f->experiment.tolerance = tolerance;
	f->experiment.startTime = startTime;
	f->experiment.stopTime = stopTime;

	return fmi2OK;
}

fmi2Status fmi2EnterInitializationMode(fmi2Component c)
{
	struct fmu *f = (struct fmu *) c;

	assert(f->state == FMI2_STATE_INSTANTIATED);

	f->state = FMI2_STATE_INITIALIZATION_MODE;

	return fmi2OK;
}

fmi2Status fmi2ExitInitializationMode(fmi2Component c)
{
	struct fmu *f = (struct fmu *) c;

	assert(f->state == FMI2_STATE_INITIALIZATION_MODE);

	/* Load configuration */
	if (sn.state == STATE_DESTROYED) {
		ret = super_node_parse_uri(&sn, f->configPath);
		if (ret)
			error("Failed to parse configuration");

		ret = super_node_init(&sn);
		if (ret)
			error("Failed to initialize super-node");

#if 0 /* For now we dont want to intefere with the runtime environment of the FMI master */
		ret = memory_init(sn.hugepages);
		if (ret)
			error("Failed to initialize memory");

		ret = rt_init(sn.priority, sn.affinity);
		if (ret)
			error("Failed to initalize real-time");
#endif
	}
	else {
		/* In case this is the second FMU within the same simulation environment,
		 * we require that all FMUs use the same configuration file. */
		if (strcmp(sn.uri, f->configPath)) {
			error()
		}
	}

	f->node = list_lookup(&sn.nodes, f->nodeName);
	if (!f->node)
		error("Node '%s' does not exist!", f->nodeName);

#ifdef WITH_NODE_WEBSOCKET
	/* Only start web subsystem if villas-pipe is used with a websocket node */
	if (f->node->_vt->start == websocket_start) {
		web_start(&sn.web);
		api_start(&sn.api);
	}
#endif /* WITH_NODE_WEBSOCKET */

	ret = node_check(f->node);
	if (ret)
		error("Invalid node configuration");

	ret = node_type_start(f->node->_vt, &sn);
	if (ret)
		error("Failed to intialize node type: %s", node_type_name(node->_vt));

	ret = node_start(f->node);
	if (ret)
		error("Failed to start node %s: reason=%d", node_name(node), ret);

	f->state = FMI2_STATE_STEP_COMPLETE;

	return fmi2OK;
}

fmi2Status fmi2Terminate(fmi2Component c)
{
	struct fmu *f = (struct fmu *) c;

	assert(f->state == FMI2_STATE_STEP_COMPLETE || f->state == FMI2_STATE_STEP_FAILED);

	ret = node_stop(f->node);
	if (ret)
		error("Failed to stop node %s: reason=%d", node_name(node), ret);

	f->state = FMI2_STATE_TERMINATED;

	return fmi2OK;
}

fmi2Status fmi2Reset(fmi2Component c)
{
	struct fmu *f = (struct fmu *) c;

	assert(f->state != FMI2_STATE_STEP_INPROGRESS && f->state != FMI2_STATE_FATAL);

	f->state = FMI2_STATE_INSTANTIATED;

	return fmi2OK;
}
