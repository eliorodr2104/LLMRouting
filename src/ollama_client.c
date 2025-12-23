#include "ollama_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

void log_debug(const char *message) {
    FILE *f = fopen("debug_log.txt", "a"); // "a" appende al file
    if (f) {
        fprintf(f, "[DEBUG] %s\n", message);
        fclose(f);
    }
}

// Struttura per la memoria
struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(
    const void *contents,
    const size_t size,
    const size_t nmemb,
    const void *userp
) {
    size_t real_size = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct*)userp;

    char *ptr = realloc(mem->memory, mem->size + real_size + 1);
    if(!ptr) {
        log_debug("Error: Out of memory in callback!");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, real_size);
    mem->size += real_size;
    mem->memory[mem->size] = 0;

    return real_size;
}

char* send_request_to_ollama(const char* json_payload) {
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    CURL *curl_handle = curl_easy_init();

    if(curl_handle) {
        // URL
        curl_easy_setopt(curl_handle, CURLOPT_URL, "http://localhost:11434/api/generate");

        // Headers
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);

        // Payload
        curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, json_payload);

        // Callback
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

        // Exec
        log_debug("Eseguendo curl_easy_perform...");
        const CURLcode res = curl_easy_perform(curl_handle);

        if(res != CURLE_OK) {
            // Log error
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "CURL FAILED: %s", curl_easy_strerror(res));
            log_debug(error_msg);

            free(chunk.memory);
            chunk.memory = nullptr;

        } else {  log_debug("CURL success! Response received."); }

        // Clean
        curl_easy_cleanup(curl_handle);
        curl_slist_free_all(headers);

    } else { log_debug("Init failed: curl_easy_init"); }

    curl_global_cleanup();
    return chunk.memory;
}