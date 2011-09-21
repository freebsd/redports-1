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
#include <sys/time.h>

#include "util.h"

struct configparam
{
    char key[CONFIGMAXKEY];
    char value[CONFIGMAXVALUE];
};

struct configparam config[] = {
    { "dbHost",        "localhost" },
    { "dbUsername",    "root" },
    { "dbPassword",    "" },
    { "dbDatabase",    "trac" },
    { "wwwurl",        "" },
    { "wwwroot",       "" },
    { "", "" }
};

int configparse(char *filename)
{
    char *value;
    char line[255];
    FILE *file = fopen(filename, "r");

    if(file == NULL)
        return 1;
    
    while(fgets(line, sizeof(line), file) != NULL)
    {
        if(line[0] == '#')
            continue;
        line[strlen(line)-1] = '\0';
        
        if((value = strstr(line, " ")) != NULL || (value = strstr(line, "\t")) != NULL)
        {
            *value = '\0';
            *value++;

            while(*value == ' ' || *value == '\t')
                *value++;

            if(strlen(line) == 0 || strlen(value) == 0)
                 continue;

            if(configset(line, value) != 0)
                return 1;
        }
    }

    fclose(file);
    return 0;
}

char* configget(char *key)
{
    int i;

    for(i=0; config[i].key[0] != '\0'; i++)
    {
        if(strcmp(key, config[i].key) == 0)
        {
            return config[i].value;
        }
    }

    return NULL;
}

int configset(char *key, char *value)
{
    int i;

    for(i=0; config[i].key[0] != '\0'; i++)
    {
        if(strcmp(key, config[i].key) == 0)
        {
            strncpy(config[i].value, value, sizeof(config[i].value)-1);
            config[i].value[sizeof(config[i].value)-1] = '\0';
            return 0;
        }
    }

    printf("configset: Unknown config param <%s>\n", key);

    return 1;
}

unsigned long long microtime(void)
{
    struct timeval time;
    unsigned long long microtime;

    gettimeofday(&time, NULL);

    microtime = time.tv_sec;
    microtime *= 1000000L;
    microtime += time.tv_usec;

    return microtime;
}

