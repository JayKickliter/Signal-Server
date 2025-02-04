#include <curl/curl.h>
#include <jansson.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE (256 * 1024) /* 256 KB */

#define URL_FORMAT "http://portal.hamwan.ca/coverages"
#define URL_SIZE 256

/* Return the offset of the first newline in text or the length of
   text if there's no newline */
static int newline_offset(const char * text) {
    const char * newline = strchr(text, '\n');
    if (!newline)
        return strlen(text);
    else
        return (int)(newline - text);
}

struct write_result {
    char * data;
    int pos;
};

static size_t write_response(void * ptr, size_t size, size_t nmemb, void * stream) {
    struct write_result * result = (struct write_result *)stream;

    if (result->pos + size * nmemb >= BUFFER_SIZE - 1) {
        fprintf(stderr, "error: too small buffer\n");
        return 0;
    }

    memcpy(result->data + result->pos, ptr, size * nmemb);
    result->pos += size * nmemb;

    return size * nmemb;
}

static char * request(const char * url) {
    CURL * curl;
    CURLcode status;
    char * data;
    long code;

    curl = curl_easy_init();
    data = (char *)malloc(BUFFER_SIZE);
    if (!curl || !data)
        return NULL;

    struct write_result write_result = {.data = data, .pos = 0};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);

    status = curl_easy_perform(curl);
    if (status != 0) {
        fprintf(stderr, "error: unable to request data from %s:\n", url);
        fprintf(stderr, "%s\n", curl_easy_strerror(status));
        return NULL;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if (code != 200) {
        fprintf(stderr, "error: server responded with code %ld\n", code);
        return NULL;
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    /* zero-terminate the result */
    data[write_result.pos] = '\0';

    return data;
}

int main(int argc, char * argv[]) {
    size_t i;
    char * text;
    char url[URL_SIZE];

    json_t * root;
    json_error_t error;
    json_t * commits;

    if (argc != 3) {
        fprintf(stderr, "usage: %s USER REPOSITORY\n\n", argv[0]);
        fprintf(stderr, "List commits at USER's REPOSITORY.\n\n");
        return 2;
    }

    snprintf(url, URL_SIZE, URL_FORMAT, argv[1], argv[2]);

    text = request(URL_FORMAT);
    if (!text)
        return 1;

    root = json_loads(text, 0, &error);
    free(text);

    if (!root) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return 1;
    }

    commits = json_object_get(root, "sites");
    if (!json_is_array(commits)) {
        fprintf(stderr, "error: commits is not an array\n");
        return 1;
    }

    for (i = 0; i < json_array_size(commits); i++) {
        json_t *commit, *id, *message;
        const char * message_text;

        commit = json_array_get(commits, i);
        if (!json_is_object(commit)) {
            fprintf(stderr, "error: commit %d is not an object\n", i + 1);
            return 1;
        }

        id = json_object_get(commit, "id");
        if (!json_is_string(id)) {
            fprintf(stderr, "error: commit %d: id is not a string\n", i + 1);
            return 1;
        }

        message = json_object_get(commit, "message");
        if (!json_is_string(message)) {
            fprintf(stderr, "error: commit %d: message is not a string\n", i + 1);
            return 1;
        }

        message_text = json_string_value(message);
        printf("%.8s %.*s\n",
               json_string_value(id),
               newline_offset(message_text),
               message_text);
    }

    json_decref(root);
    return 0;
}
