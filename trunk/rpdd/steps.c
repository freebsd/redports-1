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


/* Recover builds on a backend */
int handleStep104(void)
{
    PGconn *conn;
    PGresult *result;
    PGresult *result2;
    char url[250];
    int i;
    int newstatus;

    if((conn = PQautoconnect()) == NULL)
        return 1;

    result = PQexec(conn, "SELECT id, buildname, backendid FROM backendbuilds WHERE backendbuilds.status = 4 FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
    {
        if (strcmp(PQgetErrorCode(result), PQERROR_LOCK_NOT_AVAILABLE) == 0)
            RETURN_COMMIT(conn);

    	RETURN_ROLLBACK(conn);
    }

    for(i=0; i < PQntuples(result); i++)
    {
        result2 = PQselect(conn, "SELECT id, status, protocol, host, uri, credentials FROM backends WHERE id = %ld", atol(PQgetvalue(result, i, 2)));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK)
           RETURN_ROLLBACK(conn);

        if(atoi(PQgetvalue(result2, 0, 1)) != 1)
        {
            loginfo("Backend %s is disabled. Cannot recover build.", PQgetvalue(result, i, 2));
            continue;
        }

        loginfo("Recovering build %s on backend %s", PQgetvalue(result, i, 1), PQgetvalue(result, i, 2));

        sprintf(url, "%s://%s%srecover?build=%s", PQgetvalue(result2, 0, 2), PQgetvalue(result2, 0, 3),
                        PQgetvalue(result2, 0, 4), PQgetvalue(result, i, 1));
        if(getpage(url, PQgetvalue(result2, 0, 5), REMOTE_SHORTTIMEOUT) != CURLE_OK)
        {
           logcgi(url, getenv("ERROR"));
           newstatus = 2;
        }
        else
           newstatus = 1;

        if(!PQupdate(conn, "UPDATE backendbuilds SET status = %d WHERE id = %ld", newstatus, atol(PQgetvalue(result, i, 0))))
           RETURN_ROLLBACK(conn);

        PQclear(result2);

        /* only 1 at a time */
        break;
    }

    PQclear(result);

    RETURN_COMMIT(conn);
}

/* Update portstrees on backend */
int handleStep103(void)
{
    PGconn *conn;
    PGresult *result;
    PGresult *result2;
    char url[250];
    int i;

    if((conn = PQautoconnect()) == NULL)
        return 1;

    result = PQexec(conn, "SELECT id, buildname, backendid FROM backendbuilds WHERE backendbuilds.status = 3 FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
    {
        if (strcmp(PQgetErrorCode(result), PQERROR_LOCK_NOT_AVAILABLE) == 0)
            RETURN_COMMIT(conn);

    	RETURN_ROLLBACK(conn);
    }

    for(i=0; i < PQntuples(result); i++)
    {
        result2 = PQselect(conn, "SELECT id, status, protocol, host, uri, credentials FROM backends WHERE id = %ld", atol(PQgetvalue(result, i, 2)));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK)
           RETURN_ROLLBACK(conn);

        if(atoi(PQgetvalue(result2, 0, 1)) != 1)
        {
            loginfo("Backend %s is disabled. Cannot update portstree.", PQgetvalue(result, i, 2));
            continue;
        }

        loginfo("Checking build %s on backend %s", PQgetvalue(result, i, 1), PQgetvalue(result, i, 2));

        sprintf(url, "%s://%s%sstatus?build=%s", PQgetvalue(result2, 0, 2), PQgetvalue(result2, 0, 3),
        		PQgetvalue(result2, 0, 4), PQgetvalue(result, i, 1));
        if(getpage(url, PQgetvalue(result2, 0, 5), REMOTE_SHORTTIMEOUT) != CURLE_OK)
        {
           logcgi(url, getenv("ERROR"));
           continue;
        }

        if((getenv("STATUS") != NULL && strcmp(getenv("STATUS"), "idle") != 0) || getenv("PORTSTREELASTBUILT") == NULL)
           continue;

        loginfo("Updating portstree for build %s on backend %s", PQgetvalue(result, i, 1), PQgetvalue(result, i, 2));

        sprintf(url, "%s://%s%supdate?build=%s", PQgetvalue(result2, 0, 2), PQgetvalue(result2, 0, 3),
        		PQgetvalue(result2, 0, 4), PQgetvalue(result, i, 1));
        if(getpage(url, PQgetvalue(result2, 0, 5), REMOTE_NOTIMEOUT) != CURLE_OK)
        {
           logcgi(url, getenv("ERROR"));
           continue;
        }

        loginfo("Updating portstree for build %s finished.", PQgetvalue(result, i, 1));

        if(!PQupdate(conn, "UPDATE backendbuilds SET status = 1 WHERE id = %ld", atol(PQgetvalue(result, i, 0))))
           RETURN_ROLLBACK(conn);


        PQclear(result2);

        /* only 1 at a time */
        break;
    }

    PQclear(result);

    RETURN_COMMIT(conn);
}


/* Finding backendbuilds for updating the portstree */
int handleStep102(void)
{
    PGconn *conn;
    PGresult *reslock;
    PGresult *result;
    PGresult *result2;
    char url[250];
    struct tm tm;
    int i;

    int maxage = atoi(configget("tbPortsTreeMaxAge"));

    if((conn = PQautoconnect()) == NULL)
        return 1;

    result = PQexec(conn, "SELECT backends.id, protocol, host, uri, credentials, buildname, backendbuilds.id FROM backends, backendbuilds WHERE backendbuilds.backendid = backends.id AND backends.status = 1 AND backendbuilds.status = 1");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        reslock = PQexec(conn, "SAVEPOINT lastbackendbuild");
        if (PQresultStatus(reslock) != PGRES_COMMAND_OK)
            RETURN_ROLLBACK(conn);

        reslock = PQselect(conn, "SELECT id FROM backendbuilds WHERE id = %ld FOR UPDATE NOWAIT", atol(PQgetvalue(result, i, 6)));
        if (PQresultStatus(reslock) != PGRES_TUPLES_OK || PQntuples(reslock) != 1)
        {
            if (strcmp(PQgetErrorCode(reslock), PQERROR_LOCK_NOT_AVAILABLE) == 0)
            {
                logwarn("Could not lock backendbuild %s. Continuing.", PQgetvalue(result, i, 6));
           
                reslock = PQexec(conn, "ROLLBACK TO lastbackendbuild");
                if (PQresultStatus(reslock) != PGRES_COMMAND_OK)
                    RETURN_ROLLBACK(conn);

                continue;
            }
        
            RETURN_ROLLBACK(conn);
        }

        result2 = PQselect(conn, "SELECT count(*) FROM builds WHERE status > 20 AND status < 70 AND backendid = %ld AND buildgroup = '%s'", atol(PQgetvalue(result, i, 0)), PQgetvalue(result, i, 5));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK)
           RETURN_ROLLBACK(conn);

        if(atoi(PQgetvalue(result2, 0, 0)) > 0)
           continue;

        sprintf(url, "%s://%s%sstatus?build=%s", PQgetvalue(result, i, 1), PQgetvalue(result, i, 2),
                        PQgetvalue(result, i, 3), PQgetvalue(result, i, 5));
        if(getpage(url, PQgetvalue(result, i, 4), REMOTE_SHORTTIMEOUT) != CURLE_OK)
        {
           logcgi(url, getenv("ERROR"));
           break;
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

        if(!PQupdate(conn, "UPDATE backendbuilds SET status = 3 WHERE id = %ld", atol(PQgetvalue(result, i, 6))))
           RETURN_ROLLBACK(conn);

        PQclear(result2);
        PQclear(reslock);
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
        if(getpage(url, PQgetvalue(result, i, 4), REMOTE_SHORTTIMEOUT) != CURLE_OK)
        {
           logwarn("%s is not available", PQgetvalue(result, i, 5));

           if(updateBackendbuildFailed(conn, atoi(PQgetvalue(result, i, 6))) != 0)
              RETURN_ROLLBACK(conn);
        }
        else
           logdebug("%s is available", PQgetvalue(result, i, 5));
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

    result = PQexec(conn, "SELECT id, protocol, host, uri, credentials FROM backends WHERE status = 1 FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        logdebug("Checking backend %s", PQgetvalue(result, i, 2));

        sprintf(url, "%s://%s%sping", PQgetvalue(result, i, 1), PQgetvalue(result, i, 2), PQgetvalue(result, i, 3));
        if(getpage(url, PQgetvalue(result, i, 4), REMOTE_SHORTTIMEOUT) != CURLE_OK)
        {
           status = 2; /* Status failure */
           logcgi(url, getenv("ERROR"));
           logwarn("Backend %s failed", PQgetvalue(result, i, 2));

           if(updateBackendFailed(conn, atoi(PQgetvalue(result, i, 0))) != 0)
              RETURN_ROLLBACK(conn);
        }
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

        if(strlen(PQgetvalue(result, i, 3)) > 0)
        {
            sprintf(wrkdir, "%s/%s/%s-%s/%s", configget("wwwroot"), PQgetvalue(result, i, 2), PQgetvalue(result, i, 1), PQgetvalue(result, i, 0), PQgetvalue(result, i, 3));

            if(unlink(wrkdir) != 0)
                logerror("Failure while deleting %s", wrkdir);
        }

        if(!PQupdate(conn, "UPDATE builds SET status = 96 WHERE id = %ld", atol(PQgetvalue(result, i, 0))))
           RETURN_ROLLBACK(conn);

        result2 = PQselect(conn, "SELECT count(*) FROM builds WHERE queueid = '%s' AND status < 96", PQgetvalue(result, i, 1));
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
int handleStep91(void)
{
    PGconn *conn;
    PGresult *result;

    unsigned long long archivedays = atoi(configget("archivedays"));
    unsigned long long limit = microtime()-(archivedays*86400*1000000L);

    if((conn = PQautoconnect()) == NULL)
        return 1;

    result = PQselect(conn, "UPDATE buildqueue SET status = 95 WHERE status = 91 AND ( (enddate > 0 AND enddate < %lli) OR (startdate > 0 AND startdate < %lli) )", limit, limit);
    if (PQresultStatus(result) != PGRES_COMMAND_OK)
        RETURN_ROLLBACK(conn);

    if(atoi(PQcmdTuples(result)) > 0)
        loginfo("Updated %s buildqueue entries to status 95", PQcmdTuples(result));

    RETURN_COMMIT(conn);
}

/* Send notifications for all finished builds */
int handleStep90(void)
{
    PGconn *conn;
    PGresult *result;
    char url[250];
    int i;

    if((conn = PQautoconnect()) == NULL)
        return -1;

    result = PQexec(conn, "SELECT id FROM buildqueue WHERE status = 90 FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        loginfo("Sending notifications for build %s", PQgetvalue(result, i, 0));

        sprintf(url, "%s/backend/notify/%s", configget("wwwurl"), PQgetvalue(result, i, 0));
        if(getpage(url, NULL, REMOTE_TIMEOUT) != CURLE_OK)
            logcgi(url, getenv("ERROR"));

        if(!PQupdate(conn, "UPDATE buildqueue SET status = 91 WHERE id = '%s'", PQgetvalue(result, i, 0)))
           RETURN_ROLLBACK(conn);
    }
    PQclear(result);

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
    int i;

    if((conn = PQautoconnect()) == NULL)
        return -1;

    result = PQexec(conn, "SELECT builds.id, backendid, buildgroup, buildqueue.id FROM builds, buildqueue WHERE buildqueue.id = builds.queueid AND builds.status = 80 FOR UPDATE NOWAIT");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        result2 = PQselect(conn, "SELECT protocol, host, uri, credentials, buildname, backendbuilds.id FROM backends, backendbuilds WHERE backendbuilds.backendid = backends.id AND backends.id = %ld AND buildgroup = '%s'", atol(PQgetvalue(result, i, 1)), PQgetvalue(result, i, 2));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK || PQntuples(result2) != 1)
           RETURN_ROLLBACK(conn);

        loginfo("Cleaning build %s on backend %s", PQgetvalue(result2, 0, 4), PQgetvalue(result2, 0, 1));

        sprintf(url, "%s://%s%sclean?build=%s", PQgetvalue(result2, 0, 0), PQgetvalue(result2, 0, 1),
        		PQgetvalue(result2, 0, 2), PQgetvalue(result2, 0, 4));
        if(getpage(url, PQgetvalue(result2, 0, 3), REMOTE_NOTIMEOUT) != CURLE_OK)
        {
            logcgi(url, getenv("ERROR"));

            if(!PQupdate(conn, "UPDATE backendbuilds SET status = 2 WHERE id = %ld", atol(PQgetvalue(result2, 0, 5))))
                RETURN_ROLLBACK(conn);
        }

        loginfo("Checking build %s on backend %s", PQgetvalue(result2, 0, 4), PQgetvalue(result2, 0, 1));

        sprintf(url, "%s://%s%sstatus?build=%s", PQgetvalue(result2, 0, 0), PQgetvalue(result2, 0, 1),
        		PQgetvalue(result2, 0, 2), PQgetvalue(result2, 0, 4));
        if(getpage(url, PQgetvalue(result2, 0, 3), REMOTE_SHORTTIMEOUT) != CURLE_OK)
        {
            logcgi(url, getenv("ERROR"));

            if(!PQupdate(conn, "UPDATE backendbuilds SET status = 2 WHERE id = %ld", atol(PQgetvalue(result2, 0, 5))))
                RETURN_ROLLBACK(conn);
        }

        if(recalcBuildPriority(conn, atol(PQgetvalue(result, i, 0))) != 0)
           RETURN_ROLLBACK(conn);

        if(!PQupdate(conn, "UPDATE builds SET status = 90 WHERE id = %ld", atol(PQgetvalue(result, i, 0))))
           RETURN_ROLLBACK(conn);

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
    char *localfilesql;
    char *buildstatus;
    char *failreason;
    char url[250];
    char remotefile[255];
    char localfile[PATH_MAX];
    char localdir[PATH_MAX];
    int i;

    if((conn = PQautoconnect()) == NULL)
        return -1;

    result = PQexec(conn, "SELECT id, backendid, buildgroup, portname, pkgversion FROM builds WHERE status = 71");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        result4 = PQselect(conn, "SELECT id FROM builds WHERE id = %ld AND status = 71 FOR UPDATE NOWAIT", atol(PQgetvalue(result, i, 0)));
        if (PQresultStatus(result4) != PGRES_TUPLES_OK || PQntuples(result4) != 1)
        {
           logwarn("Could not lock build %ld. Download already in progress.", atol(PQgetvalue(result, i, 0)));

           restmp = PQexec(conn, "ROLLBACK");
           if (PQresultStatus(restmp) != PGRES_COMMAND_OK)
               RETURN_ROLLBACK(conn);

           restmp = PQexec(conn, "BEGIN");
           if (PQresultStatus(restmp) != PGRES_COMMAND_OK)
               RETURN_ROLLBACK(conn);

           continue;
        }

    	result2 = PQselect(conn, "SELECT protocol, host, uri, credentials, buildname FROM backends, backendbuilds WHERE backendbuilds.backendid = backends.id AND backends.id = %ld AND buildgroup = '%s'", atol(PQgetvalue(result, i, 1)), PQgetvalue(result, i, 2));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK || PQntuples(result2) != 1)
           RETURN_ROLLBACK(conn);

        result3 = PQselect(conn, "SELECT owner, queueid FROM buildqueue, builds WHERE builds.queueid = buildqueue.id AND builds.id = %ld", atol(PQgetvalue(result, i, 0)));
        if (PQresultStatus(result3) != PGRES_TUPLES_OK || PQntuples(result3) != 1)
           RETURN_ROLLBACK(conn);

        sprintf(url, "%s://%s%sstatus?build=%s", PQgetvalue(result2, 0, 0), PQgetvalue(result2, 0, 1),
        		PQgetvalue(result2, 0, 2), PQgetvalue(result2, 0, 4));
        if(getpage(url, PQgetvalue(result2, 0, 3), REMOTE_SHORTTIMEOUT) != CURLE_OK)
        {
           logcgi(url, getenv("ERROR"));
           RETURN_ROLLBACK(conn);
        }

        sprintf(localdir, "%s/%s/%s-%s", configget("wwwroot"), PQgetvalue(result3, 0, 0),
        		PQgetvalue(result3, 0, 1), PQgetvalue(result, i, 0));
        if(mkdirrec(localdir) != 0)
           RETURN_ROLLBACK(conn);

        if(getenv("BUILDLOG") != NULL)
        {
           sprintf(localfile, "%s/%s", localdir, basename(getenv("BUILDLOG")));
           sprintf(remotefile, "%s://%s%s", PQgetvalue(result2, 0, 0), PQgetvalue(result2, 0, 1), getenv("BUILDLOG"));
           loginfo("Downloading Log %s to %s", remotefile, localfile);
           if(downloadfile(remotefile, PQgetvalue(result2, 0, 3), localfile) != CURLE_OK){
              logerror("Download of %s failed", remotefile);
              RETURN_ROLLBACK(conn);
           }

           localfilesql = PQescapeLiteral(conn, basename(localfile), 50);

           if(!PQupdate(conn, "UPDATE builds SET buildlog = %s WHERE id = %ld", localfilesql, atol(PQgetvalue(result, i, 0))))
              RETURN_ROLLBACK(conn);

           PQfreemem(localfilesql);
        }

        if(getenv("WRKDIR") != NULL)
        {
           restmp = PQselect(conn, "SELECT count(*) FROM session_attribute WHERE sid = '%s' AND name = 'build_wrkdirdownload'", PQgetvalue(result3, 0, 0));
        if (PQresultStatus(restmp) != PGRES_TUPLES_OK)
           RETURN_ROLLBACK(conn);

           if(atol(PQgetvalue(restmp, 0, 0)) > 0)
           {
              sprintf(localfile, "%s/%s", localdir, basename(getenv("WRKDIR")));
              sprintf(remotefile, "%s://%s%s", PQgetvalue(result2, 0, 0), PQgetvalue(result2, 0, 1), getenv("WRKDIR"));
              loginfo("Downloading Wrkdir %s to %s", remotefile, localfile);
              if(downloadfile(remotefile, PQgetvalue(result2, 0, 3), localfile) != CURLE_OK){
                 logerror("Download of %s failed", remotefile);
                 RETURN_ROLLBACK(conn);
              }

              localfilesql = PQescapeLiteral(conn, basename(localfile), 50);

              if(!PQupdate(conn, "UPDATE builds SET wrkdir = %s WHERE id = %ld", localfilesql, atol(PQgetvalue(result, i, 0))))
                 RETURN_ROLLBACK(conn);

              PQfreemem(localfilesql);
           }

           PQclear(restmp);
        }

        if(getenv("BUILDSTATUS") != NULL)
           buildstatus = PQescapeLiteral(conn, getenv("BUILDSTATUS"), 25);
        else
           buildstatus = PQescapeLiteral(conn, "", 0);

        if(getenv("FAIL_REASON") != NULL)
           failreason = PQescapeLiteral(conn, getenv("FAIL_REASON"), 255);
        else
           failreason = PQescapeLiteral(conn, "", 0);

        loginfo("Updating build status for build %ld", atol(PQgetvalue(result, i, 0)));
        if(!PQupdate(conn, "UPDATE builds SET buildstatus = %s, buildreason = %s, enddate = %lli, status = 80 WHERE id = %ld", buildstatus, failreason, microtime(), atol(PQgetvalue(result, i, 0))))
           RETURN_ROLLBACK(conn);

        setenv("RPSTATUS", "71", 1);
        setenv("RPOWNER", PQgetvalue(result3, 0, 0), 1);
        setenv("RPBUILDID", PQgetvalue(result, i, 0), 1);
        setenv("RPBUILDQUEUEID", PQgetvalue(result3, i, 1), 1);
        setenv("RPBUILDGROUP", PQgetvalue(result, i, 2), 1);
        setenv("RPPORTNAME", PQgetvalue(result, i, 3), 1);
        setenv("RPPKGVERSION", PQgetvalue(result, i, 4), 1);
        setenv("RPBUILDSTATUS", getenv("BUILDSTATUS") != NULL ? getenv("BUILDSTATUS") : "", 1);
        setenv("RPBUILDREASON", getenv("FAIL_REASON") != NULL ? getenv("FAIL_REASON") : "", 1);
        setenv("RPBUILDLOG", "", 1);
        if(getenv("BUILDLOG") != NULL)
            setenv("RPBUILDLOG", basename(getenv("BUILDLOG")), 1);
        callHook("BUILD_FINISHED");

        PQfreemem(failreason);
        PQfreemem(buildstatus);
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
        if (PQresultStatus(result2) != PGRES_TUPLES_OK || PQntuples(result2) != 1)
           RETURN_ROLLBACK(conn);

        sprintf(url, "%s://%s%sstatus?build=%s", PQgetvalue(result2, 0, 0), PQgetvalue(result2, 0, 1),
        		PQgetvalue(result2, 0, 2), PQgetvalue(result2, 0, 4));
        if(getpage(url, PQgetvalue(result2, 0, 3), REMOTE_SHORTTIMEOUT) != CURLE_OK)
        {
           logcgi(url, getenv("ERROR"));
           PQclear(result2);
           continue;
        }

        if(getenv("STATUS") != NULL && (strcmp(getenv("STATUS"), "finished") == 0 || strcmp(getenv("STATUS"), "idle") == 0))
        {
           if(!PQupdate(conn, "UPDATE builds SET status = 70 WHERE id = %ld", atol(PQgetvalue(result, i, 0))))
               RETURN_ROLLBACK(conn);
        }
        else if(getenv("STATUS") != NULL && strcmp(getenv("STATUS"), "building") == 0)
        {
           if(!PQupdate(conn, "UPDATE builds SET status = 50 WHERE id = %ld", atol(PQgetvalue(result, i, 0))))
               RETURN_ROLLBACK(conn);
        }
        else if(getenv("STATUS") == NULL)
        {
           logwarn("Backendbuild %s has no status", PQgetvalue(result2, 0, 4));
           PQclear(result2);
           continue;
        }
        else
        {
           logwarn("Backendbuild %s has invalid status %s", PQgetvalue(result2, 0, 4), getenv("STATUS") != NULL ? getenv("STATUS") : "UNKNOWN");

           if(updateBackendbuildFailed(conn, atoi(PQgetvalue(result2, 0, 5))) != 0)
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
    PGresult *result;
    unsigned long long limit1;
    unsigned long long limit2;
    unsigned long long limit3;
    int i;

    if((conn = PQautoconnect()) == NULL)
        return -1;

    /* 0-15min buildtime = check every 30 seconds */
    limit1 = microtime()-(30*1000000L);

    /* 15-60min buildtime = check every 60 seconds */
    limit2 = microtime()-(60*1000000L);

    /* > 60min buildtime = check every 2 minutes */
    limit3 = microtime()-(120*1000000L);

    result = PQselect(conn, "SELECT id, backendid FROM builds WHERE status = 50 AND ((checkdate-startdate < 900000000 AND checkdate < %lli) OR (checkdate-startdate < 3600000000 AND checkdate < %lli) OR (checkdate < %lli)) FOR UPDATE NOWAIT", limit1, limit2, limit3);
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        loginfo("Updating build %s to 51", PQgetvalue(result, i, 0));
        if(!PQupdate(conn, "UPDATE builds SET status = 51 WHERE id = %ld", atol(PQgetvalue(result, i, 0))))
            RETURN_ROLLBACK(conn);
    }

    PQclear(result);

    RETURN_COMMIT(conn);
}


/* Start new build on backend */
int handleStep31(void)
{
    PGconn *conn;
    PGresult *result;
    PGresult *result2;
    PGresult *result3;
    char *pkgversion;
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
    	if (PQresultStatus(result2) != PGRES_TUPLES_OK || PQntuples(result2) != 1)
            RETURN_ROLLBACK(conn);

        result3 = PQselect(conn, "SELECT owner FROM buildqueue WHERE id = '%s'", PQgetvalue(result, i, 4));
    	if (PQresultStatus(result3) != PGRES_TUPLES_OK || PQntuples(result3) != 1)
            RETURN_ROLLBACK(conn);
        

        setenv("RPSTATUS", "31", 1);
        setenv("RPOWNER", PQgetvalue(result3, 0, 0), 1);
        setenv("RPBUILDID", PQgetvalue(result, i, 0), 1);
        setenv("RPBUILDQUEUEID", PQgetvalue(result, i, 4), 1);
        setenv("RPBUILDGROUP", PQgetvalue(result, i, 2), 1);
        setenv("RPPORTNAME", PQgetvalue(result, i, 5), 1);
        setenv("RPPKGVERSION", "", 1);
        setenv("RPBUILDSTATUS", "", 1);
        setenv("RPBUILDREASON", "", 1);
        setenv("RPBUILDLOG", "", 1);
        callHook("BUILD_STARTED");

        sprintf(url, "%s://%s%sbuild?port=%s&build=%s&priority=%s&finishurl=%s/backend/finished/%s",
        		PQgetvalue(result2, 0, 0), PQgetvalue(result2, 0, 1), PQgetvalue(result2, 0, 2),
        		PQgetvalue(result, i, 5), PQgetvalue(result2, 0, 4), "5", configget("wwwurl"),
        		PQgetvalue(result, i, 3));
        if(getpage(url, PQgetvalue(result2, 0, 3), REMOTE_TIMEOUT) == CURLE_OK)
        {
            if(!PQupdate(conn, "UPDATE builds SET status = 50, checkdate = %lli WHERE id = %ld", microtime(), atol(PQgetvalue(result, i, 0))))
               RETURN_ROLLBACK(conn);
        }
        else
        {
            logcgi(url, getenv("ERROR"));

            if(updateBackendbuildFailed(conn, atol(PQgetvalue(result2, 0, 5))) != 0)
               RETURN_ROLLBACK(conn);
        }

        if(getenv("PKGVERSION") != NULL)
        {
            pkgversion = PQescapeLiteral(conn, getenv("PKGVERSION"), 25);

            if(!PQupdate(conn, "UPDATE builds SET pkgversion = %s WHERE id = %ld OR (queueid = '%s' AND portname = '%s' AND pkgversion IS NULL)", pkgversion, atol(PQgetvalue(result, i, 0)), PQgetvalue(result, i, 4), PQgetvalue(result, i, 5)))
                RETURN_ROLLBACK(conn);

            PQfreemem(pkgversion);
        }

        PQclear(result3);
        PQclear(result2);

        /* we've successfully started a build so be sure to commit */
        break;
    }

    PQclear(result);

    RETURN_COMMIT(conn);
}


/* Prepare backend for building a new job (checkout portstree, ZFS snapshot, ...) */
int handleStep30(void)
{
    PGconn *conn;
    PGresult *reslock;
    PGresult *result;
    PGresult *result2;
    PGresult *result3;
    PGresult *result4;
    char url[512];
    int i;

    if((conn = PQautoconnect()) == NULL)
        return -1;

    result = PQexec(conn, "SELECT builds.id, backendid, buildgroup, revision, buildqueue.id FROM builds, buildqueue WHERE builds.queueid = buildqueue.id AND builds.status = 30");
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
    	RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        loginfo("Trying to lock build %s / %ld", PQgetvalue(result, i, 4), PQgetvalue(result, i, 0));

        reslock = PQselect(conn, "SELECT id FROM buildqueue WHERE id = '%s' FOR UPDATE NOWAIT", PQgetvalue(result, i, 4));
        if (PQresultStatus(reslock) != PGRES_TUPLES_OK || PQntuples(reslock) != 1)
        {
            if (strcmp(PQgetErrorCode(reslock), PQERROR_LOCK_NOT_AVAILABLE) == 0)
            {
                logwarn("Could not lock buildqueue entry %s. Continuing.", PQgetvalue(result, i, 4));

                reslock = PQexec(conn, "ROLLBACK");
                if (PQresultStatus(reslock) != PGRES_COMMAND_OK)
                    RETURN_ROLLBACK(conn);

                reslock = PQexec(conn, "BEGIN");
                if (PQresultStatus(reslock) != PGRES_COMMAND_OK)
                    RETURN_ROLLBACK(conn);

                continue;
            }

            RETURN_ROLLBACK(conn);
        }

        loginfo("Trying to lock backend %s for buildgroup %s", PQgetvalue(result, i, 1), PQgetvalue(result, i, 2));

        result2 = PQselect(conn, "SELECT protocol, host, uri, credentials, buildname, backendbuilds.id FROM backends, backendbuilds WHERE backendbuilds.backendid = backends.id AND backends.id = %ld AND buildgroup = '%s' FOR UPDATE NOWAIT", atol(PQgetvalue(result, i, 1)), PQgetvalue(result, i, 2));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK || PQntuples(result2) != 1)
           RETURN_ROLLBACK(conn);

        result4 = PQselect(conn, "SELECT count(*) FROM builds WHERE backendid = %ld AND buildgroup = '%s' AND status >= 30 AND status < 90", atol(PQgetvalue(result, i, 0)), PQgetvalue(result, i, 2));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK || PQntuples(result2) != 1)
           RETURN_ROLLBACK(conn);

        if(atoi(PQgetvalue(result4, 0, 0)) > 0)
        {
            logwarn("Already some builds running on backendbuild %s", PQgetvalue(result, i, 2));
            continue;
        }
         
        sprintf(url, "%s://%s%sstatus?build=%s", PQgetvalue(result2, 0, 0), PQgetvalue(result2, 0, 1),
        		PQgetvalue(result2, 0, 2), PQgetvalue(result2, 0, 4));
        if(getpage(url, PQgetvalue(result2, 0, 3), REMOTE_SHORTTIMEOUT) != CURLE_OK || (getenv("STATUS") != NULL && strcmp(getenv("STATUS"), "idle") != 0))
        {
           if(getenv("STATUS") != NULL && strcmp(getenv("STATUS"), "busy") == 0)
              logwarn("Status of backendbuild %s should be IDLE is: %s", PQgetvalue(result2, 0, 4), getenv("STATUS"));

           if(getenv("ERROR") != NULL)
              logcgi(url, getenv("ERROR"));
           
           if(updateBackendbuildFailed(conn, atol(PQgetvalue(result2, 0, 5))) != 0)
              RETURN_ROLLBACK(conn);

           PQclear(result2);
           break;
        }

        result3 = PQselect(conn, "SELECT type, replace(replace(replace(replace(url, '%%OWNER%%', owner), '%%PORTNAME%%', portname), '%%QUEUEID%%', buildqueue.id::text), '%%BUILDID%%', builds.id::text) FROM portrepositories, buildqueue, builds WHERE portrepositories.id = repository AND buildqueue.id = queueid AND builds.id = %ld", atol(PQgetvalue(result, i, 0)));
        if (PQresultStatus(result3) != PGRES_TUPLES_OK || PQntuples(result3) != 1)
           RETURN_ROLLBACK(conn);

        loginfo("Start checkout for build %s", PQgetvalue(result, i, 0));

        sprintf(url, "%s://%s%scheckout?type=%s&repository=%s&revision=%s&build=%s", PQgetvalue(result2, 0, 0),
        		PQgetvalue(result2, 0, 1), PQgetvalue(result2, 0, 2), PQgetvalue(result3, 0, 0),
                        PQgetvalue(result3, 0, 1), PQgetvalue(result, i, 3), PQgetvalue(result2, 0, 4));
        if(getpage(url, PQgetvalue(result2, 0, 3), REMOTE_NOTIMEOUT) == CURLE_OK)
        {
           if(!PQupdate(conn, "UPDATE builds SET status = 31 WHERE id = %ld", atol(PQgetvalue(result, i, 0))))
              RETURN_ROLLBACK(conn);
        }
        else
        {
           logcgi(url, getenv("ERROR"));
           if(updateBackendbuildFailed(conn, atol(PQgetvalue(result2, 0, 5))) != 0)
              RETURN_ROLLBACK(conn);
        }

        if(getenv("REVISION") != NULL)
        {
           if(!PQupdate(conn, "UPDATE buildqueue SET revision = %ld WHERE id = '%s'", atol(getenv("REVISION")), PQgetvalue(result, i, 4)))
              RETURN_ROLLBACK(conn);
        }

        PQclear(result4);
        PQclear(result3);
        PQclear(result2);

        /* we've successfully checked out a build so be sure to commit */
        break;
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
    PGresult *reslock;
    int i, j, done;

    done = 0;

    if((conn = PQautoconnect()) == NULL)
        return -1;

    result = PQexec(conn, "SELECT DISTINCT ON (buildgroup) buildgroup, id, queueid FROM (SELECT builds.id, builds.buildgroup, builds.queueid FROM buildqueue, builds WHERE buildqueue.id = builds.queueid AND buildqueue.status < 90 AND builds.status = 20 AND builds.backendid = 0 ORDER BY priority, builds.id DESC) as query");

    if (PQresultStatus(result) != PGRES_TUPLES_OK)
    	RETURN_ROLLBACK(conn);

    for(i=0; i < PQntuples(result) && done < 1; i++)
    {
        reslock = PQselect(conn, "SELECT id FROM buildqueue WHERE id = '%s' FOR UPDATE NOWAIT", PQgetvalue(result, i, 2));
        if (PQresultStatus(reslock) != PGRES_TUPLES_OK || PQntuples(reslock) != 1)
        {
            if (strcmp(PQgetErrorCode(reslock), PQERROR_LOCK_NOT_AVAILABLE) == 0)
            {
                logwarn("Could not lock build %s. Continuing.", PQgetvalue(result, i, 2));

                reslock = PQexec(conn, "ROLLBACK");
                if (PQresultStatus(reslock) != PGRES_COMMAND_OK)
                    RETURN_ROLLBACK(conn);

                reslock = PQexec(conn, "BEGIN");
                if (PQresultStatus(reslock) != PGRES_COMMAND_OK)
                    RETURN_ROLLBACK(conn);

                continue;
            }

            RETURN_ROLLBACK(conn);
        }

        result2 = PQselect(conn, "SELECT backendid, maxparallel FROM backendbuilds, backends WHERE backendid = backends.id AND buildgroup = '%s' AND backendbuilds.status = 1 AND backends.status = 1 ORDER BY priority", PQgetvalue(result, i, 0));
        if (PQresultStatus(result2) != PGRES_TUPLES_OK)
            RETURN_ROLLBACK(conn);

        if(PQntuples(result2) < 1)
        {
            logdebug("No backend available for buildgroup %s", PQgetvalue(result, i, 0));
            continue;
        }

        for(j=0; j < PQntuples(result2); j++)
        {
            result3 = PQselect(conn, "SELECT count(*) FROM builds WHERE backendid = %ld AND status < 70", atol(PQgetvalue(result2, j, 0)));
            if (PQresultStatus(result3) != PGRES_TUPLES_OK)
                RETURN_ROLLBACK(conn);

            if(atoi(PQgetvalue(result3, 0, 0)) < atoi(PQgetvalue(result2, j, 1)))
            {
                result4 = PQselect(conn, "SELECT count(*) FROM builds WHERE backendid = %ld AND buildgroup = '%s' AND status >= 30 AND status < 90", atol(PQgetvalue(result2, j, 0)), PQgetvalue(result, i, 0));
                if (PQresultStatus(result4) != PGRES_TUPLES_OK)
                    RETURN_ROLLBACK(conn);

                logdebug("Running %d jobs for group %s", atoi(PQgetvalue(result4, 0, 0)), PQgetvalue(result, i, 0));

                if(atoi(PQgetvalue(result4, 0, 0)) == 0)
                {
                    loginfo("Choose backend %s for build %s", PQgetvalue(result2, j, 0), PQgetvalue(result, i, 1));

                    if(!PQupdate(conn, "UPDATE builds SET backendid = %ld, status = 30, startdate = %lli WHERE id = %ld", atol(PQgetvalue(result2, j, 0)), microtime(), atol(PQgetvalue(result, i, 1))))
                        RETURN_ROLLBACK(conn);

                    done++;
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

