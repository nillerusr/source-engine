/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

%{

#define YYPARSER
#include "gmConfig.h"
#include "gmCodeTree.h"
#define YYSTYPE gmCodeTreeNode *

extern gmCodeTreeNode * g_codeTree;

#define GM_BISON_DEBUG
#ifdef GM_BISON_DEBUG
#define YYDEBUG 1
#define YYERROR_VERBOSE
#endif // GM_BISON_DEBUG

//
// HELPERS
//

void ATTACH(gmCodeTreeNode * &a_res, gmCodeTreeNode * a_a, gmCodeTreeNode * a_b)
{
  YYSTYPE t = a_a;
  if(t != NULL)
  {
    while(t->m_sibling != NULL)
    {
      t = t->m_sibling;
    }
    t->m_sibling = a_b;
    if(a_b) { a_b->m_parent = t; }
    a_res = a_a;
  }
  else
  {
    a_res = a_b;
  }
}

gmCodeTreeNode * CreateOperation(int a_subTypeType, gmCodeTreeNode * a_left = NULL, gmCodeTreeNode * a_right = NULL)
{
  gmCodeTreeNode * node = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_OPERATION, gmlineno, a_subTypeType);
  node->SetChild(0, a_left);
  node->SetChild(1, a_right);
  return node;
}

gmCodeTreeNode * CreateAsignExpression(int a_subTypeType, gmCodeTreeNode * a_left, gmCodeTreeNode * a_right)
{
  // we need to evaluate the complexety of the l-value... if it is a function call, index or dot to the left of a dot or index, we need to cache
  // into a hidden variable.

  // todo

  gmCodeTreeNode * opNode = CreateOperation(a_subTypeType, a_left, a_right);
  return CreateOperation(CTNOT_ASSIGN, a_left, opNode);
}

%}

%token KEYWORD_LOCAL
%token KEYWORD_GLOBAL
%token KEYWORD_MEMBER
%token KEYWORD_AND
%token KEYWORD_OR
%token KEYWORD_IF
%token KEYWORD_ELSE
%token KEYWORD_WHILE
%token KEYWORD_FOR
%token KEYWORD_FOREACH
%token KEYWORD_IN
%token KEYWORD_BREAK
%token KEYWORD_CONTINUE
%token KEYWORD_NULL
%token KEYWORD_DOWHILE
%token KEYWORD_RETURN
%token KEYWORD_FUNCTION
%token KEYWORD_TABLE
%token KEYWORD_THIS
%token KEYWORD_TRUE
%token KEYWORD_FALSE
%token IDENTIFIER
%token CONSTANT_HEX
%token CONSTANT_BINARY
%token CONSTANT_INT
%token CONSTANT_CHAR
%token CONSTANT_FLOAT
%token CONSTANT_FLOAT
%token CONSTANT_STRING
%token SYMBOL_ASGN_BSR
%token SYMBOL_ASGN_BSL
%token SYMBOL_ASGN_ADD
%token SYMBOL_ASGN_MINUS
%token SYMBOL_ASGN_TIMES
%token SYMBOL_ASGN_DIVIDE
%token SYMBOL_ASGN_REM
%token SYMBOL_ASGN_BAND
%token SYMBOL_ASGN_BOR
%token SYMBOL_ASGN_BXOR
%token SYMBOL_RIGHT_SHIFT
%token SYMBOL_LEFT_SHIFT
%token SYMBOL_LTE
%token SYMBOL_GTE
%token SYMBOL_EQ
%token SYMBOL_NEQ
%token TOKEN_ERROR

%start program
%%

program
  : statement_list
    {
      g_codeTree = $1;
    }
  ;

statement_list
  : statement
    {
      $$ = $1;
    }
  | statement_list statement
    {
      ATTACH($$, $1, $2);
    }
  ;

statement
  : expression_statement
    {
      $$ = $1;
    }
  | var_statement
    {
      $$ = $1;
    }
  | selection_statement
    {
      $$ = $1;
    }
  | iteration_statement
    {
      $$ = $1;
    }
  | jump_statement
    {
      $$ = $1;
    }
  ;

compound_statement
  : '{' '}'
    {
      $$ = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_COMPOUND, gmlineno);
    }
  | '{' statement_list '}'
    {
      $$ = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_COMPOUND, gmlineno);
      $$->SetChild(0, $2);
    }
  ;

var_statement
  : var_type identifier ';'
    {
      $$ = gmCodeTreeNode::Create(CTNT_DECLARATION, CTNDT_VARIABLE, gmlineno, (int) $1);
      $$->SetChild(0, $2);
    }
  | var_type identifier '=' constant_expression ';'
    {
      $$ = gmCodeTreeNode::Create(CTNT_DECLARATION, CTNDT_VARIABLE, gmlineno, (int) $1);
      $$->SetChild(0, $2);
      ATTACH($$, $$, CreateOperation(CTNOT_ASSIGN, $2, $4));
    }
  ;

var_type
  : KEYWORD_LOCAL
    {
      $$ = (YYSTYPE) CTVT_LOCAL;
    }
  | KEYWORD_GLOBAL
    {
      $$ = (YYSTYPE) CTVT_GLOBAL;
    }
  | KEYWORD_MEMBER
    {
      $$ = (YYSTYPE) CTVT_MEMBER;
    }
  ;
expression_statement
  : ';' 
    {
      $$ = NULL;
    }
  | assignment_expression ';'
    {
      $$ = $1;
    }
  ;

selection_statement
  : KEYWORD_IF '(' constant_expression ')' compound_statement
    {
      $$ = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_IF, ($3) ? $3->m_lineNumber : gmlineno);
      $$->SetChild(0, $3);
      $$->SetChild(1, $5);
    }
  | KEYWORD_IF '(' constant_expression ')' compound_statement KEYWORD_ELSE compound_statement
    {
      $$ = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_IF, ($3) ? $3->m_lineNumber : gmlineno);
      $$->SetChild(0, $3);
      $$->SetChild(1, $5);
      $$->SetChild(2, $7);
    }
  | KEYWORD_IF '(' constant_expression ')' compound_statement KEYWORD_ELSE selection_statement
    {
      $$ = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_IF, ($3) ? $3->m_lineNumber : gmlineno);
      $$->SetChild(0, $3);
      $$->SetChild(1, $5);
      $$->SetChild(2, $7);
    }
  ;

iteration_statement
  : KEYWORD_WHILE '(' constant_expression ')' compound_statement
    {
      $$ = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_WHILE, ($3) ? $3->m_lineNumber : gmlineno);
      $$->SetChild(0, $3);
      $$->SetChild(1, $5);
    }
  | KEYWORD_DOWHILE '(' constant_expression ')' compound_statement
    {
      $$ = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_DOWHILE, ($3) ? $3->m_lineNumber : gmlineno);
      $$->SetChild(0, $3);
      $$->SetChild(1, $5);
    }
  | KEYWORD_FOR '(' expression_statement constant_expression_statement ')' compound_statement
    {
      $$ = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_FOR, ($3) ? $3->m_lineNumber : gmlineno);
      $$->SetChild(0, $3);
      $$->SetChild(1, $4);
      $$->SetChild(3, $6);
    }
  | KEYWORD_FOR '(' expression_statement constant_expression_statement assignment_expression ')' compound_statement
    {
      $$ = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_FOR, ($3) ? $3->m_lineNumber : gmlineno);
      $$->SetChild(0, $3);
      $$->SetChild(1, $4);
      $$->SetChild(2, $5);
      $$->SetChild(3, $7);
    }
  | KEYWORD_FOREACH '(' identifier KEYWORD_IN constant_expression ')' compound_statement
    {
      $$ = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_FOREACH, ($5) ? $5->m_lineNumber : gmlineno);
      $$->SetChild(0, $5);
      $$->SetChild(1, $3);
      $$->SetChild(3, $7);
    }
  | KEYWORD_FOREACH '(' identifier KEYWORD_AND identifier KEYWORD_IN constant_expression')' compound_statement
    {
      $$ = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_FOREACH, ($7) ? $7->m_lineNumber : gmlineno);
      $$->SetChild(0, $7);
      $$->SetChild(1, $5);
      $$->SetChild(2, $3);
      $$->SetChild(3, $9);
    }
  ;

jump_statement
  : KEYWORD_CONTINUE ';'
    {
      $$ = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_CONTINUE, gmlineno);
    }
  | KEYWORD_BREAK ';'
    {
      $$ = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_BREAK, gmlineno);
    }
  | KEYWORD_RETURN ';'
    {
      $$ = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_RETURN, gmlineno);
    }
  | KEYWORD_RETURN constant_expression ';'
    {
      $$ = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_RETURN, gmlineno);
      $$->SetChild(0, $2);
    }
  ;

assignment_expression
  : logical_or_expression
    {
      $$ = $1;
      if($$)
      {
        $$->m_flags |= gmCodeTreeNode::CTN_POP;
      }
    }
  | postfix_expression '=' logical_or_expression
    {
      $$ = CreateOperation(CTNOT_ASSIGN, $1, $3);
    }
  | postfix_expression SYMBOL_ASGN_BSR logical_or_expression
    {
      $$ = CreateAsignExpression(CTNOT_SHIFT_RIGHT, $1, $3);
    }
  | postfix_expression SYMBOL_ASGN_BSL logical_or_expression
    {
      $$ = CreateAsignExpression(CTNOT_SHIFT_LEFT, $1, $3);
    }
  | postfix_expression SYMBOL_ASGN_ADD logical_or_expression
    {
      $$ = CreateAsignExpression(CTNOT_ADD, $1, $3);
    }
  | postfix_expression SYMBOL_ASGN_MINUS logical_or_expression
    {
      $$ = CreateAsignExpression(CTNOT_MINUS, $1, $3);
    }
  | postfix_expression SYMBOL_ASGN_TIMES logical_or_expression
    {
      $$ = CreateAsignExpression(CTNOT_TIMES, $1, $3);
    }
  | postfix_expression SYMBOL_ASGN_DIVIDE logical_or_expression
    {
      $$ = CreateAsignExpression(CTNOT_DIVIDE, $1, $3);
    }
  | postfix_expression SYMBOL_ASGN_REM logical_or_expression
    {
      $$ = CreateAsignExpression(CTNOT_REM, $1, $3);
    }
  | postfix_expression SYMBOL_ASGN_BAND logical_or_expression
    {
      $$ = CreateAsignExpression(CTNOT_BIT_AND, $1, $3);
    }
  | postfix_expression SYMBOL_ASGN_BOR logical_or_expression
    {
      $$ = CreateAsignExpression(CTNOT_BIT_OR, $1, $3);
    }
  | postfix_expression SYMBOL_ASGN_BXOR logical_or_expression
    {
      $$ = CreateAsignExpression(CTNOT_BIT_XOR, $1, $3);
    }
  ;


constant_expression_statement
  : ';' 
    {
      $$ = NULL;
    }
  | constant_expression ';'
    {
      $$ = $1;
    }
  ;

constant_expression
  : logical_or_expression
    {
      $$ = $1;
    }
  ;

logical_or_expression
  : logical_and_expression
    {
      $$ = $1;
    }
  | logical_or_expression KEYWORD_OR logical_and_expression
    {
      $$ = CreateOperation(CTNOT_OR, $1, $3);
    }
  ;

logical_and_expression
  : inclusive_or_expression
    {
      $$ = $1;
    }
  | logical_and_expression KEYWORD_AND inclusive_or_expression
    {
      $$ = CreateOperation(CTNOT_AND, $1, $3);
    }
  ;

inclusive_or_expression
  : exclusive_or_expression
    {
      $$ = $1;
    }
  | inclusive_or_expression '|' exclusive_or_expression
    {
      $$ = CreateOperation(CTNOT_BIT_OR, $1, $3);
      $$->ConstantFold();
    }
  ;

exclusive_or_expression
  : and_expression
    {
      $$ = $1;
    }
  | exclusive_or_expression '^' and_expression
    {
      $$ = CreateOperation(CTNOT_BIT_XOR, $1, $3);
      $$->ConstantFold();
    }
  ;

and_expression
  : equality_expression
    {
      $$ = $1;
    }
  | and_expression '&' equality_expression
    {
      $$ = CreateOperation(CTNOT_BIT_AND, $1, $3);
      $$->ConstantFold();
    }
  ;

equality_expression
  : relational_expression
    {
      $$ = $1;
    }
  | equality_expression SYMBOL_EQ relational_expression
    {
      $$ = CreateOperation(CTNOT_EQ, $1, $3);
    }
  | equality_expression SYMBOL_NEQ relational_expression
    {
      $$ = CreateOperation(CTNOT_NEQ, $1, $3);
    }
  ;

relational_expression
  : shift_expression
    {
      $$ = $1;
    }
  | relational_expression '<' shift_expression
    {
      $$ = CreateOperation(CTNOT_LT, $1, $3);
    }
  | relational_expression '>' shift_expression
    {
      $$ = CreateOperation(CTNOT_GT, $1, $3);
    }
  | relational_expression SYMBOL_LTE shift_expression
    {
      $$ = CreateOperation(CTNOT_LTE, $1, $3);
    }
  | relational_expression SYMBOL_GTE shift_expression
    {
      $$ = CreateOperation(CTNOT_GTE, $1, $3);
    }
  ;

shift_expression
  : additive_expression
    {
      $$ = $1;
    }
  | shift_expression SYMBOL_LEFT_SHIFT additive_expression
    {
      $$ = CreateOperation(CTNOT_SHIFT_LEFT, $1, $3);
      $$->ConstantFold();
    }
  | shift_expression SYMBOL_RIGHT_SHIFT additive_expression
    {
      $$ = CreateOperation(CTNOT_SHIFT_RIGHT, $1, $3);
      $$->ConstantFold();
    }
  ;

additive_expression
  : multiplicative_expression
    {
      $$ = $1;
    }
  | additive_expression '+' multiplicative_expression
    {
      $$ = CreateOperation(CTNOT_ADD, $1, $3);
      $$->ConstantFold();
    }
  | additive_expression '-' multiplicative_expression
    {
      $$ = CreateOperation(CTNOT_MINUS, $1, $3);
      $$->ConstantFold();
    }
  ;

multiplicative_expression
  : unary_expression
    {
      $$ = $1;
    }
  | multiplicative_expression '*' unary_expression
    {
      $$ = CreateOperation(CTNOT_TIMES, $1, $3);
      $$->ConstantFold();
    }
  | multiplicative_expression '/' unary_expression
    {
      $$ = CreateOperation(CTNOT_DIVIDE, $1, $3);
      $$->ConstantFold();
    }
  | multiplicative_expression '%' unary_expression
    {
      $$ = CreateOperation(CTNOT_REM, $1, $3);
      $$->ConstantFold();
    }
  ;

unary_expression
  : postfix_expression
    {
      $$ = $1;
    }
  | unary_operator unary_expression
    {
      $$ = $1;
      $$->SetChild(0, $2);
      $$->ConstantFold();
    }
  ;

unary_operator
  : '+'
    {
      $$ = CreateOperation(CTNOT_UNARY_PLUS);
    }
  | '-'
    {
      $$ = CreateOperation(CTNOT_UNARY_MINUS);
    }
	| '~'
    {
      $$ = CreateOperation(CTNOT_UNARY_COMPLEMENT);
    }
  | '!'
    {
      $$ = CreateOperation(CTNOT_UNARY_NOT);
    }
  ;

postfix_expression
  : primary_expression
    {
      $$ = $1;
    }
  | postfix_expression '[' constant_expression ']'
    {
      $$ = CreateOperation(CTNOT_ARRAY_INDEX, $1, $3);
    }
  | postfix_expression '(' ')'
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CALL, gmlineno);
      $$->SetChild(0, $1);
    }
  | postfix_expression '(' argument_expression_list ')'
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CALL, gmlineno);
      $$->SetChild(0, $1);
      $$->SetChild(1, $3);
    }
  | postfix_expression ':' identifier '(' ')'
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CALL, gmlineno);
      $$->SetChild(0, $3);
      $$->SetChild(2, $1);
    }
  | postfix_expression ':' identifier '(' argument_expression_list ')'
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CALL, gmlineno);
      $$->SetChild(0, $3);
      $$->SetChild(1, $5);
      $$->SetChild(2, $1);
    }
  | postfix_expression '.' identifier
    {
      $$ = CreateOperation(CTNOT_DOT, $1, $3);
    }
  ;

argument_expression_list
  : constant_expression
    {
      $$ = $1;
    }
  | argument_expression_list ',' constant_expression
    {
      ATTACH($$, $1, $3);
    }
  ;

table_constructor
  : KEYWORD_TABLE '(' ')'
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_TABLE, gmlineno);
    }
  | KEYWORD_TABLE '(' field_list ')'
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_TABLE, gmlineno);
      $$->SetChild(0, $3);
    }
  | '{' '}'
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_TABLE, gmlineno);
    }
  | '{' field_list '}'
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_TABLE, gmlineno);
      $$->SetChild(0, $2);
    }
  | '{' field_list ',' '}' 
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_TABLE, gmlineno);
      $$->SetChild(0, $2);
    }
  ;

function_constructor
  : KEYWORD_FUNCTION '(' parameter_list ')' compound_statement
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_FUNCTION, gmlineno);
      $$->SetChild(0, $3);
      $$->SetChild(1, $5);
    }
  | KEYWORD_FUNCTION '(' ')' compound_statement
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_FUNCTION, gmlineno);
      $$->SetChild(1, $4);
    }
  ;

field_list
  : field
    {
      $$ = $1;
    }
  | field_list ',' field
    {
      ATTACH($$, $1, $3);
    }
  ;

field
  : constant_expression
    {
      $$ = $1;
    }
  | identifier '=' constant_expression
    {
      $$ = CreateOperation(CTNOT_ASSIGN_FIELD, $1, $3);
    }
  ;

parameter_list
  : parameter
    {
      $$ = $1;
    }
  | parameter_list ',' parameter
    {
      ATTACH($$, $1, $3);
    }
  ;

parameter
  : identifier
    {
      $$ = gmCodeTreeNode::Create(CTNT_DECLARATION, CTNDT_PARAMETER, gmlineno);
      $$->SetChild(0, $1);
    }
  | identifier '=' constant_expression
    {
      $$ = gmCodeTreeNode::Create(CTNT_DECLARATION, CTNDT_PARAMETER, gmlineno);
      $$->SetChild(0, $1);
      $$->SetChild(1, $3);
    }
  ;

primary_expression
  : identifier
    {
      $$ = $1;
    }
  | '.' identifier
    {
      $$ = $2;
      $$->m_flags |= gmCodeTreeNode::CTN_MEMBER;
    }
  | KEYWORD_THIS
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_THIS, gmlineno);
    }
  | constant  
    {
      $$ = $1;
    }
  | table_constructor
    {
      $$ = $1;
    }
  | function_constructor
    {
      $$ = $1;
    }
  | '(' constant_expression ')'
    {
      $$ = $2;
    }
  ;

identifier
  : IDENTIFIER
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_IDENTIFIER, gmlineno);
      $$->m_data.m_string = (char *) gmCodeTree::Get().Alloc(strlen(gmtext) + 1);
      strcpy($$->m_data.m_string, gmtext);
    }
  ;

constant
  : CONSTANT_HEX
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_INT);
      $$->m_data.m_iValue = strtoul(gmtext + 2, NULL, 16);
    }
  | CONSTANT_BINARY
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_INT);
      $$->m_data.m_iValue = strtoul(gmtext + 2, NULL, 2);
    }
  | CONSTANT_INT
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_INT);
      $$->m_data.m_iValue = atoi(gmtext);
    }
  | KEYWORD_TRUE
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_INT);
      $$->m_data.m_iValue = 1;
    }
  | KEYWORD_FALSE
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_INT);
      $$->m_data.m_iValue = 0;
    }
  | CONSTANT_CHAR
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_INT);

      char * c = (char *) gmCodeTree::Get().Alloc(strlen(gmtext) + 1);
      strcpy(c, gmtext);
      int result = 0;
      int shr = 0;

      while(*c)
      {
        if(c[0] == '\'')
        {
          ++c;
          continue;
        }
        else if(c[0] == '\\')
        {
          if(shr) result <<= 8;
          switch(c[1])
          {
            case 'a' : result |= (unsigned char) '\a'; break;
            case 'b' : result |= (unsigned char) '\b'; break;
            case 'f' : result |= (unsigned char) '\f'; break;
            case 'n' : result |= (unsigned char) '\n'; break;
            case 'r' : result |= (unsigned char) '\r'; break;
            case 't' : result |= (unsigned char) '\t'; break;
            case 'v' : result |= (unsigned char) '\v'; break;
            case '\'' : result |= (unsigned char) '\''; break;
            case '\"' : result |= (unsigned char) '\"'; break;
            case '\\' : result |= (unsigned char) '\\'; break;
            default: result |= (unsigned char) c[1];
          }
          ++shr;
          c += 2;
          continue;
        }
        if(shr) result <<= 8;
        result |= (unsigned char) *(c++);
        ++shr;
      }

      if(shr > 4 && gmCodeTree::Get().GetLog()) gmCodeTree::Get().GetLog()->LogEntry("truncated char, line %d", gmlineno);

      $$->m_data.m_iValue = result;
    }
  | CONSTANT_FLOAT
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_FLOAT);
      $$->m_data.m_fValue = (float) atof(gmtext);
    }
  | constant_string_list
    {
      $$ = $1;
    }
  | KEYWORD_NULL
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_NULL);
      $$->m_data.m_iValue = 0;
    }
  ;

constant_string_list
  : CONSTANT_STRING
    {
      $$ = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_STRING);
      $$->m_data.m_string = (char *) gmCodeTree::Get().Alloc(strlen(gmtext) + 1);
      strcpy($$->m_data.m_string, gmtext);
      if(gmtext[0] == '"')
      {
        gmProcessDoubleQuoteString($$->m_data.m_string);
      }
      else if(gmtext[0] == '`')
      {
        gmProcessSingleQuoteString($$->m_data.m_string);
      }
    }
  | constant_string_list CONSTANT_STRING
    {
      $$ = $1;
      int alen = strlen($$->m_data.m_string);
      int blen = strlen(gmtext);
      char * str = (char *) gmCodeTree::Get().Alloc(alen + blen + 1);
      if(str)
      {
        memcpy(str, $1->m_data.m_string, alen);
        memcpy(str + alen, gmtext, blen);
        str[alen + blen] = '\0';
        if(str[alen] == '"')
        {
          gmProcessDoubleQuoteString(str + alen);
        }
        else if(str[alen] == '`')
        {
          gmProcessSingleQuoteString(str + alen);
        }
        $$->m_data.m_string = str;
      }
    }
  ;

%%

#include <stdio.h>











