/** Parsers for RSCAD file formats
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

#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "rscad.h"

static int rscad_inf_destroy_attribute(struct rscad_inf_attribute *a)
{
	return 0;
}

static int rscad_inf_destroy_element(struct rscad_inf_element *e)
{
	return 0;
}

static int rscad_inf_destroy_timing_record(struct rscad_inf_timing_record *a)
{
	return 0;
}

static int rscad_inf_parse_attributes(struct list *l, FILE *f)
{
	int ret = 0;
	char *line = NULL;
	size_t linelen = 0;
	
	while (!feof(f)) {
		getline(&line, &linelen, f);
		
		if (!strcmp(line, "\n")) {
			getline(&line, &linelen, f);
			ret = strcmp(line, "\n") == 0 ? 0 : -1;
			goto out;
		}
		
		info("Parsing attribute: %s", line);
	}
	
out:	free(line);

	return ret;
}

static int rscad_inf_parse_elements(struct list *l, FILE *f)
{
	int ret = 0;
	char *line = NULL;
	size_t linelen = 0;
	
	while (!feof(f)) {
		getline(&line, &linelen, f);
		
		if (!strcmp(line, "\n")) {
			getline(&line, &linelen, f);
			ret = strcmp(line, "\n") == 0 ? 0 : -1;
			goto out;
		}
		
		info("Parsing element: %s", line);
	}

out:	free(line);

	return ret;
}

static int rscad_inf_parse_timing_records(struct list *l, FILE *f)
{
	int ret = 0;
	char *line = NULL;
	size_t linelen = 0;
	
	while (!feof(f)) {
		getline(&line, &linelen, f);
		
		if (!strcmp(line, "\n")) {
			getline(&line, &linelen, f);
			ret = strcmp(line, "\n") == 0 ? 0 : -1;
			goto out;
		}
		
		info("Parsing timing_record: %s", line);
	}

out:	free(line);

	return ret;
}

int rscad_inf_init(struct rscad_inf *i)
{
	list_init(&i->attributes);
	list_init(&i->elements);
	list_init(&i->timing_records);
	
	return 0;
}

int rscad_inf_destroy(struct rscad_inf *i)
{
	list_destroy(&i->attributes,     (dtor_cb_t) rscad_inf_destroy_attribute, true);
	list_destroy(&i->elements,       (dtor_cb_t) rscad_inf_destroy_element, true);
	list_destroy(&i->timing_records, (dtor_cb_t) rscad_inf_destroy_timing_record, true);
	
	return 0;
}

int rscad_inf_parse(struct rscad_inf *i, FILE *f)
{
	int ret;

	rewind(f);
	
	ret = rscad_inf_parse_attributes(&i->attributes, f);
	if (ret)
		return ret;
	
	ret = rscad_inf_parse_elements(&i->elements, f);
	if (ret)
		return ret;

	ret = rscad_inf_parse_timing_records(&i->timing_records, f);
	if (ret)
		return ret;
	
	return 0;
}

struct rscad_inf_element * rscad_inf_lookup_element(struct rscad_inf *i, const char *name)
{
	for (size_t j = 0; j < list_length(&i->elements); j++) {
		struct rscad_inf_element *e __attribute__((unused)) = (struct rscad_inf_element *) list_at(&i->elements, j);

		
	}
	
	return NULL;
}