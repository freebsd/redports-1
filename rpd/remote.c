#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>

char pagebuffer[1024];
char *ppagebuffer;

char *remotevars[] = {
   "STATUS",
   "ERROR",
   "OK"
};

/* Internals */
int parse(char *page);
int readpage(void *ptr, size_t size, size_t nmemb, FILE *stream);


int getpage(char *url, char *credentials)
{
    CURL *curl;
    CURLcode res;

    memset(pagebuffer, '\0', sizeof(pagebuffer));
    ppagebuffer = &pagebuffer[0];

    curl = curl_easy_init();
    if(!curl)
        return 1;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &readpage);

    if(credentials != NULL)
        curl_easy_setopt(curl, CURLOPT_USERPWD, credentials);

    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return parse(pagebuffer);
}

int downloadfile(char *url, char *credentials, char *filename)
{
    CURL *curl;
    CURLcode res;

    FILE *outfile;
    outfile = fopen(filename, "w");

    curl = curl_easy_init();
    if(!curl)
        return 1;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, outfile);

    if(credentials != NULL)
        curl_easy_setopt(curl, CURLOPT_USERPWD, credentials);

    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    fclose(outfile);

    return 0;
}

int readpage(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    int i = 0;

    for(; i < nmemb ; i++){
        if((ppagebuffer-&pagebuffer[0])+size > sizeof(pagebuffer))
           return 0;

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

   for(i=0; remotevars[i] != '\0'; i++)
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
         for(i=0; remotevars[i] != '\0'; i++)
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
         status = 0;
      }
      else if(strcmp(linep, "OK") == 0)
      {
         status = 1;
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

