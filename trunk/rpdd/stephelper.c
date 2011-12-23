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

int updateBuildFailed(PGconn *conn, long buildId)
{
    logwarn("Restarting build %ld", buildId);

    if(!PQupdate(conn, "UPDATE builds SET status = 20, backendid = 0 WHERE id = %ld", buildId))
        RETURN_FAIL(conn);

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

    return 0;
}

int updateBackendFailed(PGconn *conn, int backendId)
{
    PGresult *result;
    int i;
 
    result = PQselect(conn, "SELECT id FROM builds WHERE backendid = %ld AND status >= 30 AND status < 90 FOR UPDATE NOWAIT", backendId);
    if (PQresultStatus(result) != PGRES_TUPLES_OK)
        RETURN_FAIL(conn);

    for(i=0; i < PQntuples(result); i++)
    {
        if(updateBuildFailed(conn, atol(PQgetvalue(result, i, 0))) != 0)
            return -1;
    }

    logwarn("Setting backend %d to error status", backendId);

    if(!PQupdate(conn, "UPDATE backends SET status = 2 WHERE id = %ld", backendId))
        RETURN_FAIL(conn);

    PQclear(result);

    return 0;
}

