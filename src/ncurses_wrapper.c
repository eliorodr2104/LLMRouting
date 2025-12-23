//
// Created by Eliomar Alejandro Rodriguez Ferrer on 22/12/25.
//

#include "ncurses_wrapper.h"

// Funzione helper: Stampa testo con wrapping e indentazione corretta
// Ritorna il numero di righe EFFETTIVAMENTE utilizzate
int
print_smart(
    WINDOW *win,
    const int start_y,
    const int start_x,
    const char *text
) {
    int max_y, max_x;
    getmaxyx(win, max_y, max_x);

    // Posizionamento iniziale
    wmove(win, start_y, start_x);

    const char *ptr = text;

    while (*ptr) {
        int cur_y, cur_x;
        getyx(win, cur_y, cur_x);

        // --- CASO 1: Newline esplicito nel testo ---
        if (*ptr == '\n') {
            wmove(win, cur_y + 1, start_x); // Vai a capo e indentazione forzata
            ptr++;
            continue;
        }

        // --- CASO 2: Spazi ---
        if (*ptr == ' ') {
            // Se siamo a fine riga, ignoriamo lo spazio per evitare wrap "brutti"
            // altrimenti lo stampiamo
            if (cur_x < max_x - 1) {
                waddch(win, ' ');
            }
            ptr++;
            continue;
        }

        // --- CASO 3: Parola (o sequenza di caratteri non-spazio) ---
        // Calcoliamo la lunghezza in BYTES della prossima parola
        int word_bytes = 0;
        const char *temp = ptr;
        while (*temp && *temp != ' ' && *temp != '\n') {
            temp++;
            word_bytes++;
        }

        // Controllo se la parola entra nella riga corrente.
        // Nota: word_bytes è una stima per eccesso della larghezza (perché le accentate usano 2 byte ma occupano 1 colonna).
        // È una stima sicura: se i byte stanno, sicuramente la parola visuale ci sta.
        if (cur_x + word_bytes >= max_x) {
            // Non ci sta, andiamo a capo PRIMA di stampare
            wmove(win, cur_y + 1, start_x);
        }

        // Stampiamo l'intera parola in un colpo solo.
        // waddnstr gestisce correttamente la sequenza UTF-8 (es. "è" viene stampato intero)
        waddnstr(win, ptr, word_bytes);

        // Avanziamo il puntatore nel testo originale
        ptr += word_bytes;
    }

    // Calcoliamo dove siamo finiti per ritornare l'altezza usata
    int final_y, final_x;
    getyx(win, final_y, final_x);

    return final_y - start_y + 1;
}