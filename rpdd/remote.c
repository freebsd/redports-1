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

#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>

#include "log.h"

char pagebuffer[16384];
char *ppagebuffer;

char *remotevars[] = {
    "BUILDLOG",
    "BUILDSTATUS",
    "ERROR",
    "FAIL_REASON",
    "OK",
    "PKGVERSION",
    "PORTSTREELASTBUILT",
    "REVISION",
    "STATUS",
    "WRKDIR",
    ""
};

/* Internals */
int parse(char *page);
int readpage(void *ptr, size_t size, size_t nmemb, FILE *stream);


int getpage(char *url, char *credentials, int timeout)
{
    int i;
    time_t start;
    CURL *curl;
    CURLcode res;

    memset(pagebuffer, '\0', sizeof(pagebuffer));
    ppagebuffer = &pagebuffer[0];

    for(i=0; remotevars[i][0] != '\0'; i++)
        unsetenv(remotevars[i]);

    curl = curl_easy_init();
    if(!curl)
    {
        setenv("ERROR", "curl initialization failed", 1);
        return CURLE_FAILED_INIT;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &readpage);

    if(timeout > 0)
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

    if(credentials != NULL)
        curl_easy_setopt(curl, CURLOPT_USERPWD, credentials);

    start = time(NULL);

    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    /* only log if consumed time is > 50% of timeout */
    if(time(NULL)-start > timeout*0.5)
        loginfo("Request to %s took %ld seconds (timeout %d)", url, (long)(time(NULL)-start), timeout); 

    if(res != CURLE_OK)
    {
        setenv("ERROR", curl_easy_strerror(res), 1);
        return res;
    }

    return parse(pagebuffer);
}

int downloadfile(char *url, char *credentials, char *filename)
{
    CURL *curl;
    CURLcode res;

    FILE *outfile;

    unsetenv("OK");
    unsetenv("ERROR");

    outfile = fopen(filename, "w");

    curl = curl_easy_init();
    if(!curl)
    {
        setenv("ERROR", "curl initialization failed", 1);
        return CURLE_FAILED_INIT;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, outfile);

    if(credentials != NULL)
        curl_easy_setopt(curl, CURLOPT_USERPWD, credentials);

    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    fclose(outfile);

    if(res != CURLE_OK)
        setenv("ERROR", curl_easy_strerror(res), 1);

    return res;
}

int readpage(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    int i = 0;

    for(; i < nmemb ; i++)
    {
        if((ppagebuffer-&pagebuffer[0])+size > sizeof(pagebuffer))
        {
            logerror("Could not store %ld bytes (offset %ld)", size, (ppagebuffer-&pagebuffer[0])); 
            return 0;
        }

        strncpy(ppagebuffer, ptr, size);
        ppagebuffer += size;
        ptr += size;
    }

    return size * nmemb;
}

int parse(char *page)
{
    int i;
    int status = -1;
    int end;
    char *p;
    char *linep;
    char errormsg[512];

    memset(errormsg, '\0', sizeof(errormsg));

    for(i=0; remotevars[i][0] != '\0'; i++)
        unsetenv(remotevars[i]);

    for(end=0,p=page,linep=page; !end; p++)
    {
        if(*p == '\0')
            end = 1;
        else if(*p == '\n')
            *p = '\0';
        else
            continue;

        if(strchr(linep, '=') != NULL)
        {
            for(i=0; remotevars[i][0] != '\0'; i++)
            {
                if(strncmp(remotevars[i], linep, strlen(remotevars[i])) == 0)
                {
                    setenv(remotevars[i], linep+strlen(remotevars[i])+1, 1);
                    break;
                }
            }
        }
        else if(strcmp(linep, "ERROR") == 0)
        {
            status = CURLE_RECV_ERROR;
        }
        else if(strcmp(linep, "OK") == 0)
        {
            status = CURLE_OK;
        }
        else
        {
            strncat(errormsg, linep, sizeof(errormsg)-strlen(errormsg)-1);
            strncat(errormsg, " ", sizeof(errormsg)-strlen(errormsg)-1);
        }

        linep = p+1;
    }

    if(strlen(errormsg) > 0)
    {
        if(strlen(errormsg) < sizeof(errormsg)-1)
            errormsg[strlen(errormsg)-1] = '\0';

        setenv("ERROR", errormsg, 1);
    }

    return status;
}

