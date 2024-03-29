/** Logging routines that depend on jansson.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>

#include "config.h"
#include "log.h"
#include "log_config.h"
#include "utils.h"
#include "string.h"

int log_parse(struct log *l, json_t *cfg)
{
	const char *facilities = NULL;
	const char *path = NULL;
	int ret;

	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: s, s?: s, s?: b }",
		"level", &l->level,
		"file", &path,
		"facilities", &facilities,
		"syslog", &l->syslog
	);
	if (ret)
		jerror(&err, "Failed to parse logging configuration");

	if (path)
		l->path = strdup(path);

	if (facilities)
		log_set_facility_expression(l, facilities);

	l->state = STATE_PARSED;

	return 0;
}

void jerror(json_error_t *err, const char *fmt, ...)
{
	va_list ap;
	char *buf = NULL;

	struct log *l = global_log ? global_log : &default_log;

	va_start(ap, fmt);
	vstrcatf(&buf, fmt, ap);
	va_end(ap);

	log_print(l, LOG_LVL_ERROR, "%s:", buf);
	{ INDENT
		log_print(l, LOG_LVL_ERROR, "%s in %s:%d:%d", err->text, err->source, err->line, err->column);

		if (l->syslog)
			syslog(LOG_ERR, "%s in %s:%d:%d", err->text, err->source, err->line, err->column);
	}

	free(buf);

	killme(SIGABRT);
	pause();
}
