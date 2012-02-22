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

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "log.h"
#include "rpdd.h"

char *loglevelnames[] = { "ERROR", "WARN ", "INFO ", "DEBUG" };

int loglevel = LOG_INFO;
FILE *logfile = NULL;

int logopen(char *filename)
{
    if(logfile != NULL)
        return 1;

    logfile = fopen(filename, "a");
    if(logfile == NULL)
    {
        printf("Error %s: Could not open logfile %s\n", strerror(errno), filename);
        return 1;
    }

    logwrite(LOG_INFO, "-------------------------------");
    logwrite(LOG_INFO, "%s/%s started", DAEMON_NAME, RPD_VERSION);

    return 0;
}

int logclose(void)
{
    int retval;

    if(logfile == NULL)
        return 1;

    fflush(logfile);
    retval = fclose(logfile);
    if(retval != 0)
        printf("Error %s on closing logfile\n", strerror(errno));
   
    logfile = NULL;
    return retval;
}

int logsetlevel(int loglvl)
{
    if(loglvl >= 0 && loglvl < 3)
    {
        loglevel = loglvl;
        return 0;
    }

    return 1;
}

int logwrite(int loglvl, const char *logfmt, ...)
{
    char logmsg[4096];
    va_list args;
    time_t now;
    char timeinfo[32];

    if(logfile == NULL)
        return 1;

    if(loglvl < 0 || loglvl > loglevel)
        return 0;

    time(&now);
    strftime(timeinfo, sizeof(timeinfo), "%c", localtime(&now));

    va_start(args, logfmt);
    vsprintf(logmsg, logfmt, args);

    if(logmsg[strlen(logmsg)-1] == '\n')
        logmsg[strlen(logmsg)-1] = '\0';

    if(fprintf(logfile, "[%s] %s - %s\n", timeinfo, loglevelnames[loglvl], logmsg) < 0)
    {
        va_end(args);
        return 1;
    }

    va_end(args);
    return 0;
}

