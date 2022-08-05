//struct curl_fetch_st *p = NULL;
#define YES 1
#define NO 0

struct curl_fetch_st {
    char *payload;
    size_t size;
};
int postHttpData(char *httpURL, char *httpData);
size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp);
int getHttpData(char *httpURL,struct curl_fetch_st *chunk, long timeOut);
int postHttpDataCB(char *httpURL, char *httpData, struct curl_fetch_st *chunk);

