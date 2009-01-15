/* Stress tool 
 *
 * Copyright (c) 2001 Ubizen. All rights reserved.
 *
 * This  software  is  the  confidential  and proprietary information of Ubizen
 * ("Confidential  Information").  You  shall  not  disclose  such Confidential
 * information  and  shall  use  it  only  in  accordance with the terms of the
 * license agreement you entered into with Ubizen.
 *
 * UBIZEN  MAKES  NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
 * SOFTWARE,  EITHER  EXPRESS  OR  IMPLIED,  INCLUDING  BUT  NOT LIMITED TO THE
 * IMPLIED  WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
 * NON-INFRINGEMENT.  UBIZEN  SHALL  NOT  BE LIABLE FOR ANY DAMAGES SUFFERED BY
 * LICENSEE  AS  A  RESULT OF USING, MODIFYING OR DISTRIBUTING THIS SOFTWARE OR
 * ITS DERIVATIVES.
 *
 * written by Jos Dehaes, february 2001
 * greatly enhanced, july 2002
 * support for SSL, HTTP/1.1 via libcurl, february 2003
 * 
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>

#include "stress.h"

extern int errno;
static stress_t *stress;

/* handlers */
void stress_connect_ghttp(stress_t * stress, int i, int reqidx);
void stress_connect_curl(stress_t * stress, int i, int reqidx);

/* prototypes */
static void* child_loop(void *thread);
static void usage(char *command);
static void parse_options(int argc, char *argv[]);
static char *canonize_bytes(unsigned long long bytes);
static char *canonize_time(long long secs);
static void summarize();
static void init(int argc, char *argv[]);
static void start_workers();
static void start_workers_pthreads();
static void stop_workers();
static void stop_workers_pthreads();
static void cleanup();
static void signal_handler(int signum);

/* print usage info */
static void usage(char *command)
{
    dlog("usage : %s [options]\n", command);
    dlog("\t -P protocol (http|https) (default : http) (https implies -l curl)\n");
    dlog("\t -H host (default : localhost)\n");
    dlog("\t -p port (default : 80)\n");
    dlog("\t -w warmup time in seconds (default : 10)\n");
    dlog("\t -t run time in seconds (default : 120)\n");
    dlog("\t -T use threads (default : no)\n");
    dlog("\t -s summarize every n seconds (default : 0 - don't summarize)\n");
    dlog("\t -n number of threads (default : 1)\n");
    dlog("\t -r (request|@requestfile) (default : /index.html)\n");
    dlog("\t -f show details of first error\n");
    dlog("\t -k keyfile:passwd ssl private key and passphrase (implies -l curl)\n");
    dlog("\t -K use HTTP1.1 keep-alive (default: off) (implies -l curl)\n");
    dlog("\t -l http library to use (curl|ghttp) (default: ghttp)\n");
    dlog("\t -v be verbose\n");
    dlog("\t -V be very verbose (this implies -v)\n");
    dlog("\t -h usage message\n");
}

static void parse_request_file(const char *arg)
{
    FILE *fp;
    const char *fn;
    char buf[1024];
    int i, rv;

    fn = &arg[1];               /* skip first char '@' */
    dlog("Using file '%s' for request list.\n", fn);
    /* count number of requests */
    if ((fp = fopen(fn, "r")) == NULL) {
        dlog("Unable to open request file '%s'.\n", fn);
        exit(2);
    }
    do {
        rv = fscanf(fp, "%s", buf);
        if (rv == EOF) {
            break;
        }
        if (rv != 1) {
            dlog("Unable to read line from request file.\n");
            exit(3);
        }
        //dlog("Request %03d: %s\n", stress->reqcnt, buf);
        stress->reqcnt++;
    } while (!feof(fp));
    fclose(fp);
    dlog("Request count = %d\n", stress->reqcnt);
    /* parse requests */
    if (stress->reqcnt > 0) {
        if ((fp = fopen(fn, "r")) == NULL) {
            dlog("Unable to open request file '%s'.\n", fn);
            exit(2);
        }
        stress->request = calloc(stress->reqcnt, sizeof(char *));
        for (i = 0; i < stress->reqcnt; i++) {
            stress->request[i] = (char *)strdup(arg);
            fscanf(fp, "%s", buf);
            stress->request[i] = (char *)strdup(buf);
        }
        fclose(fp);
    }
    /* done */
}

static void parse_request(const char *arg)
{
    int i;

    if (stress->reqcnt > 0) {
        for (i = 0; i < stress->reqcnt; i++) {
            free(stress->request[i]);
        }
        free(stress->request);
        stress->reqcnt = 0;
    }
    if (arg[0] == '@') {
        /* it's a file */
        parse_request_file(arg);
    } else {
        /* it's a single request */
        stress->request = calloc(1, sizeof(char *));
        stress->request[0] = (char *)strdup(arg);
        stress->reqcnt = 1;
    }
}

static void request_to_uri()
{
    int i;

    for (i = 0; i < stress->reqcnt; i++) {
        char *req = stress->request[i];
        int len = 15 + strlen(stress->host) + strlen(req);
        stress->request[i] = (char *)malloc(len);
        snprintf(stress->request[i], len, "%s://%s:%d%s",
                stress->protocol, stress->host, stress->port, req);
        //dlog("URI: %s\n", stress->request[i]);
        free(req);
    }
}

/* parse the command line options */
static void parse_options(int argc, char *argv[])
{
    int optchar;
    char *pos = NULL;
    static char optstring[] = "P:p:H:r:w:t:s:n:k:l:hfvVKT";

    while ((optchar = getopt(argc, argv, optstring)) != -1) {
        switch (optchar) {
            case 'P':
                if (stress->protocol) {
                    free(stress->protocol);
                }
                stress->protocol = (char*) strdup(optarg);
                break;
            case 'p':
                stress->port = atoi(optarg);
                break;
            case 'w':
                stress->warmup = atoi(optarg);
                break;
            case 't':
                stress->rtime = atoi(optarg);
                break;
            case 's':
                stress->summary_delay = atoi(optarg);
                break;
            case 'n':
                stress->nthreads = atoi(optarg);
                if (stress->nthreads > MAX_THREADS) {
                    dlog("The number of threads can not be larger than %d\n", MAX_THREADS);
                    exit(156);
                }
                break;
            case 'k':
                pos = strchr(optarg, ':');
                if (!pos) {
                    stress->sslcert = (char*) strdup(optarg);
                } else {
                    stress->sslcert = (char*) strndup(optarg, pos - optarg);
                    stress->sslcertpasswd = (char*) strdup(pos + 1);
                }
                break;
            case 'l':
                if (strcasecmp(optarg, "curl") == 0) {
                    stress->connect = stress_connect_curl;
                }
                break;
            case 'H':
                if (stress->host) {
                    free(stress->host);
                }
                stress->host = (char *) strdup(optarg);
                break;
            case 'r':
                parse_request(optarg);
                break;
            case 'f':
                stress->showfirsterror = 1;
                break;
            case 'v':
                stress->verbose = 1;
                break;
            case 'V':
                stress->verbose = 1;
                stress->vverbose = 1;
                break;
            case 'T':
                stress->threaded = 1;
                break;
            case 'K':
                stress->http11 = 1;
                break;
            case 'h':
                usage(argv[0]);
                exit(0);
            default:
                usage(argv[0]);
                exit(1);
        }
    }
    if (!stress->host) {
        stress->host = (char *) strdup("localhost");
    }
    if (stress->reqcnt == 0) {
        parse_request("/index.html");
    }
    if (!stress->protocol) {
        stress->protocol = (char*) strdup("http");
    }
    if (!stress->connect) {
        stress->connect = stress_connect_ghttp;
    }
    if (strcasecmp(stress->protocol, "https") == 0 || stress->sslcert || stress->http11) {
        stress->connect = stress_connect_curl;
    }
    request_to_uri();
}

/* display more friendly bytes */
static char *canonize_bytes(unsigned long long bytes)
{
    char *can_bytes;

    can_bytes = malloc(100);
    if (bytes > 1073741824) {
        snprintf(can_bytes, 99, "%.2Lf GB", bytes / 1073741824.0L);
    } else if (bytes > 1048576) {
        snprintf(can_bytes, 99, "%.2f MB", bytes / 1048576.0f);
    } else if (bytes > 1024) {
        snprintf(can_bytes, 99, "%.2f kB", bytes / 1024.0f);
    } else {
        snprintf(can_bytes, 99, "%lld B", bytes);
    }
    return can_bytes;
}

/* display a more friendly elapsed time */
static char *canonize_time(long long secs)
{
    char *can_time;

    can_time = (char *) malloc(100);
    if (secs < 60) {
        snprintf(can_time, 99, "%lld seconds", secs);
    } else if (secs < 3600) {
        int min, sec;

        min = secs / 60;
        sec = secs % 60;
        snprintf(can_time, 99, "%d minutes and %d seconds", min, sec);
    } else if (secs < 86400) {
        int hour, tmp, min, sec;

        hour = secs / 3600;
        tmp = secs % 3600;
        min = tmp / 60;
        sec = tmp % 60;
        snprintf(can_time, 99, "%d hours, %d minutes and %d seconds", hour, min, sec);
    } else {
        int day, tmp1, hour, tmp2, min, sec;

        day = secs / 86400;
        tmp1 = secs % 86400;
        hour = tmp1 / 3600;
        tmp2 = tmp1 % 3600;
        min = tmp2 / 60;
        sec = tmp2 % 60;
        snprintf(can_time, 99, "%d days, %d hours, %d minutes and %d seconds", day, hour, min, sec);
    }
    return can_time;
}

/* print summary of the results */
static void summarize(long long elapsed)
{
    int i;
    unsigned long long connects = 0;
    unsigned long long errors = 0;
    unsigned long long bytes_read = 0;
    double bps = 0;
    double cps = 0;
    time_t now;
    struct tm tm_now;
    char date_header[200];
    char *elapsed_s;
    char *can_bps;
    char *can_bytes;

    /* add up the results */
    for (i = 0; i < stress->nthreads; i++) {
        connects += stress->counter->connects[i];
        errors += stress->counter->errors[i];
        bytes_read += stress->counter->bread[i];
    }
    cps = ((double)connects) / elapsed;
    bps = ((double)bytes_read) / elapsed;
    /* display results */
    time(&now);
    localtime_r(&now, &tm_now);
    strftime(date_header, sizeof(date_header) - 1, "%Y-%m-%d %H:%M:%S", &tm_now);
    elapsed_s = canonize_time(elapsed);
    can_bytes = canonize_bytes(bytes_read);
    can_bps = canonize_bytes(bps);
    dlog("*** %s ***\n", date_header);
    dlog("HTTP library     : %s\n", stress->connect == stress_connect_curl ? "libcurl" : "libghttp");
    if (stress->reqcnt > 1) {
        dlog("Request count    : %d\n", stress->reqcnt);
    } else {
        dlog("Request          : %s\n", stress->request[0]);
    }
    dlog("Number of threads: %d\n", stress->nthreads);
    dlog("Threaded         : %s\n", stress->threaded ? "yes" : "no");
    dlog("KeepAlive        : %s\n", stress->http11 ? "enabled" : "disabled");
    dlog("Warmup time      : %d seconds\n", stress->warmup);
    dlog("Count            : %lld connects.\n", connects);
    dlog("Errors           : %lld errors.\n", errors);
    dlog("Total time       : %s\n", elapsed_s);
    dlog("Performance      : %.2f requests per second\n", cps);
    dlog("Total bytes read : %s\n", can_bytes);
    dlog("Total throughput : %s/s\n", can_bps);
    dlog("\n");
    free(elapsed_s);
    free(can_bps);
    free(can_bytes);
}

static void init(int argc, char *argv[])
{
    stress->nthreads = 1;
    stress->port = 80;
    stress->warmup = 10;
    stress->rtime = 120;
    stress->request = NULL;
    stress->reqcnt = 0;
    stress->threaded = 0;
    /* parse commandline options */
    parse_options(argc, argv);

    if (stress->summary_delay > stress->rtime) {
        stress->summary_delay = 0;
    }
    /* get a block of shared memory */
    stress->shmid = shmget(IPC_PRIVATE, sizeof(counter_t), 0600);
    if (stress->shmid == -1) {
        dlog("shmget failed %s", strerror(errno));
        exit(137);
    }
    stress->counter = (counter_t *) shmat(stress->shmid, 0, 0);
    memset(stress->counter, 0, sizeof(counter_t));

    stress->counter->run = 1;
    stress->counter->shoot = 0;
    /* allocate pid array */
    stress->pid = (pid_t *) malloc(stress->nthreads * sizeof(pid_t));
    /* allocate thread_t array */
    stress->thread = (pthread_t *) malloc(stress->nthreads * sizeof(pthread_t));
    /* setup start/stop functions */
    if (stress->threaded) {
        stress->start_workers = start_workers_pthreads;
        stress->stop_workers = stop_workers_pthreads;
    } else {
        stress->start_workers = start_workers;
        stress->stop_workers = stop_workers;
    }
}

/* child loop */
static void* child_loop(void *thread)
{
    int reqidx = 0;
    int threadid = (int)thread;
    while (stress->counter->run) {
        if (stress->counter->shoot) {
            stress->connect(stress, threadid, reqidx);
            /* select next request index */
            reqidx++;
            if (reqidx >= stress->reqcnt) {
                reqidx = 0;
            }
        } else {
            usleep(100000);
        }
    }
    return NULL;
}

static void start_workers_pthreads()
{
    int i;

    for (i = 0; i < stress->nthreads; i++) {
        int rc = pthread_create (&stress->thread[i], NULL, child_loop, (void *)i);
        if (rc != 0) {
            dlog("pthread_create failed %s\n", strerror(errno));
            exit(137);
        }
    }
}

static void start_workers()
{
    int i;

    for (i = 0; i < stress->nthreads; i++) {
        stress->pid[i] = fork();
        if (stress->pid[i] == -1) {
            dlog("fork failed %s", strerror(errno));
            exit(137);
        }
        if (stress->pid[i] == 0) {      /* we are the child */
            child_loop((void*)i);
            /* should never get here, but exit anyway to prevent 
             * forking again if the child loop should terminate
             * prematurely
             */
            exit(0);
        }
    }
}

static void stop_workers_pthreads()
{
    int i;
    void *status = NULL;

    for (i = 0; i < stress->nthreads; i++) {
        pthread_join(stress->thread[i], &status);
    }
}

static void stop_workers()
{
    int i;
    int status;

    for (i = 0; i < stress->nthreads; i++) {
        kill(stress->pid[i], SIGTERM);
        waitpid(stress->pid[i], &status, 0);
    }
}

static void cleanup()
{
    stress->counter->run = 0;
    /* stop the children */
    stress->stop_workers();

    /* !!clean up the shared memory!! */
    shmdt(stress->counter);
    shmctl(stress->shmid, IPC_RMID, 0);
}

/* kill the workers if we get are killed */
static void signal_handler(int signum)
{
    switch (signum) {
        case SIGSEGV:
            dlog("oops, segfault... \n");
        case SIGHUP:
        case SIGINT:
        case SIGTERM:
        case SIGKILL:
            cleanup();
            exit(signum);
            break;
        default:
            break;
    }
}

/* the main function */
int main(int argc, char *argv[])
{
    int run_parent = 1;

    stress = malloc(sizeof(stress_t));
    memset(stress, 0, sizeof(stress_t));
    /* initialize shared memory and parse options */
    init(argc, argv);

    /* fork the children and start them */
    stress->start_workers();

    /* add signal handler */
    signal(SIGHUP, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGKILL, signal_handler);
    signal(SIGSEGV, signal_handler);

    /* parent loop */
    stress->counter->shoot = 1;
    if (stress->warmup) {
        sleep(stress->warmup);
        stress->counter->shoot = 0;
        sleep(stress->warmup);
    }
    gettimeofday(&stress->tpstart, NULL);
    stress->counter->shoot = 1;
    stress->counter->count = 1;
    while (run_parent) {
        struct timeval current_time;
        long long elapsed = 0;

        if (stress->summary_delay) {
            sleep(stress->summary_delay);
        } else {
            sleep(stress->rtime);
            run_parent = 0;
            stress->counter->count = 0;
        }
        gettimeofday(&current_time, NULL);
        elapsed = (current_time.tv_sec - stress->tpstart.tv_sec);
        if (elapsed >= stress->rtime) {
            stress->counter->shoot = 0;
            stress->counter->count = 0;
            run_parent = 0;
        }
        summarize(elapsed);
    }

    cleanup(stress);

    exit(0);
}
