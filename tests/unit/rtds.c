/** Unit tests for RSCAD parses
 *
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

#include <criterion/criterion.h>

#include "rtds/rscad/inf.h"
#include "rtds/gtwif.h"

#define PATH_INF "tests/data/rscad/vdiv.inf"

Test(rtds, gtwif)
{
	
}

Test(rtds, rscad_inf)
{
	int ret;
	struct rscad_inf i;
	struct rscad_inf_element *e;

	FILE *f = fopen(PATH_INF, "r");
	cr_assert_not_null(f);
	
	ret = rscad_inf_init(&i);
	cr_assert_eq(ret, 0);
	
	ret = rscad_inf_parse(&i, f);
	cr_assert_eq(ret, 0);
	
	e = rscad_inf_lookup_element(&i, "Subsystem #1|Sources|src|ABCmag");
	cr_assert_not_null(e);
	
	cr_assert_eq(e->address, 0x783014);
	cr_assert_eq(e->rack, 4);
	cr_assert_eq(e->datatype, RSCAD_INF_DATATYPE_IEEE);
	cr_assert_eq(e->init_value.f, 230.0);
	cr_assert_eq(e->min.f, 0.0);
	cr_assert_eq(e->max.f, 460.0);
	
	ret = rscad_inf_destroy(&i);
	cr_assert_eq(ret, 0);
	
	fclose(f);
}