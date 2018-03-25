/** Status Getter APIs for FMI2.
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

fmi2Status fmi2GetStatus(fmi2Component c, const fmi2StatusKind s, fmi2Status* value)
{
	struct fmu *f = (struct fmu *) c;

	switch (s) {
		case fmi2DoStepStatus:

			return fmi2OK;

		default:
			return fmi2Discard;
	}
}

fmi2Status fmi2GetRealStatus(fmi2Component c, const fmi2StatusKind s, fmi2Real* value);
{
	struct fmu *f = (struct fmu *) c;

	switch (s) {
		case fmi2LastSuccessfulTime:

			return fmi2OK;

		default:
			return fmi2Discard;
	}
}

fmi2Status fmi2GetIntegerStatus(fmi2Component c, const fmi2StatusKind s, fmi2Integer* value);
{
	struct fmu *f = (struct fmu *) c;

	switch (s) {
		/* FMI 2.0 supports no status kinds of type integer */

		default:
			return fmi2Discard;
	}
}

fmi2Status fmi2GetBooleanStatus(fmi2Component c, const fmi2StatusKind s, fmi2Boolean* value);
{
	struct fmu *f = (struct fmu *) c;

	switch (s) {
		case fmi2Terminated:

			return fmi2OK;

		default:
			return fmi2Discard;
	}
}

fmi2Status fmi2GetStringStatus(fmi2Component c, const fmi2StatusKind s, fmi2String* value)
{
	struct fmu *f = (struct fmu *) c;

	switch (s) {
		case fmi2PendingStatus:

			return fmi2OK;

		default:
			return fmi2Discard;
	}
}
