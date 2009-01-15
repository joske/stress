#include <ghttp.h>

#include "stress.h"

void stress_connect_ghttp(stress_t *stress, int threadid, int reqidx)
{
	ghttp_request* req;
	ghttp_status status;
    int status_code;

    req = ghttp_request_new();
    ghttp_set_uri(req, stress->request[reqidx]);
    ghttp_prepare(req);
    if (stress->counter->shoot) {
        status = ghttp_process(req);
        status_code = ghttp_status_code(req);
        if (stress->counter->count) {
            if (status == ghttp_done && status_code == 200) {
                int bytes_read = ghttp_get_body_len(req);
                stress->counter->bread[threadid] += bytes_read;
                stress->counter->connects[threadid]++;
            } else {
                stress->counter->errors[threadid]++;
            }
        } 
        if (stress->showfirsterror && status != ghttp_done) {
            dlog("child %d : got http response code %d with status %d on request %s\n", 
                    threadid, status_code, status, stress->request[reqidx]); 
            stress->showfirsterror = 0;
        }
        if (stress->verbose) {
            dlog("child %d : got http response code %d with status %d on request %s\n", 
                    threadid, status_code, status, stress->request[reqidx]); 
        }
        if (stress->vverbose) {
            dlog("child %d : body \n%s", threadid, ghttp_get_body(req));
        }
    }
    ghttp_request_destroy(req);
}
