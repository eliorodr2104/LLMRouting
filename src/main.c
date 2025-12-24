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

// Disegna solo la cornice (il contenitore)
void draw_home_frame(WINDOW *win) {
    wattron(win, COLOR_PAIR(1));
    box(win, 0, 0); // Disegna il bordo verde
    mvwprintw(win, 0, 2, " Chat ");
    wattroff(win, COLOR_PAIR(1));
    wrefresh(win);
}

void draw_input(WINDOW *win) {
    wattron(win, COLOR_PAIR(2));
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " Input ");
    wattroff(win, COLOR_PAIR(2));
    wrefresh(win);
}

// Funzione helper per aggiornare la vista del Pad
void refresh_pad_view(WINDOW *pad, const int scroll_y, const int screen_height, const int screen_width) {
    // prefresh(pad,
    //          pad_min_y, pad_min_x,       <- Angolo alto-sx del contenuto del pad da mostrare
    //          screen_min_y, screen_min_x, <- Dove inizia a disegnare sullo schermo (dentro il bordo)
    //          screen_max_y, screen_max_x) <- Dove finisce a disegnare sullo schermo

    // Disegniamo da riga 1, colonna 1 a riga H-2, colonna W-2 (per stare dentro il box)
    prefresh(pad,
             scroll_y, 0,       // Da dove inizia il pad
             1, 1,              // Dove inizia sullo schermo (top-left)
             screen_height - 5, // <--- MODIFICA QUI: Limite in basso (bottom-right)
             screen_width - 2   // Limite a destra
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

static char* filter_str(const char* str) {
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

    // 1. HOME WIN (Solo Cornice)
    // Altezza schermo - 3 (spazio per input)
    WINDOW *home_frame = newwin(max_h - 3, max_w, 0, 0);

    // 2. CHAT PAD (Contenuto Scrorrevole)
    // Alto PAD_HEIGHT, Largo quanto l'interno della cornice (width - 2)
    WINDOW *chat_pad = newpad(PAD_HEIGHT, max_w - 2);
    // Abilita lo scrolling del pad stesso (opzionale, ma utile per waddch)
    scrollok(chat_pad, TRUE);

    // Variabili di stato per lo scroll
    int pad_pos = 0; // Dove stiamo scrivendo attualmente nel pad
    int scroll_y = 0; // La riga del pad che è visualizzata in alto nello schermo
    int view_height = max_h - 5; // Altezza visibile effettiva (tolti bordi cornice e input)

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
    while ((ch = wgetch(input_win)) != KEY_ESC) {

        // --- GESTIONE SCROLLING MANUALE ---
        // Se premi PagSu/PagGiù o Frecce su input, scorriamo la chat sopra
        if (ch == KEY_PPAGE || ch == KEY_UP) { // Page Up o Freccia Su
            if (scroll_y > 0) scroll_y--;
            if (ch == KEY_PPAGE) scroll_y -= 5; // Scroll più veloce
            if (scroll_y < 0) scroll_y = 0;

            refresh_pad_view(chat_pad, scroll_y, max_h, max_w);
            continue; // Torna al loop, non scrivere nell'input
        }

        if (ch == KEY_NPAGE || ch == KEY_DOWN) { // Page Down o Freccia Giù
            // Non scorrere oltre l'ultima riga scritta
            if (scroll_y < pad_pos - view_height + 1) scroll_y++;
            if (ch == KEY_NPAGE) scroll_y += 5;
            // Controllo limiti (non andare nel vuoto troppo oltre)
            if (scroll_y > pad_pos) scroll_y = pad_pos;

            refresh_pad_view(chat_pad, scroll_y, max_h, max_w);
            continue;
        }

        // --- GESTIONE INVIO ---
        if ((ch == '\n' || ch == KEY_ENTER) && len > 0) {

            // 1. Scrivi "You: ..." nel PAD
            wattron(chat_pad, A_BOLD);
            // Usiamo print_smart anche qui per sicurezza o mvwprintw standard
            // Nota: pad_pos è la riga, 0 è la colonna (relativa al pad)
            mvwprintw(chat_pad, pad_pos, 0, " You: %s\n", input_text);
            pad_pos++;
            wattroff(chat_pad, A_BOLD);

            // Aggiorna vista subito
            // Auto-scroll in basso se necessario
            if (pad_pos > view_height) scroll_y = pad_pos - view_height + 1;
            refresh_pad_view(chat_pad, scroll_y, max_h, max_w);

            // Pulisci input
            wmove(input_win, 1, 1);
            wclrtoeol(input_win);
            draw_input(input_win); // Ridisegna bordo input se rovinato
            wrefresh(input_win);

            // Orchester SLM query
            char* router_query = create_orchester_query(input_text);

            char* json_request = create_ollama_request(ROUTER, router_query, true);
            char* query_routing = send_request_to_ollama(json_request);
            char* response_routing = parse_ollama_response(query_routing);

            // LLM Request
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
            // free(json_routing);

            // DEBUG PRINT
            mvwprintw(chat_pad, pad_pos, 0, " Orchester: ");
            pad_pos += print_smart(chat_pad, pad_pos, 12, response_routing);
            refresh_pad_view(chat_pad, scroll_y, max_h, max_w);

            char* llm_query = create_ollama_request(model_name, input_text, false);
            char* llm_response = send_request_to_ollama(llm_query);

            if (llm_response) {
                char* clean_text = parse_ollama_response(llm_response);
                char* clean_name = filter_str(model_name);

                // 2. Scrivi risposta LLM nel PAD
                mvwprintw(chat_pad, pad_pos, 0, " %s: ", clean_name);

                // IMPORTANTE: passiamo chat_pad a print_smart.
                // start_x è 6 (dopo " LLM: "), start_y è pad_pos attuale
                const int start_x = (int)strlen(clean_name);
                pad_pos += print_smart(chat_pad, pad_pos, start_x + 3, clean_text);

                // Auto-scroll in fondo per vedere la risposta
                if (pad_pos > view_height) {
                    scroll_y = pad_pos - view_height + 1; // +1 per margine
                }

                free(clean_text);
                free(model_name);
                free(llm_query);
                free(llm_response);

                // Ridisegna il pad nella sua nuova posizione
                refresh_pad_view(chat_pad, scroll_y, max_h, max_w);
            }

            memset(input_text, 0, buf_size);
            len = 0;

            wmove(input_win, 1, 1);
            wclrtoeol(input_win);
            draw_input(input_win);
            wmove(input_win, 1, 1);

        } else if ((ch == KEY_BACKSPACE || ch == 127 || ch == '\b') && len > 0) {
            len--;
            input_text[len] = '\0';
            mvwaddch(input_win, 1, 1 + len, ' ');
            wmove(input_win, 1, (int)len + 1);

        } else if (len < buf_size - 1 && ch >= 32 && ch <= 126) {
            input_text[len] = (char)ch;
            input_text[len + 1] = '\0';
            waddch(input_win, ch);
            len++;
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