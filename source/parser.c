#include "parser.h"
#include "main.h"

#include "expression.h"
#include "symtable.h"
#include "memwork.h"
#include "scanner.h"
#include <stdlib.h>
#include <stdbool.h>
#include "codegen.h"

TTable *tTable;

char *gen_label(char *ret){
    static int label = 0;
    char *result = my_malloc(sizeof(char)*130);
    if (ret == NULL){
        sprintf(result,"l_%i", label++);
    } else {
        sprintf(result,"l_%s_%i",ret, label++);
    }
    result[129] = 0;

    return result;
}

t_token *check_next_token_type(int type) {
    t_token *input = get_token();
    check_pointer(input);

    if (input->token_type != type) {
        error(ERR_SYNTA);
    }
    return input;
}

bool check_token_int_value(t_token *input, int value) {
    check_pointer(input);

    if (input->data.i != value) {
        return false;
    }
    return true;
}

bool check_pointer(void *input) {
    if (input == NULL) {
        error(ERR_SYNTA);
    }
    return true;
}

void error(int code) {
    fprintf(stderr, "Error v syntakticke analyze - spatny typ tokenu.\n");
    exit(code);
}

void semerror(int code) {
    fprintf(stderr, "Error v semanticke analyze.\n");
    clear_all();
    exit(code);
}

int parse(TTable *Table) {
    tTable = Table;
    t_token *input = get_token();
    check_pointer(input);
    while (input->token_type == EOL) {
        input = get_token();
        check_pointer(input);
    }

    //prvni token musi byt Declare nebo Function nebo SCOPE
    bool isCorrect = input->token_type == KEY_WORD;
    tdata value = input->data;
    if (isCorrect) {
        switch (value.i) {
            case k_declare: //declare
                input = check_next_token_type(KEY_WORD);
                check_token_int_value(input, k_function);
                function(k_declare, Table, NULL);

                return parse(Table);
            case k_function: { //function
                TTable *local = Tbl_Create(8);
                function(k_function, Table, local); //parametry, konci tokenem EOL

                int correct = commandsAndVariables(local);
                printf("popframe\n");
                printf("return\n");
                if (correct == k_function) { //function
                    return parse(Table);
                } else {
                    error(ERR_SYNTA);
                }
            }
            case k_scope: //scope
                if (scope() == SUCCESS) {
                    return SUCCESS;
                }
            default:
                error(ERR_SYNTA);
        }
    } else {
        error(ERR_SYNTA);
    }
    return SUCCESS;
}

int function(int decDef, TTable *Table, TTable *local) {
    t_token *input;
    input = check_next_token_type(ID);
    TElement *getElement = Tbl_GetDirect(Table, input->data.s);
    TFunction *func = NULL;
    TSymbol *sym = NULL;
    TData *f = NULL;
    if (getElement == NULL) {



        func = my_malloc(sizeof(TFunction));        //TODO prerob na func_create
        sym = my_malloc(sizeof(TSymbol));
        sym->data = my_malloc(sizeof(TData));
        sym->isDeclared = false;
        sym->isDefined = false;
        func->attr_count = 0;
        func->attributes = my_malloc(sizeof(TType));

        func->lok_table = local;

        if (decDef == k_declare) {
            sym->isDeclared = true;
        } else if (decDef == k_function) {
            sym->isDefined = true;
            create_3ac("LABEL", NULL, NULL, input->data.s);  //vytvorenie operacii
            create_3ac("DEFVAR", NULL, NULL, "%RETVAL"); //deklarovanie operandu
            create_3ac("PUSHFRAME", NULL, NULL, NULL);  //vytvorenie operacii
        }
        sym->name = input->data.s;
        sym->type = ST_Function;
        sym->data->func = func;
    } else {
        if (getElement->data->isDefined == true && decDef == k_function) {
            semerror(ERR_SEM_P);
        } else if (getElement->data->isDeclared == true && decDef == k_declare) {
            semerror(ERR_SEM_P);
        } else if (getElement->data->isDefined == true && decDef == k_declare) {
            semerror(ERR_SEM_P);
        }
        func = my_malloc(sizeof(TFunction));
        // func = getElement->data->data->func;
        func->attr_count = 0;
        func->attributes = my_malloc(sizeof(TType));
        sym = getElement->data;

    }
    check_next_token_type(LPAR);
    input = get_token();
    check_pointer(input);
    char *name;
    if (input->token_type == ID) {
        if (decDef == k_function) {
            name = input->data.s;
            create_3ac("DEFVAR", NULL, NULL, name); //deklarovanie operandu
            create_3ac("POPS", NULL, NULL, name);  //vytvorenie operacii
        }
        params(func, local,
               name, decDef); //vraci success, mozna kontrola? ale ono je to celkem jedno, protoze to pri chybe stejne spadne driv
    } else if (input->token_type == RPAR) {}
    else {
        error(ERR_SYNTA);
    }

    input = check_next_token_type(KEY_WORD);
    check_token_int_value(input, k_as); //as

    input = check_next_token_type(KEY_WORD);
    if (check_token_int_value(input, k_integer) || check_token_int_value(input, k_double) ||
        check_token_int_value(input, k_string)) //return_type je datovy typ
    {
        func->return_type = input->data.i;
        check_next_token_type(EOL);
        if (getElement != NULL) {
            if (func->attr_count == sym->data->func->attr_count
                && *(func->attributes) == *(sym->data->func->attributes)
                && func->return_type == sym->data->func->return_type) {
                sym->isDefined = true;
            } else {
                semerror(ERR_SEM_P);
            }
        } else {
            Tbl_Insert(Table, El_Create(sym));
        }
        create_3ac("PUSHFRAME", NULL, NULL, NULL);  //vytvorenie operacii
        create_3ac("RETURN", NULL, NULL, NULL);  //vytvorenie operacii
        return SUCCESS;
    } else {
        error(ERR_SYNTA);
    }


}


int params(TFunction *functions, TTable *local, char *name, int decDef) {
    t_token *input = check_next_token_type(KEY_WORD);
    if (check_token_int_value(input, k_as)) { //as
        functions->attr_count++;
        functions->attributes = my_realloc(functions->attributes, sizeof(TType) * functions->attr_count);
        input = check_next_token_type(KEY_WORD);

        if (check_token_int_value(input, k_integer) || check_token_int_value(input, k_double) ||
            check_token_int_value(input, k_string)) //return_type je datovy typ
        {
            functions->attributes[functions->attr_count - 1] = input->data.i;
            if (name != NULL) {
                TElement *lElement = Tbl_GetDirect(local, name);

                /*create symbol*/
                TData *var;
                TValue value;
                switch (input->data.i) {
                    case k_integer:
                        value.i = 0;
                        break;
                    case k_double:
                        value.d = 0.0;
                        break;
                    case k_string:
                        value.s = "";
                }
                var = Var_Create(value, input->token_type);
                TSymbol *lSymbol = Sym_Create(ST_Variable, var, name);
                lSymbol->isDeclared = true;     //neni v konstruktore

                if (lElement != NULL) {
                    semerror(ERR_SEM_P);
                }

                TElement *element = El_Create(lSymbol);
                Tbl_Insert(local, element);
            }

            input = get_token();
            check_pointer(input);

            if (input->token_type == COMMA) {
                input = check_next_token_type(ID);
                name = input->data.s;
                if (decDef) {
                    create_3ac("DEFVAR", NULL, NULL, name); //deklarovanie operandu
                    create_3ac("POPS", NULL, NULL, name);  //vytvorenie operacii
                }
                return params(functions, local, name, decDef);
            } else if (input->token_type == RPAR) {
                return SUCCESS;
            }
            error(ERR_SYNTA);
        }
        error(ERR_SYNTA);
    }
    error(ERR_SYNTA);
}

int commandsAndVariables(TTable *local) {
    t_token *input = get_token();
    check_pointer(input);

    while (input->token_type == EOL) {
        input = get_token();
    }

    tdata value = input->data;
    bool isCorrect = input->token_type == KEY_WORD;

    if (input->token_type == ID) { //todo co ak je id funckia a ma sa volat funkcia bez parametru
        char *name;
        name = input->data.s;

        t_token *loaded = get_token();

        if (loaded->token_type == EQ){
            expression(local);
        } else if (loaded->token_type==LPAR){
            //todo load function and call
        } else {
            semerror(ERR_SEM_P);
        }


        return commandsAndVariables(local);
    }

    t_token *tmp1, *tmp2, *tmp3;
    tmp1 = NULL;
    tmp2 = NULL;
    tmp3 = NULL;

    if (isCorrect) {
        switch (value.i) {
            case k_dim: //dim
                input = check_next_token_type(ID);
                tmp1 = input;
                char *name = input->data.s;
                printf("DEFVAR %s\n",name);
                input = check_next_token_type(KEY_WORD);
                if (check_token_int_value(input, 0)) { //AS
                    input = check_next_token_type(KEY_WORD);
                    int type = input->data.i;
                    if (check_token_int_value(input, k_integer) || check_token_int_value(input, k_double) ||
                        check_token_int_value(input, k_string)) { //typ
                        //check_next_token_type(EOL);
                        t_token *tmp = get_token();
                        int imp_i = 0;
                        double imp_d = 0.0;
                        char *imp_s = "";

                        if (tmp ->token_type == EOL){
                        } else if (tmp->token_type == EQ){
                            tmp = get_token();
                            if (tmp->token_type == STR && input->data.i == k_string){
                                imp_s = tmp->data.s;
                            } else if (tmp->token_type == DOUBLE && input->data.i == k_double){
                                imp_d = tmp->data.d;
                            } else if (tmp->token_type == INT && input->data.i == k_integer){
                                imp_i = tmp->data.i;
                            } else if (tmp->token_type == DOUBLE && input->data.i == k_integer){
                                imp_i = (int)tmp->data.d;
                            } else if (tmp->token_type == INT && input->data.i == k_double){
                                imp_d = (int)tmp->data.d;
                            } else {
                                semerror(ERR_SEM_P);//nevym o aku chyby sa jedna
                            }
                        } else {
                            semerror(ERR_SYNTA);
                        }

                        TElement *lElement = Tbl_GetDirect(local, name);

                        TData *var;
                        TValue value;
                        switch (input->data.i) {
                            case k_integer:
                                value.i = imp_i;
                                printf("move %s int@%i\n", name, imp_i);
                                break;
                            case k_double:
                                value.d = imp_d;
                                printf("move %s float@%f\n", name, imp_d);
                                break;
                            case k_string:
                                value.s = imp_s;
                                printf("move %s string@%s\n", name, imp_s);

                        }
                        var = Var_Create(value, input->token_type);
                        TSymbol *lSymbol = Sym_Create(ST_Variable, var, name);
                        lSymbol->isDeclared = true;

                        if (lElement != NULL) {
                            semerror(ERR_SEM_P);
                        }

                        Tbl_Insert(local, El_Create(lSymbol));

                        return commandsAndVariables(local);
                    }
                }
                error(ERR_SYNTA);
            case k_input: //input
                check_next_token_type(ID);
                check_next_token_type(EOL);

                return commandsAndVariables(local);
            case k_print: //print
                print_params(); //skonci vcetne EOL
                return commandsAndVariables(local);
            case k_if: //if
                if (expression(local) != k_then) {
                    error(ERR_SYNTA);
                }
                check_next_token_type(EOL);
                int correct = commandsAndVariables(local);
                if (correct == k_if) {//skoncilo to end if
                    return commandsAndVariables(local);
                }
                error(ERR_SYNTA);
            case k_else: //else
                check_next_token_type(EOL);
                return commandsAndVariables(local);
            case k_end: //end
                input = check_next_token_type(KEY_WORD);
                if (check_token_int_value(input, k_if)) { //if
                    check_next_token_type(EOL);
                    return k_if;
                } else if (check_token_int_value(input, k_scope)) { //Scope
                    return k_scope;
                } else if (check_token_int_value(input, k_function)) { //Function
                    check_next_token_type(EOL);
                    return k_function;
                }
                error(ERR_SYNTA);
            case k_do: //do
                input = check_next_token_type(KEY_WORD);
                if (check_token_int_value(input, k_while)) { //while
                    if (expression(local) != EOL) {
                        error(ERR_SYNTA);
                    } //tohle asi nepojede
                    if (commandsAndVariables(local) == k_loop) { //skoncilo to loop
                        return commandsAndVariables(local);
                    }
                    error(ERR_SYNTA);
                }
                error(ERR_SYNTA);
            case k_loop: //loop
                check_next_token_type(EOL);
                return k_loop; //tady to ma asi vracet k_loop
            case k_return: //return
                expression(local);
                return commandsAndVariables(local);
            default:
                error(ERR_SYNTA);
        }
    }
}

int print_params() {
    int result = expression(tTable);
    while (result == SEMICOLLON) {
        result = expression(tTable);
    }
    if (result == EOL) {
        return SUCCESS;
    }
    error(ERR_SYNTA);
}

/*parametry funkce, mozna to k necemu bude, mozna ne, zatim se to nidke nepouziva*/
/*int idList(){
    t_token * input = get_token();
    check_pointer(input);

    if(input->token_type == ID){
        input = get_token();
        check_pointer(input);

        if(input->token_type == COMMA){
            return idList();
        }
        else if(input->token_type == RPAR){
            check_next_token_type(EOL);
            return SUCCESS;
        }
        error(ERR_SYNTA);
    }
    else if(input->token_type == RPAR){
        check_next_token_type(EOL);
        return SUCCESS;
    }
    error(ERR_SYNTA);
}*/

int scope() {
    TTable *local = Tbl_Create(8);
    check_next_token_type(EOL);
    int res = commandsAndVariables(local);
    if (res == k_scope) {
        t_token *input = get_token();
        check_pointer(input);
        while (input->token_type == EOL) {
            input = get_token();
            check_pointer(input);
        }
        if (input->token_type == EMPTY) {
            return SUCCESS;
        }
    }
    error(ERR_SYNTA);
}
