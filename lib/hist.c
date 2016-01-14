/** Histogram functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <time.h>

#include "utils.h"
#include "hist.h"

#define VAL(h, i)	((h)->low + (i) * (h)->resolution)
#define INDEX(h, v)	round((v - (h)->low) / (h)->resolution)

void hist_create(struct hist *h, double low, double high, double resolution)
{
	h->low = low;
	h->high = high;
	h->resolution = resolution;
	h->length = (high - low) / resolution;
	h->data = alloc(h->length * sizeof(unsigned));

	hist_reset(h);
}

void hist_destroy(struct hist *h)
{
	free(h->data);
}

void hist_put(struct hist *h, double value)
{
	int idx = INDEX(h, value);
	
	h->last = value;

	/* Update min/max */
	if (value > h->highest)
		h->highest = value;
	if (value < h->lowest)
		h->lowest = value;

	/* Check bounds and increment */
	if      (idx >= h->length)
		h->higher++;
	else if (idx < 0)
		h->lower++;
	else
		h->data[idx]++;

	h->total++;

	/* Online / running calculation of variance and mean
	 *  by Donald Knuth’s Art of Computer Programming, Vol 2, page 232, 3rd edition */
	if (h->total == 1) {
		h->_m[1] = h->_m[0] = value;
		h->_s[1] = 0.0;
	}
	else {
		h->_m[0] = h->_m[1] + (value - h->_m[1]) / h->total;
		h->_s[0] = h->_s[1] + (value - h->_m[1]) * (value - h->_m[0]);

		// set up for next iteration
		h->_m[1] = h->_m[0];
		h->_s[1] = h->_s[0];
	}

}

void hist_reset(struct hist *h)
{
	h->total = 0;
	h->higher = 0;
	h->lower = 0;

	h->highest = DBL_MIN;
	h->lowest = DBL_MAX;

	memset(h->data, 0, h->length * sizeof(unsigned));
}

double hist_mean(struct hist *h)
{
	return (h->total > 0) ? h->_m[0] : 0.0;
}

double hist_var(struct hist *h)
{
	return (h->total > 1) ? h->_s[0] / (h->total - 1) : 0.0;
}

double hist_stddev(struct hist *h)
{
	return sqrt(hist_var(h));
}

void hist_print(struct hist *h)
{ INDENT
	stats("Counted values: %u (%u between %f and %f)", h->total, h->total-h->higher-h->lower, h->high, h->low);
	stats("Highest: %f Lowest: %f", h->highest, h->lowest);
	stats("Mu: %f Sigma2: %f Sigma: %f", hist_mean(h), hist_var(h), hist_stddev(h));

	if (h->total - h->higher - h->lower > 0) {
		char *buf = hist_dump(h);
		stats("Matlab: %s", buf);
		free(buf);

		hist_plot(h);
	}
}

void hist_plot(struct hist *h)
{
	char buf[HIST_HEIGHT];
	memset(buf, '#', sizeof(buf));

	hist_cnt_t max = 1;

	/* Get highest bar */
	for (int i = 0; i < h->length; i++) {
		if (h->data[i] > max)
			max = h->data[i];
	}

	/* Print plot */
	stats("%9s | %5s | %s", "Value", "Count", "Plot");
	line();

	for (int i = 0; i < h->length; i++) {
		double value = VAL(h, i);
		int cnt = h->data[i];
		int bar = HIST_HEIGHT * ((double) cnt / max);

		if (value > h->lowest || value < h->highest)
			stats("%+9g | "     "%5u"  " | %.*s", value, cnt, bar, buf);
	}
}

char * hist_dump(struct hist *h)
{
	char *buf = alloc(128);
	
	strcatf(&buf, "[ ");

	for (int i = 0; i < h->length; i++)
		strcatf(&buf, "%u ", h->data[i]);

	strcatf(&buf, "]");
	
	return buf;
}

void hist_matlab(struct hist *h, FILE *f)
{
	char *buf = hist_dump(h);

	fprintf(f, "%lu = struct( ", time(NULL));
	fprintf(f, "'min', %f, 'max', %f, ", h->low, h->high);
	fprintf(f, "'total', %u, higher', %u, 'lower', %u, ", h->total, h->higher, h->lower);
	fprintf(f, "'highest', %f, 'lowest', %f, ", h->highest, h->lowest);
	fprintf(f, "'mean', %f, ", hist_mean(h));
	fprintf(f, "'var', %f, ", hist_var(h));
	fprintf(f, "'stddev', %f, ", hist_stddev(h));
	fprintf(f, "'hist', %s ", buf);
	fprintf(f, "),\n");
	
	free(buf);
}