#ifndef STRESS_H
#define STRESS_H 1

#include <sys/types.h>
#include <stdio.h>

/* avoid buffering the output -> write to stderr */
#define dlog( ...) fprintf(stderr, __VA_ARGS__)
    
#define MAX_THREADS  	1000

typedef struct {
	int count;
	int shoot;
    int run;
	unsigned long long connects[MAX_THREADS];
    unsigned long long errors[MAX_THREADS];
	unsigned long long bread[MAX_THREADS];
} counter_t;

typedef struct __stress_s stress_t;
    
struct __stress_s {
    int nthreads;
    pid_t* pid;
    pthread_t *thread;
    char *protocol;
    int http11;
    int port;
    int rtime;
    int summary_delay;
    int warmup;
    char** request;
    int reqcnt;
    int threaded;
    int verbose;
    int vverbose;
    int showfirsterror;
    char* host;
    int shmid;
    counter_t *counter;
    struct timeval tpstart;
    char *sslcert;
    char *sslcertpasswd;
    void (*connect)(stress_t *stress, int threadid, int reqidx);
    void (*start_workers)(void);
    void (*stop_workers)(void);
};

#endif /* STRESS_H */
