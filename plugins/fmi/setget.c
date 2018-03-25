/** Attribute setter / getter APIs for FMI2.
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

/* Getter */

fmi2Status fmi2GetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Real value[])
{
	struct fmu *f = (struct fmu *) c;
	struct node *n = f->node;

	for (int i = 0; i < nvr; i++) {
		switch (vr[i]) {
			case FMI_VR_OUTPUTS .. FMI_VR_OUTPUTS + FMI_MAX_SAMPLELEN:
				if (f->lastReceived && vr[i] < f->lastReceived->length)
					value[i] = f->lastReceived->data[vr[i]].f;
				break;

			case FMI_VR_INPUTS .. FMI_VR_INPUTS + FMI_MAX_SAMPLELEN:
				if (f->nextSend && vr[i] < f->nextSend->length)
					value[i] = f->nextSend->data[vr[i]].f;
				break;

			case FMI_VR_OFFSET:
				value[i] = time_delta(&f->lastReceived->ts.received, &f->lastReceived->ts.origin);
				break;

			default:
				return fmi2Error;
		}
	}

	return fmi2OK;
}

fmi2Status fmi2GetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[])
{
	struct fmu *f = (struct fmu *) c;
	struct node *n = f->node;

	for (int i = 0; i < nvr; i++) {
		switch (vr[i]) {
			case FMI_VR_OUTPUTS .. FMI_VR_OUTPUTS + FMI_MAX_SAMPLELEN:
				if (f->lastReceived && vr[i] < f->lastReceived->length)
					value[i] = f->lastReceived->data[vr[i]].i;
				break;

			case FMI_VR_INPUTS .. FMI_VR_INPUTS + FMI_MAX_SAMPLELEN:
				if (f->nextSend && vr[i] < f->nextSend->length)
					value[i] = f->nextSend->data[vr[i]].i;
				break;

			case FMI_VR_SEQUENCE:
				value[i] = f->lastReceived->sequence;
				break;

			case FMI_VR_TS_ORIGIN_SEC:
				value[i] = f->lastReceived->ts.origin.tv_sec;
				break;

			case FMI_VR_TS_ORIGIN_NSEC:
				value[i] = f->lastReceived->ts.origin.tv_nsec;
				break;

			case FMI_VR_TS_RECV_SEC:
				value[i] = f->lastReceived->ts.received.tv_sec;
				break;

			case FMI_VR_TS_RECV_NSEC:
				value[i] = f->lastReceived->ts.received.tv_nsec;
				break;

			case FMI_VR_SYNC_MODE:
				value[i] = f->mode;
				break;

			default:
				return fmi2Error;
		}
	}

	return fmi2OK;
}

fmi2Status fmi2GetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Boolean value[])
{
	struct fmu *f = (struct fmu *) c;

	for (int i = 0; i < nvr; i++) {
		switch (vr[i]) {
			default:
				return fmi2Error;
		}
	}

	return fmi2OK;
}

fmi2Status fmi2GetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2String value[])
{
	struct fmu *f = (struct fmu *) c;

	for (int i = 0; i < nvr; i++) {
		switch (vr[i]) {
			case FMI_VR_CONFIG_PATH:
				value[i] = f->configPath;
				break;

			case FMI_VR_NODE_NAME:
				value[i] = f->nodeName;
				break;

			default:
				return fmi2Error;
		}
	}

	return fmi2OK;
}

/* Setter */

fmi2Status fmi2SetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Real value[])
{
	struct fmu *f = (struct fmu *) c;
	struct node *n = f->node;

	for (int i = 0; i < nvr; i++) {
		switch (vr[i]) {
			case FMI_VR_INPUTS .. FMI_VR_INPUTS + FMI_MAX_SAMPLELEN:
					if (f->nextSend && vr[i] < f->nextSend->capacity)
						f->lastSend->data[vr[i]].f = value[i];

				break;

			default:
				return fmi2Error;
		}
	}

	return fmi2OK;
}

fmi2Status fmi2SetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[])
{
	struct fmu *f = (struct fmu *) c;
	struct node *n = f->node;

	for (int i = 0; i < nvr; i++) {
		switch (vr[i]) {
			case FMI_VR_INPUTS .. FMI_VR_INPUTS + FMI_MAX_SAMPLELEN:
					if (f->nextSend && vr[i] < f->nextSend->capacity)
						f->lastSend->data[vr[i]].i = value[i];

				break;

			default:
				return fmi2Error;
		}
	}

	return fmi2OK;
}

fmi2Status fmi2SetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Boolean value[])
{
	struct fmu *f = (struct fmu *) c;

	for (int i = 0; i < nvr; i++) {
		switch (vr[i]) {
			default:
				return fmi2Error;
		}
	}

	return fmi2OK;
}

fmi2Status fmi2SetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2String value[])
{
	struct fmu *f = (struct fmu *) c;

	for (int i = 0; i < nvr; i++) {
		switch (vr[i]) {
			case FMI_VR_CONFIG_PATH:
				if (f->configPath)
					fmi2Free(c, f->configPath);

				f->configPath = fmi2StrDup(c, value[i]);
				break;

			case FMI_VR_NODE_NAME:
				if (f->nodeName)
					fmi2Free(c, f->nodeName);

				f->nodeName = fmi2StrDup(c, value[i]);
				break;

			default:
				return fmi2Error;
		}
	}

	return fmi2OK;
}
