//
// Created by Eliomar Alejandro Rodriguez Ferrer on 22/12/25.
//

#ifndef LLMROUTING_OLLAMA_JSON_H
#define LLMROUTING_OLLAMA_JSON_H

char* create_ollama_request(
    const char* model,
    const char* prompt,
    bool isJson
);

char* parse_ollama_response(const char* raw_json);

char* create_orchester_query(const char* input_text);

#endif //LLMROUTING_OLLAMA_JSON_H