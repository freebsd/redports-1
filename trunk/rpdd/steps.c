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
#include <string.h>
#include <libgen.h>

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
    { "50", handleStep50, 1, 120, 0 },
    { "51", handleStep51, 1, 0, 0 },
    { "70", handleStep70, 1, 0, 0 },
    { "71", handleStep71, 1, 0, 0 },
    { "80", handleStep80, 1, 0, 0 },
    { "91", handleStep91, 1, 120, 0 },
    { "92", handleStep92, 1, 7200, 0 },
    { "93", handleStep93, 1, 600, 0 },
    { "95", handleStep95, 1, 7200, 0 },
    { "96", handleStep96, 1, 7200, 0 }
};


/* Clean database from old entries */
int handleStep96(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    char query[1000];

    int cleandays = atoi(configget("cleandays"));

    if((conn = mysql_autoconnect()) == NULL)
        return 1;

    sprintf(query, "DELETE FROM builds WHERE status >= 90 AND enddate < %lli", microtime()-(cleandays*86400*1000000L));
    if(mysql_query(conn, query))
        RETURN_ROLLBACK(conn);

    sprintf(query, "DELETE FROM buildqueue WHERE status >= 90 AND enddate < %lli", microtime()-(cleandays*86400*1000000L));
    if(mysql_query(conn, query))
        RETURN_ROLLBACK(conn);

    RETURN_COMMIT(conn);
}


/* Clean filesystem from old logfiles and wrkdirs */
int handleStep95(void)
{
    printf("Cleaning old directories in %s\n", configget("wwwroot"));

    cleanolddir(configget("wwwroot"));

    return 0;
}


/* Update portstrees on backend */
int handleStep93(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_ROW builds;
    char url[250];
    char query[1000];
    int status;
    struct tm tm;

    int maxage = atoi(configget("tbPortsTreeMaxAge"));

    if((conn = mysql_autoconnect()) == NULL)
        return 1;

    if(mysql_query(conn, "SELECT backends.id, protocol, host, uri, credentials, buildname, backendbuilds.id FROM backends, backendbuilds WHERE backendbuilds.backendid = backends.id AND backends.status = 1 AND backendbuilds.status = 1 FOR UPDATE"))
        RETURN_ROLLBACK(conn);

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        printf("Checking build %s on backend %s\n", builds[5], builds[2]);

        sprintf(url, "%s://%s%sstatus?build=%s", builds[1], builds[2], builds[3], builds[5]);
        if(!getpage(url, builds[4]))
        {
           printf("CGI Error: %s\n", getenv("ERROR"));
           continue;
        }

        if(strcmp(getenv("STATUS"), "idle") != 0 || getenv("PORTSTREELASTBUILT") == NULL)
           continue;

        if(strptime(getenv("PORTSTREELASTBUILT"), "%Y-%m-%d %H:%M:%S", &tm) == NULL)
        {
           printf("converting portstreelastbuilt failed!");
           continue;
        }

        if(difftime(time(NULL), mktime(&tm)) < 3600*maxage)
        {
           printf("Portstree last built %s does not exceed max. age of %d hours\n", getenv("PORTSTREELASTBUILT"), maxage);
           continue;
        }

        printf("Updating portstree for build %s on backend %s\n", builds[5], builds[2]);

        sprintf(url, "%s://%s%supdate?build=%s", builds[1], builds[2], builds[3], builds[5]);
        if(!getpage(url, builds[4]))
        {
           printf("CGI Error: %s\n", getenv("ERROR"));
           continue;
        }

        printf("Updating portstree for build %s finished.\n", builds[5]);

        /* only 1 at a time */
        break;
    }

    mysql_free_result(result);

    RETURN_COMMIT(conn);
}


/* Checking builds */
int handleStep92(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_ROW builds;
    char url[250];
    char query[1000];
    int status;

    if((conn = mysql_autoconnect()) == NULL)
        return 1;

    if(mysql_query(conn, "SELECT backends.id, protocol, host, uri, credentials, buildname, backendbuilds.id FROM backends, backendbuilds WHERE backendbuilds.backendid = backends.id AND backends.status = 1 AND backendbuilds.status = 1 FOR UPDATE"))
        RETURN_ROLLBACK(conn);

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        printf("Checking build %s on backend %s\n", builds[5], builds[2]);

        sprintf(url, "%s://%s%sselftest?build=%s", builds[1], builds[2], builds[3], builds[5]);
        if(getpage(url, builds[4]) == -1)
           status = 2; /* Status failure */
        else
           status = 1; /* Status enabled */

        printf("%s is %s\n", builds[5], status == 2 ? "not available" : "available");

        sprintf(query, "UPDATE backendbuilds SET status = %d WHERE id = %d", status, atol(builds[6]));
        if(mysql_query(conn, query))
           RETURN_ROLLBACK(conn);
    }

    mysql_free_result(result);

    RETURN_COMMIT(conn);
}


/* Ping Backends */
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

    if(mysql_query(conn, "SELECT id, protocol, host, uri, credentials FROM backends WHERE status > 0 FOR UPDATE"))
        RETURN_ROLLBACK(conn);

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
        if(mysql_query(conn, query))
           RETURN_ROLLBACK(conn);
    }

    mysql_free_result(result);

    RETURN_COMMIT(conn);
}


/* Clean build on backend after everything was transferred */
int handleStep80(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_RES *result2;
    MYSQL_ROW builds;
    MYSQL_ROW active;
    char url[250];
    char query[1000];
    int status;

    if((conn = mysql_autoconnect()) == NULL)
        return -1;

    if(mysql_query(conn, "SELECT builds.id, protocol, host, uri, credentials, buildname, backendbuilds.id, buildqueue.id FROM builds, buildqueue, backends, backendbuilds WHERE builds.queueid = buildqueue.id AND builds.backendid = backends.id AND builds.backendid = backendbuilds.backendid AND builds.group = backendbuilds.buildgroup AND builds.status = 80 FOR UPDATE"))
        RETURN_ROLLBACK(conn);

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        printf("Cleaning build %s on backend %s\n", builds[5], builds[2]);

        sprintf(url, "%s://%s%sclean?build=%s", builds[1], builds[2], builds[3], builds[5]);
        if(!getpage(url, builds[4]))
        {
            printf("CGI Error: %s\n", getenv("ERROR"));

            sprintf(query, "UPDATE backendbuilds SET status = 2 WHERE id = %d", atol(builds[6]));
            if(mysql_query(conn, query))
                RETURN_ROLLBACK(conn);
        }

        sprintf(query, "UPDATE builds SET status = 90 WHERE id = %d", atoi(builds[0]));
        if(mysql_query(conn, query))
           RETURN_ROLLBACK(conn);

        sprintf(query, "SELECT count(*) FROM builds WHERE queueid = \"%s\" AND status < 90", builds[7]);
        if(mysql_query(conn, query))
           RETURN_ROLLBACK(conn);

        result2 = mysql_store_result(conn);
        active = mysql_fetch_row(result2);

        if(atoi(active[0]) == 0)
        {
           sprintf(query, "UPDATE buildqueue SET status = 90, enddate = %lli WHERE id = \"%s\"", microtime(), builds[7]);
           if(mysql_query(conn, query))
              RETURN_ROLLBACK(conn);
        }
    }

    mysql_free_result(result);

    RETURN_COMMIT(conn);
}


/* Transfer logfiles and wrkdirs from backends */
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
    char localdir[255];

    if((conn = mysql_autoconnect()) == NULL)
        return -1;

    if(mysql_query(conn, "SELECT builds.id, protocol, host, uri, credentials, buildname, owner, queueid FROM builds, buildqueue, backends, backendbuilds WHERE builds.queueid = buildqueue.id AND builds.backendid = backends.id AND builds.backendid = backendbuilds.backendid AND builds.group = backendbuilds.buildgroup AND builds.status = 71 FOR UPDATE"))
        RETURN_ROLLBACK(conn);

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        sprintf(url, "%s://%s%sstatus?build=%s", builds[1], builds[2], builds[3], builds[5]);
        if(!getpage(url, builds[4]))
        {
           printf("CGI Error: %s\n", getenv("ERROR"));
           continue;
        }

        sprintf(localdir, "%s/%s/%s-%s", configget("wwwroot"), builds[6], builds[7], builds[0]);
        printf("Log dir: %s\n", localdir);
        if(mkdirrec(localdir) != 0)
           continue;

        if(getenv("BUILDLOG") != NULL)
        {
           sprintf(localfile, "%s/%s", localdir, basename(getenv("BUILDLOG")));
           sprintf(remotefile, "%s://%s%s", builds[1], builds[2], getenv("BUILDLOG"));
           printf("Downloading Log %s to %s\n", remotefile, localfile);
           if(downloadfile(remotefile, builds[4], localfile) != 0){
              printf("Download failed!");
              continue;
           }

           sprintf(query, "UPDATE builds SET buildlog = \"%s\" WHERE id = %d", basename(localfile), atoi(builds[0]));
           if(mysql_query(conn, query))
              RETURN_ROLLBACK(conn);
        }

        if(getenv("WRKDIR") != NULL)
        {
           sprintf(localfile, "%s/%s", localdir, basename(getenv("WRKDIR")));
           sprintf(remotefile, "%s://%s%s", builds[1], builds[2], getenv("WRKDIR"));
           printf("Downloading Wrkdir %s to %s\n", remotefile, localfile);
           if(downloadfile(remotefile, builds[4], localfile) != 0){
              printf("Download failed!");
              continue;
           }

           sprintf(query, "UPDATE builds SET wrkdir = \"%s\" WHERE id = %d", basename(localfile), atoi(builds[0]));
           if(mysql_query(conn, query))
              RETURN_ROLLBACK(conn);
        }

        printf("Updating build status for %s\n", builds[0]);
        sprintf(query, "UPDATE builds SET buildstatus = \"%s\", buildreason = \"%s\", enddate = %lli, status = 80 WHERE id = %d", getenv("BUILDSTATUS") != NULL ? getenv("BUILDSTATUS") : "", getenv("FAIL_REASON") != NULL ? getenv("FAIL_REASON") : "", microtime(), atoi(builds[0]));
        if(mysql_query(conn, query))
           RETURN_ROLLBACK(conn);
    }

    mysql_free_result(result);

    RETURN_COMMIT(conn);
}


/* Start transfers from backends but only one per backend to no overload the link */
int handleStep70(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_RES *result2;
    MYSQL_ROW builds;
    MYSQL_ROW runningdownloads;
    char query[1000];

    if((conn = mysql_autoconnect()) == NULL)
        return -1;

    if(mysql_query(conn, "SELECT builds.id, builds.backendid FROM builds, buildqueue, backends, backendbuilds WHERE builds.queueid = buildqueue.id AND builds.backendid = backends.id AND builds.backendid = backendbuilds.backendid AND builds.group = backendbuilds.buildgroup AND builds.status >= 70 AND builds.status < 80 FOR UPDATE"))
        RETURN_ROLLBACK(conn);

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        sprintf(query, "SELECT count(*) FROM builds WHERE status = 71 AND backendid = %d", atoi(builds[1]));
        if(mysql_query(conn, query))
           RETURN_ROLLBACK(conn);

        result2 = mysql_store_result(conn);
        runningdownloads = mysql_fetch_row(result2);

        if(atoi(runningdownloads[0]) == 0)
        {
           sprintf(query, "UPDATE builds SET status = 71 WHERE id = %d", atoi(builds[0]));
           if(mysql_query(conn, query))
               RETURN_ROLLBACK(conn);
        }

        mysql_free_result(result2);
    }

    mysql_free_result(result);

    RETURN_COMMIT(conn);
}


/* Check build status for a running job */
int handleStep51(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_ROW builds;
    char query[1000];
    char url[250];

    if((conn = mysql_autoconnect()) == NULL)
        return -1;

    if(mysql_query(conn, "SELECT builds.id, protocol, host, uri, credentials, buildname FROM builds, buildqueue, backends, backendbuilds WHERE builds.queueid = buildqueue.id AND builds.backendid = backends.id AND builds.backendid = backendbuilds.backendid AND builds.group = backendbuilds.buildgroup AND builds.status = 51 FOR UPDATE"))
        RETURN_ROLLBACK(conn);

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        sprintf(url, "%s://%s%sstatus?build=%s", builds[1], builds[2], builds[3], builds[5]);
        if(!getpage(url, builds[4]))
        {
           printf("CGI Error: %s\n", getenv("ERROR"));
           continue;
        }

        if(strcmp(getenv("STATUS"), "finished") == 0 || strcmp(getenv("STATUS"), "idle") == 0)
        {
           sprintf(query, "UPDATE builds SET status = 70 WHERE id = %d", atoi(builds[0]));
           if(mysql_query(conn, query))
               RETURN_ROLLBACK(conn);
        }
        else
        {
           sprintf(query, "UPDATE builds SET status = 50 WHERE id = %d", atoi(builds[0]));
           if(mysql_query(conn, query))
               RETURN_ROLLBACK(conn);
        }
    }

    mysql_free_result(result);

    RETURN_COMMIT(conn);
}


/* Queue running jobs and periodicaly shift them to status 51 for actual checking  */
int handleStep50(void)
{
    MYSQL *conn;

    if((conn = mysql_autoconnect()) == NULL)
        return -1;

    if(mysql_query(conn, "UPDATE builds SET status = 51 WHERE status = 50"))
        RETURN_ROLLBACK(conn);

    RETURN_COMMIT(conn);
}


/* Start new build on backend */
int handleStep31(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_ROW builds;
    char query[1000];
    char url[500];
    int status;

    if((conn = mysql_autoconnect()) == NULL)
        return -1;

    if(mysql_query(conn, "SELECT builds.id, protocol, host, uri, credentials, portname, buildname, backendkey FROM builds, buildqueue, backends, backendbuilds WHERE builds.queueid = buildqueue.id AND builds.backendid = backends.id AND builds.backendid = backendbuilds.backendid AND builds.group = backendbuilds.buildgroup AND builds.status = 31 FOR UPDATE"))
        RETURN_ROLLBACK(conn);

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
        if(mysql_query(conn, query))
           RETURN_ROLLBACK(conn);
    }

    mysql_free_result(result);

    RETURN_COMMIT(conn);
}


/* Prepare backend for building a new job (checkout portstree, ZFS snapshot, ...) */
int handleStep30(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_ROW builds;
    char query[1000];
    char url[250];
    int status;

    if((conn = mysql_autoconnect()) == NULL)
        return -1;

    if(mysql_query(conn, "SELECT builds.id, protocol, host, uri, credentials, buildname, repository, revision, backendbuilds.id FROM builds, buildqueue, backends, backendbuilds WHERE builds.queueid = buildqueue.id AND builds.backendid = backends.id AND builds.backendid = backendbuilds.backendid AND builds.group = backendbuilds.buildgroup AND builds.status = 30 FOR UPDATE"))
        RETURN_ROLLBACK(conn);

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        sprintf(url, "%s://%s%sstatus?build=%s", builds[1], builds[2], builds[3], builds[5]);
        if(!getpage(url, builds[4]) || strcmp(getenv("STATUS"), "idle") != 0)
        {
           if(getenv("ERROR") != NULL)
              printf("CGI Error: %s\n", getenv("ERROR"));
           
           sprintf(query, "UPDATE backendbuilds SET status = 2 WHERE id = %d", atoi(builds[8]));
           if(mysql_query(conn, query))
              RETURN_ROLLBACK(conn);

           sprintf(query, "UPDATE builds SET status = 90, buildstatus = \"FAIL\" WHERE id = %d", atoi(builds[0]));
           if(mysql_query(conn, query))
              RETURN_ROLLBACK(conn);

           continue;
        }

        sprintf(url, "%s://%s%scheckout?repository=%s&revision=%s&build=%s", builds[1], builds[2], builds[3], builds[6], builds[7], builds[5]);
        if(getpage(url, builds[4]))
           status = 31;
        else
        {
           status = 80;
           printf("CGI Error: %s\n", getenv("ERROR"));
        }

        sprintf(query, "UPDATE builds SET status = %d WHERE id = %d", status, atoi(builds[0]));
        if(mysql_query(conn, query))
           RETURN_ROLLBACK(conn);
    }
         
    mysql_free_result(result);

    RETURN_COMMIT(conn);
}


/* Choose available backend for building */
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
        return -1;

    if(mysql_query(conn, "SELECT builds.id, builds.group, builds.queueid FROM buildqueue, builds WHERE buildqueue.id = builds.queueid AND buildqueue.status = 20 AND builds.backendid = 0 FOR UPDATE"))
        RETURN_ROLLBACK(conn);

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        sprintf(query, "SELECT backendid, maxparallel FROM backendbuilds, backends WHERE backendid = backends.id AND buildgroup = \"%s\" AND backendbuilds.status = 1 AND backends.status = 1 ORDER BY priority", builds[1]);

        if(mysql_query(conn, query))
            RETURN_ROLLBACK(conn);

        result2 = mysql_store_result(conn);

        while((backends = mysql_fetch_row(result2)))
        {
            sprintf(query, "SELECT count(*) FROM builds WHERE backendid = %d AND status < 90", atoi(backends[0]));
            if(mysql_query(conn, query))
                RETURN_ROLLBACK(conn);

            result3 = mysql_store_result(conn);
            runningjobs = mysql_fetch_row(result3);

            if(atoi(runningjobs[0]) < atoi(backends[1]))
            {
                sprintf(query, "UPDATE builds SET backendid = %d, status = 30, startdate = %lli WHERE id = %d", atoi(backends[0]), microtime(), atoi(builds[0]));
                if(mysql_query(conn, query))
                    RETURN_ROLLBACK(conn);
            }

            mysql_free_result(result3);
        }

        mysql_free_result(result2);


        sprintf(query, "SELECT count(*) FROM builds WHERE queueid = \"%s\" AND backendid = 0", builds[2]);
        if(mysql_query(conn, query))
            RETURN_ROLLBACK(conn);

        result2 = mysql_store_result(conn);
        runningjobs = mysql_fetch_row(result2);

        if(atoi(runningjobs[0]) == 0)
        {
            sprintf(query, "UPDATE buildqueue SET status = 30 WHERE id = \"%s\"", builds[2]);
            if(mysql_query(conn, query))
                RETURN_ROLLBACK(conn);
        }
        
        mysql_free_result(result2);
    }

    mysql_free_result(result);

    RETURN_COMMIT(conn);
}


/* Create new jobs based on the buildgroups that the user joined */
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
        return -1;

    if(mysql_query(conn, "SELECT id, owner FROM buildqueue WHERE status = 10 FOR UPDATE"))
        RETURN_ROLLBACK(conn);

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        sprintf(query, "SELECT buildgroup FROM automaticbuildgroups WHERE username = \"%s\" ORDER BY priority", builds[1]);

        if(mysql_query(conn, query))
            RETURN_ROLLBACK(conn);

        result2 = mysql_store_result(conn);

        while((backends = mysql_fetch_row(result2)))
        {
            printf("adding build %s for %s\n", builds[0], builds[1]);
            sprintf(query, "INSERT INTO builds (id, queueid, backendkey, `group`, status, buildstatus, buildreason, buildlog, wrkdir, backendid, startdate, enddate) VALUES (null, \"%s\", SUBSTRING(MD5(RAND()), 1, 25), \"%s\", 10, null, null, null, null, 0, 0, 0)", builds[0], backends[0]);
	    if(mysql_query(conn, query))
                RETURN_ROLLBACK(conn);
        }

        mysql_free_result(result2);

        sprintf(query, "UPDATE buildqueue SET status = 20 WHERE id = \"%s\"", builds[0]);
        if(mysql_query(conn, query))
            RETURN_ROLLBACK(conn);
    }

    mysql_free_result(result);

    RETURN_COMMIT(conn);
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

