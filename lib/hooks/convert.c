/** Convert hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

/** @addtogroup hooks Hook functions
 * @{
 */

#include "hook.h"
#include "plugin.h"

static int hook_convert(struct hook *h, int when, struct hook_info *j)
{
	struct {
		enum {
			TO_FIXED,
			TO_FLOAT
		} mode;
	} *private = hook_storage(h, when, sizeof(*private), NULL, NULL);

	switch (when) {
		case HOOK_PARSE:
			if (!h->parameter)
				error("Missing parameter for hook: '%s'", plugin_name(h->_vt));

			if      (!strcmp(h->parameter, "fixed"))
				private->mode = TO_FIXED;
			else if (!strcmp(h->parameter, "float"))
				private->mode = TO_FLOAT;
			else
				error("Invalid parameter '%s' for hook 'convert'", h->parameter);
			break;
		
		case HOOK_READ:
			for (int i = 0; i < j->count; i++) {
				for (int k = 0; k < j->samples[i]->length; k++) {
					switch (private->mode) {
						case TO_FIXED: j->samples[i]->data[k].i = j->samples[i]->data[k].f * 1e3; break;
						case TO_FLOAT: j->samples[i]->data[k].f = j->samples[i]->data[k].i; break;
					}
				}
			}
			
			return j->count;
	}

	return 0;
}

static struct plugin p = {
	.name		= "convert",
	.description	= "Convert message from / to floating-point / integer",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.cb	= hook_convert,
		.when	= HOOK_STORAGE | HOOK_READ
	}
};

REGISTER_PLUGIN(&p)

/** @} */