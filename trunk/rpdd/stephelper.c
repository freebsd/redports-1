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

#include "log.h"
#include "database.h"
#include "util.h"

int updateBuildFailed(PGconn *conn, long buildId);
int updateBackendbuildFailed(PGconn *conn, int backendBuildId);
int updateBackendFailed(PGconn *conn, int backendId);
int recalcBuildPriority(PGconn *conn, long buildId);

int updateBuildFailed(PGconn *conn, long buildId)
{
    PGresult *result;

    result = PQselect(conn, "SELECT id, status FROM builds WHERE id = %ld FOR UPDATE NOWAIT", buildId);
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_FAIL(conn);

    if(PQntuples(result) != 1)
    {
        logerror("Invalid buildid %ld cannot be restarted", buildId);
        return -1;
    }

    logwarn("Restarting build %ld from status %s", buildId, PQgetvalue(result, 0, 1));

    if(!PQupdate(conn, "UPDATE builds SET status = 20, backendid = 0 WHERE id = %ld", buildId))
        RETURN_FAIL(conn);

    PQclear(result);

    return 0;
}

int updateBackendbuildFailed(PGconn *conn, int backendBuildId)
{
    PGresult *result;
    int i;

    result = PQselect(conn, "SELECT builds.id FROM builds, backendbuilds WHERE builds.backendid = backendbuilds.backendid AND builds.buildgroup = backendbuilds.buildgroup AND backendbuilds.id = %d AND builds.status >= 30 AND builds.status < 90 FOR UPDATE NOWAIT", backendBuildId);
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_FAIL(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        if(updateBuildFailed(conn, atol(PQgetvalue(result, i, 0))) != 0)
            return -1;
    }

    logwarn("Setting backendbuild %d to error status", backendBuildId);

    if(!PQupdate(conn, "UPDATE backendbuilds SET status = 2 WHERE id = %d", backendBuildId))
        RETURN_FAIL(conn);

    PQclear(result);

    result= PQselect(conn, "SELECT backendbuilds.buildgroup, backends.host FROM backendbuilds, backends WHERE backendbuilds.backendid = backends.id AND backendbuilds.id = %ld", backendBuildId);
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_FAIL(conn);

    setenv("RPBUILDGROUP", PQgetvalue(result, 0, 0), 1);
    setenv("RPBACKENDHOST", PQgetvalue(result, 0, 1), 1);
    callHook("BACKENDBUILD_FAILED");

    PQclear(result);

    return 0;
}

int updateBackendFailed(PGconn *conn, int backendId)
{
    PGresult *result;
    int i;

    result = PQselect(conn, "SELECT id FROM backends WHERE id = %d FOR UPDATE NOWAIT", backendId);
    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) != 1)
        RETURN_FAIL(conn);

    PQclear(result);

    result = PQselect(conn, "SELECT id FROM builds WHERE backendid = %d AND status >= 30 AND status < 90 FOR UPDATE NOWAIT", backendId);
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_FAIL(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        if(updateBuildFailed(conn, atol(PQgetvalue(result, i, 0))) != 0)
            return -1;
    }

    logwarn("Setting backend %d to error status", backendId);

    if(!PQupdate(conn, "UPDATE backends SET status = 2 WHERE id = %d", backendId))
        RETURN_FAIL(conn);

    PQclear(result);

    result = PQselect(conn, "SELECT host FROM backends WHERE id = %d", backendId);
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_FAIL(conn);

    setenv("RPBACKENDHOST", PQgetvalue(result, 0, 0), 1);
    callHook("BACKEND_FAILED");

    PQclear(result);

    return 0;
}

int recalcBuildPriority(PGconn *conn, long buildId)
{
    int priority;
    PGresult *result;
    PGresult *result2;
    PGresult *result3;

    result = PQselect(conn, "SELECT (enddate-startdate)/1000000, buildstatus, queueid FROM builds WHERE id = %ld", buildId);
    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) != 1)
        RETURN_FAIL(conn);

    result2 = PQselect(conn, "SELECT priority, owner FROM buildqueue WHERE id = '%s' FOR UPDATE NOWAIT", PQgetvalue(result, 0, 2));
    if (PQresultStatus(result2) != PGRES_TUPLES_OK || PQntuples(result2) != 1)
        RETURN_FAIL(conn);

    priority = atol(PQgetvalue(result2, 0, 0));

    if(priority > 1 && priority < 9)
    {
        if(strcmp(PQgetvalue(result, 0, 1), "SUCCESS") == 0)
            priority -= 1;
        else
            priority += 1;
    
        if(atol(PQgetvalue(result, 0, 0)) < 300)
            priority -= 1;
        else if(atol(PQgetvalue(result, 0, 0)) > 3000)
            priority += 2;
    }

    if(priority < 1)
        priority = 1;
    if(priority > 9)
        priority = 9;

    if(!PQupdate(conn, "UPDATE buildqueue SET priority = %ld WHERE id = '%s'", priority, PQgetvalue(result, 0, 2)))
        RETURN_FAIL(conn);

    result3 = PQselect(conn, "SELECT count(*) FROM buildqueue, builds WHERE buildqueue.id = builds.queueid AND buildqueue.owner = '%s' AND builds.status < 90", PQgetvalue(result2, 0, 1));
    if (PQresultStatus(result3) != PGRES_TUPLES_OK)
        RETURN_FAIL(conn);

    /* Hard limit where we downgrade all jobs of this user */
    if(atoi(PQgetvalue(result3, 0, 0)) > 49)
    {
        if(!PQupdate(conn, "UPDATE buildqueue SET priority = 9 WHERE owner = '%s' AND status < 90 AND priority > 1", PQgetvalue(result2, 0, 1)))
            RETURN_FAIL(conn);
    }

    PQclear(result3);
    PQclear(result2);
    PQclear(result);

    return 0;
}

