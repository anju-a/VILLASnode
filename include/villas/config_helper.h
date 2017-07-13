/** Helpers for configuration parsers.
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

#pragma once

#include <jansson.h>
#include <libconfig.h>

#include "sample.h"

/* Convert a libconfig object to a jansson object */
json_t * config_to_json(config_setting_t *cfg);

/* Convert a jansson object into a libconfig object. */
int json_to_config(json_t *json, config_setting_t *parent);

/* Create a libconfig object from command line parameters. */
int config_parse_cli(config_t *cfg, int argc, char *argv[]);
