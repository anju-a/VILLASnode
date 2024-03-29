/** Setup interface queuing desciplines for network emulation
 *
 * We use the firewall mark to apply individual netem qdiscs
 * per node. Every node uses an own BSD socket.
 * By using so SO_MARK socket option (see socket(7))
 * we can classify traffic originating from a node seperately.
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

/** @addtogroup fpga Kernel @{ */

#pragma once

#include <stdint.h>

#include <netlink/route/qdisc.h>
#include <netlink/route/classifier.h>

#include <jansson.h>

typedef uint32_t tc_hdl_t;

struct interface;

/** Parse network emulator (netem) settings.
 *
 * @param cfg A jansson object containing the settings.
 * @param[out] ne A pointer to a libnl3 qdisc object where setting will be written to.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int tc_parse(struct rtnl_qdisc **ne, json_t *cfg);

/** Print network emulator (netem) setting into buffer.
 *
 * @param tc A pointer to the libnl3 qdisc object where settings will be read from.
 * @return A pointer to a string which must be freed() by the caller.
 */
char * tc_print(struct rtnl_qdisc *ne);

/** Remove all queuing disciplines and filters.
 *
 * @param i The interface
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int tc_reset(struct interface *i);

/** Create a priority (prio) queueing discipline.
 *
 * @param i[in] The interface
 * @param qd[in,out] The libnl3 object of the new prio qdisc.
 * @param handle[in] The handle for the new qdisc
 * @param parent[in] Make this qdisc a child of this class
 * @param bands[in] The number of classes for this new qdisc
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int tc_prio(struct interface *i, struct rtnl_qdisc **qd, tc_hdl_t handle, tc_hdl_t, int bands);

/** Add a new network emulator (netem) discipline.
 *
 * @param i[in] The interface to which this qdisc will be added.
 * @param qd[in,out] The libnl3 object of the new prio qdisc.
 * @param handle[in] The handle of the new qdisc.
 * @param parent[in] Make this qdisc a child of this class
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int tc_netem(struct interface *i, struct rtnl_qdisc **qd, tc_hdl_t handle, tc_hdl_t parent);

/** Add a new filter based on the netfilter mark.
 *
 * @param i The interface to which this classifier is applied to.
 * @param cls[in,out] The libnl3 object of the new prio qdisc.
 * @param flowid The destination class for matched traffic
 * @param mark The netfilter firewall mark (sometime called 'fwmark')
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
*/
int tc_mark(struct interface *i, struct rtnl_cls **cls, tc_hdl_t flowid, uint32_t mark);

/** @} */
