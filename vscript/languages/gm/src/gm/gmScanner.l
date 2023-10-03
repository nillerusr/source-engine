/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

%option yylineno

DIGIT     [0-9]
LETTER    [a-zA-Z_]
HEX       [a-fA-F0-9]
BINARY    [0-1]
SCI		    [Ee][+-]?{DIGIT}+
FL		    (f|F)

%{

#include <stdio.h>
#include "gmConfig.h"
#include "gmParser.cpp.h"

%}

%%

"/*"    {
            int c;

            while((c = yyinput()) != 0)
                {
                if(c == '*')
                    {
                    if((c = yyinput()) == '/')
                        break;
                    else
                        unput(c);
                    }
                }
            }

"//"[^\n]*                        { /* eat up comments */       }
                                                                
"local"                           { return(KEYWORD_LOCAL);      }
"global"                          { return(KEYWORD_GLOBAL);     }
"member"                          { return(KEYWORD_MEMBER);     }
"and"                             { return(KEYWORD_AND);        }
"or"                              { return(KEYWORD_OR);         }
"if"                              { return(KEYWORD_IF);         }
"else"                            { return(KEYWORD_ELSE);       }
"while"                           { return(KEYWORD_WHILE);      }
"for"                             { return(KEYWORD_FOR);        }
"foreach"                         { return(KEYWORD_FOREACH);    }
"in"                              { return(KEYWORD_IN);         }
"dowhile"                         { return(KEYWORD_DOWHILE);    }
"break"                           { return(KEYWORD_BREAK);      }
"continue"                        { return(KEYWORD_CONTINUE);   }
"null"                            { return(KEYWORD_NULL);       }
"return"                          { return(KEYWORD_RETURN);     }
"function"                        { return(KEYWORD_FUNCTION);   }
"table"                           { return(KEYWORD_TABLE);      }
"this"                            { return(KEYWORD_THIS);       }
"true"                            { return(KEYWORD_TRUE);       }
"false"                           { return(KEYWORD_FALSE);      }

{LETTER}({LETTER}|{DIGIT})*       { return(IDENTIFIER);         }
                                                                
0[xX]{HEX}+                       { return(CONSTANT_HEX);       }
0[bB]{BINARY}+                    { return(CONSTANT_BINARY);    }
{DIGIT}+                          { return(CONSTANT_INT);       }
'(\\.|[^\\'])+'                   { return(CONSTANT_CHAR);      }
{DIGIT}+{SCI}{FL}?		            { return(CONSTANT_FLOAT);     }
{DIGIT}*"."{DIGIT}+({SCI})?{FL}?	{ return(CONSTANT_FLOAT);     }
{DIGIT}+"."{DIGIT}*({SCI})?{FL}?	{ return(CONSTANT_FLOAT);     }
\"(\\.|[^\\"])*\"                 { return(CONSTANT_STRING);    }
`([^`]|`{2})*`                    { return(CONSTANT_STRING);    }

"&&"                              { return(KEYWORD_AND);        }
"||"                              { return(KEYWORD_OR);         }
">>="                             { return(SYMBOL_ASGN_BSR);    }
"<<="                             { return(SYMBOL_ASGN_BSL);    }
"+="                              { return(SYMBOL_ASGN_ADD);    }
"-="                              { return(SYMBOL_ASGN_MINUS);  }
"*="                              { return(SYMBOL_ASGN_TIMES);  }
"/="                              { return(SYMBOL_ASGN_DIVIDE); }
"%="                              { return(SYMBOL_ASGN_REM);    }
"&="                              { return(SYMBOL_ASGN_BAND);   }
"|="                              { return(SYMBOL_ASGN_BOR);    }
"^="                              { return(SYMBOL_ASGN_BXOR);   }
">>"                              { return(SYMBOL_RIGHT_SHIFT); }
"<<"                              { return(SYMBOL_LEFT_SHIFT);  }
"<="                              { return(SYMBOL_LTE);         }
">="                              { return(SYMBOL_GTE);         }
"=="                              { return(SYMBOL_EQ);          }
"!="                              { return(SYMBOL_NEQ);         }
";"                               { return(';');                }
"{"                               { return('{');                }
"}"                               { return('}');                }
","                               { return(',');                }
"="                               { return('=');                }
"("                               { return('(');                }
")"                               { return(')');                }
"["                               { return('[');                }
"]"                               { return(']');                }
"."                               { return('.');                }
"!"                               { return('!');                }
"-"                               { return('-');                }
"+"                               { return('+');                }
"*"                               { return('*');                }
"/"                               { return('/');                }
"%"                               { return('%');                }
"<"                               { return('<');                }
">"                               { return('>');                }
"&"                               { return('&');                }
"|"                               { return('|');                }
"^"                               { return('^');                }
"~"                               { return('~');                }
":"                               { return(':');                }
":"                               { return(':');                }
                                  
[ \t\v\r\n\f]                     {                             }
.                                 { return(TOKEN_ERROR);        }

%%

// yywrap
int gmwrap()
{
  return(1);
}



