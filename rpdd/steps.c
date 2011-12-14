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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>

#include "database.h"
#include "remote.h"
#include "util.h"
#include "steps.h"
#include "stephelper.h"
#include "steputil.h"


/* Update portstrees on backend */
int handleStep102(void)
{
    PGconn *conn;
    PGresult *result;
    char url[250];
    struct tm tm;
    int i;

    int maxage = atoi(configget("tbPortsTreeMaxAge"));

    if((conn = PQautoconnect()) == NULL)
        return 1;

    result = PQexec(conn, "SELECT backends.id, protocol, host, uri, credentials, buildname, backendbuilds.id FROM backends, backendbuilds WHERE backendbuilds.backendid = backends.id AND backends.status = 1 AND backendbuilds.status = 1 FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
    	RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        loginfo("Checking build %s on backend %s", PQgetvalue(result, i, 5), PQgetvalue(result, i, 2));

        sprintf(url, "%s://%s%sstatus?build=%s", PQgetvalue(result, i, 1), PQgetvalue(result, i, 2),
        		PQgetvalue(result, i, 3), PQgetvalue(result, i, 5));
        if(!getpage(url, PQgetvalue(result, i, 4)))
        {
           logcgi(url, getenv("ERROR"));
           continue;
        }

        if((getenv("STATUS") != NULL && strcmp(getenv("STATUS"), "idle") != 0) || getenv("PORTSTREELASTBUILT") == NULL)
           continue;

        if(strptime(getenv("PORTSTREELASTBUILT"), "%Y-%m-%d %H:%M:%S", &tm) == NULL)
        {
           logerror("converting portstreelastbuilt failed!");
           continue;
        }

        if(difftime(time(NULL), mktime(&tm)) < 3600*maxage)
        {
           logdebug("Portstree last built %s does not exceed max. age of %d hours", getenv("PORTSTREELASTBUILT"), maxage);
           continue;
        }

        loginfo("Updating portstree for build %s on backend %s", PQgetvalue(result, i, 5), PQgetvalue(result, i, 2));

        sprintf(url, "%s://%s%supdate?build=%s", PQgetvalue(result, i, 1), PQgetvalue(result, i, 2),
        		PQgetvalue(result, i, 3), PQgetvalue(result, i, 5));
        if(!getpage(url, PQgetvalue(result, i, 4)))
        {
           logcgi(url, getenv("ERROR"));
           continue;
        }

        loginfo("Updating portstree for build %s finished.", PQgetvalue(result, i, 5));

        /* only 1 at a time */
        break;
    }

    PQclear(result);

    RETURN_COMMIT(conn);
}


/* Checking builds */
int handleStep101(void)
{
    PGconn *conn;
    PGresult *result;
    char url[250];
    int status;
    int i;

    if((conn = PQautoconnect()) == NULL)
        return 1;

    result = PQexec(conn, "SELECT backends.id, protocol, host, uri, credentials, buildname, backendbuilds.id FROM backends, backendbuilds WHERE backendbuilds.backendid = backends.id AND backends.status = 1 AND backendbuilds.status = 1 FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
    	RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        logdebug("Checking build %s on backend %s", PQgetvalue(result, i, 5), PQgetvalue(result, i, 2));

        sprintf(url, "%s://%s%sselftest?build=%s", PQgetvalue(result, i, 1), PQgetvalue(result, i, 2),
        		PQgetvalue(result, i, 3), PQgetvalue(result, i, 5));
        if(getpage(url, PQgetvalue(result, i, 4)) == -1)
           status = 2; /* Status failure */
        else
           status = 1; /* Status enabled */

        logdebug("%s is %s", PQgetvalue(result, i, 5), status == 2 ? "not available" : "available");

        if(!PQupdate(conn, "UPDATE backendbuilds SET status = %d WHERE id = %ld", status, atol(PQgetvalue(result, i, 6))))
           RETURN_ROLLBACK(conn);
    }

    PQclear(result);

    RETURN_COMMIT(conn);
}


/* Ping Backends */
int handleStep100(void)
{
    PGconn *conn;
    PGresult *result;
    char url[250];
    int status;
    int i;

    if((conn = PQautoconnect()) == NULL)
        return 1;

    result = PQexec(conn, "SELECT id, protocol, host, uri, credentials FROM backends WHERE status > 0 FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        logdebug("Checking backend %s", PQgetvalue(result, i, 2));

        sprintf(url, "%s://%s%sping", PQgetvalue(result, i, 1), PQgetvalue(result, i, 2), PQgetvalue(result, i, 3));
        if(getpage(url, PQgetvalue(result, i, 4)) == -1)
           status = 2; /* Status failure */
        else
           status = 1; /* Status enabled */

        logdebug("%s is %s", PQgetvalue(result, i, 2), status == 2 ? "not available" : "available");

        if(!PQupdate(conn, "UPDATE backends SET status = %d WHERE id = %ld", status, atol(PQgetvalue(result, i, 0))))
           RETURN_ROLLBACK(conn);
    }

    PQclear(result);

    RETURN_COMMIT(conn);
}


/* Clean filesystem from old logfiles and wrkdirs */
int handleStep99(void)
{
    loginfo("Cleaning old directories in %s", configget("wwwroot"));

    cleanolddir(configget("wwwroot"));

    return 0;
}


/* Clean filesystem and database from deleted builds */
int handleStep98(void)
{
    PGconn *conn;
    PGresult *result;
    PGresult *result2;
    char localdir[PATH_MAX];
    int i;

    if((conn = PQautoconnect()) == NULL)
        return 1;

    result = PQexec(conn, "SELECT builds.id, buildqueue.id, buildqueue.owner FROM builds, buildqueue WHERE builds.queueid = buildqueue.id AND (builds.status = 98 OR buildqueue.status = 98) FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
    	RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        loginfo("Cleaning build %s", PQgetvalue(result, i, 0));

        sprintf(localdir, "%s/%s/%s-%s", configget("wwwroot"), PQgetvalue(result, i, 2), PQgetvalue(result, i, 1), PQgetvalue(result, i, 0));

        if(rmdirrec(localdir) != 0)
           logerror("Failure while deleting %s", localdir);

        if(!PQupdate(conn, "DELETE FROM builds WHERE id = %ld", atol(PQgetvalue(result, i, 0))))
           RETURN_ROLLBACK(conn);

        result2 = PQselect(conn, "SELECT count(*) FROM builds WHERE queueid = '%s'", PQgetvalue(result, i, 1));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK)
           RETURN_ROLLBACK(conn);

        if(atoi(PQgetvalue(result2, 0, 0)) == 0)
        {
           if(!PQupdate(conn, "DELETE FROM buildqueue WHERE id = '%s' AND status >= 90", PQgetvalue(result, i, 1)))
              RETURN_ROLLBACK(conn);
        }
    }

    PQclear(result);

    RETURN_COMMIT(conn);
}

/* Automatically delete builds after $cleandays */
int handleStep96(void)
{
    PGconn *conn;
    PGresult *result;

    unsigned long long cleandays = atoi(configget("cleandays"));
    unsigned long long limit = microtime()-(cleandays*86400*1000000L);

    if((conn = PQautoconnect()) == NULL)
        return 1;

    result = PQselect(conn, "UPDATE buildqueue SET status = 98 WHERE status = 96 AND ( (enddate > 0 AND enddate < %lli) OR (startdate > 0 AND startdate < %lli) )", limit, limit);
    if (PQresultStatus(result) != PGRES_COMMAND_OK)
        RETURN_ROLLBACK(conn);

    if(atoi(PQcmdTuples(result)) > 0)
        loginfo("Updated %s buildqueue entries to status 98", PQcmdTuples(result));

    RETURN_COMMIT(conn);
}


/* Remove non essential files for builds (wrkdir) */
int handleStep95(void)
{
    PGconn *conn;
    PGresult *result;
    PGresult *result2;
    char wrkdir[PATH_MAX];
    int i;

    if((conn = PQautoconnect()) == NULL)
        return 1;

    result = PQexec(conn, "SELECT builds.id, buildqueue.id, buildqueue.owner, wrkdir FROM builds, buildqueue WHERE builds.queueid = buildqueue.id AND (builds.status = 95 OR buildqueue.status = 95) FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
    	RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        loginfo("Archiving build %s", PQgetvalue(result, i, 0));

        sprintf(wrkdir, "%s/%s/%s-%s/%s", configget("wwwroot"), PQgetvalue(result, i, 2), PQgetvalue(result, i, 1), PQgetvalue(result, i, 0), PQgetvalue(result, i, 3));

        if(unlink(wrkdir) != 0)
           logerror("Failure while deleting %s", wrkdir);

        if(!PQupdate(conn, "UPDATE builds SET status = 96 WHERE id = %ld", atol(PQgetvalue(result, i, 0))))
           RETURN_ROLLBACK(conn);

        result2 = PQselect(conn, "SELECT count(*) FROM builds WHERE queueid = '%s' AND status < 98", PQgetvalue(result, i, 1));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK)
           RETURN_ROLLBACK(conn);

        if(atoi(PQgetvalue(result2, 0, 0)) == 0)
        {
           if(!PQupdate(conn, "UPDATE buildqueue SET status = 96 WHERE id = '%s'", PQgetvalue(result, i, 1)))
              RETURN_ROLLBACK(conn);
        }

        PQclear(result2);
    }

    PQclear(result);

    RETURN_COMMIT(conn);
}


/* Automatically mark finished entries as deleted after $archivedays */
int handleStep90(void)
{
    PGconn *conn;
    PGresult *result;

    unsigned long long archivedays = atoi(configget("archivedays"));
    unsigned long long limit = microtime()-(archivedays*86400*1000000L);

    if((conn = PQautoconnect()) == NULL)
        return 1;

    result = PQselect(conn, "UPDATE buildqueue SET status = 95 WHERE status = 90 AND ( (enddate > 0 AND enddate < %lli) OR (startdate > 0 AND startdate < %lli) )", limit, limit);
    if (PQresultStatus(result) != PGRES_COMMAND_OK)
        RETURN_ROLLBACK(conn);

    if(atoi(PQcmdTuples(result)) > 0)
        loginfo("Updated %s buildqueue entries to status 95", PQcmdTuples(result));

    RETURN_COMMIT(conn);
}


/* Clean build on backend after everything was transferred */
int handleStep80(void)
{
    PGconn *conn;
    PGresult *result;
    PGresult *result2;
    PGresult *result3;
    char url[250];
    long priority;
    int i;

    if((conn = PQautoconnect()) == NULL)
        return -1;

    result = PQexec(conn, "SELECT builds.id, backendid, buildgroup, buildqueue.id, buildqueue.priority FROM builds, buildqueue WHERE buildqueue.id = builds.queueid AND builds.status = 80 FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        result2 = PQselect(conn, "SELECT protocol, host, uri, credentials, buildname, backendbuilds.id FROM backends, backendbuilds WHERE backendbuilds.backendid = backends.id AND backends.id = %ld AND buildgroup = '%s'", atol(PQgetvalue(result, i, 1)), PQgetvalue(result, i, 2));
        if (PQresultStatus(result) != PGRES_TUPLES_OK)
           RETURN_ROLLBACK(conn);

        loginfo("Cleaning build %s on backend %s", PQgetvalue(result2, 0, 4), PQgetvalue(result2, 0, 1));

        sprintf(url, "%s://%s%sclean?build=%s", PQgetvalue(result2, 0, 0), PQgetvalue(result2, 0, 1),
        		PQgetvalue(result2, 0, 2), PQgetvalue(result2, 0, 4));
        if(!getpage(url, PQgetvalue(result2, 0, 3)))
        {
            logcgi(url, getenv("ERROR"));

            if(!PQupdate(conn, "UPDATE backendbuilds SET status = 2 WHERE id = %ld", atol(PQgetvalue(result2, 0, 5))))
                RETURN_ROLLBACK(conn);
        }

        if(!PQupdate(conn, "UPDATE builds SET status = 90 WHERE id = %ld", atol(PQgetvalue(result, i, 0))))
           RETURN_ROLLBACK(conn);

        result3 = PQselect(conn, "SELECT (enddate-startdate)/1000000, buildstatus FROM builds WHERE id = %ld", atol(PQgetvalue(result, i, 0)));
        if (PQresultStatus(result3) != PGRES_TUPLES_OK)
           RETURN_ROLLBACK(conn);

        priority = atol(PQgetvalue(result, i, 4));

        if(strcmp(PQgetvalue(result3, 0, 1), "SUCCESS") == 0)
           priority -= 1;
        else
           priority += 1;

        if(atol(PQgetvalue(result3, 0, 0)) < 300)
           priority -= 1;
        else if(atol(PQgetvalue(result3, 0, 0)) > 3000)
           priority += 2;

        if(!PQupdate(conn, "UPDATE buildqueue SET priority = %ld WHERE id = '%s'", priority, PQgetvalue(result, i, 3)))
           RETURN_ROLLBACK(conn);

        PQclear(result3);

        result3 = PQselect(conn, "SELECT count(*) FROM builds WHERE queueid = '%s' AND status < 90", PQgetvalue(result, i, 3));
        if (PQresultStatus(result3) != PGRES_TUPLES_OK)
           RETURN_ROLLBACK(conn);

        if(atoi(PQgetvalue(result3, 0, 0)) == 0)
        {
           if(!PQupdate(conn, "UPDATE buildqueue SET status = 90, enddate = %lli WHERE id = '%s'", microtime(), PQgetvalue(result, i, 3)))
              RETURN_ROLLBACK(conn);
        }

        PQclear(result3);
        PQclear(result2);
    }

    PQclear(result);

    RETURN_COMMIT(conn);
}


/* Transfer logfiles and wrkdirs from backends */
int handleStep71(void)
{
    PGconn *conn;
    PGresult *result;
    PGresult *result2;
    PGresult *result3;
    PGresult *result4;
    PGresult *restmp;
    char url[250];
    char remotefile[255];
    char localfile[PATH_MAX];
    char localdir[PATH_MAX];
    int i;

    if((conn = PQautoconnect()) == NULL)
        return -1;

    result = PQexec(conn, "SELECT id, backendid, buildgroup FROM builds WHERE status = 71");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        result4 = PQselect(conn, "SELECT id FROM builds WHERE id = %ld AND status = 71 FOR UPDATE NOWAIT", atol(PQgetvalue(result, i, 0)));
        if (PQresultStatus(result4) != PGRES_TUPLES_OK)
        {
           logwarn("Could not lock build #%ld. Download already in progress.", atol(PQgetvalue(result, i, 0)));

           restmp = PQexec(conn, "ROLLBACK");
           if (PQresultStatus(restmp) != PGRES_COMMAND_OK)
               RETURN_ROLLBACK(conn);

           restmp = PQexec(conn, "BEGIN");
           if (PQresultStatus(restmp) != PGRES_COMMAND_OK)
               RETURN_ROLLBACK(conn);

           continue;
        }

    	result2 = PQselect(conn, "SELECT protocol, host, uri, credentials, buildname FROM backends, backendbuilds WHERE backendbuilds.backendid = backends.id AND backends.id = %ld AND buildgroup = '%s'", atol(PQgetvalue(result, i, 1)), PQgetvalue(result, i, 2));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK)
           RETURN_ROLLBACK(conn);

        result3 = PQselect(conn, "SELECT owner, queueid FROM buildqueue, builds WHERE builds.queueid = buildqueue.id AND builds.id = %ld", atol(PQgetvalue(result, i, 0)));
        if (PQresultStatus(result3) != PGRES_TUPLES_OK)
           RETURN_ROLLBACK(conn);

        sprintf(url, "%s://%s%sstatus?build=%s", PQgetvalue(result2, 0, 0), PQgetvalue(result2, 0, 1),
        		PQgetvalue(result2, 0, 2), PQgetvalue(result2, 0, 4));
        if(!getpage(url, PQgetvalue(result2, 0, 3)))
        {
           logcgi(url, getenv("ERROR"));
           RETURN_ROLLBACK(conn);
        }

        sprintf(localdir, "%s/%s/%s-%s", configget("wwwroot"), PQgetvalue(result3, i, 0),
        		PQgetvalue(result3, i, 1), PQgetvalue(result, i, 0));
        if(mkdirrec(localdir) != 0)
           RETURN_ROLLBACK(conn);

        if(getenv("BUILDLOG") != NULL)
        {
           sprintf(localfile, "%s/%s", localdir, basename(getenv("BUILDLOG")));
           sprintf(remotefile, "%s://%s%s", PQgetvalue(result2, 0, 0), PQgetvalue(result2, 0, 1), getenv("BUILDLOG"));
           loginfo("Downloading Log %s to %s", remotefile, localfile);
           if(downloadfile(remotefile, PQgetvalue(result2, 0, 3), localfile) != 0){
              logerror("Download of %s failed", remotefile);
              RETURN_ROLLBACK(conn);
           }

           if(!PQupdate(conn, "UPDATE builds SET buildlog = '%s' WHERE id = %ld", basename(localfile), atol(PQgetvalue(result, i, 0))))
              RETURN_ROLLBACK(conn);
        }

        if(getenv("WRKDIR") != NULL)
        {
           sprintf(localfile, "%s/%s", localdir, basename(getenv("WRKDIR")));
           sprintf(remotefile, "%s://%s%s", PQgetvalue(result2, 0, 0), PQgetvalue(result2, 0, 1), getenv("WRKDIR"));
           loginfo("Downloading Wrkdir %s to %s", remotefile, localfile);
           if(downloadfile(remotefile, PQgetvalue(result2, 0, 3), localfile) != 0){
              logerror("Download of %s failed", remotefile);
              RETURN_ROLLBACK(conn);
           }

           if(!PQupdate(conn, "UPDATE builds SET wrkdir = '%s' WHERE id = %ld", basename(localfile), atol(PQgetvalue(result, i, 0))))
              RETURN_ROLLBACK(conn);
        }

        loginfo("Updating build status for build #%ld", atol(PQgetvalue(result, i, 0)));
        if(!PQupdate(conn, "UPDATE builds SET buildstatus = '%s', buildreason = '%s', enddate = %lli, status = 80 WHERE id = %ld",
        		getenv("BUILDSTATUS") != NULL ? getenv("BUILDSTATUS") : "",
        		getenv("FAIL_REASON") != NULL ? getenv("FAIL_REASON") : "", microtime(), atol(PQgetvalue(result, i, 0))))
           RETURN_ROLLBACK(conn);

        PQclear(result3);
        PQclear(result2);
        PQclear(result4);

        /* Only one download at a time */
        break;
    }

    PQclear(result);

    RETURN_COMMIT(conn);
}


/* Start transfers from backends but only one per backend to avoid overloading the link */
int handleStep70(void)
{
    PGconn *conn;
    PGresult *result;
    PGresult *result2;
    int i;

    if((conn = PQautoconnect()) == NULL)
        return -1;

    result = PQexec(conn, "SELECT id, backendid FROM builds WHERE status = 70 FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        result2 = PQselect(conn, "SELECT count(*) FROM builds WHERE status = 71 AND backendid = %ld", atol(PQgetvalue(result, i, 1)));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK)
           RETURN_ROLLBACK(conn);

        if(atoi(PQgetvalue(result2, 0, 0)) == 0)
        {
           loginfo("Starting transfer for build %s", PQgetvalue(result, i, 0));

           if(!PQupdate(conn, "UPDATE builds SET status = 71 WHERE id = %ld", atol(PQgetvalue(result, i, 0))))
               RETURN_ROLLBACK(conn);
        }

        PQclear(result2);
    }

    PQclear(result);

    RETURN_COMMIT(conn);
}


/* Check build status for a running job */
int handleStep51(void)
{
    PGconn *conn;
    PGresult *result;
    PGresult *result2;
    char url[250];
    int i;

    if((conn = PQautoconnect()) == NULL)
        return -1;

    result = PQexec(conn, "SELECT id, backendid, buildgroup FROM builds WHERE status = 51 FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        loginfo("Checking backend for status of build %s", PQgetvalue(result, i, 0));

        result2 = PQselect(conn, "SELECT protocol, host, uri, credentials, buildname, backendbuilds.id FROM backends, backendbuilds WHERE backendbuilds.backendid = backends.id AND backends.id = %ld AND buildgroup = '%s'", atol(PQgetvalue(result, i, 1)), PQgetvalue(result, i, 2));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK)
           RETURN_ROLLBACK(conn);

        sprintf(url, "%s://%s%sstatus?build=%s", PQgetvalue(result2, 0, 0), PQgetvalue(result2, 0, 1),
        		PQgetvalue(result2, 0, 2), PQgetvalue(result2, 0, 4));
        if(!getpage(url, PQgetvalue(result2, 0, 3)))
        {
           logcgi(url, getenv("ERROR"));
           continue;
        }

        if(getenv("STATUS") != NULL && (strcmp(getenv("STATUS"), "finished") == 0 || strcmp(getenv("STATUS"), "idle") == 0))
        {
           if(!updateBackendbuildFailed(conn, atoi(PQgetvalue(result2, 0, 5))))
               RETURN_ROLLBACK(conn);
        }
        else
        {
           if(!PQupdate(conn, "UPDATE builds SET status = 50 WHERE id = %ld", atol(PQgetvalue(result, i, 0))))
               RETURN_ROLLBACK(conn);
        }

        if(!PQupdate(conn, "UPDATE builds SET checkdate = %lli WHERE id = %ld", microtime(), atol(PQgetvalue(result, i, 0))))
           RETURN_ROLLBACK(conn);

        PQclear(result2);
    }

    PQclear(result);

    RETURN_COMMIT(conn);
}


/* Queue running jobs and periodicaly shift them to status 51 for actual checking  */
int handleStep50(void)
{
    PGconn *conn;
    unsigned long long limit;

    if((conn = PQautoconnect()) == NULL)
        return -1;

    /* 0-15min buildtime = check every 30 seconds */
    limit = microtime()-(30*1000000L);
    if(!PQupdate(conn, "UPDATE builds SET status = 51 WHERE status = 50 AND checkdate-startdate < %lli AND checkdate < %lli", 15*60*1000000L, limit))
        RETURN_ROLLBACK(conn);

    /* 15-60min buildtime = check every 60 seconds */
    limit = microtime()-(60*1000000L);
    if(!PQupdate(conn, "UPDATE builds SET status = 51 WHERE status = 50 AND checkdate-startdate < %lli AND checkdate < %lli", 60*60*1000000L, limit))
        RETURN_ROLLBACK(conn);

    /* > 60min buildtime = check every 2 minutes */
    limit = microtime()-(120*1000000L);
    if(!PQupdate(conn, "UPDATE builds SET status = 51 WHERE status = 50 AND checkdate < %lli", limit))
        RETURN_ROLLBACK(conn);

    RETURN_COMMIT(conn);
}


/* Start new build on backend */
int handleStep31(void)
{
    PGconn *conn;
    PGresult *result;
    PGresult *result2;
    char url[500];
    int i;

    if((conn = PQautoconnect()) == NULL)
        return -1;

    result = PQexec(conn, "SELECT id, backendid, buildgroup, backendkey, queueid, portname FROM builds WHERE status = 31 FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        loginfo("Start building for build %s", PQgetvalue(result, i, 0));

        result2 = PQselect(conn, "SELECT protocol, host, uri, credentials, buildname, backendbuilds.id FROM backends, backendbuilds WHERE backendbuilds.backendid = backends.id AND backends.id = %ld AND buildgroup = '%s' FOR UPDATE NOWAIT", atol(PQgetvalue(result, i, 1)), PQgetvalue(result, i, 2));
    	if (PQresultStatus(result2) != PGRES_TUPLES_OK)
            RETURN_ROLLBACK(conn);

        sprintf(url, "%s://%s%sbuild?port=%s&build=%s&priority=%s&finishurl=%s/backend/finished/%s",
        		PQgetvalue(result2, 0, 0), PQgetvalue(result2, 0, 1), PQgetvalue(result2, 0, 2),
        		PQgetvalue(result, i, 5), PQgetvalue(result2, 0, 4), "5", configget("wwwurl"),
        		PQgetvalue(result, i, 3));
        if(getpage(url, PQgetvalue(result2, 0, 3)))
        {
            if(!PQupdate(conn, "UPDATE builds SET status = 50, checkdate = %lli WHERE id = %ld", microtime(), atol(PQgetvalue(result, i, 0))))
               RETURN_ROLLBACK(conn);
        }
        else
        {
            logcgi(url, getenv("ERROR"));

            if(!updateBackendbuildFailed(conn, atol(PQgetvalue(result2, 0, 5))))
               RETURN_ROLLBACK(conn);
        }

        if(getenv("PKGVERSION") != NULL)
        {
            if(!PQupdate(conn, "UPDATE builds SET pkgversion = '%s' WHERE id = %ld", getenv("PKGVERSION"), atol(PQgetvalue(result, i, 0))))
                RETURN_ROLLBACK(conn);
        }

        PQclear(result2);
    }

    PQclear(result);

    RETURN_COMMIT(conn);
}


/* Prepare backend for building a new job (checkout portstree, ZFS snapshot, ...) */
int handleStep30(void)
{
    PGconn *conn;
    PGresult *result;
    PGresult *result2;
    PGresult *result3;
    char url[512];
    int i;

    if((conn = PQautoconnect()) == NULL)
        return -1;

    result = PQexec(conn, "SELECT builds.id, backendid, buildgroup, revision, buildqueue.id FROM builds, buildqueue WHERE builds.queueid = buildqueue.id AND builds.status = 30 FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
    	RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        loginfo("Start checkout for build %s", PQgetvalue(result, i, 0));

        result2 = PQselect(conn, "SELECT protocol, host, uri, credentials, buildname, backendbuilds.id FROM backends, backendbuilds WHERE backendbuilds.backendid = backends.id AND backends.id = %ld AND buildgroup = '%s' FOR UPDATE NOWAIT", atol(PQgetvalue(result, i, 1)), PQgetvalue(result, i, 2));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK)
           RETURN_ROLLBACK(conn);

        sprintf(url, "%s://%s%sstatus?build=%s", PQgetvalue(result2, 0, 0), PQgetvalue(result2, 0, 1),
        		PQgetvalue(result2, 0, 2), PQgetvalue(result2, 0, 4));
        if(!getpage(url, PQgetvalue(result2, 0, 3)) || (getenv("STATUS") != NULL && strcmp(getenv("STATUS"), "idle") != 0))
        {
           if(getenv("ERROR") != NULL)
              logcgi(url, getenv("ERROR"));
           
           if(!updateBackendbuildFailed(conn, atol(PQgetvalue(result2, 0, 5))))
              RETURN_ROLLBACK(conn);

           PQclear(result2);
           continue;
        }

        result3 = PQselect(conn, "SELECT type, replace(replace(replace(replace(url, '%%OWNER%%', owner), '%%PORTNAME%%', portname), '%%QUEUEID%%', buildqueue.id::text), '%%BUILDID%%', builds.id::text) FROM portrepositories, buildqueue, builds WHERE portrepositories.id = repository AND buildqueue.id = queueid AND builds.id = %ld", atol(PQgetvalue(result, i, 0)));
        if (PQresultStatus(result3) != PGRES_TUPLES_OK)
           RETURN_ROLLBACK(conn);

        sprintf(url, "%s://%s%scheckout?type=%s&repository=%s&revision=%s&build=%s", PQgetvalue(result2, 0, 0),
        		PQgetvalue(result2, 0, 1), PQgetvalue(result2, 0, 2), PQgetvalue(result3, 0, 0),
                        PQgetvalue(result3, 0, 1), PQgetvalue(result, i, 3), PQgetvalue(result2, 0, 4));
        if(getpage(url, PQgetvalue(result2, 0, 3)))
        {
           if(!PQupdate(conn, "UPDATE builds SET status = 31 WHERE id = %ld", atol(PQgetvalue(result, i, 0))))
              RETURN_ROLLBACK(conn);
        }
        else
        {
           logcgi(url, getenv("ERROR"));
           if(!updateBackendbuildFailed(conn, atol(PQgetvalue(result2, 0, 5))))
              RETURN_ROLLBACK(conn);
        }

        if(getenv("REVISION") != NULL)
        {
           if(!PQupdate(conn, "UPDATE buildqueue SET revision = %ld WHERE id = '%s'", atol(getenv("REVISION")), PQgetvalue(result, i, 4)))
              RETURN_ROLLBACK(conn);
        }

        PQclear(result3);
        PQclear(result2);
    }
         
    PQclear(result);

    RETURN_COMMIT(conn);
}


/* Choose available backend for building */
int handleStep20(void)
{
    PGconn *conn;
    PGresult *result;
    PGresult *result2;
    PGresult *result3;
    PGresult *result4;
    int i, j;

    if((conn = PQautoconnect()) == NULL)
        return -1;

    result = PQexec(conn, "SELECT builds.id, builds.buildgroup, builds.queueid FROM buildqueue, builds WHERE buildqueue.id = builds.queueid AND buildqueue.status = 20 AND builds.status = 20 AND builds.backendid = 0 ORDER BY priority, builds.id DESC FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
    	RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        result2 = PQselect(conn, "SELECT backendid, maxparallel FROM backendbuilds, backends WHERE backendid = backends.id AND buildgroup = '%s' AND backendbuilds.status = 1 AND backends.status = 1 ORDER BY priority", PQgetvalue(result, i, 1));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK)
            RETURN_ROLLBACK(conn);

        for(j=0; j < PQntuples(result2); j++)
        {
            result3 = PQselect(conn, "SELECT count(*) FROM builds WHERE backendid = %ld AND status < 70", atol(PQgetvalue(result2, j, 0)));
            if (PQresultStatus(result3) != PGRES_TUPLES_OK)
                RETURN_ROLLBACK(conn);

            if(atoi(PQgetvalue(result3, 0, 0)) < atoi(PQgetvalue(result2, j, 1)))
            {
                result4 = PQselect(conn, "SELECT count(*) FROM builds WHERE backendid = %ld AND buildgroup = '%s' AND status >= 30 AND status < 90", atol(PQgetvalue(result2, j, 0)), PQgetvalue(result, i, 1));
                if (PQresultStatus(result4) != PGRES_TUPLES_OK)
                    RETURN_ROLLBACK(conn);

                logdebug("Running %d jobs for group %s", atoi(PQgetvalue(result4, 0, 0)), PQgetvalue(result, i, 1));

                if(atoi(PQgetvalue(result4, 0, 0)) == 0)
                {
                    loginfo("Choose backend %s for build %s", PQgetvalue(result2, j, 0), PQgetvalue(result, i, 0));

                    if(!PQupdate(conn, "UPDATE builds SET backendid = %ld, status = 30, startdate = %lli WHERE id = %ld", atol(PQgetvalue(result2, j, 0)), microtime(), atol(PQgetvalue(result, i, 0))))
                        RETURN_ROLLBACK(conn);
                }

                PQclear(result4);
            }

            PQclear(result3);
        }

        PQclear(result2);


        result2 = PQselect(conn, "SELECT count(*) FROM builds WHERE queueid = '%s' AND backendid = 0", PQgetvalue(result, i, 2));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK)
            RETURN_ROLLBACK(conn);

        if(atoi(PQgetvalue(result2, 0, 0)) == 0)
        {
            if(!PQupdate(conn, "UPDATE buildqueue SET status = 30 WHERE id = '%s'", PQgetvalue(result, i, 2)))
                RETURN_ROLLBACK(conn);
        }
        
        PQclear(result2);
    }

    PQclear(result);

    RETURN_COMMIT(conn);
}

