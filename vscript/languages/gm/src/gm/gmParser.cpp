
/*  A Bison parser, made from gmparser.y with Bison version GNU Bison version 1.24
  */

#define YYBISON 1  /* Identify Bison output.  */

#define yyparse gmparse
#define yylex gmlex
#define yyerror gmerror
#define yylval gmlval
#define yychar gmchar
#define yydebug gmdebug
#define yynerrs gmnerrs
#define	KEYWORD_LOCAL	258
#define	KEYWORD_GLOBAL	259
#define	KEYWORD_MEMBER	260
#define	KEYWORD_AND	261
#define	KEYWORD_OR	262
#define	KEYWORD_IF	263
#define	KEYWORD_ELSE	264
#define	KEYWORD_WHILE	265
#define	KEYWORD_FOR	266
#define	KEYWORD_FOREACH	267
#define	KEYWORD_IN	268
#define	KEYWORD_BREAK	269
#define	KEYWORD_CONTINUE	270
#define	KEYWORD_NULL	271
#define	KEYWORD_DOWHILE	272
#define	KEYWORD_RETURN	273
#define	KEYWORD_FUNCTION	274
#define	KEYWORD_TABLE	275
#define	KEYWORD_THIS	276
#define	KEYWORD_TRUE	277
#define	KEYWORD_FALSE	278
#define	IDENTIFIER	279
#define	CONSTANT_HEX	280
#define	CONSTANT_BINARY	281
#define	CONSTANT_INT	282
#define	CONSTANT_CHAR	283
#define	CONSTANT_FLOAT	284
#define	CONSTANT_STRING	285
#define	SYMBOL_ASGN_BSR	286
#define	SYMBOL_ASGN_BSL	287
#define	SYMBOL_ASGN_ADD	288
#define	SYMBOL_ASGN_MINUS	289
#define	SYMBOL_ASGN_TIMES	290
#define	SYMBOL_ASGN_DIVIDE	291
#define	SYMBOL_ASGN_REM	292
#define	SYMBOL_ASGN_BAND	293
#define	SYMBOL_ASGN_BOR	294
#define	SYMBOL_ASGN_BXOR	295
#define	SYMBOL_RIGHT_SHIFT	296
#define	SYMBOL_LEFT_SHIFT	297
#define	SYMBOL_LTE	298
#define	SYMBOL_GTE	299
#define	SYMBOL_EQ	300
#define	SYMBOL_NEQ	301
#define	TOKEN_ERROR	302



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

// Must be last header
#include "memdbgon.h"


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


#ifndef YYLTYPE
typedef
  struct yyltype
    {
      int timestamp;
      int first_line;
      int first_column;
      int last_line;
      int last_column;
      char *text;
   }
  yyltype;

#define YYLTYPE yyltype
#endif

#ifndef YYSTYPE
#define YYSTYPE int
#endif
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		228
#define	YYFLAG		-32768
#define	YYNTBASE	71

#define YYTRANSLATE(x) ((unsigned)(x) <= 302 ? yytranslate[x] : 108)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    65,     2,     2,     2,    63,    56,     2,    52,
    53,    61,    59,    70,    60,    69,    62,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    68,    50,    57,
    51,    58,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    66,     2,    67,    55,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    48,    54,    49,    64,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     2,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
    36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
    46,    47
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     2,     4,     7,     9,    11,    13,    15,    17,    20,
    24,    28,    34,    36,    38,    40,    42,    45,    51,    59,
    67,    73,    79,    86,    94,   102,   112,   115,   118,   121,
   125,   127,   131,   135,   139,   143,   147,   151,   155,   159,
   163,   167,   171,   173,   176,   178,   180,   184,   186,   190,
   192,   196,   198,   202,   204,   208,   210,   214,   218,   220,
   224,   228,   232,   236,   238,   242,   246,   248,   252,   256,
   258,   262,   266,   270,   272,   275,   277,   279,   281,   283,
   285,   290,   294,   299,   305,   312,   316,   318,   322,   326,
   331,   334,   338,   343,   349,   354,   356,   360,   362,   366,
   368,   372,   374,   378,   380,   383,   385,   387,   389,   391,
   395,   397,   399,   401,   403,   405,   407,   409,   411,   413,
   415,   417
};

static const short yyrhs[] = {    72,
     0,    73,     0,    72,    73,     0,    77,     0,    75,     0,
    78,     0,    79,     0,    80,     0,    48,    49,     0,    48,
    72,    49,     0,    76,   105,    50,     0,    76,   105,    51,
    83,    50,     0,     3,     0,     4,     0,     5,     0,    50,
     0,    81,    50,     0,     8,    52,    83,    53,    74,     0,
     8,    52,    83,    53,    74,     9,    74,     0,     8,    52,
    83,    53,    74,     9,    78,     0,    10,    52,    83,    53,
    74,     0,    17,    52,    83,    53,    74,     0,    11,    52,
    77,    82,    53,    74,     0,    11,    52,    77,    82,    81,
    53,    74,     0,    12,    52,   105,    13,    83,    53,    74,
     0,    12,    52,   105,     6,   105,    13,    83,    53,    74,
     0,    15,    50,     0,    14,    50,     0,    18,    50,     0,
    18,    83,    50,     0,    84,     0,    96,    51,    84,     0,
    96,    31,    84,     0,    96,    32,    84,     0,    96,    33,
    84,     0,    96,    34,    84,     0,    96,    35,    84,     0,
    96,    36,    84,     0,    96,    37,    84,     0,    96,    38,
    84,     0,    96,    39,    84,     0,    96,    40,    84,     0,
    50,     0,    83,    50,     0,    84,     0,    85,     0,    84,
     7,    85,     0,    86,     0,    85,     6,    86,     0,    87,
     0,    86,    54,    87,     0,    88,     0,    87,    55,    88,
     0,    89,     0,    88,    56,    89,     0,    90,     0,    89,
    45,    90,     0,    89,    46,    90,     0,    91,     0,    90,
    57,    91,     0,    90,    58,    91,     0,    90,    43,    91,
     0,    90,    44,    91,     0,    92,     0,    91,    42,    92,
     0,    91,    41,    92,     0,    93,     0,    92,    59,    93,
     0,    92,    60,    93,     0,    94,     0,    93,    61,    94,
     0,    93,    62,    94,     0,    93,    63,    94,     0,    96,
     0,    95,    94,     0,    59,     0,    60,     0,    64,     0,
    65,     0,   104,     0,    96,    66,    83,    67,     0,    96,
    52,    53,     0,    96,    52,    97,    53,     0,    96,    68,
   105,    52,    53,     0,    96,    68,   105,    52,    97,    53,
     0,    96,    69,   105,     0,    83,     0,    97,    70,    83,
     0,    20,    52,    53,     0,    20,    52,   100,    53,     0,
    48,    49,     0,    48,   100,    49,     0,    48,   100,    70,
    49,     0,    19,    52,   102,    53,    74,     0,    19,    52,
    53,    74,     0,   101,     0,   100,    70,   101,     0,    83,
     0,   105,    51,    83,     0,   103,     0,   102,    70,   103,
     0,   105,     0,   105,    51,    83,     0,   105,     0,    69,
   105,     0,    21,     0,   106,     0,    98,     0,    99,     0,
    52,    83,    53,     0,    24,     0,    25,     0,    26,     0,
    27,     0,    22,     0,    23,     0,    28,     0,    29,     0,
   107,     0,    16,     0,    30,     0,   107,    30,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   122,   129,   133,   140,   144,   148,   152,   156,   163,   167,
   175,   180,   189,   193,   197,   203,   207,   214,   220,   227,
   237,   243,   249,   256,   264,   271,   282,   286,   290,   294,
   302,   310,   314,   318,   322,   326,   330,   334,   338,   342,
   346,   350,   358,   362,   369,   376,   380,   387,   391,   398,
   402,   410,   414,   422,   426,   434,   438,   442,   449,   453,
   457,   461,   465,   472,   476,   481,   489,   493,   498,   506,
   510,   515,   520,   528,   532,   541,   545,   549,   553,   560,
   564,   568,   573,   579,   585,   592,   599,   603,   610,   614,
   619,   623,   628,   636,   642,   650,   654,   661,   665,   672,
   676,   683,   688,   697,   701,   706,   710,   714,   718,   722,
   729,   738,   743,   748,   753,   758,   763,   809,   814,   818,
   826,   840
};

static const char * const yytname[] = {   "$","error","$undefined.","KEYWORD_LOCAL",
"KEYWORD_GLOBAL","KEYWORD_MEMBER","KEYWORD_AND","KEYWORD_OR","KEYWORD_IF","KEYWORD_ELSE",
"KEYWORD_WHILE","KEYWORD_FOR","KEYWORD_FOREACH","KEYWORD_IN","KEYWORD_BREAK",
"KEYWORD_CONTINUE","KEYWORD_NULL","KEYWORD_DOWHILE","KEYWORD_RETURN","KEYWORD_FUNCTION",
"KEYWORD_TABLE","KEYWORD_THIS","KEYWORD_TRUE","KEYWORD_FALSE","IDENTIFIER","CONSTANT_HEX",
"CONSTANT_BINARY","CONSTANT_INT","CONSTANT_CHAR","CONSTANT_FLOAT","CONSTANT_STRING",
"SYMBOL_ASGN_BSR","SYMBOL_ASGN_BSL","SYMBOL_ASGN_ADD","SYMBOL_ASGN_MINUS","SYMBOL_ASGN_TIMES",
"SYMBOL_ASGN_DIVIDE","SYMBOL_ASGN_REM","SYMBOL_ASGN_BAND","SYMBOL_ASGN_BOR",
"SYMBOL_ASGN_BXOR","SYMBOL_RIGHT_SHIFT","SYMBOL_LEFT_SHIFT","SYMBOL_LTE","SYMBOL_GTE",
"SYMBOL_EQ","SYMBOL_NEQ","TOKEN_ERROR","'{'","'}'","';'","'='","'('","')'","'|'",
"'^'","'&'","'<'","'>'","'+'","'-'","'*'","'/'","'%'","'~'","'!'","'['","']'",
"':'","'.'","','","program","statement_list","statement","compound_statement",
"var_statement","var_type","expression_statement","selection_statement","iteration_statement",
"jump_statement","assignment_expression","constant_expression_statement","constant_expression",
"logical_or_expression","logical_and_expression","inclusive_or_expression","exclusive_or_expression",
"and_expression","equality_expression","relational_expression","shift_expression",
"additive_expression","multiplicative_expression","unary_expression","unary_operator",
"postfix_expression","argument_expression_list","table_constructor","function_constructor",
"field_list","field","parameter_list","parameter","primary_expression","identifier",
"constant","constant_string_list",""
};
#endif

static const short yyr1[] = {     0,
    71,    72,    72,    73,    73,    73,    73,    73,    74,    74,
    75,    75,    76,    76,    76,    77,    77,    78,    78,    78,
    79,    79,    79,    79,    79,    79,    80,    80,    80,    80,
    81,    81,    81,    81,    81,    81,    81,    81,    81,    81,
    81,    81,    82,    82,    83,    84,    84,    85,    85,    86,
    86,    87,    87,    88,    88,    89,    89,    89,    90,    90,
    90,    90,    90,    91,    91,    91,    92,    92,    92,    93,
    93,    93,    93,    94,    94,    95,    95,    95,    95,    96,
    96,    96,    96,    96,    96,    96,    97,    97,    98,    98,
    98,    98,    98,    99,    99,   100,   100,   101,   101,   102,
   102,   103,   103,   104,   104,   104,   104,   104,   104,   104,
   105,   106,   106,   106,   106,   106,   106,   106,   106,   106,
   107,   107
};

static const short yyr2[] = {     0,
     1,     1,     2,     1,     1,     1,     1,     1,     2,     3,
     3,     5,     1,     1,     1,     1,     2,     5,     7,     7,
     5,     5,     6,     7,     7,     9,     2,     2,     2,     3,
     1,     3,     3,     3,     3,     3,     3,     3,     3,     3,
     3,     3,     1,     2,     1,     1,     3,     1,     3,     1,
     3,     1,     3,     1,     3,     1,     3,     3,     1,     3,
     3,     3,     3,     1,     3,     3,     1,     3,     3,     1,
     3,     3,     3,     1,     2,     1,     1,     1,     1,     1,
     4,     3,     4,     5,     6,     3,     1,     3,     3,     4,
     2,     3,     4,     5,     4,     1,     3,     1,     3,     1,
     3,     1,     3,     1,     2,     1,     1,     1,     1,     3,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     2
};

static const short yydefact[] = {     0,
    13,    14,    15,     0,     0,     0,     0,     0,     0,   120,
     0,     0,     0,     0,   106,   115,   116,   111,   112,   113,
   114,   117,   118,   121,     0,    16,     0,    76,    77,    78,
    79,     0,     1,     2,     5,     0,     4,     6,     7,     8,
     0,    31,    46,    48,    50,    52,    54,    56,    59,    64,
    67,    70,     0,    74,   108,   109,    80,   104,   107,   119,
     0,     0,     0,     0,    28,    27,     0,    29,     0,    45,
    74,     0,     0,    91,    98,     0,    96,   104,     0,   105,
     3,     0,    17,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,    75,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,   122,     0,     0,
     0,     0,     0,    30,     0,     0,   100,   102,    89,     0,
    92,     0,     0,   110,    11,     0,    47,    49,    51,    53,
    55,    57,    58,    62,    63,    60,    61,    66,    65,    68,
    69,    71,    72,    73,    33,    34,    35,    36,    37,    38,
    39,    40,    41,    42,    32,    82,    87,     0,     0,     0,
    86,     0,     0,    43,     0,     0,     0,     0,     0,     0,
    95,     0,     0,     0,    90,     0,    93,    97,    99,     0,
    83,     0,    81,     0,    18,    21,     0,     0,    44,     0,
     0,    22,     9,     0,    94,   101,   103,    12,    88,    84,
     0,     0,    23,     0,     0,     0,    10,    85,    19,    20,
    24,     0,    25,     0,    26,     0,     0,     0
};

static const short yydefgoto[] = {   226,
    33,    34,   181,    35,    36,    37,    38,    39,    40,    41,
   175,    75,    70,    43,    44,    45,    46,    47,    48,    49,
    50,    51,    52,    53,    71,   168,    55,    56,    76,    77,
   126,   127,    57,    58,    59,    60
};

static const short yypact[] = {   332,
-32768,-32768,-32768,    15,    17,    27,    31,   -10,    -1,-32768,
    34,   383,    35,    36,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,   434,-32768,   683,-32768,-32768,-32768,
-32768,    32,   332,-32768,-32768,    32,-32768,-32768,-32768,-32768,
     8,    82,    84,    37,    38,    39,    -2,    16,     6,     2,
   -51,-32768,   683,   722,-32768,-32768,-32768,-32768,-32768,    62,
   683,   683,   485,    32,-32768,-32768,   683,-32768,    44,    82,
   -27,   -16,   126,-32768,-32768,   -43,-32768,    45,    47,-32768,
-32768,    25,-32768,   683,   683,   683,   683,   683,   683,   683,
   683,   683,   683,   683,   683,   683,   683,   683,   683,   683,
   683,-32768,   683,   683,   683,   683,   683,   683,   683,   683,
   683,   683,   683,   500,   683,    32,    32,-32768,    48,    49,
   551,    11,    63,-32768,    67,   -44,-32768,    46,-32768,   -40,
-32768,   602,   683,-32768,-32768,   683,    84,    37,    38,    39,
    -2,    16,    16,     6,     6,     6,     6,     2,     2,   -51,
   -51,-32768,-32768,-32768,    82,    82,    82,    82,    82,    82,
    82,    82,    82,    82,    82,-32768,-32768,   -38,    50,    68,
-32768,    67,    67,-32768,   617,    71,    32,   683,    67,   206,
-32768,    67,    32,   683,-32768,   683,-32768,-32768,-32768,    72,
-32768,   683,-32768,   668,   114,-32768,    67,    73,-32768,   111,
    74,-32768,-32768,   269,-32768,-32768,-32768,-32768,-32768,-32768,
   -34,    -3,-32768,    67,   683,    67,-32768,-32768,-32768,-32768,
-32768,    75,-32768,    67,-32768,   129,   130,-32768
};

static const short yypgoto[] = {-32768,
   -48,   -31,  -144,-32768,-32768,    70,   -81,-32768,-32768,   -41,
-32768,     4,     0,    51,    53,    55,    52,    56,   -12,   -71,
   -14,   -13,   -49,-32768,     1,   -58,-32768,-32768,    85,  -129,
-32768,   -26,-32768,   -18,-32768,-32768
};


#define	YYLAST		791


static const short yytable[] = {    42,
    54,    81,   188,   102,     4,   131,    78,    18,   182,    99,
   100,   101,   185,    80,   191,    69,   177,    82,   218,   144,
   145,   146,   147,   178,   114,   183,   132,   195,   196,   186,
    79,   192,    42,    54,   202,   192,   125,   205,   115,    65,
   116,   117,    89,    90,   180,   122,    95,    96,    66,   152,
   153,   154,   213,   128,    78,    18,   188,    83,    91,    92,
    97,    98,    42,    54,   119,   120,    61,   219,    62,   221,
   123,   223,    93,    94,   135,   136,   142,   143,    63,   225,
   148,   149,    64,   150,   151,    67,    72,    73,    84,    85,
    86,   118,    87,   124,    88,   133,   184,   170,   171,   134,
   172,   173,   155,   156,   157,   158,   159,   160,   161,   162,
   163,   164,   165,    78,   180,   179,   193,   167,   169,   194,
   199,   208,   212,   215,   176,   214,   216,   224,   227,   228,
   220,   204,   121,   198,   137,   211,   189,   138,   140,   190,
   139,    10,     0,   141,    13,    14,    15,    16,    17,    18,
    19,    20,    21,    22,    23,    24,   206,   130,   200,     0,
     0,     0,     0,     0,   128,     0,     0,    78,     0,     0,
     0,     0,    81,    25,    42,    54,     0,    27,   129,    42,
    54,   201,     0,     0,    28,    29,     0,   207,     0,    30,
    31,     0,     0,     0,    32,   209,     0,   167,     0,     0,
     0,     0,     0,    42,    54,     0,     0,     0,     1,     2,
     3,     0,     0,     4,     0,     5,     6,     7,   222,     8,
     9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
    19,    20,    21,    22,    23,    24,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,    25,   203,    26,     0,    27,     0,     0,
     0,     0,     0,     0,    28,    29,     0,     0,     0,    30,
    31,     1,     2,     3,    32,     0,     4,     0,     5,     6,
     7,     0,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,    24,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,    25,   217,    26,     0,
    27,     0,     0,     0,     0,     0,     0,    28,    29,     0,
     0,     0,    30,    31,     1,     2,     3,    32,     0,     4,
     0,     5,     6,     7,     0,     8,     9,    10,    11,    12,
    13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
    23,    24,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,    25,
     0,    26,     0,    27,     0,     0,     0,     0,     0,     0,
    28,    29,     0,     0,     0,    30,    31,     0,    10,     0,
    32,    13,    14,    15,    16,    17,    18,    19,    20,    21,
    22,    23,    24,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    25,     0,    68,     0,    27,     0,     0,     0,     0,     0,
     0,    28,    29,     0,     0,     0,    30,    31,     0,    10,
     0,    32,    13,    14,    15,    16,    17,    18,    19,    20,
    21,    22,    23,    24,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,    25,    74,     0,     0,    27,     0,     0,     0,     0,
     0,     0,    28,    29,     0,     0,     0,    30,    31,     0,
    10,     0,    32,    13,    14,    15,    16,    17,    18,    19,
    20,    21,    22,    23,    24,    10,     0,     0,    13,    14,
    15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
     0,     0,    25,     0,    26,     0,    27,     0,     0,     0,
     0,     0,     0,    28,    29,     0,     0,    25,    30,    31,
     0,    27,   166,    32,     0,     0,     0,     0,    28,    29,
     0,     0,     0,    30,    31,     0,    10,     0,    32,    13,
    14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
    24,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,    25,     0,
   174,     0,    27,     0,     0,     0,     0,     0,     0,    28,
    29,     0,     0,     0,    30,    31,     0,    10,     0,    32,
    13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
    23,    24,    10,     0,     0,    13,    14,    15,    16,    17,
    18,    19,    20,    21,    22,    23,    24,     0,     0,    25,
   187,     0,     0,    27,     0,     0,     0,     0,     0,     0,
    28,    29,     0,     0,    25,    30,    31,     0,    27,   197,
    32,     0,     0,     0,     0,    28,    29,     0,     0,     0,
    30,    31,     0,    10,     0,    32,    13,    14,    15,    16,
    17,    18,    19,    20,    21,    22,    23,    24,    10,     0,
     0,    13,    14,    15,    16,    17,    18,    19,    20,    21,
    22,    23,    24,     0,     0,    25,     0,     0,     0,    27,
   210,     0,     0,     0,     0,     0,    28,    29,     0,     0,
    25,    30,    31,     0,    27,     0,    32,     0,     0,     0,
     0,    28,    29,     0,     0,     0,    30,    31,     0,     0,
     0,    32,   103,   104,   105,   106,   107,   108,   109,   110,
   111,   112,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,   113,   114,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,   115,     0,   116,
   117
};

static const short yycheck[] = {     0,
     0,    33,   132,    53,     8,    49,    25,    24,    53,    61,
    62,    63,    53,    32,    53,    12,     6,    36,    53,    91,
    92,    93,    94,    13,    52,    70,    70,   172,   173,    70,
    27,    70,    33,    33,   179,    70,    53,   182,    66,    50,
    68,    69,    45,    46,    48,    64,    41,    42,    50,    99,
   100,   101,   197,    72,    73,    24,   186,    50,    43,    44,
    59,    60,    63,    63,    61,    62,    52,   212,    52,   214,
    67,   216,    57,    58,    50,    51,    89,    90,    52,   224,
    95,    96,    52,    97,    98,    52,    52,    52,     7,     6,
    54,    30,    55,    50,    56,    51,    51,   116,   117,    53,
    53,    53,   103,   104,   105,   106,   107,   108,   109,   110,
   111,   112,   113,   132,    48,    53,    67,   114,   115,    52,
    50,    50,     9,    13,   121,    53,    53,    53,     0,     0,
   212,   180,    63,   175,    84,   194,   133,    85,    87,   136,
    86,    16,    -1,    88,    19,    20,    21,    22,    23,    24,
    25,    26,    27,    28,    29,    30,   183,    73,   177,    -1,
    -1,    -1,    -1,    -1,   183,    -1,    -1,   186,    -1,    -1,
    -1,    -1,   204,    48,   175,   175,    -1,    52,    53,   180,
   180,   178,    -1,    -1,    59,    60,    -1,   184,    -1,    64,
    65,    -1,    -1,    -1,    69,   192,    -1,   194,    -1,    -1,
    -1,    -1,    -1,   204,   204,    -1,    -1,    -1,     3,     4,
     5,    -1,    -1,     8,    -1,    10,    11,    12,   215,    14,
    15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
    25,    26,    27,    28,    29,    30,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    48,    49,    50,    -1,    52,    -1,    -1,
    -1,    -1,    -1,    -1,    59,    60,    -1,    -1,    -1,    64,
    65,     3,     4,     5,    69,    -1,     8,    -1,    10,    11,
    12,    -1,    14,    15,    16,    17,    18,    19,    20,    21,
    22,    23,    24,    25,    26,    27,    28,    29,    30,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    48,    49,    50,    -1,
    52,    -1,    -1,    -1,    -1,    -1,    -1,    59,    60,    -1,
    -1,    -1,    64,    65,     3,     4,     5,    69,    -1,     8,
    -1,    10,    11,    12,    -1,    14,    15,    16,    17,    18,
    19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
    29,    30,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
    -1,    50,    -1,    52,    -1,    -1,    -1,    -1,    -1,    -1,
    59,    60,    -1,    -1,    -1,    64,    65,    -1,    16,    -1,
    69,    19,    20,    21,    22,    23,    24,    25,    26,    27,
    28,    29,    30,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    48,    -1,    50,    -1,    52,    -1,    -1,    -1,    -1,    -1,
    -1,    59,    60,    -1,    -1,    -1,    64,    65,    -1,    16,
    -1,    69,    19,    20,    21,    22,    23,    24,    25,    26,
    27,    28,    29,    30,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    48,    49,    -1,    -1,    52,    -1,    -1,    -1,    -1,
    -1,    -1,    59,    60,    -1,    -1,    -1,    64,    65,    -1,
    16,    -1,    69,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    28,    29,    30,    16,    -1,    -1,    19,    20,
    21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
    -1,    -1,    48,    -1,    50,    -1,    52,    -1,    -1,    -1,
    -1,    -1,    -1,    59,    60,    -1,    -1,    48,    64,    65,
    -1,    52,    53,    69,    -1,    -1,    -1,    -1,    59,    60,
    -1,    -1,    -1,    64,    65,    -1,    16,    -1,    69,    19,
    20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
    30,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,    -1,
    50,    -1,    52,    -1,    -1,    -1,    -1,    -1,    -1,    59,
    60,    -1,    -1,    -1,    64,    65,    -1,    16,    -1,    69,
    19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
    29,    30,    16,    -1,    -1,    19,    20,    21,    22,    23,
    24,    25,    26,    27,    28,    29,    30,    -1,    -1,    48,
    49,    -1,    -1,    52,    -1,    -1,    -1,    -1,    -1,    -1,
    59,    60,    -1,    -1,    48,    64,    65,    -1,    52,    53,
    69,    -1,    -1,    -1,    -1,    59,    60,    -1,    -1,    -1,
    64,    65,    -1,    16,    -1,    69,    19,    20,    21,    22,
    23,    24,    25,    26,    27,    28,    29,    30,    16,    -1,
    -1,    19,    20,    21,    22,    23,    24,    25,    26,    27,
    28,    29,    30,    -1,    -1,    48,    -1,    -1,    -1,    52,
    53,    -1,    -1,    -1,    -1,    -1,    59,    60,    -1,    -1,
    48,    64,    65,    -1,    52,    -1,    69,    -1,    -1,    -1,
    -1,    59,    60,    -1,    -1,    -1,    64,    65,    -1,    -1,
    -1,    69,    31,    32,    33,    34,    35,    36,    37,    38,
    39,    40,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    51,    52,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    66,    -1,    68,
    69
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */


/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

#ifndef alloca
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi)
#include <alloca.h>
#else /* not sparc */
#if defined (MSDOS) && !defined (__TURBOC__)
#include <malloc.h>
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
#include <malloc.h>
 #pragma alloca
#else /* not MSDOS, __TURBOC__, or _AIX */
#ifdef __hpux
#ifdef __cplusplus
extern "C" {
void *alloca (unsigned int);
};
#else /* not __cplusplus */
void *alloca ();
#endif /* not __cplusplus */
#endif /* __hpux */
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc.  */
#endif /* not GNU C.  */
#endif /* alloca not defined.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         -2
#define YYEOF           0
#define YYACCEPT        return(0)
#define YYABORT         return(1)
#define YYERROR         goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL          goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do                                                              \
  if (yychar == YYEMPTY && yylen == 1)                          \
    { yychar = (token), yylval = (value);                       \
      yychar1 = YYTRANSLATE (yychar);                           \
      YYPOPSTACK;                                               \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    { yyerror ("syntax error: cannot back up"); YYERROR; }      \
while (0)

#define YYTERROR        1
#define YYERRCODE       256

#ifndef YYPURE
#define YYLEX           yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX           yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX           yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX           yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX           yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int     yychar;                 /*  the lookahead symbol                */
YYSTYPE yylval;                 /*  the semantic value of the           */
                                /*  lookahead symbol                    */

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;                 /*  location data for the lookahead     */
                                /*  symbol                              */
#endif

int yynerrs;                    /*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;                    /*  nonzero means print parse trace     */
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks       */

#ifndef YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
int yyparse (void);
#endif

#if __GNUC__ > 1                /* GNU C and GNU C++ define this.  */
#define __yy_memcpy(FROM,TO,COUNT)      memcpy(TO,FROM,COUNT)
#else                           /* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (from, to, count)
     char *from;
     char *to;
     int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *from, char *to, int count)
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif



/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#else
#define YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#endif

int
yyparse(YYPARSE_PARAM)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;      /*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;              /*  lookahead token as an internal (translated) token number */

  short yyssa[YYINITDEPTH];     /*  the state stack                     */
  YYSTYPE yyvsa[YYINITDEPTH];   /*  the semantic value stack            */

  short *yyss = yyssa;          /*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;        /*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];   /*  the location stack                  */
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;                /*  the variable used to return         */
                                /*  semantic values from the action     */
                                /*  routines                            */

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;             /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = (short) yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
         the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
         but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
                 &yyss1, size * sizeof (*yyssp),
                 &yyvs1, size * sizeof (*yyvsp),
                 &yyls1, size * sizeof (*yylsp),
                 &yystacksize);
#else
      yyoverflow("parser stack overflow",
                 &yyss1, size * sizeof (*yyssp),
                 &yyvs1, size * sizeof (*yyvsp),
                 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
        {
          yyerror("parser stack overflow");
          return 2;
        }
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
        yystacksize = YYMAXDEPTH;
      yyss = (short *) alloca (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss1, (char *)yyss, size * sizeof (*yyssp));
      yyvs = (YYSTYPE *) alloca (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs1, (char *)yyvs, size * sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) alloca (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls1, (char *)yyls, size * sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
        fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
        YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
        fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)              /* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;           /* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
        fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
        {
          fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
          /* Give the individual parser a way to print the precise meaning
             of a token, for further debugging info.  */
#ifdef YYPRINT
          YYPRINT (stderr, yychar, yylval);
#endif
          fprintf (stderr, ")\n");
        }
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
               yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
        fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 1:
{
      g_codeTree = yyvsp[0];
    ;
    break;}
case 2:
{
      yyval = yyvsp[0];
    ;
    break;}
case 3:
{
      ATTACH(yyval, yyvsp[-1], yyvsp[0]);
    ;
    break;}
case 4:
{
      yyval = yyvsp[0];
    ;
    break;}
case 5:
{
      yyval = yyvsp[0];
    ;
    break;}
case 6:
{
      yyval = yyvsp[0];
    ;
    break;}
case 7:
{
      yyval = yyvsp[0];
    ;
    break;}
case 8:
{
      yyval = yyvsp[0];
    ;
    break;}
case 9:
{
      yyval = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_COMPOUND, gmlineno);
    ;
    break;}
case 10:
{
      yyval = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_COMPOUND, gmlineno);
      yyval->SetChild(0, yyvsp[-1]);
    ;
    break;}
case 11:
{
      yyval = gmCodeTreeNode::Create(CTNT_DECLARATION, CTNDT_VARIABLE, gmlineno, (int) yyvsp[-2]);
      yyval->SetChild(0, yyvsp[-1]);
    ;
    break;}
case 12:
{
      yyval = gmCodeTreeNode::Create(CTNT_DECLARATION, CTNDT_VARIABLE, gmlineno, (int) yyvsp[-4]);
      yyval->SetChild(0, yyvsp[-3]);
      ATTACH(yyval, yyval, CreateOperation(CTNOT_ASSIGN, yyvsp[-3], yyvsp[-1]));
    ;
    break;}
case 13:
{
      yyval = (YYSTYPE) CTVT_LOCAL;
    ;
    break;}
case 14:
{
      yyval = (YYSTYPE) CTVT_GLOBAL;
    ;
    break;}
case 15:
{
      yyval = (YYSTYPE) CTVT_MEMBER;
    ;
    break;}
case 16:
{
      yyval = NULL;
    ;
    break;}
case 17:
{
      yyval = yyvsp[-1];
    ;
    break;}
case 18:
{
      yyval = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_IF, (yyvsp[-2]) ? yyvsp[-2]->m_lineNumber : gmlineno);
      yyval->SetChild(0, yyvsp[-2]);
      yyval->SetChild(1, yyvsp[0]);
    ;
    break;}
case 19:
{
      yyval = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_IF, (yyvsp[-4]) ? yyvsp[-4]->m_lineNumber : gmlineno);
      yyval->SetChild(0, yyvsp[-4]);
      yyval->SetChild(1, yyvsp[-2]);
      yyval->SetChild(2, yyvsp[0]);
    ;
    break;}
case 20:
{
      yyval = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_IF, (yyvsp[-4]) ? yyvsp[-4]->m_lineNumber : gmlineno);
      yyval->SetChild(0, yyvsp[-4]);
      yyval->SetChild(1, yyvsp[-2]);
      yyval->SetChild(2, yyvsp[0]);
    ;
    break;}
case 21:
{
      yyval = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_WHILE, (yyvsp[-2]) ? yyvsp[-2]->m_lineNumber : gmlineno);
      yyval->SetChild(0, yyvsp[-2]);
      yyval->SetChild(1, yyvsp[0]);
    ;
    break;}
case 22:
{
      yyval = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_DOWHILE, (yyvsp[-2]) ? yyvsp[-2]->m_lineNumber : gmlineno);
      yyval->SetChild(0, yyvsp[-2]);
      yyval->SetChild(1, yyvsp[0]);
    ;
    break;}
case 23:
{
      yyval = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_FOR, (yyvsp[-3]) ? yyvsp[-3]->m_lineNumber : gmlineno);
      yyval->SetChild(0, yyvsp[-3]);
      yyval->SetChild(1, yyvsp[-2]);
      yyval->SetChild(3, yyvsp[0]);
    ;
    break;}
case 24:
{
      yyval = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_FOR, (yyvsp[-4]) ? yyvsp[-4]->m_lineNumber : gmlineno);
      yyval->SetChild(0, yyvsp[-4]);
      yyval->SetChild(1, yyvsp[-3]);
      yyval->SetChild(2, yyvsp[-2]);
      yyval->SetChild(3, yyvsp[0]);
    ;
    break;}
case 25:
{
      yyval = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_FOREACH, (yyvsp[-2]) ? yyvsp[-2]->m_lineNumber : gmlineno);
      yyval->SetChild(0, yyvsp[-2]);
      yyval->SetChild(1, yyvsp[-4]);
      yyval->SetChild(3, yyvsp[0]);
    ;
    break;}
case 26:
{
      yyval = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_FOREACH, (yyvsp[-2]) ? yyvsp[-2]->m_lineNumber : gmlineno);
      yyval->SetChild(0, yyvsp[-2]);
      yyval->SetChild(1, yyvsp[-4]);
      yyval->SetChild(2, yyvsp[-6]);
      yyval->SetChild(3, yyvsp[0]);
    ;
    break;}
case 27:
{
      yyval = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_CONTINUE, gmlineno);
    ;
    break;}
case 28:
{
      yyval = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_BREAK, gmlineno);
    ;
    break;}
case 29:
{
      yyval = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_RETURN, gmlineno);
    ;
    break;}
case 30:
{
      yyval = gmCodeTreeNode::Create(CTNT_STATEMENT, CTNST_RETURN, gmlineno);
      yyval->SetChild(0, yyvsp[-1]);
    ;
    break;}
case 31:
{
      yyval = yyvsp[0];
      if(yyval)
      {
        yyval->m_flags |= gmCodeTreeNode::CTN_POP;
      }
    ;
    break;}
case 32:
{
      yyval = CreateOperation(CTNOT_ASSIGN, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 33:
{
      yyval = CreateAsignExpression(CTNOT_SHIFT_RIGHT, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 34:
{
      yyval = CreateAsignExpression(CTNOT_SHIFT_LEFT, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 35:
{
      yyval = CreateAsignExpression(CTNOT_ADD, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 36:
{
      yyval = CreateAsignExpression(CTNOT_MINUS, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 37:
{
      yyval = CreateAsignExpression(CTNOT_TIMES, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 38:
{
      yyval = CreateAsignExpression(CTNOT_DIVIDE, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 39:
{
      yyval = CreateAsignExpression(CTNOT_REM, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 40:
{
      yyval = CreateAsignExpression(CTNOT_BIT_AND, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 41:
{
      yyval = CreateAsignExpression(CTNOT_BIT_OR, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 42:
{
      yyval = CreateAsignExpression(CTNOT_BIT_XOR, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 43:
{
      yyval = NULL;
    ;
    break;}
case 44:
{
      yyval = yyvsp[-1];
    ;
    break;}
case 45:
{
      yyval = yyvsp[0];
    ;
    break;}
case 46:
{
      yyval = yyvsp[0];
    ;
    break;}
case 47:
{
      yyval = CreateOperation(CTNOT_OR, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 48:
{
      yyval = yyvsp[0];
    ;
    break;}
case 49:
{
      yyval = CreateOperation(CTNOT_AND, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 50:
{
      yyval = yyvsp[0];
    ;
    break;}
case 51:
{
      yyval = CreateOperation(CTNOT_BIT_OR, yyvsp[-2], yyvsp[0]);
      yyval->ConstantFold();
    ;
    break;}
case 52:
{
      yyval = yyvsp[0];
    ;
    break;}
case 53:
{
      yyval = CreateOperation(CTNOT_BIT_XOR, yyvsp[-2], yyvsp[0]);
      yyval->ConstantFold();
    ;
    break;}
case 54:
{
      yyval = yyvsp[0];
    ;
    break;}
case 55:
{
      yyval = CreateOperation(CTNOT_BIT_AND, yyvsp[-2], yyvsp[0]);
      yyval->ConstantFold();
    ;
    break;}
case 56:
{
      yyval = yyvsp[0];
    ;
    break;}
case 57:
{
      yyval = CreateOperation(CTNOT_EQ, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 58:
{
      yyval = CreateOperation(CTNOT_NEQ, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 59:
{
      yyval = yyvsp[0];
    ;
    break;}
case 60:
{
      yyval = CreateOperation(CTNOT_LT, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 61:
{
      yyval = CreateOperation(CTNOT_GT, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 62:
{
      yyval = CreateOperation(CTNOT_LTE, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 63:
{
      yyval = CreateOperation(CTNOT_GTE, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 64:
{
      yyval = yyvsp[0];
    ;
    break;}
case 65:
{
      yyval = CreateOperation(CTNOT_SHIFT_LEFT, yyvsp[-2], yyvsp[0]);
      yyval->ConstantFold();
    ;
    break;}
case 66:
{
      yyval = CreateOperation(CTNOT_SHIFT_RIGHT, yyvsp[-2], yyvsp[0]);
      yyval->ConstantFold();
    ;
    break;}
case 67:
{
      yyval = yyvsp[0];
    ;
    break;}
case 68:
{
      yyval = CreateOperation(CTNOT_ADD, yyvsp[-2], yyvsp[0]);
      yyval->ConstantFold();
    ;
    break;}
case 69:
{
      yyval = CreateOperation(CTNOT_MINUS, yyvsp[-2], yyvsp[0]);
      yyval->ConstantFold();
    ;
    break;}
case 70:
{
      yyval = yyvsp[0];
    ;
    break;}
case 71:
{
      yyval = CreateOperation(CTNOT_TIMES, yyvsp[-2], yyvsp[0]);
      yyval->ConstantFold();
    ;
    break;}
case 72:
{
      yyval = CreateOperation(CTNOT_DIVIDE, yyvsp[-2], yyvsp[0]);
      yyval->ConstantFold();
    ;
    break;}
case 73:
{
      yyval = CreateOperation(CTNOT_REM, yyvsp[-2], yyvsp[0]);
      yyval->ConstantFold();
    ;
    break;}
case 74:
{
      yyval = yyvsp[0];
    ;
    break;}
case 75:
{
      yyval = yyvsp[-1];
      yyval->SetChild(0, yyvsp[0]);
      yyval->ConstantFold();
    ;
    break;}
case 76:
{
      yyval = CreateOperation(CTNOT_UNARY_PLUS);
    ;
    break;}
case 77:
{
      yyval = CreateOperation(CTNOT_UNARY_MINUS);
    ;
    break;}
case 78:
{
      yyval = CreateOperation(CTNOT_UNARY_COMPLEMENT);
    ;
    break;}
case 79:
{
      yyval = CreateOperation(CTNOT_UNARY_NOT);
    ;
    break;}
case 80:
{
      yyval = yyvsp[0];
    ;
    break;}
case 81:
{
      yyval = CreateOperation(CTNOT_ARRAY_INDEX, yyvsp[-3], yyvsp[-1]);
    ;
    break;}
case 82:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CALL, gmlineno);
      yyval->SetChild(0, yyvsp[-2]);
    ;
    break;}
case 83:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CALL, gmlineno);
      yyval->SetChild(0, yyvsp[-3]);
      yyval->SetChild(1, yyvsp[-1]);
    ;
    break;}
case 84:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CALL, gmlineno);
      yyval->SetChild(0, yyvsp[-2]);
      yyval->SetChild(2, yyvsp[-4]);
    ;
    break;}
case 85:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CALL, gmlineno);
      yyval->SetChild(0, yyvsp[-3]);
      yyval->SetChild(1, yyvsp[-1]);
      yyval->SetChild(2, yyvsp[-5]);
    ;
    break;}
case 86:
{
      yyval = CreateOperation(CTNOT_DOT, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 87:
{
      yyval = yyvsp[0];
    ;
    break;}
case 88:
{
      ATTACH(yyval, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 89:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_TABLE, gmlineno);
    ;
    break;}
case 90:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_TABLE, gmlineno);
      yyval->SetChild(0, yyvsp[-1]);
    ;
    break;}
case 91:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_TABLE, gmlineno);
    ;
    break;}
case 92:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_TABLE, gmlineno);
      yyval->SetChild(0, yyvsp[-1]);
    ;
    break;}
case 93:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_TABLE, gmlineno);
      yyval->SetChild(0, yyvsp[-2]);
    ;
    break;}
case 94:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_FUNCTION, gmlineno);
      yyval->SetChild(0, yyvsp[-2]);
      yyval->SetChild(1, yyvsp[0]);
    ;
    break;}
case 95:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_FUNCTION, gmlineno);
      yyval->SetChild(1, yyvsp[0]);
    ;
    break;}
case 96:
{
      yyval = yyvsp[0];
    ;
    break;}
case 97:
{
      ATTACH(yyval, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 98:
{
      yyval = yyvsp[0];
    ;
    break;}
case 99:
{
      yyval = CreateOperation(CTNOT_ASSIGN_FIELD, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 100:
{
      yyval = yyvsp[0];
    ;
    break;}
case 101:
{
      ATTACH(yyval, yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 102:
{
      yyval = gmCodeTreeNode::Create(CTNT_DECLARATION, CTNDT_PARAMETER, gmlineno);
      yyval->SetChild(0, yyvsp[0]);
    ;
    break;}
case 103:
{
      yyval = gmCodeTreeNode::Create(CTNT_DECLARATION, CTNDT_PARAMETER, gmlineno);
      yyval->SetChild(0, yyvsp[-2]);
      yyval->SetChild(1, yyvsp[0]);
    ;
    break;}
case 104:
{
      yyval = yyvsp[0];
    ;
    break;}
case 105:
{
      yyval = yyvsp[0];
      yyval->m_flags |= gmCodeTreeNode::CTN_MEMBER;
    ;
    break;}
case 106:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_THIS, gmlineno);
    ;
    break;}
case 107:
{
      yyval = yyvsp[0];
    ;
    break;}
case 108:
{
      yyval = yyvsp[0];
    ;
    break;}
case 109:
{
      yyval = yyvsp[0];
    ;
    break;}
case 110:
{
      yyval = yyvsp[-1];
    ;
    break;}
case 111:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_IDENTIFIER, gmlineno);
      yyval->m_data.m_string = (char *) gmCodeTree::Get().Alloc(strlen(gmtext) + 1);
      strcpy(yyval->m_data.m_string, gmtext);
    ;
    break;}
case 112:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_INT);
      yyval->m_data.m_iValue = strtoul(gmtext + 2, NULL, 16);
    ;
    break;}
case 113:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_INT);
      yyval->m_data.m_iValue = strtoul(gmtext + 2, NULL, 2);
    ;
    break;}
case 114:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_INT);
      yyval->m_data.m_iValue = atoi(gmtext);
    ;
    break;}
case 115:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_INT);
      yyval->m_data.m_iValue = 1;
    ;
    break;}
case 116:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_INT);
      yyval->m_data.m_iValue = 0;
    ;
    break;}
case 117:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_INT);

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

      yyval->m_data.m_iValue = result;
    ;
    break;}
case 118:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_FLOAT);
      yyval->m_data.m_fValue = (float) atof(gmtext);
    ;
    break;}
case 119:
{
      yyval = yyvsp[0];
    ;
    break;}
case 120:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_NULL);
      yyval->m_data.m_iValue = 0;
    ;
    break;}
case 121:
{
      yyval = gmCodeTreeNode::Create(CTNT_EXPRESSION, CTNET_CONSTANT, gmlineno, CTNCT_STRING);
      yyval->m_data.m_string = (char *) gmCodeTree::Get().Alloc(strlen(gmtext) + 1);
      strcpy(yyval->m_data.m_string, gmtext);
      if(gmtext[0] == '"')
      {
        gmProcessDoubleQuoteString(yyval->m_data.m_string);
      }
      else if(gmtext[0] == '`')
      {
        gmProcessSingleQuoteString(yyval->m_data.m_string);
      }
    ;
    break;}
case 122:
{
      yyval = yyvsp[-1];
      int alen = strlen(yyval->m_data.m_string);
      int blen = strlen(gmtext);
      char * str = (char *) gmCodeTree::Get().Alloc(alen + blen + 1);
      if(str)
      {
        memcpy(str, yyvsp[-1]->m_data.m_string, alen);
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
        yyval->m_data.m_string = str;
      }
    ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */


  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
        fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
        {
          int size = 0;
          char *msg;
          int x, count;

          count = 0;
          /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
          for (x = (yyn < 0 ? -yyn : 0);
               x < (int)(sizeof(yytname) / sizeof(char *)); x++) //_GD_
            if (yycheck[x + yyn] == x)
              size += strlen(yytname[x]) + 15, count++;
          //_GD_ msg = (char *) malloc(size + 15);
          msg = GM_NEW( char [size + 15] );
          if (msg != 0)
            {
              strcpy(msg, "parse error");

              if (count < 5)
                {
                  count = 0;
                  for (x = (yyn < 0 ? -yyn : 0);
                       x < (sizeof(yytname) / sizeof(char *)); x++)
                    if (yycheck[x + yyn] == x)
                      {
                        strcat(msg, count == 0 ? ", expecting `" : " or `");
                        strcat(msg, yytname[x]);
                        strcat(msg, "'");
                        count++;
                      }
                }
              yyerror(msg);
              //_GD_ free(msg);
              delete [] msg;
            }
          else
            yyerror ("parse error; also virtual memory exceeded");
        }
      else
#endif /* YYERROR_VERBOSE */
        yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
        YYABORT;

#if YYDEBUG != 0
      if (yydebug)
        fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;              /* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
        fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
        goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;
}


#include <stdio.h>











