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

#include <arpa/inet.h>
#include <errno.h>

#include "rtds/gtwif.h"

int gtwif_cmd_readlist2(int sd, uint32_t cnt, uint32_t addrs[], uint32_t vals[])
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

int gtwif_cmd_modify(int sd, uint32_t addr, uint32_t val)
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
int gtwif_cmd_modify_many(int sd, uint32_t cnt, uint32_t addrs[], uint32_t vals[], uint32_t old_vals[])
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

int gtwif_cmd_ping(int sd, char *name, size_t namelen, char *casename, size_t casenamelen)
{
	/**< @todo Implement */
	
	return -1;
}

int gtwif_cmd_examine(int sd, uint32_t addr, uint32_t *val)
{
	/**< @todo Implement */
	
	return -1;
}
