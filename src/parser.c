#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <ctype.h>
#include "symbol.h"
#include "buffer.h"
#include "parser.h"
#include "ast.h"
#include "utils.h"
#include "stack.h"
#include "lexer.h"

#define DEBUG true

extern symbol_t **pglobal_table;
extern ast_t **past;

int parse_var_type (buffer_t *buffer)
{
  buf_skipblank(buffer);
  char *lexem = lexer_getalphanum(buffer);
  if (strcmp(lexem, "entier") == 0) {
    return AST_INTEGER;
  }
  printf("Expected a valid type. exiting.\n");
  exit(1);
}

/**
 * 
 * (entier a, entier b, entier c) => une liste d'ast_t contenue dans un ast_list_t
 */
ast_list_t *parse_parameters (buffer_t *buffer)
{
  printf("start parse parameters\n");
  ast_list_t *list = NULL;
  buf_skipblank(buffer);
  lexer_assert_openbrace(buffer, "Expected a '(' after function name. exiting.\n");
  
  for (;;) {
    int type = parse_var_type(buffer);

    buf_skipblank(buffer);
    char *var_name = lexer_getalphanum(buffer);
    buf_skipblank(buffer);
    
    ast_list_add(&list, ast_new_variable(var_name, type));

    char next = buf_getchar(buffer);
    if (next == ')') break;
    if (next != ',') {
      printf("Expected either a ',' or a ')'. exiting.\n");
      exit(1);
    }
  }
  return list;
}

int parse_return_type (buffer_t *buffer)
{
  buf_skipblank(buffer);
  lexer_assert_twopoints(buffer, "Expected ':' after function parameters");
  buf_skipblank(buffer);
  char *lexem = lexer_getalphanum(buffer);
  if (strcmp(lexem, "entier") == 0) {
    return AST_INTEGER;
  }
  else if (strcmp(lexem, "rien") == 0) {
    return AST_VOID;
  }
  printf("Expected a valid type for a parameter. exiting.\n");
  exit(1);
}

bool parse_is_type (char *lexem)
{
  if (strcmp(lexem, "entier") != 0) { // si le mot-clé n'est pas "entier", on retourne faux
    return false;
  }
  return true;
}
ast_t *convert_to_binary_or_integer_ast_t(char *c){
  if(*c >= '0' && *c <= '9') {
    long clong ; 
    sscanf(c, "%ld", &clong);
    return ast_new_integer(clong);
  }
  else if(*c == '+') return ast_new_binary(AST_BIN_PLUS, NULL, NULL);
  else if(*c == '-') return ast_new_binary(AST_BIN_MINUS, NULL, NULL);
  else if(*c == '*') return ast_new_binary(AST_BIN_MULT, NULL, NULL);
  else return NULL;
}
ast_t *internal_init_convert_tree(mystack_t *queue){
  if (stack_isempty(*queue)) return NULL;
  //print_stack((*queue));printf(" : la queue que j'ai passé en param\n");
  ast_t *curr = stack_pop(queue);
  printf("**** avaant ******\n");
  ast_print(curr);
  if(curr->type == AST_BINARY ) {
    if(!curr->binary.right) curr->binary.right = internal_init_convert_tree(queue);
    if(!curr->binary.left) curr->binary.left = internal_init_convert_tree(queue);
  }
  printf("**** apres ******\n");
  ast_print(curr);
  return curr;
}
ast_t *init_convert_tree(mystack_t *chaine){
  return internal_init_convert_tree(chaine);
}
/**
 * Dans mon algo, pour reconnaitre une variable,
 * on part du principe que le nom de la variable ne comporte qu'une lettre.
 * Cela doit fonctionner une fois l'ajout des symbol dans la table finalisé
 */
ast_t *create_ast (char *items, size_t size){
  if (size == 0) return NULL;
  size_t i = 0;
  ast_t *last = NULL;
  mystack_t stack = NULL;
  mystack_t ordered = NULL;
  mystack_t *pstack = &stack,
          *pordered = &ordered;
  stack_push(pstack, NULL);
  while (stack_top(stack) != NULL || i < size) {
    symbol_t *sym = sym_search(*pglobal_table, &items[i]);
    ast_t *curr = NULL;
    if(sym == NULL) {
      curr = convert_to_binary_or_integer_ast_t(&items[i]);
    }else{
      curr = sym->attributes;
    }
    if ( ast_binary_priority( stack_top(stack) ) < ast_binary_priority(curr)){
      stack_push(pstack, curr);
      i++;
    } else {
      do {
        last = stack_pop(&stack);
        stack_push(pordered, last);
      } while (stack_isempty(stack)!=0 && ast_binary_priority(stack_top(stack)) > ast_binary_priority(last) );
    }
  }
  return init_convert_tree(pordered);
}

/**
 * a = 2;
 */
ast_t *parse_expression (buffer_t *buffer)
{
  printf("start parse expression\n");
  buf_skipblank(buffer);
  char items[100];
  size_t i=0;
  char next = buf_getchar(buffer);
  do {
    items[i]=next;
    buf_skipblank(buffer);
    next = buf_getchar(buffer);
    i++;
  } while (next != ';');
  printf("chaine a traiter : %s de taille %zu\n", items, i);
  return create_ast(items, i);
}

ast_t *parse_assignment (buffer_t *buffer)
{
  printf("start parse affectation\n");
  buf_skipblank(buffer);
  char *var_name = lexer_getalphanum(buffer);
  if (var_name == NULL) {
    printf("Expected a variable name. exiting.\n");
    exit(1);
  }
  symbol_t *curr = sym_search(*pglobal_table, var_name);
  if(curr == NULL) {
    printf("variable doesn't exist. exiting.\n");
    exit(1);
  }
  ast_t *var = curr->attributes ;

  buf_skipblank(buffer);
  char next = buf_getchar(buffer);
  if (next == '=') {
    ast_t *expression = parse_expression(buffer);
    sym_remove (pglobal_table, curr);
    return ast_new_declaration(var, expression);
  }
  printf("Expected either a ';' or a '='. exiting.\n");
  buf_print(buffer);
  exit(1);
}
/**
 * entier a;
 * entier a = 2;
 */
ast_t *parse_declaration (buffer_t *buffer)
{
  printf("start parse declaration\n");
  int type = parse_var_type(buffer);
  buf_skipblank(buffer);
  char *var_name = lexer_getalphanum(buffer);
  if (var_name == NULL) {
    printf("Expected a variable name. exiting.\n");
    exit(1);
  }

  ast_t *var = ast_new_variable(var_name, type);
  buf_skipblank(buffer);
  char next = buf_getchar(buffer);
  if (next == ';') {
    return ast_new_declaration(var, NULL);
  }
  else if (next == '=') {
    ast_t *expression = parse_expression(buffer);
    return ast_new_declaration(var, expression);
  }
  printf("Expected either a ';' or a '='. exiting.\n");
  buf_print(buffer);
  exit(1);
}

ast_t *parse_statement (buffer_t *buffer)
{
  buf_skipblank(buffer);
  char *lexem = lexer_getalphanum_rollback(buffer);
  if (parse_is_type(lexem)) {
    // ceci est une déclaration de variable
    return parse_declaration(buffer);
  }else{
    // ceci est une affectation de variable
    return parse_assignment(buffer);
  }
  // TODO:
  return NULL;
}

ast_list_t *parse_function_body (buffer_t *buffer)
{
  printf("start parse body\n");
  ast_list_t *stmts = NULL;
  buf_skipblank(buffer);
  lexer_assert_openbracket(buffer, "Function body should start with a '{'");
  char next;
  do {
    ast_t *statement = parse_statement(buffer);
    ast_list_add(&stmts, statement);
    buf_skipblank(buffer);
    next = buf_getchar_rollback(buffer);
  } while (next != '}');

  return stmts;
}

/**
 * exercice: cf slides: https://docs.google.com/presentation/d/1AgCeW0vBiNX23ALqHuSaxAneKvsteKdgaqWnyjlHTTA/edit#slide=id.g86e19090a1_0_527
 */
ast_t *parse_function (buffer_t *buffer)
{
  buf_skipblank(buffer);
  char *lexem = lexer_getalphanum(buffer);
  if (strcmp(lexem, "fonction") != 0) {
    printf("Expected a 'fonction' keyword on global scope.exiting.\n");
    buf_print(buffer);
    exit(1);
  }
  buf_skipblank(buffer);
  // TODO
  char *name = lexer_getalphanum(buffer);
  
  ast_list_t *params = parse_parameters(buffer);
  int return_type = parse_return_type(buffer);
  ast_list_t *stmts = parse_function_body(buffer);

  return ast_new_function(name, return_type, params, stmts);
}

/**
 * This function generates ASTs for each global-scope function
 */
ast_list_t *parse (buffer_t *buffer)
{
  ast_t *function = parse_function(buffer);
  ast_print(function);

  if (DEBUG) printf("** end of file. **\n");
  return NULL;
}
