/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2015, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/
/* <DESC>
 * simple HTTP POST using the easy interface
 * </DESC>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "curlApi.h"

struct curl_fetch_st *p = NULL;
char processing = NO;
#if 0
int main(void)
{
	CURL *curl;
	CURLcode res;
	struct curl_slist *list = NULL;
	list = curl_slist_append(list, "Content-Type:application/json");

	/* In windows, this will init the winsock stuff */
	curl_global_init(CURL_GLOBAL_ALL);

	/* get a curl handle */
	curl = curl_easy_init();

	if(curl) {

	/* First set the URL that is about to receive our POST. This URL can
	   just as well be a https:// URL if that is what should receive the
	   data. */
	curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8100/session");

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	/* Now specify the POST data */
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{\"desiredCapabilities\":{\"bundleId\":\"com.apple.mobilesafari\"}}");

	/* Perform the request, res will get the return code */
	res = curl_easy_perform(curl);

	/* Check for errors */
	if(res != CURLE_OK)
	  fprintf(stderr, "curl_easy_perform() failed: %s\n",
		      curl_easy_strerror(res));

	/* always cleanup */
	curl_easy_cleanup(curl);
	}
	curl_global_cleanup();
	return 0;
}
#endif

int postHttpData(char *httpURL, char *httpData)
{
	processing  = YES;
	CURL *curl;
	  CURLcode res;
		struct curl_slist *list = NULL;
	list = curl_slist_append(list, "Content-Type:application/json");
	
	  /* In windows, this will init the winsock stuff */
	  curl_global_init(CURL_GLOBAL_ALL);

	  /* get a curl handle */
	  curl = curl_easy_init();

	  if(curl) {

	    curl_easy_setopt(curl, CURLOPT_URL, httpURL);

	    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
		    /* Now specify the POST data */
	    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, httpData);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(httpData));

	    /* Perform the request, res will get the return code */
	    res = curl_easy_perform(curl);

	    /* Check for errors */
	    if(res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() postHttpData failed: %s\n",
		      curl_easy_strerror(res));
            processing = NO;
            return -1;
        }
	    /* always cleanup */
	    curl_easy_cleanup(curl);
	  }
	  curl_global_cleanup();
	processing  = NO;
	  return 0;	


}

int postHttpDataCB(char *httpURL, char *httpData, struct curl_fetch_st *chunk)
{
	processing  = YES;
    CURL *curl;
    CURLcode res;
    struct curl_slist *list = NULL;
    list = curl_slist_append(list, "Content-Type:application/json");
    chunk->payload = malloc(1);  /* will be grown as needed by the realloc above */
    chunk->size = 0;     /* In windows, this will init the winsock stuff */
    curl_global_init(CURL_GLOBAL_ALL);
    
    /* get a curl handle */
    curl = curl_easy_init();
    
    if(curl) {
        
        curl_easy_setopt(curl, CURLOPT_URL, httpURL);
        
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);        /* Now specify the POST data */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, httpData);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(httpData));
        
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        
        /* Check for errors */
        if(res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() postHttpData failed: %s\n",
                    curl_easy_strerror(res));
            processing = NO;
            return -1;
        }
        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
	processing  = NO;
    return 0;
    
    
}
size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;                             /* calculate buffer size */
    p = (struct curl_fetch_st *) userp;   /* cast pointer to fetch struct */

    /* expand buffer */
    p->payload = (char *) realloc(p->payload, p->size + realsize + 1);
    /* check buffer */
    if (p->payload == NULL) {
      /* this isn't good */
      fprintf(stderr, "ERROR: Failed to expand buffer in curl_callback");
      /* free buffer */
      free(p->payload);
      /* return */
      return -1;
    }
    /* copy contents to buffer */
    memcpy(&(p->payload[p->size]), contents, realsize);
    /* set new buffer size */
    p->size += realsize;
    /* ensure null termination */
    p->payload[p->size] = 0;
	/* return size */
    return realsize;
}


int getHttpData(char *httpURL,struct curl_fetch_st *chunk, long timeOut)
{
	processing  = YES;
	CURL *curl;
	CURLcode res;
    //struct curl_fetch_st chunk;
    //printf("curl_version %s\n",curl_version());
    chunk->payload = malloc(1);  /* will be grown as needed by the realloc above */
    chunk->size = 0;    /* no data at this point */
    
	struct curl_slist *list = NULL;
	list = curl_slist_append(list, "Content-Type:application/json");

	/* In windows, this will init the winsock stuff */
	curl_global_init(CURL_GLOBAL_ALL);

	/* get a curl handle */
	curl = curl_easy_init();

	if(curl) {

	/* First set the URL that is about to receive our POST. This URL can
	   just as well be a https:// URL if that is what should receive the
	   data. */
	curl_easy_setopt(curl, CURLOPT_URL, httpURL);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	//curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeOut);
	//curl_easy_setopt(curl, CURLOPT_NOSIGNAL,1L);
	
	/* Perform the request, res will get the return code */
	res = curl_easy_perform(curl);

	/* Check for errors */
	if(res != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform()  getHttpData failed: %s\n",
		      curl_easy_strerror(res));
        processing = NO;
        return -1;
    }
	/* always cleanup */
        //printf("%s\n",__FUNCTION__);
	curl_easy_cleanup(curl);
	}
	curl_global_cleanup();
	processing  = NO;
	return 0;
}
/*
int main()
{
	//postHttpData("http://localhost:8100/homescreen","");
	getHttpData(NULL);

}*/
