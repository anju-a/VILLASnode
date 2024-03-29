#!/bin/bash
#
# Integration test for print hook.
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

INPUT_FILE=$(mktemp)
OUTPUT_FILE1=$(mktemp)
OUTPUT_FILE2=$(mktemp)

NUM_SAMPLES=${NUM_SAMPLES:-100}

# Prepare some test data
villas-signal random -v 1 -r 10 -l ${NUM_SAMPLES} -n > ${INPUT_FILE}

villas-hook print -o format=villas-human -o output=${OUTPUT_FILE1} < ${INPUT_FILE} > ${OUTPUT_FILE2}

# Compare only the data values
villas-test-cmp ${OUTPUT_FILE1} ${INPUT_FILE} && \
villas-test-cmp ${OUTPUT_FILE2} ${INPUT_FILE}
RC=$?

rm -f ${INPUT_FILE} ${OUTPUT_FILE1} ${OUTPUT_FILE2}

exit $RC
