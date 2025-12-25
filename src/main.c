#include <locale.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "ncurses_wrapper.h"
#include "ollama_client.h"
#include "ollama_json.h"

#define ROUTER "router_SLM:latest"
#define KEY_ESC 27
#define PAD_HEIGHT 3000

void
draw_home_frame(WINDOW *win) {
    wattron(win, COLOR_PAIR(1));
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " Chat ");
    wattroff(win, COLOR_PAIR(1));
    wrefresh(win);
}

void
draw_input(WINDOW *win) {
    wattron(win, COLOR_PAIR(2));
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " Input ");
    wattroff(win, COLOR_PAIR(2));
    wrefresh(win);
}

void
refresh_pad_view(
    WINDOW *pad,
    const int scroll_y,
    const int screen_height,
    const int screen_width
) {

    prefresh(
        pad,
        scroll_y,
        0,
        1,
        1,
        screen_height - 5,
        screen_width - 2
    );
}

static bool
cJSON_error_print(const cJSON* json) {
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error json parsing: %s\n", error_ptr);
        }

        return false;
    }

    return true;
}

static char*
    filter_str(const char* str) {
    size_t len = 0;
    for (len = 0; str[len] != '\0'; len++) {
        if (str[len] == '-' || str[len] == '_' || str[len] == ':') break;
    }

    char* str_temp = calloc(len + 1, sizeof(char));
    if (!str_temp) return nullptr;

    for (size_t j = 0; j < len; j++) {
        str_temp[j] = str[j];
    }
    return str_temp;
}

int main(void) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();

    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_CYAN, COLOR_BLACK);

    int max_h, max_w;
    getmaxyx(stdscr, max_h, max_w);

    WINDOW *home_frame = newwin(max_h - 3, max_w, 0, 0);

    WINDOW *chat_pad = newpad(PAD_HEIGHT, max_w - 2);
    scrollok(chat_pad, TRUE);

    int pad_pos = 0;
    int scroll_y = 0;
    const int view_height = max_h - 5;

    WINDOW *input_win = newwin(3, max_w, max_h - 3, 0);
    keypad(input_win, TRUE);

    refresh();
    draw_home_frame(home_frame); // Disegna bordo fisso
    draw_input(input_win);

    wmove(input_win, 1, 1);
    wrefresh(input_win);

    size_t buf_size = 1024;
    size_t len = 0;
    char *input_text = calloc(buf_size, sizeof(char));

    int ch;
    int input_cursor_index = 1;
    while ((ch = wgetch(input_win)) != KEY_ESC) {

        // Manual Scrolling
        // Page Up o Arrow Up
        if (ch == KEY_PPAGE || ch == KEY_UP) {
            if (scroll_y > 0) scroll_y--;
            if (ch == KEY_PPAGE) scroll_y -= 5;
            if (scroll_y < 0) scroll_y = 0;

            refresh_pad_view(chat_pad, scroll_y, max_h, max_w);
            continue;
        }

        // Page Down or Arrow Down
        if (ch == KEY_NPAGE || ch == KEY_DOWN) {
            if (scroll_y < pad_pos - view_height + 1) scroll_y++;
            if (ch == KEY_NPAGE) scroll_y += 5;

            if (scroll_y > pad_pos) scroll_y = pad_pos;

            refresh_pad_view(chat_pad, scroll_y, max_h, max_w);
            continue;
        }

        const int input_len = (int)strlen(input_text);
        if (ch == KEY_LEFT && input_len > 0 && input_cursor_index > 1) {
            input_cursor_index--;
            wmove(input_win, 1, input_cursor_index);
            wrefresh(input_win);
            continue;
        }

        if (ch == KEY_RIGHT && input_len > 0 && input_cursor_index <= input_len) {
            input_cursor_index++;
            wmove(input_win, 1, input_cursor_index);
            wrefresh(input_win);
            continue;
        }

        // Enter pressed
        if ((ch == '\n' || ch == KEY_ENTER) && len > 0) {

            // Write You tag
            wattron(chat_pad, A_BOLD);
            mvwprintw(chat_pad, pad_pos, 0, " You: %s\n", input_text);
            pad_pos++;
            wattroff(chat_pad, A_BOLD);

            // Update view and auto-scroll if is needed
            if (pad_pos > view_height) scroll_y = pad_pos - view_height + 1;
            refresh_pad_view(chat_pad, scroll_y, max_h, max_w);

            // Clean input
            input_cursor_index = 1;
            wmove(input_win, 1, input_cursor_index);
            wclrtoeol(input_win);
            draw_input(input_win);
            wrefresh(input_win);

            // Orchester SLM Query
            char* router_query = create_orchester_query(input_text);

            char* json_request = create_ollama_request(ROUTER, router_query, true);
            char* query_routing = send_request_to_ollama(json_request);
            char* response_routing = parse_ollama_response(query_routing);

            // Get json
            cJSON* routing_info = cJSON_Parse(response_routing);

            if (!cJSON_error_print(routing_info)) {
                free(router_query);
                free(json_request);
                free(query_routing);
                free(response_routing);

                cJSON_Delete(routing_info);
                return EXIT_FAILURE;
            }

            cJSON* model_field = cJSON_GetObjectItem(routing_info, "target_model");
            if (!cJSON_IsString(model_field)) {
                free(router_query);
                free(json_request);
                free(query_routing);
                free(response_routing);

                cJSON_Delete(routing_info);
                cJSON_Delete(model_field);
                return EXIT_FAILURE;
            }

            char* model_name = nullptr;
            if (cJSON_IsString(model_field))
                model_name = strdup(model_field->valuestring);

            else {
                free(router_query);
                free(json_request);
                free(query_routing);
                free(response_routing);
                free(model_name);

                cJSON_Delete(routing_info);
                cJSON_Delete(model_field);
                return EXIT_FAILURE;
            }

            free(query_routing);

            // DEBUG PRINT
            mvwprintw(chat_pad, pad_pos, 0, " Orchester: ");
            pad_pos += print_smart(chat_pad, pad_pos, 12, response_routing);
            refresh_pad_view(chat_pad, scroll_y, max_h, max_w);

            free(response_routing);

            // LLM Query
            char* llm_query = create_ollama_request(model_name, input_text, false);
            char* llm_response = send_request_to_ollama(llm_query);

            if (llm_response) {
                char* clean_text = parse_ollama_response(llm_response);
                char* clean_name = filter_str(model_name);

                // Write LLM response
                mvwprintw(chat_pad, pad_pos, 0, " %s: ", clean_name);

                const int start_x = (int)strlen(clean_name);
                pad_pos += print_smart(chat_pad, pad_pos, start_x + 3, clean_text);

                if (pad_pos > view_height) {
                    scroll_y = pad_pos - view_height + 1;
                }

                free(clean_text);
                free(model_name);
                free(llm_query);
                free(llm_response);

                // Redraw pad
                refresh_pad_view(chat_pad, scroll_y, max_h, max_w);
            }

            memset(input_text, 0, buf_size);
            len = 0;

            // wmove(input_win, input_cursor_index, 1);
            // wclrtoeol(input_win);
            // draw_input(input_win);
            // wmove(input_win, 1, 1);

        } else if ((ch == KEY_BACKSPACE || ch == 127 || ch == '\b') && len > 0) {

            // C I A O \0
            // C
            // I A O \0

            const int array_cursor = input_cursor_index - 1;
            memmove(
                &input_text[array_cursor - 1],
                &input_text[array_cursor],
                strlen(input_text) - array_cursor + 1
            );

            input_cursor_index--;
            len--;

            wmove(input_win, 1, 1);
            wclrtoeol(input_win);
            draw_input(input_win);

            mvwaddstr(input_win, 1, 1, input_text);

            wmove(input_win, 1, input_cursor_index);
            wrefresh(input_win);

        } else if (len < buf_size - 1 && ch >= 32 && ch <= 126) {
            const int array_pos = input_cursor_index - 1;

            memmove(
                &input_text[array_pos + 1],
                &input_text[array_pos],
                len - array_pos + 1
            );

            input_text[array_pos] = (char)ch;

            len++;
            input_cursor_index++;

            wmove(input_win, 1, 1);
            wclrtoeol(input_win);
            draw_input(input_win);
            mvwaddstr(input_win, 1, 1, input_text);
            wmove(input_win, 1, input_cursor_index);
        }

        wrefresh(input_win);
    }

    free(input_text);
    delwin(input_win);
    delwin(home_frame);
    delwin(chat_pad);
    endwin();
    return 0;
}