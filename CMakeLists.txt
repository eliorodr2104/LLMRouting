cmake_minimum_required(VERSION 4.1)
project(LLMRouting C)

set(CMAKE_C_STANDARD 23)

find_package(Curses REQUIRED)
find_package(CURL REQUIRED)

include_directories(
        ${CURSES_INCLUDE_DIR}
        ${CURL_INCLUDE_DIRS}
        src/include
)

add_executable(LLMRouting
        src/main.c
        src/ollama_json.c
        src/cJSON.c
        src/ollama_client.c
        src/ncurses_wrapper.c
        src/include/ncurses_wrapper.h
)

target_link_libraries(LLMRouting
        ${CURSES_LIBRARIES}
        ${CURL_LIBRARIES}
)