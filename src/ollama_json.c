//
// Created by Eliomar Alejandro Rodriguez Ferrer on 22/12/25.
//

#include <stdio.h>
#include <_string.h>

#include "ollama_json.h"

#include <stdlib.h>

#include "cJSON.h"

char*
create_ollama_request(
    const char* model,
    const char* prompt,
    const bool isJson
) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "model", model);
    cJSON_AddStringToObject(json, "prompt", prompt);

    if (isJson) {
        cJSON_AddStringToObject(json, "format", "json");
    }

    cJSON_AddItemToObject(json, "stream", cJSON_CreateFalse());

    char* string = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return string;
}

char*
parse_ollama_response(const char* raw_json) {
    cJSON *json = cJSON_Parse(raw_json);
    if (json == NULL) {
        return strdup("Internal Error: Server returned no-JSON data (or empty).");
    }

    const cJSON *error_item = cJSON_GetObjectItemCaseSensitive(json, "error");
    if (cJSON_IsString(error_item) && (error_item->valuestring != NULL)) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Ollama Error: %s", error_item->valuestring);

        cJSON_Delete(json);
        return strdup(error_msg);
    }

    const cJSON *response_item = cJSON_GetObjectItemCaseSensitive(json, "response");
    char *result;

    if (cJSON_IsString(response_item) && (response_item->valuestring != NULL)) {
        result = strdup(response_item->valuestring);

    } else {
        result = strdup("Error: 'response' field not found in JSON.");
    }

    cJSON_Delete(json);
    return result;
}

char*
create_orchester_query(const char* input_text) {
    // --- Orchestrator Request ---
    cJSON *json_orchestrator = cJSON_CreateObject();

    // Models placeholder
    const char *models_data[] = {
        "qwen2.5-coder:7b"
    };
    cJSON *models_array = cJSON_CreateStringArray(models_data, 1);
    cJSON_AddItemToObject(json_orchestrator, "available_models", models_array);

    // Context placeholder
    // const char *history_data[] = {
    //     "Intelligente"
    // };
    // cJSON *history_array = cJSON_CreateStringArray(history_data, 1);
    cJSON *history_array = cJSON_CreateArray();
    cJSON_AddItemToObject(json_orchestrator, "chat_history", history_array);

    // Input
    cJSON_AddStringToObject(json_orchestrator, "current_input", input_text);

    // Pase json to SLM
    char* json = cJSON_Print(json_orchestrator);

    cJSON_Delete(json_orchestrator);

    return json;
}