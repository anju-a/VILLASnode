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

#include <sys/socket.h>
#include <arpa/inet.h>
#include <math.h>

#include "plugin.h"
#include "timing.h"
#include "nodes/gtwif.h"

#define GTWIF_CMD_MODIFY	0x004D	
#define GTWIF_CMD_READLIST2	0x0143

#define GTWIF_MAX_RATE		10.0

static int gtwif_cmd_readlist2(int sd, uint32_t cnt, uint32_t addrs[], uint32_t vals[])
{
	ssize_t sent, recvd;
	
	uint32_t buf[cnt + 4];
	
	int retries = 3;
	
	buf[0] = htons(GTWIF_CMD_READLIST2);
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = cnt;
	
	for (int i = 0; i < cnt; i++)
		buf[i+3] = addrs[i];
	
retry:	sent = send(sd, buf, sizeof(buf), 0);
	if (sent < 0)
		return -1;
	
	recvd = recv(sd, buf, sizeof(buf), 0);
	if (recvd < 0) {
		if (errno == ETIMEDOUT && retries > 0) {
			retries--;
			goto retry;
		}
		else
			return -1;
	}
	
	if (buf[0] != htons(GTWIF_CMD_READLIST2))
		return -1;
	
	/* We update the number of values with the returned number of values */
	cnt = buf[1];
	
	for (int i = 0; i < cnt; i++)
		vals[i] = buf[i+3];

	return cnt;
}

static int gtwif_cmd_modify(int sd, uint32_t addr, uint32_t val)
{
	ssize_t sent, recvd;
	uint32_t buf[4];
	int retries = 3;
	
	buf[0] = htons(GTWIF_CMD_MODIFY);
	buf[1] = htonl(addr);
	buf[2] = htonl(1);
	buf[3] = htonl(val);
	
retry:	sent = send(sd, buf, sizeof(buf), 0);
	if (sent < 0)
		return -1;
	
	recvd = recv(sd, buf, sizeof(buf), 0);
	if (recvd < 0) {
		if (errno == ETIMEDOUT && retries > 0) {
			retries--;
			goto retry;
		}
		else
			return -1;
	}
	
	return 0;
}

/** Sends multiple CMD_MODIFY commands to the GTWIF card.
 *
 * We we only modify values which have been changed.
 * This is checked by comparing \p vals[] with \p old_vals[]
 */
static int gtwif_cmd_modify_many(int sd, uint32_t cnt, uint32_t addrs[], uint32_t vals[], uint32_t old_vals[])
{
	int ret, mod = 0;

	for (int i = 0; i < cnt; i++) {
		if (old_vals && vals[i] == old_vals[i])
			continue;
		
		ret = gtwif_cmd_modify(sd, addrs[i], vals[i]);
		if (ret)
			continue;
		
		mod++;
	}
	
	return mod;
}

static int gtwif_parse_direction(struct gtwif_direction *d, config_setting_t *cfg)
{
	if (config_setting_type(cfg) != CONFIG_TYPE_ARRAY)
		cerror(cfg, "GTWIF node 'in' / 'out' must be any array of addresses or strings");
	
	d->count = config_setting_length(cfg);
	d->addresses = alloc(d->count * sizeof(d->addresses[0]));
	
	for (int i = 0; i < d->count; i++) {
		config_setting_t *cfg_elm = config_setting_get_elem(cfg, i);
		
		switch (config_setting_type(cfg_elm)) {
#if 0
			case CONFIG_TYPE_STRING: {
				const char *elm = config_setting_get_string(cfg, i);
				
				
				
				break;
			}
#endif				
			case CONFIG_TYPE_INT:
				d->addresses[i] = config_setting_get_int(cfg);
				break;
			
			default:
				cerror(cfg_elm, "Invalid type");
		}
	}

	return 0;
}

int gtwif_parse(struct node *n, config_setting_t *cfg)
{
	struct gtwif *g = n->_vd;
	
	int ret;
	const char *remote;
	config_setting_t *cfg_in, *cfg_out, *cfg_remote, *cfg_rate;

	cfg_in = config_setting_get_member(cfg, "in");
	if (cfg_in) {
		ret = gtwif_parse_direction(&g->in, cfg_in);
		if (ret)
			cerror(cfg_in, "Failed to parse inputs of GTWIF node: %s", node_name(n)); 
		
	}
	
	cfg_out = config_setting_get_member(cfg, "out");
	if (cfg_out) {
		ret = gtwif_parse_direction(&g->out, cfg_out);
		if (ret)
			cerror(cfg_out, "Failed to parse outputs of GTWIF node: %s", node_name(n)); 
		
	}
	
	if (!config_setting_lookup_float(cfg, "timeout", &g->timeout))
		g->timeout = 1;

	cfg_rate = config_setting_get_member(cfg, "rate");
	if (cfg_rate) {
		g->rate = config_setting_get_float(cfg_rate);
		if (g->rate == 0)
			cerror(cfg_rate, "The GTWIF node %s has an invalid rate", node_name(n));
		if (g->rate > GTWIF_MAX_RATE)
			cerror(cfg_rate, "The GTWIF node %s 'rate' setting (%f) exceeds the allowed maximum (%f)", node_name(n), g->rate, GTWIF_MAX_RATE);
	}
	else
		g->rate = 1;
	
	cfg_remote = config_setting_get_member(cfg, "remote");
	if (cfg_remote) {
		remote = config_setting_get_string(cfg_remote);
		if (!remote)
			cerror(cfg_remote, "The 'remote' setting must be a string!");
		
		ret = inet_aton(remote, &g->remote.sin_addr);
		if (!ret)
			cerror(cfg_remote, "The setting 'remote' = %s is not a valid IP address!", remote);

		g->remote.sin_family = AF_INET;
		g->remote.sin_port = htons(2);
	}
	else
		cerror(cfg, "GTWIF node %s is missing setting 'rack'", node_name(n));

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
	struct gtwif *g = n->_vd;
	int ret;

	g->sd = socket(PF_INET, SOCK_DGRAM, 0);
	if (g->sd < 0)
		return -1;
	
	ret = connect(g->sd, &g->remote, sizeof(g->remote));
	if (ret)
		return ret;
	
	struct timeval tv = {
		.tv_sec  = (int) g->timeout,
		.tv_usec = fmod(g->timeout, 1.0) * 1e6
	};
	
	setsockopt(g->sd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof(tv));
	
	g->last = NULL;

	return 0;
}

int gtwif_stop(struct node *n)
{
	int ret;
	struct gtwif *g = n->_vd;
	
	ret = close(g->sd);
	if (ret)
		return ret;
	
	sample_put(g->last);

	return 0;
}

int gtwif_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int ret;
	struct gtwif *g = n->_vd;
	struct sample *smp = smps[0];
	
	uint32_t count = MIN(g->in.count, smp->capacity);
	uint32_t *vals = (uint32_t *) &smp->data;

	ret = gtwif_cmd_readlist2(g->sd, count, g->in.addresses, vals);
	if (ret < 0)
		return ret;

	return 1;
}

int gtwif_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	int ret;
	struct gtwif *g = n->_vd;
	
	struct sample *smp = smps[0];
	
	uint32_t count = MIN(g->out.count, smp->length);
	uint32_t     *vals = (uint32_t *) &smp->data;
	uint32_t *old_vals = (uint32_t *) (g->last ? &g->last->data : NULL);
	
	ret = gtwif_cmd_modify_many(g->sd, count, g->out.addresses, vals, old_vals);
	if (ret < 0)
		return ret;
	
	sample_get(smp);
	sample_put(g->last);
	
	g->last = smp;

	return 1;
}

static struct plugin p = {
	.name		= "gtwif",
	.description	= "GTWIF - RSCAD protocol for RTDS",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 1,
		.size		= sizeof(struct gtwif),
		.parse		= gtwif_parse,
		.print		= gtwif_print,
		.start		= gtwif_start,
		.stop		= gtwif_stop,
		.read		= gtwif_read,
		.write		= gtwif_write,
		.instances	= LIST_INIT()
	}
};

REGISTER_PLUGIN(&p)