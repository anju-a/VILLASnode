#!/bin/bash
#
# Run integration tests
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
##################################################################################

SCRIPT=$(realpath ${BASH_SOURCE[0]})
SCRIPTPATH=$(dirname $SCRIPT)

export SRCDIR=$(realpath ${SCRIPTPATH}/..)
export BUILDDIR=${SRCDIR}/build/release
export LOGDIR=${BUILDDIR}/tests/integration
export PATH=${BUILDDIR}:${PATH}

# Default values
VERBOSE=0
FILTER='*'

export NUM_SAMPLES=100

# Parse command line arguments
while getopts ":f:v" OPT; do
	case ${OPT} in
		f)
			FILTER=${OPTARG}
			;;
		v)
			VERBOSE=1
			;;
		\?)
			echo "Invalid option: -${OPTARG}" >&2
			;;
		:)
			echo "Option -$OPTARG requires an argument." >&2
			exit 1
			;;
	esac
done

TESTS=${SRCDIR}/tests/integration/${FILTER}.sh

# Preperations
mkdir -p ${LOGDIR}

NUM_PASS=0
NUM_FAIL=0

# Preamble
echo -e "Starting integration tests for VILLASnode/fpga:\n"

for TEST in ${TESTS}; do
	TESTNAME=$(basename -s .sh ${TEST})

	# Run test
	if (( ${VERBOSE} == 0 )); then
		${TEST} &> ${LOGDIR}/${TESTNAME}.log
	else
		${TEST}
	fi
	
	RC=$?

	if (( $RC != 0 )); then
		echo -e "\e[31m[Fail] \e[39m ${TESTNAME} with code $RC"
		NUM_FAIL=$((${NUM_FAIL} + 1))
	else
		echo -e "\e[32m[Pass] \e[39m ${TESTNAME}"
		NUM_PASS=$((${NUM_PASS} + 1))
	fi
done

# Show summary
if (( ${NUM_FAIL} > 0 )); then
	echo -e "\nSummary: ${NUM_FAIL} of $((${NUM_FAIL} + ${NUM_PASS})) tests failed."
	exit 1
else
	echo -e "\nSummary: all tests passed!"
	exit 0
fi