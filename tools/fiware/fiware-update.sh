#!/bin/bash
#
# Update context of FIRWARE ORION Context Broker.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASnode
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
##################################################################################

(curl http://46.101.131.212:1026/v1/updateContext -s -S \
	--header 'Content-Type: application/json' \
	--header 'Accept: application/json' -d @- | python -mjson.tool) <<EOF
{
    "contextElements": [
        {
            "type": "type_one",
            "isPattern": "false",
            "id": "rtds_sub3",
            "attributes": [
                {
                    "name": "v1",
                    "type": "float",
                    "value": "26.5"
                },
                {
                    "name": "v2",
                    "type": "float",
                    "value": "763"
                }
            ]
        }
    ],
    "updateAction": "UPDATE"
}
EOF
