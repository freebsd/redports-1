#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>

#include <my_global.h>
#include <mysql.h>

#include <curl/curl.h>
 
#define DAEMON_NAME "rpd"
#define PID_FILE "/var/run/rpd.pid"


size_t  write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    char buf[size*nmemb+1];
    char *pbuf = &buf[0];
    size_t i = 0;

    memset(buf, '\0', size*nmemb+1);
    for(; i < nmemb ; i++){
      strncpy(pbuf,ptr,size);
      pbuf += size;
      ptr += size;
    }

    printf("%s",buf);
    return size * nmemb;
}


int getPage(char *url)
{
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if(!curl) {
        return 1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_data);
    curl_easy_setopt(curl, CURLOPT_USERPWD, "user:test");
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return 0;
}

int handleStep11(void)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_RES *result2;
    MYSQL_RES *result3;
    MYSQL_ROW builds;
    MYSQL_ROW backends;
    MYSQL_ROW runningjobs;
    char query[1000];

    conn = mysql_init(NULL);
    if(conn == NULL){
        printf("Error %u on line %d: %s\n", mysql_errno(conn), __LINE__, mysql_error(conn));
        return 1;
    }

    if(mysql_real_connect(conn, "localhost", "root", "", "trac", 0, NULL, 0) == NULL){
        printf("Error %u on line %d: %s\n", mysql_errno(conn), __LINE__, mysql_error(conn));
        return 1;
    }

    if(mysql_query(conn, "SELECT builds.id, builds.group, builds.queueid FROM buildqueue, builds WHERE buildqueue.id = builds.queueid AND buildqueue.status = 11 AND builds.backendid = 0")){
        printf("Error %u on line %d: %s\n", mysql_errno(conn), __LINE__, mysql_error(conn));
        return 1;
    }

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        sprintf(query, "SELECT backendid, maxparallel FROM backendbuilds, backends WHERE backendid = backends.id AND buildgroup = \"%s\" ORDER BY priority", builds[1]);

        if(mysql_query(conn, query)){
            printf("Error %u on line %d: %s\n", mysql_errno(conn), __LINE__, mysql_error(conn));
            return 1;
        }

        result2 = mysql_store_result(conn);

        while((backends = mysql_fetch_row(result2)))
        {
            sprintf(query, "SELECT count(*) FROM builds WHERE backendid = %d AND status < 90", atoi(backends[0]));
            if(mysql_query(conn, query)){
                printf("Error %u on line: %s\n", mysql_errno(conn), __LINE__, mysql_error(conn));
                return 1;
            }

            result3 = mysql_store_result(conn);
            runningjobs = mysql_fetch_row(result3);

            printf("Group %s running %d max %d (backend=%s, id=%s)\n", builds[1], atoi(runningjobs[0]), atoi(backends[1]), backends[0], builds[0]);

            if(atoi(runningjobs[0]) < atoi(backends[1]))
            {
                sprintf(query, "UPDATE builds SET backendid = %d WHERE id = %d", atoi(backends[0]), atoi(builds[0]));
                if(mysql_query(conn, query)){
                    printf("Error %u on line %d: %s\n", mysql_errno(conn), __LINE__, mysql_error(conn));
                    return 1;
                }
            }

            mysql_free_result(result3);
        }

        mysql_free_result(result2);


        sprintf(query, "SELECT count(*) FROM builds WHERE queueid = \"%s\" AND backendid = 0", builds[2]);
        if(mysql_query(conn, query)){
            printf("Error %u on line: %s\n", mysql_errno(conn), __LINE__, mysql_error(conn));
            return 1;
        }

        result2 = mysql_store_result(conn);
        runningjobs = mysql_fetch_row(result2);

        if(atoi(runningjobs[0]) == 0)
        {
            sprintf(query, "UPDATE buildqueue SET status = 20 WHERE id = \"%s\"", builds[2]);
            if(mysql_query(conn, query)){
                printf("Error %u on line: %s\n", mysql_errno(conn), __LINE__, mysql_error(conn));
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

    conn = mysql_init(NULL);
    if(conn == NULL){
        printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
        return 1;
    }

    if(mysql_real_connect(conn, "localhost", "root", "", "trac", 0, NULL, 0) == NULL){
        printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
        return 1;
    }

    if(mysql_query(conn, "SELECT id, owner FROM buildqueue WHERE status = 10")){
        printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
        return 1;
    }

    result = mysql_store_result(conn);

    while ((builds = mysql_fetch_row(result)))
    {
        sprintf(query, "SELECT buildgroup FROM automaticbuildgroups WHERE username = \"%s\" ORDER BY priority", builds[1]);

        if(mysql_query(conn, query)){
            printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
            return 1;
        }

        result2 = mysql_store_result(conn);

        while((backends = mysql_fetch_row(result2)))
        {
            sprintf(query, "INSERT INTO builds VALUES (null, \"%s\",  \"%s\", 10, 0, 0, 0, 0)", builds[0], backends[0]);
	    if(mysql_query(conn, query)){
                printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
                return 1;
            }
        }

        mysql_free_result(result2);

        sprintf(query, "UPDATE buildqueue SET status = 11 WHERE id = \"%s\"", builds[0]);
        if(mysql_query(conn, query)){
            printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
            return 1;
        }
    }

    mysql_free_result(result);
    mysql_close(conn);

    return 0;
}


void run()
{
    while(1)
    {
        handleStep10();
        sleep(3);
        handleStep11();
        sleep(3);
    }
}
 
void usage(int argc, char *argv[]){
    if (argc >= 1){
        printf("Usage: %s -h -n\n", argv[0]);
        printf("  Options:\n");
        printf("      -n   Don't fork off as a daemon.\n");
        printf("      -h   Show this help screen.\n");
        printf("\n");
    }
}
 
void signal_handler(int sig) {
 
    switch(sig) {
        case SIGINT:
            syslog(LOG_WARNING, "Received SIGINT signal.");
            exit(1);
            break;
        case SIGHUP:
            syslog(LOG_WARNING, "Received SIGHUP signal.");
            exit(1);
            break;
        case SIGTERM:
            syslog(LOG_WARNING, "Received SIGTERM signal.");
            exit(1);
            break;
        default:
            syslog(LOG_WARNING, "Unhandled signal (%d) %s", strsignal(sig));
            break;
    }
}
 
int main(int argc, char *argv[]) {
 
#if defined(DEBUG)
    int daemonize = 0;
#else
    int daemonize = 1;
#endif
 
    // Setup signal handling before we start
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
 
    int c;
    while( (c = getopt(argc, argv, "nh|help")) != -1) {
        switch(c){
            case 'h':
                usage(argc, argv);
                exit(0);
                break;
            case 'n':
                daemonize = 0;
                break;
            default:
                usage(argc, argv);
                exit(0);
                break;
        }
    }
 
    syslog(LOG_INFO, "%s daemon starting up", DAEMON_NAME);
 
    // Setup syslog logging - see SETLOGMASK(3)
#if defined(DEBUG)
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);
#else
    setlogmask(LOG_UPTO(LOG_INFO));
    openlog(DAEMON_NAME, LOG_CONS, LOG_USER);
#endif
 
    /* Our process ID and Session ID */
    pid_t pid, sid;
 
    if (daemonize) {
        syslog(LOG_INFO, "starting the daemonizing process");
 
        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
            exit(EXIT_FAILURE);
        }
        /* If we got a good PID, then
           we can exit the parent process. */
        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }
 
        /* Change the file mode mask */
        umask(0);
 
        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }
 
        /* Change the current working directory */
        if ((chdir("/")) < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }
 
        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }
 
    run();
 
    syslog(LOG_INFO, "%s daemon exiting", DAEMON_NAME);
 
    // free everything

    exit(0);
}

