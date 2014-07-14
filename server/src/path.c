/** Message paths
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include <sys/syscall.h>

#include "cfg.h"
#include "utils.h"
#include "path.h"

#define sigev_notify_thread_id   _sigev_un._tid

/** Send messages */
static void * path_send(void *arg)
{
	int sig;
	struct path *p = (struct path *) arg;
	timer_t tmr;
	sigset_t set;

	struct sigevent sev = {
		.sigev_notify = SIGEV_THREAD_ID,
		.sigev_signo = SIGALRM,
		.sigev_notify_thread_id = syscall(SYS_gettid)
	};

	struct itimerspec its = {
		.it_interval = timespec_rate(p->rate),
		.it_value = { 1, 0 }
	};

	sigemptyset(&set);
	sigaddset(&set, SIGALRM);
	if(pthread_sigmask(SIG_BLOCK, &set, NULL))
		perror("Set signal mask");

	if (timer_create(CLOCK_REALTIME, &sev, &tmr))
		perror("Failed to create timer");

	if (timer_settime(tmr, 0, &its, NULL))
		perror("Failed to start timer");

	while (1) {
		sigwait(&set, &sig);
		if (p->last) {
			msg_send(p->last, p->out);
			p->last = NULL;
			p->sent++;
		}
	}

	return NULL;
}

/** Receive messages */
static void * path_run(void *arg)
{
	struct path *p = (struct path *) arg;
	struct msg m;

	/* Main thread loop */
	while (1) {
		msg_recv(&m, p->in); /* Receive message */
		p->received++;

		/** Check header fields */
		if (m.version != MSG_VERSION) {
			p->invalid++;
			continue;
		}

		if (m.type != MSG_TYPE_DATA) {
			p->invalid++;
			continue;
		}

		/* Check sequence number */
		if (m.sequence <= 1) {
			path_stats(p);
			info("Simulation started");

			p->sequence	= 0;
			p->received	= 1;
			p->sent		= 0;
			p->skipped	= 0;
			p->delayed	= 0;
			p->duplicated	= 0;
			p->invalid	= 0;
		}
		else if (m.sequence < p->sequence) {
			p->delayed++;
			continue;
		}
		else if (m.sequence == p->sequence) {
			p->duplicated++;
			continue;
		}

		if (p->hook && p->hook(&m)) {
			p->skipped++;
			continue;
		}

		/* At fixed rate mode, messages are send by another thread */
		if (p->rate)
			p->last = &m;
		else
			msg_send(&m, p->out);

		p->sequence = m.sequence;
		p->sent++;
	}

	return NULL;
}

int path_start(struct path *p)
{
	/* At fixed rate mode, we start another thread for sending */
	if (p->rate)
		pthread_create(&p->tid2, NULL, &path_send, (void *) p);

	return pthread_create(&p->tid, NULL, &path_run, (void *) p);
}

int path_stop(struct path *p)
{
	pthread_cancel(p->tid);
	pthread_join(p->tid, NULL);

	if (p->rate) {
		pthread_cancel(p->tid2);
		pthread_join(p->tid2, NULL);
	}

	return 0;
}

void path_stats(struct path *p)
{
	info("%12s " MAG("=>") " %-12s:   %-8u %-8u %-8u %-8u %-8u",
		p->in->name, p->out->name,
		p->sent, p->received, p->delayed,
		p->duplicated, p->invalid
	);
}