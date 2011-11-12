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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <libgen.h>
#include <limits.h>

#include "log.h"
#include "util.h"

struct configparam
{
    char key[CONFIGMAXKEY];
    char value[CONFIGMAXVALUE];
};

struct configparam config[] = {
    { "cleandays",     "16" },
    { "dbHost",        "localhost" },
    { "dbUsername",    "root" },
    { "dbPassword",    "" },
    { "dbDatabase",    "trac" },
    { "tbPortsTreeMaxAge", "48" },
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

int mkdirrec(char *dir)
{
   struct stat sb;
   char directory[PATH_MAX];

   strncpy(directory, dir, sizeof(directory)-1);
   directory[sizeof(directory)-1] = '\0';

   if(stat(directory, &sb) == 0)
      return 0;

   if(mkdirrec(dirname(directory)) == 0)
      return mkdir(directory, (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH));

   return -1;
}

int rmdirrec(char *directory)
{
   DIR *dp;
   struct dirent *ep;
   struct stat sp;
   char tmp[PATH_MAX];

   dp = opendir(directory);
   if(dp == NULL)
      return 1;

   while((ep = readdir(dp)))
   {
       if(strcmp(ep->d_name, ".") == 0)
          continue;

       if(strcmp(ep->d_name, "..") == 0)
          continue;

       sprintf(tmp, "%s/%s", directory, ep->d_name);

       if(stat(tmp, &sp) < 0)
          return -1;

       if(S_ISDIR(sp.st_mode))
       {
          if(rmdirrec(tmp) != 0)
             return -1;
       }
       else
          unlink(tmp);
   }

   closedir(dp);

   loginfo("Removing %s", directory);

   return rmdir(directory);
}

int checkdir(char *directory)
{
   struct tm tm;
   char *file;
   int cleandays = atoi(configget("cleandays"));

   file = basename(directory);

   if(strlen(file) < 15)
      return 0;

   if(strncmp(file, "20", 2) != 0)
      return 0;

   if(file[14] != '-')
      return 0;

   if(strptime(file, "%Y%m%d%H%M%S", &tm) == NULL)
      return 0;

   return difftime(time(NULL), mktime(&tm)) > cleandays*3600*24;
}

int cleanolddir(char *directory)
{
   DIR *dp;
   struct dirent *ep;
   struct stat sp;
   char tmp[PATH_MAX];

   dp = opendir(directory);
   if(dp == NULL)
      return 1;

   while((ep = readdir(dp)))
   {
       if(ep->d_name[0] == '.')
          continue;

       sprintf(tmp, "%s/%s", directory, ep->d_name);

       if(stat(tmp, &sp) < 0)
          continue;

       if(!S_ISDIR(sp.st_mode))
          continue;

       if(!checkdir(tmp)){
          cleanolddir(tmp);
          continue;
       }

       rmdirrec(tmp);
   }

   closedir(dp);

   return 0;
}

