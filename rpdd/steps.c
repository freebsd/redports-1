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

#include "database.h"
#include "remote.h"
#include "util.h"
#include "steps.h"


/* internal functions */
extern int nextstepindex(void);

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
    { "10", handleStep10, 1, 0, 0 },
    { "20", handleStep20, 1, 0, 0 },
    { "30", handleStep30, 1, 0, 0 },
    { "31", handleStep31, 1, 0, 0 },
    { "70", handleStep70, 1, 0, 0 },
    { "71", handleStep71, 1, 0, 0 },
    { "80", handleStep80, 1, 0, 0 },
    { "91", handleStep91, 1, 120, 0 },
    { "95", handleStep95, 1, 7200, 0 }
};

int handleStep95(void)
{
    printf("Cleaning old directories in %s\n", configget("wwwroot"));

    cleanolddir(configget("wwwroot"));

    return 0;
}

int handleStep91(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_ROW builds;
    char url[250];
    char query[1000];
    int status;

    if((conn = mysql_autoconnect()) == NULL)
        return 1;

    if(mysql_query(conn, "SELECT id, protocol, host, uri, credentials FROM backends WHERE status > 0")){
        LOGSQL(conn);
        return 1;
    }

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        printf("Checking backend %s\n", builds[2]);

        sprintf(url, "%s://%s%sping", builds[1], builds[2], builds[3]);
        if(getpage(url, builds[4]) == -1)
           status = 2; /* Status failure */
        else
           status = 1; /* Status enabled */

        printf("%s is %s\n", builds[2], status == 2 ? "not available" : "available");

        sprintf(query, "UPDATE backends SET status = %d WHERE id = %d", status, atol(builds[0]));
        if(mysql_query(conn, query)){
           LOGSQL(conn);
           return 1;
        }
    }

    return 0;
}

int handleStep80(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_ROW builds;
    char url[250];
    char query[1000];
    int status;

    if((conn = mysql_autoconnect()) == NULL)
        return 1;

    if(mysql_query(conn, "SELECT builds.id, protocol, host, uri, credentials, buildname, backendbuilds.id FROM builds, buildqueue, backends, backendbuilds WHERE builds.queueid = buildqueue.id AND builds.backendid = backends.id AND builds.backendid = backendbuilds.backendid AND builds.group = backendbuilds.buildgroup AND builds.status = 80")){
        LOGSQL(conn);
        return 1;
    }

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        printf("Cleaning build %s on backend %s\n", builds[5], builds[2]);

        sprintf(url, "%s://%s%sclean?build=%s", builds[1], builds[2], builds[3], builds[5]);
        if(!getpage(url, builds[4]))
        {
            printf("CGI Error: %s\n", getenv("ERROR"));

            sprintf(query, "UPDATE backendbuilds SET status = 2 WHERE id = %d", atol(builds[6]));
            if(mysql_query(conn, query)){
                LOGSQL(conn);
                return 1;
            }
        }

        sprintf(query, "UPDATE builds SET status = 90 WHERE id = %d", atoi(builds[0]));
        if(mysql_query(conn, query)){
           LOGSQL(conn);
           return 1;
        }
    }

    return 0;
}

int handleStep71(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_RES *result2;
    MYSQL_ROW builds;
    MYSQL_ROW runningdownloads;
    char url[250];
    char query[1000];
    char remotefile[255];
    char localfile[255];
    char *status = "error";

    if((conn = mysql_autoconnect()) == NULL)
        return 1;

    if(mysql_query(conn, "SELECT builds.id, protocol, host, uri, credentials, buildname, owner, queueid FROM builds, buildqueue, backends, backendbuilds WHERE builds.queueid = buildqueue.id AND builds.backendid = backends.id AND builds.backendid = backendbuilds.backendid AND builds.group = backendbuilds.buildgroup AND builds.status = 71")){
        LOGSQL(conn);
        return 1;
    }

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        sprintf(url, "%s://%s%sstatus?build=%s", builds[1], builds[2], builds[3], builds[5]);
        if(!getpage(url, builds[4]))
        {
           printf("CGI Error: %s\n", getenv("ERROR"));
           continue;
        }

        if(strcmp(getenv("STATUS"), "finished") == 0)
        {
           if(getenv("BUILDSTATUS") != NULL)
              status = getenv("BUILDSTATUS");
           if(getenv("FAIL_REASON") != NULL)
              status = getenv("FAIL_REASON");
        }

        sprintf(localfile, "%s/%s/%s", configget("wwwroot"), builds[6], builds[7]);
        printf("Log dir: %s\n", localfile);
        if(mkdirrec(localfile) != 0)
           continue;

        if(getenv("BUILDLOG") != NULL)
        {
           sprintf(localfile, "%s/%s/%s/build.log", configget("wwwroot"), builds[6], builds[7]);
           sprintf(remotefile, "%s://%s%s", builds[1], builds[2], getenv("BUILDLOG"));
           printf("Downloading Log %s to %s\n", remotefile, localfile);
           if(downloadfile(remotefile, builds[4], localfile) != 0){
              printf("Download failed!");
              continue;
           }
        }

        if(getenv("WRKDIR") != NULL)
        {
           sprintf(localfile, "%s/%s/%s/wrkdir.tar.gz", configget("wwwroot"), builds[6], builds[7]);
           sprintf(remotefile, "%s://%s%s", builds[1], builds[2], getenv("WRKDIR"));
           printf("Downloading Wrkdir %s to %s\n", remotefile, localfile);
           if(downloadfile(remotefile, builds[4], localfile) != 0){
              printf("Download failed!");
              continue;
           }
        }

        printf("Updating build status for %s to %s\n", builds[0], status);
        sprintf(query, "UPDATE builds SET buildstatus = \"%s\", enddate = %lli, status = 80 WHERE id = %d", status, microtime(), atoi(builds[0]));
        if(mysql_query(conn, query)){
           LOGSQL(conn);
           return 1;
        }
    }

    mysql_free_result(result);
    mysql_close(conn);

    return 0;
}

int handleStep70(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_RES *result2;
    MYSQL_ROW builds;
    MYSQL_ROW runningdownloads;
    char query[1000];

    if((conn = mysql_autoconnect()) == NULL)
        return 1;

    if(mysql_query(conn, "SELECT builds.id, builds.backendid FROM builds, buildqueue, backends, backendbuilds WHERE builds.queueid = buildqueue.id AND builds.backendid = backends.id AND builds.backendid = backendbuilds.backendid AND builds.group = backendbuilds.buildgroup AND builds.status >= 70 AND builds.status < 80")){
        LOGSQL(conn);
        return 1;
    }

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        sprintf(query, "SELECT count(*) FROM builds WHERE status = 71 AND backendid = %d", atoi(builds[1]));
        if(mysql_query(conn, query)){
           LOGSQL(conn);
           return 1;
        }

        result2 = mysql_store_result(conn);
        runningdownloads = mysql_fetch_row(result2);

        if(atoi(runningdownloads[0]) == 0)
        {
           sprintf(query, "UPDATE builds SET status = 71 WHERE id = %d", atoi(builds[0]));
           if(mysql_query(conn, query)){
              LOGSQL(conn);
              return 1;
           }
        }

        mysql_free_result(result2);
    }

    mysql_free_result(result);
    mysql_close(conn);

    return 0;
}


int handleStep31(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_ROW builds;
    char query[1000];
    char url[500];
    int status;

    if((conn = mysql_autoconnect()) == NULL)
        return 1;

    if(mysql_query(conn, "SELECT builds.id, protocol, host, uri, credentials, portname, buildname, backendkey FROM builds, buildqueue, backends, backendbuilds WHERE builds.queueid = buildqueue.id AND builds.backendid = backends.id AND builds.backendid = backendbuilds.backendid AND builds.group = backendbuilds.buildgroup AND builds.status = 31")){
        LOGSQL(conn);
        return 1;
    }

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        sprintf(url, "%s://%s%sbuild?port=%s&build=%s&priority=%s&finishurl=%s/backend/finished/%s", builds[1], builds[2], builds[3], builds[5], builds[6], "5", configget("wwwurl"), builds[7]);
        if(getpage(url, builds[4]))
           status = 50;
        else
        {
           status = 80;
           printf("CGI Error: %s\n", getenv("ERROR"));
        }

        sprintf(query, "UPDATE builds SET status = %d WHERE id = %d", status, atoi(builds[0]));
        if(mysql_query(conn, query)){
           LOGSQL(conn);
           return 1;
        }
    }

    mysql_free_result(result);
    mysql_close(conn);

    return 0;
}

int handleStep30(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_ROW builds;
    char query[1000];
    char url[250];
    int status;

    if((conn = mysql_autoconnect()) == NULL)
        return 1;

    if(mysql_query(conn, "SELECT builds.id, protocol, host, uri, credentials, buildname, repository, revision FROM builds, buildqueue, backends, backendbuilds WHERE builds.queueid = buildqueue.id AND builds.backendid = backends.id AND builds.backendid = backendbuilds.backendid AND builds.group = backendbuilds.buildgroup AND builds.status = 30")){
        LOGSQL(conn);
        return 1;
    }

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        sprintf(url, "%s://%s%scheckout?repository=%s&revision=%s&build=%s", builds[1], builds[2], builds[3], builds[6], builds[7], builds[5]);
        if(getpage(url, builds[4]))
           status = 31;
        else
        {
           status = 80;
           printf("CGI Error: %s\n", getenv("ERROR"));
        }

        sprintf(query, "UPDATE builds SET status = %d WHERE id = %d", status, atoi(builds[0]));
        if(mysql_query(conn, query)){
           LOGSQL(conn);
           return 1;
        }
    }
         
    mysql_free_result(result);
    mysql_close(conn);

    return 0;
}


int handleStep20(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_RES *result2;
    MYSQL_RES *result3;
    MYSQL_ROW builds;
    MYSQL_ROW backends;
    MYSQL_ROW runningjobs;
    char query[1000];

    if((conn = mysql_autoconnect()) == NULL)
        return 1;

    if(mysql_query(conn, "SELECT builds.id, builds.group, builds.queueid FROM buildqueue, builds WHERE buildqueue.id = builds.queueid AND buildqueue.status = 20 AND builds.backendid = 0")){
        LOGSQL(conn);
        return 1;
    }

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        sprintf(query, "SELECT backendid, maxparallel FROM backendbuilds, backends WHERE backendid = backends.id AND buildgroup = \"%s\" ORDER BY priority", builds[1]);

        if(mysql_query(conn, query)){
            LOGSQL(conn);
            return 1;
        }

        result2 = mysql_store_result(conn);

        while((backends = mysql_fetch_row(result2)))
        {
            sprintf(query, "SELECT count(*) FROM builds WHERE backendid = %d AND status < 90", atoi(backends[0]));
            if(mysql_query(conn, query)){
                LOGSQL(conn);
                return 1;
            }

            result3 = mysql_store_result(conn);
            runningjobs = mysql_fetch_row(result3);

            if(atoi(runningjobs[0]) < atoi(backends[1]))
            {
                sprintf(query, "UPDATE builds SET backendid = %d, status = 30, startdate = %lli WHERE id = %d", atoi(backends[0]), microtime(), atoi(builds[0]));
                if(mysql_query(conn, query)){
                    LOGSQL(conn);
                    return 1;
                }
            }

            mysql_free_result(result3);
        }

        mysql_free_result(result2);


        sprintf(query, "SELECT count(*) FROM builds WHERE queueid = \"%s\" AND backendid = 0", builds[2]);
        if(mysql_query(conn, query)){
            LOGSQL(conn);
            return 1;
        }

        result2 = mysql_store_result(conn);
        runningjobs = mysql_fetch_row(result2);

        if(atoi(runningjobs[0]) == 0)
        {
            sprintf(query, "UPDATE buildqueue SET status = 30 WHERE id = \"%s\"", builds[2]);
            if(mysql_query(conn, query)){
                LOGSQL(conn);
                return 1;
            }
        }
        
        mysql_free_result(result2);
    }

    mysql_free_result(result);
    mysql_close(conn);
    
    return 0;
}

int handleStep10(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_RES *result2;
    MYSQL_ROW builds;
    MYSQL_ROW backends;
    int num_fields;
    int i;
    char query[1000];


    if((conn = mysql_autoconnect()) == NULL)
        return 1;

    if(mysql_query(conn, "SELECT id, owner FROM buildqueue WHERE status = 10")){
        LOGSQL(conn);
        return 1;
    }

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        sprintf(query, "SELECT buildgroup FROM automaticbuildgroups WHERE username = \"%s\" ORDER BY priority", builds[1]);

        if(mysql_query(conn, query)){
            LOGSQL(conn);
            return 1;
        }

        result2 = mysql_store_result(conn);

        while((backends = mysql_fetch_row(result2)))
        {
            printf("adding build %s for %s\n", builds[0], builds[1]);
            sprintf(query, "INSERT INTO builds VALUES (null, \"%s\", SUBSTRING(MD5(RAND()), 1, 25), \"%s\", 10, \"\", 0, 0, 0)", builds[0], backends[0]);
	    if(mysql_query(conn, query)){
                LOGSQL(conn);
                return 1;
            }
        }

        mysql_free_result(result2);

        sprintf(query, "UPDATE buildqueue SET status = 20 WHERE id = \"%s\"", builds[0]);
        if(mysql_query(conn, query)){
            LOGSQL(conn);
            return 1;
        }
    }

    mysql_free_result(result);
    mysql_close(conn);

    return 0;
}

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

    printf("- Step %s -------------------------\n", stepreg[step].name);

    rv = stepreg[step].handler();

    printf("- End Step %s = %s -----------\n", stepreg[step].name, (rv == 0) ? "success" : "failure" );
 
    return rv;
}

int setlastrun(int step)
{
    if(step < 0 || step >= (sizeof(stepreg)/sizeof(struct StepHandler)))
        return -1;

    stepreg[step].lastrun = time(NULL);
    return 0;
}

