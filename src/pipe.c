/** Receive messages from server snd print them on stdout.
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
 *
 * @addtogroup tools Test and debug tools
 * @{
 *********************************************************************************/

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include <villas/super_node.h>
#include <villas/utils.h>
#include <villas/node.h>
#include <villas/timing.h>
#include <villas/pool.h>
#include <villas/io.h>
#include <villas/kernel/rt.h>
#include <villas/plugin.h>
#include <villas/config_helper.h>

#include <villas/nodes/websocket.h>

#include "config.h"

static struct super_node sn = { .state = STATE_DESTROYED }; /**< The global configuration */
static struct io io = { .state = STATE_DESTROYED };

static struct dir {
	struct pool pool;
	pthread_t thread;
	bool enabled;
	int limit;
} sendd, recvv;

struct node *node;

static void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	if (signal == SIGALRM)
		info("Reached timeout. Terminating...");

	if (recvv.enabled) {
		pthread_cancel(recvv.thread);
		pthread_join(recvv.thread, NULL);
	}

	if (sendd.enabled) {
		pthread_cancel(sendd.thread);
		pthread_join(sendd.thread, NULL);
	}

	super_node_stop(&sn);

	if (recvv.enabled)
		pool_destroy(&recvv.pool);

	if (sendd.enabled)
		pool_destroy(&sendd.pool);

	super_node_destroy(&sn);

	info(CLR_GRN("Goodbye!"));
	exit(EXIT_SUCCESS);
}

static void usage()
{
	printf("Usage: villas-pipe [OPTIONS] CONFIG NODE\n");
	printf("  CONFIG  path to a configuration file\n");
	printf("  NODE    the name of the node to which samples are sent and received from\n");
	printf("  OPTIONS are:\n");
	printf("    -f FMT           set the format\n");
	printf("    -d LVL           set debug log level to LVL\n");
	printf("    -o OPTION=VALUE  overwrite options in config file\n");
	printf("    -x               swap read / write endpoints\n");
	printf("    -s               only read data from stdin and send it to node\n");
	printf("    -r               only read data from node and write it to stdout\n");
	printf("    -t NUM           terminate after NUM seconds\n");
	printf("    -L NUM           terminate after NUM samples sent\n");
	printf("    -l NUM           terminate after NUM samples received\n");
	printf("    -V               show the version of the tool\n\n");

	print_copyright();
}

static void * send_loop(void *ctx)
{
	unsigned last_sequenceno = 0;
	int ret, scanned, sent, ready, cnt = 0;
	struct sample *smps[node->vectorize];

	/* Initialize memory */
	ret = pool_init(&sendd.pool, LOG2_CEIL(node->vectorize), SAMPLE_LEN(DEFAULT_SAMPLELEN), &memtype_hugepage);
	if (ret < 0)
		error("Failed to allocate memory for receive pool.");

	while (!io_eof(&io)) {
		ready = sample_alloc_many(&sendd.pool, smps, node->vectorize);
		if (ret < 0)
			error("Failed to get %u samples out of send pool (%d).", node->vectorize, ret);
		else if (ready < node->vectorize)
			warn("Send pool underrun");

		scanned = io_scan(&io, smps, ready);
		if (scanned < 0) {
			continue;
			warn("Failed to read samples from stdin");
		}
		else if (scanned == 0)
			continue;

		/* Fill in missing sequence numbers */
		for (int i = 0; i < scanned; i++) {
			if (smps[i]->flags & SAMPLE_HAS_SEQUENCE)
				last_sequenceno = smps[i]->sequence;
			else
				smps[i]->sequence = last_sequenceno++;
		}

		sent = node_write(node, smps, scanned);
		if (sent < 0) {
			warn("Failed to sent samples to node %s: reason=%d", node_name(node), sent);
			continue;
		}

		sample_put_many(smps, ready);

		cnt += sent;
		if (sendd.limit > 0 && cnt >= sendd.limit)
			goto leave;

		pthread_testcancel();
	}

leave:	if (io_eof(&io)) {
		if (recvv.limit < 0) {
			info("Reached end-of-file. Terminating...");
			killme(SIGTERM);
		}
		else
			info("Reached end-of-file. Wait for receive side...");
	}
	else {
		info("Reached send limit. Terminating...");
		killme(SIGTERM);
	}

	return NULL;
}

static void * recv_loop(void *ctx)
{
	int recv, ret, cnt = 0, ready = 0;
	struct sample *smps[node->vectorize];

	/* Initialize memory */
	ret = pool_init(&recvv.pool, LOG2_CEIL(node->vectorize), SAMPLE_LEN(DEFAULT_SAMPLELEN), &memtype_hugepage);
	if (ret < 0)
		error("Failed to allocate memory for receive pool.");

	for (;;) {
		ready = sample_alloc_many(&recvv.pool, smps, node->vectorize);
		if (ready < 0)
			error("Failed to allocate %u samples from receive pool.", node->vectorize);
		else if (ready < node->vectorize)
			warn("Receive pool underrun");

		recv = node_read(node, smps, ready);
		if (recv < 0) {
			warn("Failed to receive samples from node %s: reason=%d", node_name(node), recv);
			continue;
		}
		else if (recv == 0)
			continue;

		io_print(&io, smps, recv);

		sample_put_many(smps, ready);

		cnt += recv;
		if (recvv.limit > 0 && cnt >= recvv.limit)
			goto leave;

		pthread_testcancel();
	}

leave:	info("Reached receive limit. Terminating...");
	killme(SIGTERM);

	return NULL;
}

int main(int argc, char *argv[])
{
	int ret, level = V, timeout = 0;
	bool reverse = false;
	char *format = "villas-human";

	sendd = recvv = (struct dir) {
		.enabled = true,
		.limit = -1
	};

	json_t *cfg_cli = json_object();

	char c, *endptr;
	while ((c = getopt(argc, argv, "Vhxrsd:l:L:t:f:o:")) != -1) {
		switch (c) {
			case 'V':
				print_version();
				exit(EXIT_SUCCESS);
			case 'f':
				format = optarg;
				break;
			case 'x':
				reverse = true;
				break;
			case 's':
				recvv.enabled = false; // send only
				break;
			case 'r':
				sendd.enabled = false; // receive only
				break;
			case 'd':
				level = strtoul(optarg, &endptr, 10);
				goto check;
			case 'l':
				recvv.limit = strtoul(optarg, &endptr, 10);
				goto check;
			case 'L':
				sendd.limit = strtoul(optarg, &endptr, 10);
				goto check;
			case 't':
				timeout = strtoul(optarg, &endptr, 10);
				goto check;
			case 'o':
				ret = json_object_extend_str(cfg_cli, optarg);
				if (ret)
					error("Invalid option: %s", optarg);
				break;
			case 'h':
			case '?':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}

		continue;

check:		if (optarg == endptr)
			error("Failed to parse parse option argument '-%c %s'", c, optarg);
	}

	if (argc != optind + 2) {
		usage();
		exit(EXIT_FAILURE);
	}

	char *configfile = argv[optind];
	char *nodestr    = argv[optind+1];
	struct plugin *p;

	ret = log_init(&sn.log, level, LOG_ALL);
	if (ret)
		error("Failed to initialize log");

	ret = log_start(&sn.log);
	if (ret)
		error("Failed to start log");

	ret = signals_init(quit);
	if (ret)
		error("Failed to initialize signals");

	ret = super_node_init(&sn);
	if (ret)
		error("Failed to initialize super-node");

	ret = super_node_parse_uri(&sn, configfile);
	if (ret)
		error("Failed to parse configuration");

	ret = memory_init(sn.hugepages);
	if (ret)
		error("Failed to initialize memory");

	ret = rt_init(sn.priority, sn.affinity);
	if (ret)
		error("Failed to initalize real-time");

	p = plugin_lookup(PLUGIN_TYPE_IO, format);
	if (!p)
		error("Invalid format: %s", format);

	ret = io_init(&io, &p->io, SAMPLE_HAS_ALL);
	if (ret)
		error("Failed to initialize IO");

	ret = io_open(&io, NULL);
	if (ret)
		error("Failed to open IO");

	node = list_lookup(&sn.nodes, nodestr);
	if (!node)
		error("Node '%s' does not exist!", nodestr);

#ifdef WITH_WEBSOCKET
	/* Only start web subsystem if villas-pipe is used with a websocket node */
	if (node->_vt->start == websocket_start) {
		web_start(&sn.web);
		api_start(&sn.api);
	}
#endif

	if (reverse)
		node_reverse(node);

	ret = node_type_start(node->_vt, &sn);
	if (ret)
		error("Failed to intialize node type: %s", node_type_name(node->_vt));

	ret = node_check(node);
	if (ret)
		error("Invalid node configuration");

	ret = node_start(node);
	if (ret)
		error("Failed to start node %s: reason=%d", node_name(node), ret);

	/* Start threads */
	if (recvv.enabled)
		pthread_create(&recvv.thread, NULL, recv_loop, NULL);

	if (sendd.enabled)
		pthread_create(&sendd.thread, NULL, send_loop, NULL);

	alarm(timeout);

	for (;;)
		pause();

	return 0;
}

/** @} */
