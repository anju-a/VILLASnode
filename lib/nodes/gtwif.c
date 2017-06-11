/** Node type: GTWIF - RSCAD protocol for RTDS
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

#include "plugin.h"
#include "nodes/gtwif.h"
#include "utils.h"
#include "msg.h"

int gtwif_reverse(struct node *n)
{
	struct gtwif *g __attribute__((unused)) = n->_vd;

	return 0;
}

int gtwif_parse(struct node *n, config_setting_t *cfg)
{
	struct gtwif *g __attribute__((unused)) = n->_vd;

	return 0;
}

char * gtwif_print(struct node *n)
{
	char *buf = NULL;
	struct gtwif *g __attribute__((unused)) = n->_vd;

	return buf;
}

int gtwif_start(struct node *n)
{
	struct gtwif *g __attribute__((unused)) = n->_vd;

	return 0;
}

int gtwif_stop(struct node *n)
{
	struct gtwif *g __attribute__((unused)) = n->_vd;

	return 0;
}

int gtwif_deinit()
{

	return 0;
}

int gtwif_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct gtwif *g __attribute__((unused)) = n->_vd;

	return 0;
}

int gtwif_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct gtwif *g __attribute__((unused)) = n->_vd;

	return 0;
}

static struct plugin p = {
	.name		= "gtwif",
	.description	= "GTWIF - RSCAD protocol for RTDS",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct gtwif),
		.reverse	= gtwif_reverse,
		.parse		= gtwif_parse,
		.print		= gtwif_print,
		.start		= gtwif_start,
		.stop		= gtwif_stop,
		.deinit		= gtwif_deinit,
		.read		= gtwif_read,
		.write		= gtwif_write,
		.instances	= LIST_INIT()
	}
};

REGISTER_PLUGIN(&p)