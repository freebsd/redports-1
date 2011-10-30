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

#include <my_global.h>
#include <mysql.h>

#define LOGSQL(con) \
{ \
    printf("SQL Error %u in %s#%d: %s\n", \
        mysql_errno(con), __FILE__, __LINE__, mysql_error(con)); \
}

#define RETURN_ROLLBACK(conn) \
{ \
   LOGSQL(conn); \
   if(mysql_query(conn, "ROLLBACK") != 0) \
      LOGSQL(conn); \
   mysql_close(conn); \
   return -1; \
}

#define RETURN_COMMIT(conn) \
{ \
   if(mysql_query(conn, "COMMIT") != 0) \
   { \
      RETURN_ROLLBACK(conn); \
   } \
   mysql_close(conn); \
   return 0; \
}
   

extern MYSQL* mysql_autoconnect(void);

#endif /* _DATABASE_H */
