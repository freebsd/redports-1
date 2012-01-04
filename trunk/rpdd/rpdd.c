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

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <sys/param.h>
#include <libutil.h>

#include "log.h"
#include "steputil.h"
#include "util.h"

#define RPD_VERSION "1.0.91"
#define DAEMON_NAME "rpdd"
#define CONF_FILE "rpdd.conf"
#define PID_FILE "/var/run/rpdd.pid"

#define MAXCHILDS 5

void usage(void);
void version(void);
void run(void);
void signal_handler(int sig);


void run(void)
{
    pid_t pids[MAXCHILDS];
    int stats[MAXCHILDS];
    int slot;

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

        usleep(500000);
    }
}
 
void usage(void){
    printf("Usage: %s [options]\n", DAEMON_NAME);
    printf("Options:\n");
    printf("    -c file  Config file\n");
    printf("    -n       Don't fork off as a daemon\n");
    printf("    -h       Show this help screen\n");
    printf("    -v       Show version information\n");
}

void version(void){
    printf("%s %s\n", DAEMON_NAME, RPD_VERSION);
}
 
void signal_handler(int sig) {
     
    switch(sig) {
        case SIGINT:
            exit(1);
            break;
        case SIGHUP:
            logclose();
            logopen(configget("logFile"));
            break;
        case SIGTERM:
            exit(1);
            break;
        default:
            logwarn("Unhandled signal %s", strsignal(sig));
            break;
    }
}
 
int main(int argc, char *argv[])
{
    char config[PATH_MAX] = CONF_FILE;
    struct pidfh *pfh;
    pid_t otherpid;
    int daemonize = 1;
    int c;
 
    /* Setup signal handling before we start */
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
 
    while( (c = getopt(argc, argv, "c:hnv")) != -1) {
        switch(c){
            case 'c':
                strncpy(config, optarg, sizeof(config)-1);
                config[sizeof(config)-1] = '\0';
                break;
            case 'h':
                usage();
                exit(0);
                break;
            case 'n':
                daemonize = 0;
                break;
            case 'v':
                version();
                exit(0);
                break;
            case '?':
            default:
                usage();
                exit(1);
                break;
        }
    }

    if(configparse(config)){
        printf("Could not load config file %s\n", config);
        exit(EXIT_FAILURE);
    }

    if(logopen(configget("logFile")) != 0)
        exit(EXIT_FAILURE);

    if(daemonize)
    {
        pfh = pidfile_open(PID_FILE, 0600, &otherpid);
        if(pfh == NULL)
        {
            if(errno == EEXIST)
                printf("Daemon already running, pid: %jd.", (pid_t)otherpid);

            printf("Cannot open or create pidfile");
        }

        if (daemon(0, 0) == -1)
        {
            logwarn("Cannot daemonize");
            pidfile_remove(pfh);
            exit(EXIT_FAILURE);
        }

        pidfile_write(pfh);
        pidfile_close(pfh);
    }
 
    run();
 
    logclose();
    pidfile_remove(pfh);
    exit(EXIT_SUCCESS);
}

