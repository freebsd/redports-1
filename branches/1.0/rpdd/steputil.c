/*
 * Copyright (C) 2011 Bernhard Froehlich <decke@bluelife.at>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Author's name may not be used endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <time.h>

#include "database.h"
#include "remote.h"
#include "util.h"
#include "steps.h"
#include "steputil.h"


/* internal functions */
extern int nextstepindex(void);
extern void segvhandler(int sig);

/* internal data structs */
struct StepHandler
{
    char name[25];
    int (*handler)(void);
    int maxparallel;
    int delay;
    time_t lastrun;
};

struct StepHandler stepreg[] = {
    { "20", handleStep20, 1, 30, 0 },
    { "30", handleStep30, 4, 0, 0 },
    { "31", handleStep31, 1, 0, 0 },
    { "50", handleStep50, 1, 0, 0 },
    { "51", handleStep51, 1, 0, 0 },
    { "70", handleStep70, 1, 0, 0 },
    { "71", handleStep71, 2, 0, 0 },
    { "80", handleStep80, 1, 0, 0 },
    { "90", handleStep90, 1, 0, 0 },
    { "91", handleStep91, 1, 7200, 0 },
    { "95", handleStep95, 1, 3600, 0 },
    { "96", handleStep96, 1, 3600, 0 },
    { "98", handleStep98, 1, 3600, 0 },
    { "99", handleStep99, 1, 7200, 0 },
    { "100", handleStep100, 1, 120, 0 },
    { "101", handleStep101, 1, 7200, 0 },
    { "102", handleStep102, 1, 600, 0 },
    { "103", handleStep103, 1, 30, 0 },
};

static int currstep;


int nextstepindex(void)
{
    static long step;
    return (step++%(sizeof(stepreg)/sizeof(struct StepHandler)));
}


int nextstep(int steps[], int max)
{
    int i;
    int sum;
    int next;
    int count;

    for(i=0,sum=0; i < max; i++)
    {
        if(steps[i] >= 0)
           sum++;
    }

    /* no more free slots? */
    if(sum >= max-1)
        return -1;

    count = sizeof(stepreg)/sizeof(struct StepHandler);

    while(count-- > 0)
    {
        next = nextstepindex();

        for(i=0,sum=0; i < max; i++)
        {
            if(steps[i] == next)
               sum++;
        }

        if(sum >= stepreg[next].maxparallel)
            continue;

        if(stepreg[next].lastrun+stepreg[next].delay > time(NULL))
            continue;

        return next;
    }

    return -1;
}


int handlestep(int step)
{
    int rv;

    if(step < 0 || step >= (sizeof(stepreg)/sizeof(struct StepHandler)))
        return -1;

    currstep = step;

    signal(SIGSEGV, segvhandler);

    logdebug("Step %s -------------------------", stepreg[step].name);

    rv = stepreg[step].handler();

    logdebug("End Step %s = %s -----------", stepreg[step].name, (rv == 0) ? "success" : "failure" );
 
    return rv;
}

void segvhandler(int sig)
{
    logerror("SIGSEGV received in Step %s", stepreg[currstep].name);
    exit(-1);
}

int setlastrun(int step)
{
    if(step < 0 || step >= (sizeof(stepreg)/sizeof(struct StepHandler)))
        return -1;

    stepreg[step].lastrun = time(NULL);
    return 0;
}

