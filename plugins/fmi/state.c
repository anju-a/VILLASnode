/** FMU state APIs for FMI2.
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

#if 0

/* Get / Set FMU state */

fmi2Status fmi2GetFMUstate(fmi2Component c, fmi2FMUstate* FMUstate)
{
	struct fmu *f = (struct fmu *) c;

	return fmi2OK;
}

fmi2Status fmi2SetFMUstate(fmi2Component c, fmi2FMUstate FMUstate)
{
	struct fmu *f = (struct fmu *) c;

	return fmi2OK;
}

fmi2Status fmi2FreeFMUstate(fmi2Component c, fmi2FMUstate* FMUstate)
{
	struct fmu *f = (struct fmu *) c;

	return fmi2OK;
}

/* (De)Serialize FMU state */

fmi2Status fmi2SerializedFMUstateSize(fmi2Component c, fmi2FMUstate FMUstate, size_t *size)
{
	struct fmu *f = (struct fmu *) c;

	return fmi2OK;
}

fmi2Status fmi2SerializeFMUstate(fmi2Component c, fmi2FMUstate FMUstate, fmi2Byte serializedState[], size_t size)
{
	struct fmu *f = (struct fmu *) c;

	return fmi2OK;
}

fmi2Status fmi2DeSerializeFMUstate(fmi2Component c, const fmi2Byte serializedState[], size_t size, fmi2FMUstate* FMUstate)
{
	struct fmu *f = (struct fmu *) c;

	return fmi2OK;
}

#endif
