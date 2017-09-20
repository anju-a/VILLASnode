/** Path source.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/utils.h>
#include <villas/bitset.h>
#include <villas/mapping.h>
#include <villas/path.h>
#include <villas/path_source.h>
#include <villas/path_worker.h>

extern struct path_worker *worker;

struct path_source * path_source_create()
{
	struct path_source *ps;

	ps = alloc(sizeof(struct path_source));
	if (!ps)
		return NULL;

	ps->masked = false;

	ps->mappings.state = STATE_DESTROYED;

	list_init(&ps->mappings);

	return ps;
}

int path_source_init(struct path_source *ps, struct path *p, int index)
{
	int ret;

	ps->path = p;
	ps->index = index;

	if (ps->masked)
		bitset_set(&p->mask, ps->index);

	/* Calc sample length of path and initialize bitset */
	for (size_t i = 0; i < list_length(&ps->mappings); i++) {
		struct mapping_entry *me = (struct mapping_entry *) list_at(&ps->mappings, i);

		int len = me->length;
		int off = me->offset;

		if (off + len > p->samplelen)
			p->samplelen = off + len;
	}

	ret = path_worker_add_source(worker, ps);
	if (ret)
		return ret;

	return 0;
}

int path_source_destroy(struct path_source *ps)
{
	int ret;

	ret = list_destroy(&ps->mappings, NULL, true);
	if (ret)
		return ret;

	return 0;
}

void path_source_mux(struct path_source *ps, struct sample *smps[], unsigned cnt)
{
	bitset_set(&ps->path->received, ps->index);

	if (ps->path->mode == PATH_MODE_ALL) { /* Mux only last sample and discard others */
		smps += cnt - 1;
		cnt = 1;
	}

	struct sample *previous, *muxed[cnt];

	for (unsigned i = 0; i < cnt; i++) {
		do {
			previous = atomic_load(&ps->path->previous);

			muxed[i] = sample_clone(previous);
			muxed[i]->sequence++;

			mapping_remap(&ps->mappings, muxed[i], smps[i], NULL);
		} while (atomic_compare_exchange_weak(&ps->path->previous, &previous, muxed[i]));
	}

	if (bitset_test(&ps->path->mask, ps->index)) {
		/* Check if we received an update from all nodes/ */
		if ((ps->path->mode == PATH_MODE_ANY) ||
		    (ps->path->mode == PATH_MODE_ALL && !bitset_cmp(&ps->path->mask, &ps->path->received)))
		{
			path_process(ps->path, muxed, cnt);

			/* Reset bitset of updated nodes */
			bitset_clear_all(&ps->path->received);
		}
	}

	sample_put_many(muxed, cnt);
}
