/** Parsers for RSCAD file formats
 *
 * @file
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

#pragma once

#include <stdint.h>
#include <stdio.h>

#include "list.h"

struct rscad_inf_attribute {
	char *key;
	char *value; 
};

struct rscad_inf_timing_record {
	
};

union rscad_inf_value {
	float f;
	int   i;
	char *s;
};

struct rscad_inf_element {
	char *name;
	
	enum rscad_inf_type {
		RSCAD_INF_ELEMENT_STRING,
		RSCAD_INF_ELEMENT_OUTPUT,
		RSCAD_INF_ELEMENT_PUSHBUTTON,
		RSCAD_INF_ELEMENT_SLIDER
	} type;
	
	enum rscad_inf_datatype {
		RSCAD_INF_DATATYPE_IEEE,
		RSCAD_INF_DATATYPE_INT,
		RSCAD_INF_DATATYPE_STRING
	} datatype;
	
	union rscad_inf_value init_value;
	union rscad_inf_value min;
	union rscad_inf_value max;
	
	int rack;
	uint32_t address;
	
	char *group;
	char *description;
	char *units;
};

struct rscad_inf {
	struct list elements;		/**< List of struct rscad_inf_element */
	struct list attributes;		/**< List of struct rscad_inf_attribute */
	struct list timing_records;	/**< List of struct rscad_inf_timing_record */
	
	int rack;
	char *name;
	double delta;
	
	bool vsc;
	bool nrt;
	bool distribution;
};

int rscad_inf_init(struct rscad_inf *i);

int rscad_inf_parse(struct rscad_inf *i, FILE *f);

int rscad_inf_destroy(struct rscad_inf *i);

void rscad_inf_dump(struct rscad_inf *i);

struct rscad_inf_element * rscad_inf_lookup_element(struct rscad_inf *i, const char *name);
