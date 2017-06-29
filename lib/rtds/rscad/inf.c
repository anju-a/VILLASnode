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
#include <regex.h>

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

static int rscad_inf_getattr(char **line, char **key, char **value)
{
	int ret;

	size_t kl, vl;
	regex_t regex;
	regmatch_t matches[4];
	regmatch_t *km, *vm, *all;

	ret = regcomp(&regex, "([a-z]+)=(\"([^\"]+)\"|[^\" ]+)", REG_EXTENDED | REG_ICASE);
	if (ret) {
		char buf[512];
		regerror(ret, &regex, buf, sizeof(buf));
		
		warn("Failed to compile RE: %s", buf);
		return ret;
	}

	ret = regexec(&regex, *line, 4, matches, 0);
	if (ret)
		return ret;

	int quotes = matches[3].rm_so > 0;

	all = &matches[0];
	km  = &matches[1];
	vm  = &matches[quotes ? 3 : 2];
	
	kl = km->rm_eo - km->rm_so;
	vl = vm->rm_eo - vm->rm_so;
	
	*key   = realloc(*key,   kl + 1);
	*value = realloc(*value, vl + 1);

	strncpy(*key,   *line + km->rm_so, kl);
	strncpy(*value, *line + vm->rm_so, vl);	
	
	(*key)[kl] = '\0';
	(*value)[vl] = '\0';
	
	*line += all->rm_eo;

	regfree(&regex);
	
	return 0;
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
	
	if (e->name)
		free(e->name);
	
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

static int rscad_inf_parse_value(const char *value, enum rscad_inf_datatype dt, union rscad_inf_value *v)
{
	char *endptr;
		
	switch (dt) {
		case RSCAD_INF_DATATYPE_IEEE:
			v->f = strtod(value, &endptr);
			break;

		case RSCAD_INF_DATATYPE_INT:
			v->i = strtoul(value, &endptr, 10);
			break;

		case RSCAD_INF_DATATYPE_STRING:
			v->s = strdup(value);
			break;

		default:
			return -1;
	}

	return value == endptr;
}

static int rscad_inf_parse_elements(struct list *l, FILE *f)
{
	int ret;
	char *line = NULL, *key = NULL, *value = NULL;
	size_t linelen = 0;

	while (rscad_inf_getline(&line, &linelen, f)) {
		char *type, *attrs;
		
		type = line;
		
		attrs = strchr(line, ' ');
		if (!attrs)
			goto fail;
		
		*attrs++ = '\0';
		
		struct rscad_inf_element e = {
			.name = NULL,
			.description = NULL,
			.group = NULL,
			.units = NULL,
			.address = -1,
			.rack = -1,
			.datatype = RSCAD_INF_DATATYPE_STRING,
			.init_value.i = 0,
			.min.i = 0,
			.max.i = 0
		};

		if      (!strcmp(type, "String"))
			e.type = RSCAD_INF_ELEMENT_STRING;
		else if (!strcmp(type, "Output"))
			e.type = RSCAD_INF_ELEMENT_OUTPUT;
		else if (!strcmp(type, "Pushbutton"))
			e.type = RSCAD_INF_ELEMENT_PUSHBUTTON;
		else if (!strcmp(type, "Slider"))
			e.type = RSCAD_INF_ELEMENT_SLIDER;
		else
			goto fail;
		
		while (rscad_inf_getattr(&attrs, &key, &value) == 0) {
			if      (!strcmp(key, "Desc"))
				e.description = strdup(value);
			else if (!strcmp(key, "Group"))
				e.group = strdup(value);
			else if (!strcmp(key, "Units"))
				e.units = strdup(value);
			else if (!strcmp(key, "Adr"))
				e.address = strtoul(value, NULL, 16);
			else if (!strcmp(key, "Rack"))
				e.rack = strtoul(value, NULL, 10);
			else if (!strcmp(key, "Type")) {
				if      (!strcmp(value, "INT"))
					e.datatype = RSCAD_INF_DATATYPE_INT;
				else if (!strcmp(value, "IEEE"))
					e.datatype = RSCAD_INF_DATATYPE_IEEE;
				else
					goto fail;
			}
			else if (!strcmp(key, "InitValue")) {
				ret = rscad_inf_parse_value(value, e.datatype, &e.init_value);
				if (ret)
					goto fail;
			}
			else if (!strcmp(key, "Min")) {
				ret = rscad_inf_parse_value(value, e.datatype, &e.min);
				if (ret)
					goto fail;
			}
			else if (!strcmp(key, "Max")) {
				ret = rscad_inf_parse_value(value, e.datatype, &e.max);
				if (ret)
					goto fail;
			}
			else
				goto fail;
		}
		
		if (e.group && e.description)
			strcatf(&e.name, "%s|%s", e.group, e.description);

		list_push(l, memdup(&e, sizeof(e)));

		continue;
fail:
		warn("Failed to parse element: %s", line);
	}

	free(line);
	free(key);
	free(value);

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
		
		info("Element: %s", e->name);
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