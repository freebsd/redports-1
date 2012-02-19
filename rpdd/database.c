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

#include <stdarg.h>
#include <stdlib.h>

#include "database.h"
#include "log.h"
#include "util.h"

void logNoticeProcessor(void *arg, char *message);

void logNoticeProcessor(void *arg, char *message)
{
    if(strlen(message) > 0)
        logerror(message);
}

PGconn* PQautoconnect(void)
{
    char conninfo[255];
    PGconn *conn = NULL;
    PGresult *res;

    sprintf(conninfo, "user=%s password=%s dbname=%s hostaddr=%s port=%s",
        configget("dbUsername"), configget("dbPassword"), configget("dbDatabase"),
        configget("dbHost"), "5432");

    conn = PQconnectdb(conninfo);
    if(PQstatus(conn) != CONNECTION_OK)
    {
        logsql(conn);
        return NULL;
    }

    PQsetNoticeProcessor(conn, logNoticeProcessor, NULL);

    res = PQexec(conn, "BEGIN");
    if(PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        logsql(conn);
        PQclear(res);
        PQfinish(conn);
        return NULL;
    }

    PQclear(res);

    return conn;
}

int PQupdate(PGconn *conn, char *queryfmt, ...)
{
    PGresult *res;
    char query[4096];
    va_list args;

    va_start(args, queryfmt);
    vsprintf(query, queryfmt, args);

    res = PQexec(conn, query);

    va_end(args);

    if(PQresultStatus(res) != PGRES_COMMAND_OK)
        return 0;

    if(atol(PQcmdTuples(res)) < 1)
        logwarn("Update query did not affect any rows! (Query: %s)", query);

    return 1;
}

PGresult* PQselect(PGconn *conn, char *queryfmt, ...)
{
    PGresult *res;
    char query[4096];
    va_list args;

    va_start(args, queryfmt);
    vsprintf(query, queryfmt, args);

    res = PQexec(conn, query);

    va_end(args);

    return res;
}

char* PQgetErrorCode(PGresult *res)
{
    return PQresultErrorField(res, PG_DIAG_SQLSTATE);
}
