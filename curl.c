#include <curl/curl.h>
#include <string.h>

#include "stress.h"

static int init = 0;

void stress_connect_curl(stress_t * stress, int threadid, int reqidx)
{
    static CURL *curl;
    CURLcode rv;
    static FILE *null;
    int http_code;
    double bytes;

    if (!init) {
        curl_global_init(CURL_GLOBAL_SSL);
        init = 1;
    }
    if (!null) {
        null = fopen("/dev/null", "w");
    }
    if (!curl || !stress->http11) {
        curl = curl_easy_init();
        if (stress->http11) {
            curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
        }
    }
    curl_easy_setopt(curl, CURLOPT_FILE, null); /* send the response to oblivion */
    curl_easy_setopt(curl, CURLOPT_URL, stress->request[reqidx]);
    if (strcasecmp(stress->protocol, "https") == 0) {
        if (stress->sslcert) {
            curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_SSLv3);
            curl_easy_setopt(curl, CURLOPT_SSLCERT, stress->sslcert);
            if (stress->sslcertpasswd) {
                curl_easy_setopt(curl, CURLOPT_SSLKEYPASSWD, stress->sslcertpasswd);
            }
        }
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
    }
    if (stress->counter->shoot) {
        rv = curl_easy_perform(curl);
        if (stress->counter->count) {
            if (rv == 0) {
                rv = curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &http_code);
                if (http_code == 200) {
                    rv = curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &bytes);
                    stress->counter->bread[threadid] += bytes;
                    stress->counter->connects[threadid]++;
                } else {
                    stress->counter->errors[threadid]++;
                }
            } else {
                char *sslerr;

                curl_easy_getinfo(curl, CURLINFO_SSL_VERIFYRESULT, &sslerr);
                dlog("error: %d, sslerr: %s\n", rv, sslerr);
                stress->counter->errors[threadid]++;
            }
        }
    }
    if (curl && !stress->http11) {
        curl_easy_cleanup(curl);
    }
//    fclose(null);
}
