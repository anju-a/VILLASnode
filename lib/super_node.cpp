/** The super node object holding the state of the application.
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

#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

#include <villas/config.h>
#include <villas/super_node.h>
#include <villas/node.h>
#include <villas/path.h>
#include <villas/utils.h>
#include <villas/list.h>
#include <villas/hook.h>
#include <villas/advio.h>
#include <villas/web.h>
#include <villas/log.h>
#include <villas/api.h>
#include <villas/plugin.h>
#include <villas/memory.h>
#include <villas/config.h>
#include <villas/config_helper.h>

#include <villas/kernel/rt.h>

using namespace villas::node;

SuperNode::SuperNode() :
	priority(0),
	affinity(0),
	hugepages(DEFAULT_NR_HUGEPAGES),
	stats(0),
	state(STATE_INITIALIZED)
{
	list_init(&nodes);
	list_init(&paths);
	list_init(&plugins);

	name = (char *) alloc(128);
	gethostname(name, 128);

	init();
}

int SuperNode::init()
{
	int ret;

	ret = log_init(&log, V, LOG_ALL);
	if (ret)
		return ret;

	ret = rt_init(priority, affinity);
	if (ret)
		return ret;
	
	ret = memory_init(hugepages);
	if (ret)
		return ret;

#ifdef WITH_API
	ret = api_init(&api);//, this); // @todo: port to C++
	if (ret)
		return ret;
#endif /* WITH_API */

#ifdef WITH_WEB
	ret = web_init(&web, &api);
	if (ret)
		return ret;
#endif /* WITH_WEB */

	return 0;
}

int SuperNode::parseUri(const char *u)
{
	json_error_t err;

	info("Parsing configuration");

	if (u) { INDENT
		FILE *f;
		AFILE *af;

		/* Via stdin */
		if (!strcmp("-", u)) {
			info("Reading configuration from stdin");

			af = NULL;
			f = stdin;
		}
		else {
			info("Reading configuration from URI: %s", u);

			af = afopen(u, "r");
			if (!af)
				error("Failed to open configuration from: %s", u);

			f = af->file;
		}

		/* Parse config */
		json = json_loadf(f, 0, &err);
		if (json == NULL) {
#ifdef LIBCONFIG_FOUND
			int ret;

			config_t cfg;
			config_setting_t *json_root = NULL;

			warn("Failed to parse JSON configuration. Re-trying with old libconfig format.");
			{ INDENT
				warn("Please consider migrating to the new format using the 'conf2json' command.");
			}

			config_init(&cfg);
			config_set_auto_convert(&cfg, 1);

			/* Setup libconfig include path.
			 * This is only supported for local files */
			if (access(uri, F_OK) != -1) {
				char *cpy = strdup(uri);

				config_set_include_dir(&cfg, dirname(cpy));

				free(cpy);
			}

			if (af)
				arewind(af);
			else
				rewind(f);

			ret = config_read(&cfg, f);
			if (ret != CONFIG_TRUE) {
				{ INDENT
					warn("conf: %s in %s:%d", config_error_text(&cfg), uri, config_error_line(&cfg));
					warn("json: %s in %s:%d:%d", err.text, err.source, err.line, err.column);
				}
				error("Failed to parse configuration");
			}

			json_root = config_root_setting(&cfg);

			json = config_to_json(json_root);
			if (json == NULL)
				error("Failed to convert JSON to configuration file");

			config_destroy(&cfg);
#else
			jerror(&err, "Failed to parse configuration file");
#endif /* LIBCONFIG_FOUND */
		}

		/* Close configuration file */
		if (af)
			afclose(af);
		else if (f != stdin)
			fclose(f);

		uri = strdup(u);

		return parseJson(json);
	}
	else { INDENT
		warn("No configuration file specified. Starting unconfigured. Use the API to configure this instance.");
	}

	return 0;
}

int SuperNode::parseJson(json_t *j)
{
	int ret;
	const char *nme = NULL;

	assert(state != STATE_STARTED);

	json_t *json_nodes = NULL;
	json_t *json_paths = NULL;
	json_t *json_plugins = NULL;
	json_t *json_logging = NULL;
	json_t *json_web = NULL;

	json_error_t err;

	ret = json_unpack_ex(j, &err, 0, "{ s?: o, s?: o, s?: o, s?: o, s?: o, s?: i, s?: i, s?: i, s?: F, s?: s }",
		"http", &json_web,
		"logging", &json_logging,
		"plugins", &json_plugins,
		"nodes", &json_nodes,
		"paths", &json_paths,
		"hugepages", &hugepages,
		"affinity", &affinity,
		"priority", &priority,
		"stats", &stats,
		"name", &nme
	);
	if (ret)
		jerror(&err, "Failed to parse global configuration");

	if (nme) {
		name = (char *) realloc(name, strlen(nme)+1);
		sprintf(name, "%s", nme);
	}

#ifdef WITH_WEB
	if (json_web)
		web_parse(&web, json_web);
#endif /* WITH_WEB */

	if (json_logging)
		log_parse(&log, json_logging);

	/* Parse plugins */
	if (json_plugins) {
		if (!json_is_array(json_plugins))
			error("Setting 'plugins' must be a list of strings");

		size_t index;
		json_t *json_plugin;
		json_array_foreach(json_plugins, index, json_plugin) {
			struct plugin *p = (struct plugin *) alloc(sizeof(struct plugin));

			ret = plugin_init(p);
			if (ret)
				error("Failed to initialize plugin");

			ret = plugin_parse(p, json_plugin);
			if (ret)
				error("Failed to parse plugin");

			list_push(&plugins, p);
		}
	}

	/* Parse nodes */
	if (json_nodes) {
		if (!json_is_object(json_nodes))
			error("Setting 'nodes' must be a group with node name => group mappings.");

		const char *name;
		json_t *json_node;
		json_object_foreach(json_nodes, name, json_node) {
			struct node_type *nt;
			const char *type;

			ret = json_unpack_ex(json_node, &err, 0, "{ s: s }", "type", &type);
			if (ret)
				jerror(&err, "Failed to parse node");

			nt = node_type_lookup(type);
			if (!nt)
				error("Invalid node type: %s", type);

			struct node *n = (struct node *) alloc(sizeof(struct node));

			ret = node_init(n, nt);
			if (ret)
				error("Failed to initialize node");

			ret = node_parse(n, json_node, name);
			if (ret)
				error("Failed to parse node");

			list_push(&nodes, n);
		}
	}

	/* Parse paths */
	if (json_paths) {
		if (!json_is_array(json_paths))
			warn("Setting 'paths' must be a list.");

		size_t index;
		json_t *json_path;
		json_array_foreach(json_paths, index, json_path) {
			struct path *p = (struct path *) alloc(sizeof(struct path));

			ret = path_init(p);
			if (ret)
				error("Failed to initialize path");

			ret = path_parse(p, json_path, &nodes);
			if (ret)
				error("Failed to parse path");

			list_push(&paths, p);

			if (p->reverse) {
				struct path *r = (struct path *) alloc(sizeof(struct path));

				ret = path_init(r);
				if (ret)
					error("Failed to init path");

				ret = path_reverse(p, r);
				if (ret)
					error("Failed to reverse path %s", path_name(p));

				list_push(&paths, r);
			}
		}
	}

	json = j;

	state = STATE_PARSED;

	return 0;
}

int SuperNode::check()
{
	int ret;

	assert(state == STATE_PARSED || state == STATE_PARSED || state == STATE_CHECKED);

	for (size_t i = 0; i < list_length(&nodes); i++) {
		struct node *n = (struct node *) list_at(&nodes, i);

		ret = node_check(n);
		if (ret)
			error("Invalid configuration for node %s", node_name(n));
	}

	for (size_t i = 0; i < list_length(&paths); i++) {
		struct path *p = (struct path *) list_at(&paths, i);

		ret = path_check(p);
		if (ret)
			error("Invalid configuration for path %s", path_name(p));
	}

	state = STATE_CHECKED;

	return 0;
}

int SuperNode::start()
{
	int ret;

	assert(state == STATE_CHECKED);

	memory_init(hugepages);
	rt_init(priority, affinity);

	log_open(&log);
#ifdef WITH_API
	api_start(&api);
#endif
#ifdef WITH_WEB
	web_start(&web);
#endif

	info("Starting node-types");
	for (size_t i = 0; i < list_length(&nodes); i++) { INDENT
		struct node *n = (struct node *) list_at(&nodes, i);

		ret = node_type_start(n->_vt);//, this); // @todo: port to C++
		if (ret)
			error("Failed to start node-type: %s", plugin_name(n->_vt));
	}

	info("Starting nodes");
	for (size_t i = 0; i < list_length(&nodes); i++) { INDENT
		struct node *n = (struct node *) list_at(&nodes, i);

		int refs = list_count(&paths, (cmp_cb_t) path_uses_node, n);
		if (refs > 0) { INDENT
			ret = node_start(n);
			if (ret)
				error("Failed to start node: %s", node_name(n));
		}
		else
			warn("No path is using the node %s. Skipping...", node_name(n));
	}

	info("Starting paths");
	for (size_t i = 0; i < list_length(&paths); i++) { INDENT
		struct path *p = (struct path *) list_at(&paths, i);

		if (p->enabled) { INDENT
			ret = path_init2(p);
			if (ret)
				error("Failed to start path: %s", path_name(p));

			ret = path_start(p);
			if (ret)
				error("Failed to start path: %s", path_name(p));
		}
		else
			warn("Path %s is disabled. Skipping...", path_name(p));
	}

	state = STATE_STARTED;

	return 0;
}

int SuperNode::stop()
{
	int ret;

	if (stats > 0)
		stats_print_footer(STATS_FORMAT_HUMAN);

	info("Stopping paths");
	for (size_t i = 0; i < list_length(&paths); i++) { INDENT
		struct path *p = (struct path *) list_at(&paths, i);

		ret = path_stop(p);
		if (ret)
			error("Failed to stop path: %s", path_name(p));
	}

	info("Stopping nodes");
	for (size_t i = 0; i < list_length(&nodes); i++) { INDENT
		struct node *n = (struct node *) list_at(&nodes, i);

		ret = node_stop(n);
		if (ret)
			error("Failed to stop node: %s", node_name(n));
	}

	info("Stopping node-types");
	for (size_t i = 0; i < list_length(&plugins); i++) { INDENT
		struct plugin *p = (struct plugin *) list_at(&plugins, i);

		if (p->type == PLUGIN_TYPE_NODE) {
			ret = node_type_stop(&p->node);
			if (ret)
				error("Failed to stop node-type: %s", plugin_name(p));
		}
	}

#ifdef WITH_API
	api_stop(&api);
#endif
#ifdef WITH_WEB
	web_stop(&web);
#endif
	log_close(&log);

	state = STATE_STOPPED;

	return 0;
}

void SuperNode::run()
{
#ifdef WITH_HOOKS
	int ret;

	if (stats > 0) {
		stats_print_header(STATS_FORMAT_HUMAN);

		struct task t;

		ret = task_init(&t, 1.0 / stats, CLOCK_REALTIME);
		if (ret)
			error("Failed to create stats timer");

		for (;;) {
			task_wait(&t);

			for (size_t i = 0; i < list_length(&paths); i++) {
				struct path *p = (struct path *) list_at(&paths, i);

				if (p->state != STATE_STARTED)
					continue;

				for (size_t j = 0; j < list_length(&p->hooks); j++) {
					struct hook *h = (struct hook *) list_at(&p->hooks, j);

					hook_periodic(h);
				}
			}

			for (size_t i = 0; i < list_length(&nodes); i++) {
				struct node *n = (struct node *) list_at(&nodes, i);

				if (n->state != STATE_STARTED)
					continue;

				for (size_t j = 0; j < list_length(&n->in.hooks); j++) {
					struct hook *h = (struct hook *) list_at(&n->in.hooks, j);

					hook_periodic(h);
				}

				for (size_t j = 0; j < list_length(&n->out.hooks); j++) {
					struct hook *h = (struct hook *) list_at(&n->out.hooks, j);

					hook_periodic(h);
				}
			}
		}
	}
	else
#endif /* WITH_HOOKS */
		for (;;) pause();
}

SuperNode::~SuperNode()
{
	assert(state == STATE_STOPPED);

	list_destroy(&plugins, (dtor_cb_t) plugin_destroy, false);
	list_destroy(&paths,   (dtor_cb_t) path_destroy, true);
	list_destroy(&nodes,   (dtor_cb_t) node_destroy, true);

#ifdef WITH_WEB
	web_destroy(&web);
#endif /* WITH_WEB */
#ifdef WITH_API
	api_destroy(&api);
#endif /* WITH_API */

	json_decref(json);
	log_destroy(&log);

	if (name)
		free(name);
}
