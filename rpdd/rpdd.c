/*
 * Copyright (C) 2011 Bernhard Froehlich <decke@FreeBSD.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "steps.h"

#define DAEMON_NAME "rpd"
#define PID_FILE "/var/run/rpd.pid"
#define CONF_FILE "rpdd.conf"

#define MAXCHILDS 5


void run()
{
    pid_t pids[MAXCHILDS];
    int stats[MAXCHILDS];
    int slot;
    int step=0;

    memset(pids, '\0', sizeof(pids));
    memset(stats, -1, sizeof(stats));

    while(1)
    {
        pid_t pid;
        int step;
        step = nextstep(stats, MAXCHILDS);

        if(step >= 0)
        {
            for(slot=0; slot < MAXCHILDS; slot++)
            {
                if(pids[slot] != 0)
                    continue;

                /* free slot found */
                stats[slot] = step;
                pids[slot] = fork();

                if(pids[slot] == 0)
                {
                    /* child process */
                    handlestep(step);
                    exit(0);
                }
                setlastrun(step);
                break;
            }
        }

        /* wait for any child to exit */
        while((pid = waitpid(-1, 0, WNOHANG)) > 0)
        {
            for(slot=0; slot < MAXCHILDS; slot++)
            {
                if(pids[slot] == pid)
                {
                    stats[slot] = -1;
                    pids[slot] = 0;
                    break;
                }
            }
        }

        sleep(1);
    }
}
 
void usage(int argc, char *argv[]){
    if (argc >= 1){
        printf("Usage: %s -h -n\n", argv[0]);
        printf("  Options:\n");
        printf("      -n   Don't fork off as a daemon.\n");
        printf("      -h   Show this help screen.\n");
        printf("\n");
    }
}
 
void signal_handler(int sig) {
 
    switch(sig) {
        case SIGINT:
            syslog(LOG_WARNING, "Received SIGINT signal.");
            exit(1);
            break;
        case SIGHUP:
            syslog(LOG_WARNING, "Received SIGHUP signal.");
            exit(1);
            break;
        case SIGTERM:
            syslog(LOG_WARNING, "Received SIGTERM signal.");
            exit(1);
            break;
        default:
            syslog(LOG_WARNING, "Unhandled signal (%d) %s", strsignal(sig));
            break;
    }
}
 
int main(int argc, char *argv[]) {
 
#if defined(DEBUG)
    int daemonize = 0;
#else
    int daemonize = 1;
#endif
 
    // Setup signal handling before we start
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
 
    int c;
    while( (c = getopt(argc, argv, "nh|help")) != -1) {
        switch(c){
            case 'h':
                usage(argc, argv);
                exit(0);
                break;
            case 'n':
                daemonize = 0;
                break;
            default:
                usage(argc, argv);
                exit(0);
                break;
        }
    }

    configparse(CONF_FILE);
 
    /* Our process ID and Session ID */
    pid_t pid, sid;
 
    if (daemonize) {
        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
            exit(EXIT_FAILURE);
        }
        /* If we got a good PID, then
           we can exit the parent process. */
        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }
 
        /* Change the file mode mask */
        umask(0);
 
        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }
 
        /* Change the current working directory */
        if ((chdir("/")) < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }
 
        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }
 
    run();
 
    exit(0);
}
