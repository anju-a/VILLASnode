#!/bin/bash
#
# Integration loopback test for villas-pipe.
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

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})
source ${SCRIPTPATH}/../../tools/integration-tests-helper.sh

CONFIG_FILE=$(mktemp)
CONFIG_FILE2=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

NUM_SAMPLES=${NUM_SAMPLES:-100}

cat > ${CONFIG_FILE} << EOF
{
	"nodes" : {
		"node1" : {
			"type" : "websocket",

			"destinations" : [
				"ws://127.0.0.1:81/node2"
			]
		}
	}
}
EOF

cat > ${CONFIG_FILE2} << EOF
{
	"http" : {
		"port" : 81
	},
	"nodes" : {
		"node2" : {
			"type" : "websocket"
		}
	}
}
EOF

# Generate test data
VILLAS_LOG_PREFIX=$(colorize "[Signal]") \
villas-signal random -l ${NUM_SAMPLES} -n > ${INPUT_FILE}

VILLAS_LOG_PREFIX=$(colorize "[Recv]  ") \
villas-pipe -r -d 15 -l ${NUM_SAMPLES} ${CONFIG_FILE2} node2 > ${OUTPUT_FILE} &

VILLAS_LOG_PREFIX=$(colorize "[Send]  ") \
villas-pipe -s -d 15 ${CONFIG_FILE} node1 < ${INPUT_FILE}

wait $!

# Compare data
villas-test-cmp ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

rm ${OUTPUT_FILE} ${INPUT_FILE} ${CONFIG_FILE} ${CONFIG_FILE2}

exit $RC
