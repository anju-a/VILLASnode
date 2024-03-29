/** Node-type for shared memory communication.
 *
 * @file
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
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

#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kernel/kernel.h"
#include "log.h"
#include "shmem.h"
#include "nodes/shmem.h"
#include "plugin.h"
#include "timing.h"
#include "utils.h"

int shmem_parse(struct node *n, json_t *cfg)
{
	struct shmem *shm = (struct shmem *) n->_vd;
	const char *val;

	int ret;
	json_t *json_exec = NULL;
	json_error_t err;

	/* Default values */
	shm->conf.queuelen = MAX(DEFAULT_SHMEM_QUEUELEN, n->vectorize);
	shm->conf.samplelen = n->samplelen;
	shm->conf.polling = false;
	shm->exec = NULL;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: s, s: s, s?: i, s?: b, s?: o }",
		"out_name", &shm->out_name,
		"in_name", &shm->in_name,
		"queuelen", &shm->conf.queuelen,
		"polling", &shm->conf.polling,
		"exec", &json_exec
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (json_exec) {
		if (!json_is_array(json_exec))
			error("Setting 'exec' of node %s must be a JSON array of strings", node_name(n));

		shm->exec = alloc(sizeof(char *) * (json_array_size(json_exec) + 1));

		size_t index;
		json_t *json_val;
		json_array_foreach(json_exec, index, json_val) {
			val = json_string_value(json_exec);
			if (!val)
				error("Setting 'exec' of node %s must be a JSON array of strings", node_name(n));

			shm->exec[index] = strdup(val);
		}

		shm->exec[index] = NULL;
	}

	return 0;
}

int shmem_open(struct node *n)
{
	struct shmem *shm = (struct shmem *) n->_vd;
	int ret;

	if (shm->exec) {
		ret = spawn(shm->exec[0], shm->exec);
		if (!ret)
			serror("Failed to spawn external program");
	}

	ret = shmem_int_open(shm->out_name, shm->in_name, &shm->intf, &shm->conf);
	if (ret < 0)
		serror("Opening shared memory interface failed");

	return 0;
}

int shmem_close(struct node *n)
{
	struct shmem* shm = (struct shmem *) n->_vd;

	return shmem_int_close(&shm->intf);
}

int shmem_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct shmem *shm = (struct shmem *) n->_vd;
	int recv;
	struct sample *shared_smps[cnt];

	do {
		recv = shmem_int_read(&shm->intf, shared_smps, cnt);
	} while (recv == 0);

	if (recv < 0) {
		/* This can only really mean that the other process has exited, so close
		 * the interface to make sure the shared memory object is unlinked */
		shmem_int_close(&shm->intf);
		return recv;
	}

	sample_copy_many(smps, shared_smps, recv);
	sample_put_many(shared_smps, recv);

	return recv;
}

int shmem_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct shmem *shm = (struct shmem *) n->_vd;
	struct sample *shared_smps[cnt]; /* Samples need to be copied to the shared pool first */
	int avail, pushed, copied;

	avail = sample_alloc_many(&shm->intf.write.shared->pool, shared_smps, cnt);
	if (avail != cnt)
		warn("Pool underrun for shmem node %s", shm->out_name);

	copied = sample_copy_many(shared_smps, smps, avail);

	for (int i = 0; i < copied; i++) {
		/* Since the node isn't in shared memory, the source can't be accessed */
		shared_smps[i]->source = NULL;
		shared_smps[i]->flags &= ~SAMPLE_HAS_SOURCE;
	}

	pushed = shmem_int_write(&shm->intf, shared_smps, avail);
	if (pushed != avail)
		warn("Outgoing queue overrun for node %s", node_name(n));

	return pushed;
}

char * shmem_print(struct node *n)
{
	struct shmem *shm = (struct shmem *) n->_vd;
	char *buf = NULL;

	strcatf(&buf, "out_name=%s, in_name=%s, queuelen=%d, polling=%s",
		shm->out_name, shm->in_name, shm->conf.queuelen, shm->conf.polling ? "yes" : "no");

	if (shm->exec) {
		strcatf(&buf, ", exec='");

		for (int i = 0; shm->exec[i]; i++)
			strcatf(&buf, shm->exec[i+1] ? "%s " : "%s", shm->exec[i]);

		strcatf(&buf, "'");
	}

	return buf;
}

static struct plugin p = {
	.name = "shmem",
	.description = "POSIX shared memory interface with external processes",
	.type = PLUGIN_TYPE_NODE,
	.node = {
		.vectorize = 0,
		.size	= sizeof(struct shmem),
		.parse	= shmem_parse,
		.print	= shmem_print,
		.start	= shmem_open,
		.stop	= shmem_close,
		.read	= shmem_read,
		.write	= shmem_write
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
