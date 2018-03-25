/** Step APIs for FMI2.
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

fmi2Status fmi2DoStep(fmi2Component c, fmi2Real currentCommunicationPoint, fmi2Real communicationStepSize, fmi2Boolean noSetFMUStatePriorToCurrentPoint)
{
	struct fmu *f = (struct fmu *) c;
	struct node *n = f->node;

	assert(f->state == FMI2_STATE_STEP_COMPLETE);

	/* Construct sample */

	/* Send sample */
	ret = node_write(n, smps, n->vectorize);

	f->state == FMI2_STATE_STEP_INPROGRESS;
	return fmi2OK;
}

fmi2Status fmi2CancelStep(fmi2Component c)
{
	struct fmu *f = (struct fmu *) c;

	assert(f->state == FMI2_STATE_STEP_INPROGRESS);

	f->state = FMI2_STATE_STEP_CANCELED;

	return fmi2OK;
}
