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

#ifndef _DATABASE_H_
#define _DATABASE_H_

#include <string.h>
#include <libpq-fe.h>
#include "log.h"

#define RETURN_ROLLBACK(conn) \
{ \
    PGresult *res_rollback; \
    logsql(conn); \
    res_rollback = PQexec(conn, "ROLLBACK"); \
    if(PQresultStatus(res_rollback) != PGRES_COMMAND_OK) \
        logsql(conn); \
    PQclear(res_rollback); \
    PQfinish(conn); \
    return -1; \
}

#define RETURN_COMMIT(conn) \
{ \
    PGresult *res_commit; \
    res_commit = PQexec(conn, "COMMIT"); \
    if(PQresultStatus(res_commit) != PGRES_COMMAND_OK) \
    { \
        RETURN_ROLLBACK(conn); \
    } \
    PQclear(res_commit); \
    PQfinish(conn); \
    return 0; \
}

#define RETURN_FAIL(conn) \
{ \
    logsql(conn); \
    return -1; \
}

/**
 * PostgreSQL Error Codes
 *
 * http://www.postgresql.org/docs/9.1/static/errcodes-appendix.html
 */

#define PQERROR_LOCK_NOT_AVAILABLE "55P03"
   

extern PGconn* PQautoconnect(void);
extern int PQupdate(PGconn *conn, char *queryfmt, ...);
extern PGresult* PQselect(PGconn *conn, char *queryfmt, ...);
extern char* PQgetErrorCode(PGresult *res);

#endif /* _DATABASE_H */
