/*  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  Implementace prekladace imperativniho jazyka IFJ17
 *
 *  Autori:
 *      xvenge00 - Adam Venger
 *      xbabka01 - Peter Babka
 *      xrandy00 - Vojtech Randysek
 *      xdosed08 - Ondrej Dosedel
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include "scanner.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "memwork.h"
#include "str_buff.h"
#include "err.h"

#define key_size 35

/*
 * zaciatok a konec fronty tokenou
 */
t_scanner_node *scanner_head = NULL;
t_scanner_node *scanner_tail = NULL;

/**
 * zaradi token na konec fronty
 * @param token
 */
void scanner_append(t_token *token) {
    t_scanner_node *result = my_malloc(sizeof(t_scanner_node));
    result->token = token;
    result->next = NULL;
    if (scanner_head == NULL) {
        scanner_head = scanner_tail = result;
    } else {
        scanner_tail->next = result;
        scanner_tail = result;
    }
}

/**
 *  zoberie prvy prvok z fronty a vrati token ak je fronta prazdna nacita token
 * @return token
 */
t_token *get_token() {
    t_token *result = NULL;
    if (scanner_head == NULL) {
        scanner_tail = NULL;

        return load_token();
    } else {
        result = scanner_head->token;
        t_scanner_node *temp = scanner_head;
        if (scanner_tail == temp) {
            scanner_tail = NULL;
        }

        scanner_head = scanner_head->next;
        my_free(temp);
        return result;
    }
}

/**
 * zaradi token na zaciatok fronty
 * @param token
 */
void return_token(t_token *token) {
    if (token != NULL) {
        t_scanner_node *result = my_malloc(sizeof(t_scanner_node));
        result->token = token;
        result->next = scanner_head;
        scanner_head = result;
        if (scanner_tail == NULL) {
            scanner_tail = scanner_head;
        }
    }
}

/**
 * uvolni token
 * @param token
 */
void discard_token(t_token *token) {
    if (token == NULL) {
    } else if (token->token_type == ID || token->token_type == STR) {
        my_free(token->data.s);
    }
    my_free(token);
}

/**
 * vymenovane klucove slova
 */
const char key_word_str[35][20] = {
        "as",       //0
        "asc",
        "declare",
        "dim",
        "do",
        "double",   //5
        "else",
        "end",
        "chr",
        "function",
        "if",       //10
        "input",
        "integer",
        "length",
        "loop",
        "print",    //15
        "return",
        "scope",
        "string",
        "substr",
        "then",     //20
        "while",
        "and",
        "boolean",
        "continue",
        "elseif",   //25
        "exit",
        "false",
        "for",
        "next",
        "not",      //30
        "or",
        "shared",
        "static",
        "true"      //34
};


/**
 * chybove hlasenia
 */
void ERR_LEX(tstate state, char *loaded, int line) {

    fprintf(stderr, "ERR_LEX : neplatna lexema na riadku :%i  -- %s\n", line, loaded);
    if (state == s_block_coment_1 || state == s_block_coment_2) {
        fprintf(stderr, "neukonceny blokovy koment\n");
    } else if (state == s_START) {
        fprintf(stderr, "neplatny znak\n");
    } else if (state == s_double_1) {
        fprintf(stderr, "po e musi nasledovat cislo alebo +/-\n");
    } else if (state == s_double_2) {
        fprintf(stderr, "po e+/- musi nasledovat cislo\n");
    } else if (state == s_str0) {
        fprintf(stderr, "mozno ste mysleli !\"\"\n");
    } else if (state == s_strL) {
        fprintf(stderr, "retazec moze obsahovat znaky z ASCII vysie ako 31 !\"\"\n");
    } else if (state == s_str_spec) {
        fprintf(stderr, "po znaku \\ moye nasledovat cislo 000 - 255 n t ...\n");
    } else if (state == s_str_spec_hexa0 || state == s_str_spec_hexa1) {
        fprintf(stderr, "escape sekvenia potrebuje 3 cisla\n");
    } else {
        fprintf(stderr, " mozno ste mysleli ");
        for (unsigned i = 0; i < strlen(loaded) - 1; ++i) {
            fputc(loaded[i], stderr);
        }
        putchar('\n');
    }

    clear_all();
    exit(ERR_LEXIK);
}


/**
 * skusa ci ret je klucove slovo
 * @param ret porovnacany retazec
 * @return index +1 klucoveho slova v premennej key_word_strs alebo 0 pokial to nieje klucove slovo
 */
int is_keyword(char *ret) {
    for (unsigned i = 0; i < key_size; i++) {
        if (strcmp(ret, key_word_str[i]) == 0) {
            return i + 1;
        }
    }

    return 0;
}

t_str_buff *scanner_buff = NULL;
int beginning = 1;

/**
 * konstruktor tokenu
 * @param typ akeho typu je token (KEYWORD, ID, ...)
 * @param data hodnotu ktoru ma obsahovat
 * @param line riadok z ktoreho bol nacitany
 * @return token
 */
t_token *create_token(ttype typ, tdata data, unsigned *line) {
    t_token *tmp = my_malloc(sizeof(t_token));

    tmp->token_type = typ;
    tmp->data = data;
    tmp->line = *line;
    //vymaz buffer
    my_free(scanner_buff->ret);
    //my_free(scanner_buff);       //valgrind ukazuje chybu
    scanner_buff->top = 0;
    scanner_buff->max = 0;
    if (typ == EOL) {
        (*line)++;
    }
    if (typ == EMPTY) {
        beginning = 1;
    }

    return tmp;
}

/**
 * funckia ktore nacitava zo f a vytvara z neho token ktory vyhovuje ak nacita nieco co nevyhovuje vola ERR_LEX a konci
 * @return token vytvoreny z f
 */
t_token *load_token() {
    //zachovany znak z predchadzajuceho volania
    static int old = 0;

    tdata data;
    data.s = NULL;

    scanner_buff = init_buff();                 //priprava bufferu
    static int loaded = 0;                      //inicializacia znaku
    static unsigned line = 1;                   //riadok ktory je spracovavany
    //restartovanie pocitadla raidkov pre testy
    if (beginning) {
        beginning = 0;
        line = 1;
    }

    tstate state = s_START;                     //inicializovanie stavu na stav STRAT

    int pom = 0;                                //pomocna premmena pre stavy
/**********************************************
 *  stavovy automat
 ***********************************************/

    //vykonava cyklus pokial neskoncil subor alebo pokial sa nenasiel token
    do {
        if (old != 0) {
            loaded = old;
            old = 0;
        } else {
            loaded = fgetc(f);
        }

        switch (state) {
            /*****************************************************************************************************/
            case s_START: //stav START
                if (isspace(loaded) && (loaded != '\n')) {
                    //state = s_START; nerob nic
                } else if (isalpha(loaded) || (loaded == '_')) { // nasiel sa zaciatok ID
                    append_buff(scanner_buff, (char) loaded);
                    state = s_ID;
                } else if (loaded == '\'') { // riadkovy komment
                    state = s_line_comment;
                } else if (loaded == '/') {  //blokovy koment alebo deleno
                    state = s_block_coment_0;
                } else if (isdigit(loaded)) {    // zaciatok INT
                    append_buff(scanner_buff, (char) loaded);
                    state = s_INT;
                } else if (loaded == '!') {  // mozny retazec
                    state = s_str0;
                } else if (loaded == '+') {  // operacia plus
                    return create_token(PLUS, data, &line);
                } else if (loaded == '-') {  // operacia minus
                    return create_token(MINUS, data, &line);
                } else if (loaded == '*') {  // operacia krat
                    return create_token(KRAT, data, &line);
                } else if (loaded == '\\') { // operacia modulo
                    return create_token(MOD, data, &line);
                } else if (loaded == '=') {  // operacia zhodne
                    return create_token(EQ, data, &line);
                } else if (loaded == '>') {  // operacia porovnania moyne vysledky >= >
                    state = s_GT;
                } else if (loaded == '<') {// operacia porovnania moyne vysledky <> <= <
                    state = s_LT;
                } else if (loaded == '\n') { //EOL

                    while (isspace(old = fgetc(f))){
                        if (old == '\n'){
                            line++;
                        }
                    }   //odstranuje zbytocne medzery a parzdne riadky

                    return create_token(EOL, data, &line);
                } else if (loaded == '(') {
                    return create_token(LPAR, data, &line);
                } else if (loaded == ')') {
                    return create_token(RPAR, data, &line);
                } else if (loaded == ',') {
                    return create_token(COMMA, data, &line);
                } else if (loaded == ';') {
                    return create_token(SEMICOLLON, data, &line);
                } else if (loaded == EOF) {
                    return create_token(EMPTY, data, &line);
                } else if (loaded == '&') {
                    state = s_Ampersand;
                } else {    //narazil na necakany znak
                    append_buff(scanner_buff, (char) loaded);
                    append_buff(scanner_buff, 0);
                    ERR_LEX(state, get_buff(scanner_buff), line);
                }
                break;
                /*****************************************************************************************************/

            case s_line_comment:    // stav sa nemeni pokial neskonci riadok
                if (loaded == '\n') {
                    line++;
                    return create_token(EOL, data, &line);
                } else if (loaded == EOF) {
                    return create_token(EMPTY, data, &line);
                }
                break;
                /*****************************************************************************************************/

            case s_block_coment_0:  // ked je dalsi znak ' tak sa jedna o blok koment, inak je to deleno
                if (loaded == '\'') {
                    state = s_block_coment_1;
                } else {
                    old = loaded;
                    return create_token(DELENO, data, &line);
                }
                break;
                /*****************************************************************************************************/

            case s_block_coment_1: //blokovy koment pokial nenajde ' potom sa mozno jedna o konec komentu takze dalsi stav
                if (loaded == '\'') {
                    state = s_block_coment_2;
                } else if (loaded == EOF) {
                    ERR_LEX(state, get_buff(scanner_buff), line);
                } else {
                    state = s_block_coment_1;
                }
                break;
                /*****************************************************************************************************/

            case s_block_coment_2:  // pokial sa naslo / tak konci blokovy koment inak sa vrati do predosleho stavu
                if (loaded == '/') {
                    state = s_START;
                } else if (loaded == EOF) {
                    ERR_LEX(state, get_buff(scanner_buff), line);
                } else {
                    state = s_block_coment_1;
                }
                break;
                /*****************************************************************************************************/

            case s_ID:  // ostava pokial nenajde iny znak ako 0..9, a..z, A..Z, _ ak ano vrati token s menom
                if (isalnum(loaded) || (loaded == '_')) {
                    append_buff(scanner_buff, (char) loaded);
                } else {
                    // generovanie tokenu
                    append_buff(scanner_buff, 0);
                    char *buff = get_buff(scanner_buff);
                    data.s = my_malloc(sizeof(char) * (buff_size(scanner_buff)));
                    //prekopirovanie str
                    for (int i = 0; i < buff_size(scanner_buff); i++) {
                        data.s[i] = (char) tolower(buff[i]);
                    }

                    old = loaded;


                    pom = is_keyword(data.s);
                    if (pom) {
                        my_free(data.s);
                        data.i = pom - 1;
                        return create_token(KEY_WORD, data, &line);
                    } else {
                        return create_token(ID, data, &line);
                    }
                }
                break;
                /*****************************************************************************************************/

            case s_INT: // bud je int alebo double nacitava pokial je 0..9 ak je . tak prechadza na desatinne ak e na exponent
                if (isdigit(loaded)) {
                    append_buff(scanner_buff, (char) loaded);
                } else if (loaded == '.') {
                    state = s_double_comma;
                    append_buff(scanner_buff, (char) loaded);
                } else if ((loaded == 'e') || (loaded == 'E')) {
                    state = s_double_1;
                    append_buff(scanner_buff, (char) loaded);
                } else {
                    //vygeneruj token
                    old = loaded;
                    append_buff(scanner_buff, 0);
                    data.i = (int) strtol(get_buff(scanner_buff), NULL, 10);
                    return create_token(INT, data, &line);
                }
                break;
                /*****************************************************************************************************/
            case s_double_comma:    // desatinna cast realneho cisla
                if (isdigit(loaded)) {
                    append_buff(scanner_buff, (char) loaded);
                    state = s_double_0;
                } else {
                    old = loaded;
                    append_buff(scanner_buff, 0);
                    ERR_LEX(state, get_buff(scanner_buff),line);
                }
                break;
                /*****************************************************************************************************/


            case s_double_0:    // desatinna cast realneho cisla
                if (isdigit(loaded)) {
                    append_buff(scanner_buff, (char) loaded);
                } else if ((loaded == 'e') || (loaded == 'E')) {
                    state = s_double_1;
                    append_buff(scanner_buff, (char) loaded);
                } else {
                    old = loaded;
                    append_buff(scanner_buff, 0);
                    data.d = strtod(get_buff(scanner_buff), NULL);
                    return create_token(DOUBLE, data, &line);
                }
                break;
                /*****************************************************************************************************/

            case s_double_1:    // exponent prvy znak
                append_buff(scanner_buff, (char) loaded);
                if ((loaded == '+') || (loaded == '-')) {
                    state = s_double_2;
                } else if (isdigit(loaded)) {
                    state = s_double_3;
                } else {
                    append_buff(scanner_buff, (char) loaded);
                    append_buff(scanner_buff, 0);
                    ERR_LEX(state, get_buff(scanner_buff), line);
                }
                break;
                /*****************************************************************************************************/

            case s_double_2: // exponent 2. znak
                if (isdigit(loaded)) {
                    append_buff(scanner_buff, (char) loaded);
                    state = s_double_3;
                } else {
                    append_buff(scanner_buff, (char) loaded);

                    append_buff(scanner_buff, 0);
                    ERR_LEX(state, get_buff(scanner_buff), line);
                }
                break;
                /*****************************************************************************************************/

            case s_double_3:// ostavajuce cisla exponentu
                if (isdigit(loaded)) {
                    append_buff(scanner_buff, (char) loaded);
                } else {
                    append_buff(scanner_buff, 0);
                    data.d = strtod(get_buff(scanner_buff), NULL);
                    old = loaded;
                    return create_token(DOUBLE, data, &line);
                }
                break;
                /*****************************************************************************************************/

            case s_str0: // overuje ci sa jedna o string ci su "
                if (loaded == '"') {
                    state = s_strL;
                } else {
                    append_buff(scanner_buff, '!');
                    append_buff(scanner_buff, (char) loaded);
                    append_buff(scanner_buff, 0);
                    ERR_LEX(state, get_buff(scanner_buff), line);
                }
                break;
                /*****************************************************************************************************/

            case s_strL:     // naxitava string ak najde \ dekoduje
                if (loaded == '"') {
                    char *buff = get_buff(scanner_buff);
                    data.s = my_malloc(sizeof(char) * (buff_size(scanner_buff) + 1));
                    for (int i = 0; i <= buff_size(scanner_buff); i++) {
                        data.s[i] = buff[i];
                    }
                    return create_token(STR, data, &line);
                } else if (loaded == '\\') {
                    append_buff(scanner_buff, (char)loaded);
                    state = s_str_spec;
                } else if (loaded > 31 && (loaded < 256)) {
                    if (isalnum(loaded)){
                        append_buff(scanner_buff, (char) loaded);
                    } else {
                        char esc_code[5];
                        snprintf(esc_code, 4, "%03i", loaded);
                        append_buff(scanner_buff,'\\');
                        append_buff(scanner_buff, esc_code[0]);
                        append_buff(scanner_buff,esc_code[1]);
                        append_buff(scanner_buff, esc_code[2]);
                    }
                } else {
                    append_buff(scanner_buff, (char) loaded);
                    append_buff(scanner_buff, 0);
                    ERR_LEX(state, get_buff(scanner_buff), line);
                }
                break;
                /*****************************************************************************************************/

            case s_str_spec:    /* zistovanie o typu kodovania znaku za \ */
                if (isdigit(loaded)) {
                    pom = (loaded - '0')*100;
                    append_buff(scanner_buff, loaded);
                    state = s_str_spec_hexa0;
                } else if (loaded == 'n') {
                    append_buff(scanner_buff, '0');
                    append_buff(scanner_buff,'1');
                    append_buff(scanner_buff,'0');
                    state = s_strL;
                } else if (loaded == '"') {
                    append_buff(scanner_buff, '0');
                    append_buff(scanner_buff,'3');
                    append_buff(scanner_buff,'4');
                    state = s_strL;
                } else if (loaded == 't') {
                    append_buff(scanner_buff, '0');
                    append_buff(scanner_buff,'0');
                    append_buff(scanner_buff,'9');
                    state = s_strL;
                } else if (loaded == '\\') {
                    append_buff(scanner_buff, '0');
                    append_buff(scanner_buff,'9');
                    append_buff(scanner_buff,'2');
                    state = s_strL;
                } else {
                    append_buff(scanner_buff, (char) loaded);
                    append_buff(scanner_buff, 0);
                    ERR_LEX(state, get_buff(scanner_buff), line);
                }
                break;
                /*****************************************************************************************************/

            case s_str_spec_hexa0:  // druhy znak cislenho kodovania
                if (isdigit(loaded)) {
                    pom += (loaded - '0') * 10;
                    append_buff(scanner_buff, (char)loaded);
                    state = s_str_spec_hexa1;
                } else {
                    append_buff(scanner_buff, (char) loaded);
                    append_buff(scanner_buff, 0);
                    ERR_LEX(state, get_buff(scanner_buff), line);
                }
                break;
                /*****************************************************************************************************/

            case s_str_spec_hexa1:  // treti znak ciselneho kodovania
                if (isdigit(loaded)) {
                    pom += (loaded - '0');
                    append_buff(scanner_buff, (char) loaded);
                    if (pom > 255 || pom <=0) {
                        append_buff(scanner_buff, 0);
                        ERR_LEX(state, get_buff(scanner_buff), line);
                    }
                    state = s_strL;
                } else {
                    append_buff(scanner_buff, (char) loaded);
                    append_buff(scanner_buff, 0);
                    ERR_LEX(state, get_buff(scanner_buff), line);
                }
                break;
                /*****************************************************************************************************/

            case s_LT:  // stav mensi nez
                if (loaded == '>') {
                    return create_token(NEQ, data, &line);
                } else if (loaded == '=') {
                    return create_token(LE, data, &line);
                } else {
                    old = loaded;
                    return create_token(LT, data, &line);
                }
                /*****************************************************************************************************/

            case s_GT:  // stav vasci nez
                if (loaded == '=') {
                    return create_token(GE, data, &line);
                } else {
                    old = loaded;
                    return create_token(GT, data, &line);
                }
                /*****************************************************************************************************/

            case s_Ampersand:
                append_buff(scanner_buff, '0');
                if (loaded == 'b') {
                    state = s_bin_load;
                } else if (loaded == 'o') {
                    state = s_octa_load;
                } else if (loaded == 'h') {
                    state = s_hexa_load;
                } else {
                    ERR_LEX(state, "&", line);
                }
                break;
                /*****************************************************************************************************/

            case s_bin_load:
                if (loaded == '0' || loaded == '1') {
                    append_buff(scanner_buff, (char) loaded);
                } else {
                    old = loaded;
                    append_buff(scanner_buff, 0);
                    data.i = (int) strtol(get_buff(scanner_buff), NULL, 2);
                    return create_token(INT, data, &line);
                }
                break;
                /*****************************************************************************************************/

            case s_octa_load:
                if (loaded >= '0' && loaded <= '7') {
                    append_buff(scanner_buff, (char) loaded);
                } else {
                    old = loaded;
                    append_buff(scanner_buff, 0);
                    data.i = (int) strtol(get_buff(scanner_buff), NULL, 8);
                    return create_token(INT, data, &line);
                }
                break;
                /*****************************************************************************************************/

            case s_hexa_load:
                if (isdigit(loaded) || (loaded >= 'a' && loaded <= 'f') || (loaded >= 'A' && loaded <= 'F')) {
                    append_buff(scanner_buff, (char) loaded);
                } else {
                    old = loaded;
                    append_buff(scanner_buff, 0);
                    data.i = (int) strtol(get_buff(scanner_buff), NULL, 16);
                    return create_token(INT, data, &line);
                }
                break;
                /*****************************************************************************************************/
            default:
                my_free(scanner_buff->ret);
                my_free(scanner_buff);
                fprintf(stderr, "ERROR -- lexikalna analyza skoncila zle\n");
                exit(ERR_INTER);

        }
    } while (loaded != EOF);
    //sem by sa nikdy nemal dostat ak ano niekde je chyba
    my_free(scanner_buff->ret);
    my_free(scanner_buff);
    fprintf(stderr, "ERROR -- lexikalna analyza skoncila zle\n");
    exit(ERR_INTER);
    return NULL;
}

void load_all_token() {

    t_token *input = NULL;
    do {
        input = load_token();
        scanner_append(input);
    } while (input->token_type != EMPTY);

}
