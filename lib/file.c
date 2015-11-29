/** Node type: File
 *
 * This file implements the file type for nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <unistd.h>
#include <string.h>

#include "msg.h"
#include "file.h"
#include "utils.h"
#include "timing.h"

int file_reverse(struct node *n)
{
	struct file *f = n->_vd;

	SWAP(f->read, f->write);

	return 0;
}

static char * file_format_name(const char *format, struct timespec *ts)
{
	struct tm tm;
	char *buf = alloc(FILE_MAX_PATHLEN);
	
	/* Convert time */
	gmtime_r(&ts->tv_sec, &tm);

	strftime(buf, FILE_MAX_PATHLEN, format, &tm);

	return buf;
}

static FILE * file_reopen(struct file_direction *dir)
{
	char buf[FILE_MAX_PATHLEN];
	const char *path = buf;

	/* Append chunk number to filename */
	if (dir->chunk >= 0)
		snprintf(buf, FILE_MAX_PATHLEN, "%s_%03u", dir->path, dir->chunk);
	else
		path = dir->path;

	if (dir->handle)
		fclose(dir->handle);

	return fopen(path, dir->mode);
}

static int file_parse_direction(config_setting_t *cfg, struct file *f, int d)
{
	struct file_direction *dir = (d == FILE_READ) ? &f->read : &f->write;

	if (!config_setting_lookup_string(cfg, "path", &dir->fmt))
		return -1;

	if (!config_setting_lookup_string(cfg, "mode", &dir->mode))
		dir->mode = (d == FILE_READ) ? "r" : "w+";

	return 0;
}

int file_parse(struct node *n, config_setting_t *cfg)
{
	struct file *f = n->_vd;
	
	config_setting_t *cfg_in, *cfg_out;
	
	cfg_out = config_setting_get_member(cfg, "out"); 
	if (cfg_out) {
		if (file_parse_direction(cfg_out, f, FILE_WRITE))
			cerror(cfg_out, "Failed to parse output file for node %s", node_name(n));

		/* More write specific settings */
		if (!config_setting_lookup_int(cfg_out, "split", &f->write.split))
			f->write.split = 0; /* Save all samples in a single file */
	}

	cfg_in = config_setting_get_member(cfg, "in");
	if (cfg_in) {
		if (file_parse_direction(cfg_in, f, FILE_READ))
			cerror(cfg_in, "Failed to parse input file for node %s", node_name(n));

		/* More read specific settings */
		if (!config_setting_lookup_bool(cfg_in, "splitted", &f->read.split))
			f->read.split = 0; /* Save all samples in a single file */
		if (!config_setting_lookup_float(cfg_in, "rate", &f->read_rate))
			f->read_rate = 0; /* Disable fixed rate sending. Using timestamps of file instead */
		
		double epoch_flt;
		if (!config_setting_lookup_float(cfg_in, "epoch", &epoch_flt))
			epoch_flt = 0;
	
		f->read_epoch = time_from_double(epoch_flt);

		const char *epoch_mode;
		if (!config_setting_lookup_string(cfg_in, "epoch_mode", &epoch_mode))
			epoch_mode = "direct";

		if (!strcmp(epoch_mode, "direct"))
			f->read_epoch_mode = EPOCH_DIRECT;
		else if (!strcmp(epoch_mode, "wait"))
			f->read_epoch_mode = EPOCH_WAIT;
		else if (!strcmp(epoch_mode, "relative"))
			f->read_epoch_mode = EPOCH_RELATIVE;
		else if (!strcmp(epoch_mode, "absolute"))
			f->read_epoch_mode = EPOCH_ABSOLUTE;
		else
			cerror(cfg_in, "Invalid value '%s' for setting 'epoch_mode'", epoch_mode);
	}

	n->_vd = f;

	return 0;
}

char * file_print(struct node *n)
{
	struct file *f = n->_vd;
	char *buf = NULL;
	
	if (f->read.fmt) {
		const char *epoch_str = NULL;
		switch (f->read_epoch_mode) {
			case EPOCH_DIRECT:	epoch_str = "direct"; break;
			case EPOCH_WAIT:	epoch_str = "wait"; break;
			case EPOCH_RELATIVE:	epoch_str = "relative"; break;
			case EPOCH_ABSOLUTE:	epoch_str = "absolute"; break;
		}
		
		strcatf(&buf, "in=%s, epoch_mode=%s, epoch=%.2f, ",
			f->read.path ? f->read.path : f->read.fmt,
			epoch_str,
			time_to_double(&f->read_epoch)
		);
			
		if (f->read_rate)
			strcatf(&buf, "rate=%.1f, ", f->read_rate);
	}
	
	if (f->write.fmt) {
		strcatf(&buf, "out=%s, mode=%s, ",
			f->write.path ? f->write.path : f->write.fmt,
			f->write.mode
		);
	}
	
	if (f->read_first.tv_sec || f->read_first.tv_nsec)
		strcatf(&buf, "first=%.2f, ", time_to_double(&f->read_first));
	
	if (f->read_offset.tv_sec || f->read_offset.tv_nsec)
		strcatf(&buf, "offset=%.2f, ", time_to_double(&f->read_offset));
	
	if ((f->read_first.tv_sec || f->read_first.tv_nsec) &&
	    (f->read_offset.tv_sec || f->read_offset.tv_nsec)) {
		struct timespec eta, now = time_now();

		eta = time_add(&f->read_first, &f->read_offset);
		eta = time_diff(&now, &eta);

		if (eta.tv_sec || eta.tv_nsec)
		strcatf(&buf, "eta=%.2f sec, ", time_to_double(&eta));
	}
	
	if (strlen(buf) > 2)
		buf[strlen(buf) - 2] = 0;

	return buf;
}

int file_open(struct node *n)
{
	struct file *f = n->_vd;
	
	struct timespec now = time_now();

	if (f->read.fmt) {
		/* Prepare file name */
		f->read.chunk = f->read.split ? 0 : -1;
		f->read.path = file_format_name(f->read.fmt, &now);
		
		/* Open file */
		f->read.handle = file_reopen(&f->read);
		if (!f->read.handle)
			serror("Failed to open file for reading: '%s'", f->read.path);

		/* Create timer */
		f->read_timer = timerfd_create(CLOCK_REALTIME, 0);
		if (f->read_timer < 0)
			serror("Failed to create timer");
		
		/* Arm the timer with a fixed rate */
		if (f->read_rate) {
			struct itimerspec its = {
				.it_interval = time_from_double(1 / f->read_rate),
				.it_value = { 0, 1 },
			};

			int ret = timerfd_settime(f->read_timer, 0, &its, NULL);
			if (ret)
				serror("Failed to start timer");
		}
		
		/* Get current time */
		struct timespec now = time_now();

		/* Get timestamp of first line */
		struct msg m;
		int ret = msg_fscan(f->read.handle, &m, NULL, NULL); rewind(f->read.handle);
		if (ret < 0)
			error("Failed to read first timestamp of node '%s'", n->name);
		
		f->read_first = MSG_TS(&m);

		/* Set read_offset depending on epoch_mode */
		switch (f->read_epoch_mode) {
			case EPOCH_DIRECT: /* read first value at now + epoch */
				f->read_offset = time_diff(&f->read_first, &now);
				f->read_offset = time_add(&f->read_offset, &f->read_epoch);
				break;

			case EPOCH_WAIT: /* read first value at now + first + epoch */
				f->read_offset = now;
				f->read_offset = time_add(&f->read_offset, &f->read_epoch);
				break;
		
			case EPOCH_RELATIVE: /* read first value at first + epoch */
				f->read_offset = f->read_epoch;
				break;
		
			case EPOCH_ABSOLUTE: /* read first value at f->read_epoch */
				f->read_offset = time_diff(&f->read_first, &f->read_epoch);
				break;
		}
	}

	if (f->write.fmt) {
		/* Prepare file name */
		f->write.chunk  = f->write.split ? 0 : -1;
		f->write.path   = file_format_name(f->write.fmt, &now);

		/* Open file */
		f->write.handle = file_reopen(&f->write);
		if (!f->write.handle)
			serror("Failed to open file for writing: '%s'", f->write.path);
	}

	return 0;
}

int file_close(struct node *n)
{
	struct file *f = n->_vd;
	
	free(f->read.path);
	free(f->write.path);

	if (f->read_timer)
		close(f->read_timer);
	if (f->read.handle)
		fclose(f->read.handle);
	if (f->write.handle)
		fclose(f->write.handle);

	return 0;
}

int file_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	struct file *f = n->_vd;
	int values, flags, i = 0;

	if (f->read.handle) {
		for (i = 0; i < cnt; i++) {
			struct msg *cur = &pool[(first+i) % poolsize];
			
			/* Get message and timestamp */
retry:			values = msg_fscan(f->read.handle, cur, &flags, NULL);
			if (values < 0) {
				if (feof(f->read.handle)) {
					if (f->read.split) {
						f->read.chunk++;
						f->read.handle = file_reopen(&f->read);
						if (!f->read.handle)
							return 0;
						
						info("Open new input chunk of node '%s': chunk=%u", n->name, f->read.chunk);
					}
					else {
						info("Rewind input file of node '%s'", n->name);
						rewind(f->read.handle);
						goto retry;
					}
				}
				else
					warn("Failed to read messages from node '%s': reason=%d", n->name, values);

				return 0;
			}
			
			/* Fix missing sequence no */
			cur->sequence = f->read_sequence = (flags & MSG_PRINT_SEQUENCE) ? cur->sequence : f->read_sequence + 1;
			
			if (!f->read_rate || ftell(f->read.handle) == 0) {
				struct timespec until = time_add(&MSG_TS(cur), &f->read_offset);
				if (timerfd_wait_until(f->read_timer, &until) < 0)
					serror("Failed to wait for timer");
			
				/* Update timestamp */
				cur->ts.sec = until.tv_sec;
				cur->ts.nsec = until.tv_nsec;
			}
			else { /* Wait with fixed rate delay */
				if (timerfd_wait(f->read_timer) < 0)
					serror("Failed to wait for timer");
			
				/* Update timestamp */
				struct timespec now = time_now();
				cur->ts.sec = now.tv_sec;
				cur->ts.nsec = now.tv_nsec;
			}
		}
	}
	else
		error("Can not read from node '%s'", n->name);

	return i;
}

int file_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	int i = 0;
	struct file *f = n->_vd;

	if (f->write.handle) {
		for (i = 0; i < cnt; i++) {
			/* Split file if requested */
			if ((f->write.split > 0) && ftell(f->write.handle) > f->write.split * (1 << 20)) {
				f->write.chunk++;
				f->write.handle = file_reopen(&f->write);
				
				info("Splitted output file for node '%s': chunk=%u", n->name, f->write.chunk);
			}
			
			struct msg *m = &pool[(first+i) % poolsize];
			msg_fprint(f->write.handle, m, MSG_PRINT_ALL & ~MSG_PRINT_OFFSET, 0);
		}
		fflush(f->write.handle);
	}
	else
		error("Can not write to node '%s'", n->name);

	return i;
}

static struct node_type vt = {
	.name		= "file",
	.description	= "support for file log / replay node type",
	.size		= sizeof(struct file),
	.reverse	= file_reverse,
	.parse		= file_parse,
	.print		= file_print,
	.open		= file_open,
	.close		= file_close,
	.read		= file_read,
	.write		= file_write
};

REGISTER_NODE_TYPE(&vt)