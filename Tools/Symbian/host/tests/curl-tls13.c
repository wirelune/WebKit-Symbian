/* Fetches a page through the host's curl and prints what was negotiated:
 * curl-tls13 URL CA_BUNDLE. Exits 0 only on HTTP/2 over TLS 1.3, which is
 * what the phone's curl gets from the same servers. */
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>

static int sawTLS13;

static size_t discard(char* data, size_t size, size_t count, void* context)
{
    (void)data;
    (void)context;
    return size * count;
}

static int trace(CURL* handle, curl_infotype type, char* data, size_t size, void* context)
{
    (void)handle;
    (void)context;
    if (type == CURLINFO_TEXT && size > 0) {
        fwrite(data, 1, size, stdout);
        if (strstr(data, "TLS1-3"))
            sawTLS13 = 1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    CURL* curl;
    CURLcode result;
    long version = 0;
    if (argc < 3)
        return 2;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, argv[1]);
    curl_easy_setopt(curl, CURLOPT_CAINFO, argv[2]);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, (long)(CURL_SSLVERSION_TLSv1_2 | CURL_SSLVERSION_MAX_TLSv1_3));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, trace);
    result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_HTTP_VERSION, &version);
    printf("result %d, http version %ld, tls 1.3 %d\n", (int)result, version, sawTLS13);
    curl_easy_cleanup(curl);
    return result == CURLE_OK && version == CURL_HTTP_VERSION_2_0 && sawTLS13 ? 0 : 1;
}
