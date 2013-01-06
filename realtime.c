/*
 * Copyright (C) 2011 Mark Hills <mark@pogo.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "device.h"
#include "realtime.h"

#define REALTIME_PRIORITY 80

/*
 * Raise the priority of the current thread
 *
 * Return: -1 if priority could not be satisfactorily raised, otherwise 0
 */

static int raise_priority()
{
    int max_pri;
    struct sched_param sp;

    fprintf(stderr, "Setting scheduler priority...\n");

    if (sched_getparam(0, &sp)) {
        perror("sched_getparam");
        return -1;
    }

    max_pri = sched_get_priority_max(SCHED_FIFO);
    sp.sched_priority = REALTIME_PRIORITY;

    if (sp.sched_priority > max_pri) {
        fprintf(stderr, "Invalid scheduling priority (maximum %d).\n", max_pri);
        return -1;
    }

    if (sched_setscheduler(0, SCHED_FIFO, &sp)) {
        perror("sched_setscheduler");
        fprintf(stderr, "Failed to set scheduler. Run as root otherwise you "
                "may get wow and skips!\n");
    }

    return 0;
}

/*
 * The realtime thread
 */

static void rt_main(struct rt_t *rt)
{
    int r;
    size_t n;

    if (raise_priority() == -1)
        return;

    while (!rt->finished) {
        r = poll(rt->pt, rt->npt, -1);
        if (r == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                perror("poll");
                return;
            }
        }

        for (n = 0; n < rt->ndv; n++)
            device_handle(rt->dv[n]);
    }
}

static void* launch(void *p)
{
    rt_main(p);
    return NULL;
}

/*
 * Initialise state of realtime handler
 */

void rt_init(struct rt_t *rt)
{
    rt->finished = false;
    rt->ndv = 0;
    rt->npt = 0;
}

/*
 * Clear resources associated with the realtime handler
 */

void rt_clear(struct rt_t *rt)
{
}

/*
 * Add a device to this realtime handler
 *
 * Return: -1 if the device could not be added, otherwise 0
 * Post: if 0 is returned the device is added
 */

int rt_add_device(struct rt_t *rt, struct device_t *dv)
{
    ssize_t z;

    /* The requested poll events never change, so populate the poll
     * entry table before entering the realtime thread */

    z = device_pollfds(dv, &rt->pt[rt->npt], MAX_DEVICE_POLLFDS - rt->npt);
    if (z == -1) {
        fprintf(stderr, "Device failed to return file descriptors.\n");
        return -1;
    }

    rt->npt += z;

    rt->dv[rt->ndv] = dv;
    rt->ndv++;

    return 0;
}

/*
 * Start realtime handling of the given devices
 *
 * This forks the realtime thread if it is required (eg. ALSA). Some
 * devices (eg. JACK) start their own thread.
 *
 * Return: -1 on error, otherwise 0
 */

int rt_start(struct rt_t *rt)
{
    size_t n;

    /* If there are any devices which returned file descriptors for
     * poll() then launch the realtime thread to handle them */

    if (rt->npt > 0) {
        fprintf(stderr, "Launching realtime thread to handle devices...\n");

        if (pthread_create(&rt->ph, NULL, launch, (void*)rt)) {
            perror("pthread_create");
            return -1;
        }
    }

    /* FIXME: To avoid audio drop on startup, devices should be
     * started after synchronising with the realtime thread */

    for (n = 0; n < rt->ndv; n++)
        device_start(rt->dv[n]);

    return 0;
}

/*
 * Stop realtime handling, which was previously started by rt_start()
 */

void rt_stop(struct rt_t *rt)
{
    size_t n;

    rt->finished = true;

    if (rt->npt > 0) {
        if (pthread_join(rt->ph, NULL) != 0)
            abort();
    }

    /* Stop audio rolling on devices */

    for (n = 0; n < rt->ndv; n++)
        device_stop(rt->dv[n]);
}
