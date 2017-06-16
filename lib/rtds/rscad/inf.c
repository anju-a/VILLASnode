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
#include <stdbool.h>

#include "utils.h"
#include "log.h"
#include "rtds/rscad/inf.h"

/** Read a line from \p f as long as there are not two consecutive empty lines.
 *
 * Two empty lines are used as separator between the INF file sections.
 * Trailing newline characters are removed
 */
static int rscad_inf_getline(char **line, size_t *linelen, FILE *f)
{
	if (feof(f))
		return 0;
	
	getline(line, linelen, f);
	
	if (!strcmp(*line, "\n")) {
		getline(line, linelen, f);

		return strcmp(*line, "\n");
	}
	
	char *nl = strchr(*line, '\n');
	if (nl)
		*nl = 0;
	
	return 1;
}

static int rscad_inf_destroy_attribute(struct rscad_inf_attribute *a)
{
	if (a->key)
		free(a->key);

	if (a->value)
		free(a->value);
	
	return 0;
}

static int rscad_inf_destroy_element(struct rscad_inf_element *e)
{
	if (e->description)
		free(e->description);

	if (e->group)
		free(e->group);

	if (e->units)
		free(e->units);
	
	if (e->type == RSCAD_INF_ELEMENT_STRING && e->init_value.s)
		free(e->init_value.s);
	
	return 0;
}

static int rscad_inf_destroy_timing_record(struct rscad_inf_timing_record *a)
{
	/** @todo Not implemented yet */
	return 0;
}

static int rscad_inf_parse_attributes(struct list *l, FILE *f)
{
	int ret = 0;
	char *line = NULL;
	size_t linelen = 0;
	
	while (rscad_inf_getline(&line, &linelen, f)) {
		struct rscad_inf_attribute a, a2;
	
		ret = sscanf(line, "%m[^:]%*[: \t]%m[^\n]", &a.key, &a.value);
		if (ret == 2) {
			// This is a special case
			if (!strcmp(a.key, "Rack")) {
				char *tmp;

				ret = sscanf(a.value, "%ms %m[^\n]", &a.value, &tmp);
				if (ret == 2) {
					ret = sscanf(tmp, "%m[^:]%*[: \t]%m[^\n]", &a2.key, &a2.value);
					if (ret == 2)
						list_push(l, memdup(&a2, sizeof(a2)));
					
					free(tmp);
				}
			}
		}
	
		list_push(l, memdup(&a, sizeof(a)));
	}
	
	free(line);

	return 0;
}

static int rscad_inf_parse_elements(struct list *l, FILE *f)
{
	int ret = 0;
	char *line = NULL;
	size_t linelen = 0;
	
	while (rscad_inf_getline(&line, &linelen, f)) {
		struct rscad_inf_element e;
		
		int i = 0;
		char *p = line;
		
		/* Handwritten tokenizer */
		char *tokens[32];
		bool inquote = false;
		
		while (*p && i < 32) {
			/* Skip leading whitespace */
			while (*p == ' ')
				p++;
			
			/* Save token */
			tokens[i] = p;
			
			while (*p) {
				if (inquote) {
					if (*p == '"') {
						inquote = false;
						break;
					}
				}
				else {
					if (*p == ' ' || *p == '=')
						break;
				}
				
				p++;
			}
			
			*p = '\0';
			
			i++;
		}
		tokens[i] = NULL;
		
		
		if      (!strcmp(type, "String"))
			e.type = RSCAD_INF_ELEMENT_STRING;
		else if (!strcmp(type, "Output"))
			e.type = RSCAD_INF_ELEMENT_OUTPUT;
		else if (!strcmp(type, "Pushbutton"))
			e.type = RSCAD_INF_ELEMENT_PUSHBUTTON;
		else if (!strcmp(type, "Slider"))
			e.type = RSCAD_INF_ELEMENT_SLIDER;
		else
			goto warn;
		
		while (1) {
			scanf(line, "%")
			
		}
		
		list_push(l, memdup(&e, sizeof(e)));
		
		continue;
warn:
		warn("Failed to parse element: %s", line);
	}

	free(line);

	return 0;
}

static int rscad_inf_parse_timing_records(struct list *l, FILE *f)
{
	/** @todo Not implemented yet */
	return 0;
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

void rscad_inf_dump(struct rscad_inf *i)
{
	for (size_t j = 0; j < list_length(&i->attributes); j++) {
		struct rscad_inf_attribute *a = list_at(&i->attributes, j);
		
		info("Attribute: key=%s, value=%s", a->key, a->value);
	}

	for (size_t j = 0; j < list_length(&i->elements); j++) {
		struct rscad_inf_element *e = list_at(&i->elements, j);
		
		info("Element: type=%d", e->type);
	}
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