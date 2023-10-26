#include "StdAfx.h"
#include "gmlangsettings.h"
#include "include/SciLexer.h"


#ifdef _DEBUG
  #define new DEBUG_NEW
  #undef THIS_FILE
  static char THIS_FILE[] = __FILE__;
#endif

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
GMLangSettings::GMLangSettings(void)
{
  // Just using the C++ Lexer.  It is not perfect, but a pretty close match without modification
  lexerId = SCLEX_CPP;

#if 1
  // Black on White scheme
  defaultStyle.background = RGB(255, 255, 255);
  defaultStyle.foreground = RGB(0, 0, 0);

  AddStyle(new StyleSettings("Default",                     SCE_C_DEFAULT));
  AddStyle(new StyleSettings("Comment",                     SCE_C_COMMENT,                RGB(  0, 150,   0)));
  AddStyle(new StyleSettings("Comment Line",                SCE_C_COMMENTLINE,            RGB(  0, 150,   0)));
  AddStyle(new StyleSettings("Comment Doc",                 SCE_C_COMMENTDOC,             RGB(  0, 150,   0)));
  AddStyle(new StyleSettings("Comment line doc",            SCE_C_COMMENTLINEDOC,         RGB(  0, 150,   0)));
  AddStyle(new StyleSettings("Number",                      SCE_C_NUMBER,                 RGB(150, 0,   150)));
  AddStyle(new StyleSettings("UUID",                        SCE_C_UUID));
  AddStyle(new StyleSettings("Preprocessor",                SCE_C_PREPROCESSOR,           RGB(  0, 150, 150)));
  AddStyle(new StyleSettings("Operator",                    SCE_C_OPERATOR));
  AddStyle(new StyleSettings("Identifier",                  SCE_C_IDENTIFIER));
  AddStyle(new StyleSettings("String",                      SCE_C_STRING,                 RGB(150, 0,   150)));
  AddStyle(new StyleSettings("String EOL",                  SCE_C_STRINGEOL,              RGB(150, 0,   150)));
  AddStyle(new StyleSettings("Character",                   SCE_C_CHARACTER,              RGB(150, 0,   150)));
  AddStyle(new StyleSettings("Verbatim",                    SCE_C_VERBATIM));
  AddStyle(new StyleSettings("Reg Ex",                      SCE_C_REGEX));
  AddStyle(new StyleSettings("Keyword 1",                   SCE_C_WORD,                   RGB(  0,   0, 150)));
  AddStyle(new StyleSettings("Keyword 2",                   SCE_C_WORD2,                  RGB(150,   0,   0)));
  AddStyle(new StyleSettings("Commented Doc Keyword",       SCE_C_COMMENTDOCKEYWORD,      RGB(  0, 150,   0)));
  AddStyle(new StyleSettings("Commented Doc Keyword Error", SCE_C_COMMENTDOCKEYWORDERROR, RGB(  0, 150,   0)));
#endif

#if 0
  // White on Blue scheme
  defaultStyle.background = RGB(0, 0, 128);
  defaultStyle.foreground = RGB(192, 192, 192);

  AddStyle(new StyleSettings("Default",                     SCE_C_DEFAULT));
  AddStyle(new StyleSettings("Comment",                     SCE_C_COMMENT,                RGB(128, 128, 128)));
  AddStyle(new StyleSettings("Comment Line",                SCE_C_COMMENTLINE,            RGB(128, 128, 128)));
  AddStyle(new StyleSettings("Comment Doc",                 SCE_C_COMMENTDOC,             RGB(128, 128, 128)));
  AddStyle(new StyleSettings("Comment line doc",            SCE_C_COMMENTLINEDOC,         RGB(128, 128, 128)));
  AddStyle(new StyleSettings("Number",                      SCE_C_NUMBER,                 RGB(  0, 222, 222)));
  AddStyle(new StyleSettings("UUID",                        SCE_C_UUID));
  AddStyle(new StyleSettings("Preprocessor",                SCE_C_PREPROCESSOR,           RGB(  0, 222, 222)));
  AddStyle(new StyleSettings("Operator",                    SCE_C_OPERATOR));
  AddStyle(new StyleSettings("Identifier",                  SCE_C_IDENTIFIER));
  AddStyle(new StyleSettings("String",                      SCE_C_STRING,                 RGB(  0, 236,   0)));
  AddStyle(new StyleSettings("String EOL",                  SCE_C_STRINGEOL,              RGB(  0, 236,   0)));
  AddStyle(new StyleSettings("Character",                   SCE_C_CHARACTER,              RGB(  0, 236,   0)));
  AddStyle(new StyleSettings("Verbatim",                    SCE_C_VERBATIM));
  AddStyle(new StyleSettings("Reg Ex",                      SCE_C_REGEX));
  AddStyle(new StyleSettings("Keyword 1",                   SCE_C_WORD,                   RGB(  0, 222, 222)));
  AddStyle(new StyleSettings("Keyword 2",                   SCE_C_WORD2,                  RGB(  0, 222, 222)));
  AddStyle(new StyleSettings("Commented Doc Keyword",       SCE_C_COMMENTDOCKEYWORD,      RGB(128, 128, 128)));
  AddStyle(new StyleSettings("Commented Doc Keyword Error", SCE_C_COMMENTDOCKEYWORDERROR, RGB(128, 128, 128)));
#endif

  int styleIndex;
  StyleSettings *style;

  if ((styleIndex = FindStyle("Operator")) != -1)
  {
    style = GetStyle(styleIndex);
    style->bold = true;
  }

  if ((styleIndex = FindStyle("Operator")) != -1)
  {
    style = GetStyle(styleIndex);
    style->bold = true;
  }

  if ((styleIndex = FindStyle("Keyword 1")) != -1)
  {
    style = GetStyle(styleIndex);
    style->bold = true;
  }

  if ((styleIndex = FindStyle("Keyword 2")) != -1)
  {
    style = GetStyle(styleIndex);
    style->bold = true;
  }

  for (int i = 0; i < GetStyleCount(); ++i)
  {
    GetStyle(i)->SetFont("Courier New");
  }

  KeywordSet *keywords;
  CString str;

  keywords = new KeywordSet;
  keywords->setId = 0;
  str = "if else for foreach in and or while dowhile function return"
        " continue break null global local member table true false this";
  keywords->SetKeywords(str);
  AddKeywordSet(keywords);

  keywords = new KeywordSet;
  keywords->setId = 1;
  
  str = "debug typeId typeName typeRegisterOperator typeRegisterVariable"
        "sysCollectGarbage sysGetMemoryUsage sysSetDesiredMemoryUsageHard"
        "sysSetDesiredMemoryUsageSoft sysSetDesiredMemoryUsageAuto sysGetDesiredMemoryUsageHard"
        "sysGetDesiredMemoryUsageSoft sysTime doString globals threadTime"
        "threadId threadAllIds threadKill threadKillAll thread yield exit"
        "assert sleep signal block stateSet stateSetOnThread stateGet"
        "stateGetLast stateSetExitFunction tableCount tableDuplicate"
        "print format";

  keywords->SetKeywords(str);
  AddKeywordSet(keywords);
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
GMLangSettings::~GMLangSettings(void)
{
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
