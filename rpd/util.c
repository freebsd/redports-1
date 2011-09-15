#include <sys/time.h>

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

