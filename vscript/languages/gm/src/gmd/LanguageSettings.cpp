#include "StdAfx.h"
#include "languagesettings.h"


#ifdef _DEBUG
  #define new DEBUG_NEW
  #undef THIS_FILE
  static char THIS_FILE[] = __FILE__;
#endif


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
StyleSettings::StyleSettings()
{
  InitDefaults();
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
StyleSettings::StyleSettings(const CString &name, int typeId)
{
  font = NULL;
  InitDefaults();
  this->name = name;
  this->typeId = typeId;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
StyleSettings::StyleSettings(const CString &name, int typeId, COLORREF foreground)
{
  font = NULL;
  InitDefaults();
  this->name        = name;
  this->typeId      = typeId;
  this->foreground  = foreground;
  defaultForeground = false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
StyleSettings::StyleSettings(const CString &name, int typeId, COLORREF foreground, COLORREF background)
{
  font = NULL;
  InitDefaults();
  this->name        = name;
  this->typeId      = typeId;
  this->foreground  = foreground;
  this->background  = background;
  defaultForeground = false;
  defaultBackground = false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
StyleSettings::~StyleSettings()
{
  delete [] font;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void StyleSettings::InitDefaults()
{
  name                = "";
  typeId              = 0;
  foreground          = RGB(0, 0, 0);
  background          = RGB(255, 255, 255);
  defaultForeground   = true;
  defaultBackground   = true;
  size                = -1;
  delete [] font;
  font                = NULL;
  bold                = false;
  italic              = false;
  underline           = false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void StyleSettings::SetFont(const char *str)
{
  delete [] font;
  font = NULL;
  if (str)
  {
    font = new char[strlen(str)+1];
    strcpy(font, str);
  }
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeywordSet::KeywordSet()
: setId(-1),
  keywords(NULL)
{
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeywordSet::~KeywordSet()
{
  delete [] keywords;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeywordSet::SetKeywords(const char *str)
{
  delete [] keywords;
  keywords = NULL;
  if (str)
  {
    keywords = new char[strlen(str)+1];
    strcpy(keywords, str);
  }
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
LanguageSettings::LanguageSettings(void)
: lexerId(0),
  styleBits(5),
  defaultStyle("Default", STYLE_DEFAULT, RGB(0, 0, 0), RGB(255, 255, 255)),
  tabWidth(2),
  tabsToSpaces(true)
{
  defaultStyle.size = 10;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
LanguageSettings::~LanguageSettings(void)
{
  int index;
  for(index=0; index < styleSettings.GetSize(); ++index)
  {
    StyleSettings * ss = GetStyle(index);
    delete ss;
  }
  styleSettings.RemoveAll();

  for(index=0; index < keywordSets.GetSize(); ++index)
  {
    KeywordSet * ks = GetKeywordSet(index);
    delete ks;
  }
  keywordSets.RemoveAll();
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LanguageSettings::AddStyle(StyleSettings *settings)
{
  if (FindStyle(settings) == -1 && FindStyle(settings->name) == -1 && FindStyle(settings->typeId) == -1)
  {
    styleSettings.Add(settings);
    return true;
  }
  return false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LanguageSettings::RemoveStyle(StyleSettings *settings, bool deleteData)
{
  int count = styleSettings.GetSize();
  for (int i = 0; i < count; ++i)
  {
    if (styleSettings.GetAt(i) == settings)
    {
      styleSettings.RemoveAt(i);
      if (deleteData)
        delete settings;
      return true;
    }
  }

  return false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LanguageSettings::RemoveStyle(const CString &name, bool deleteData)
{
  int count = styleSettings.GetSize();
  for (int i = 0; i < count; ++i)
  {
    StyleSettings *c = GetStyle(i);
    if (c->name.Compare(name) == 0)
    {
      styleSettings.RemoveAt(i);
      if (deleteData)
        delete c;
      return true;
    }
  }

  return false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LanguageSettings::RemoveStyle(int typeId, bool deleteData)
{
  int count = styleSettings.GetSize();
  for (int i = 0; i < count; ++i)
  {
    StyleSettings *c = GetStyle(i);
    if (c->typeId == typeId)
    {
      styleSettings.RemoveAt(i);
      if (deleteData)
        delete c;
      return true;
    }
  }

  return false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int LanguageSettings::FindStyle(StyleSettings *settings) const
{
  int count = styleSettings.GetSize();
  for (int i = 0; i < count; ++i)
  {
    if (GetStyle(i) == settings)
      return i;
  }

  return -1;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int LanguageSettings::FindStyle(const CString &name) const
{
  int count = styleSettings.GetSize();
  for (int i = 0; i < count; ++i)
  {
    if (GetStyle(i)->name.Compare(name) == 0)
      return i;
  }

  return -1;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int LanguageSettings::FindStyle(int typeId) const
{
  int count = styleSettings.GetSize();
  for (int i = 0; i < count; ++i)
  {
    if (GetStyle(i)->typeId == typeId)
      return i;
  }

  return -1;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int LanguageSettings::GetStyleCount() const
{
  return styleSettings.GetSize();
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
StyleSettings *LanguageSettings::GetStyle(int i)
{
  return reinterpret_cast<StyleSettings *>(styleSettings.GetAt(i));
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const StyleSettings *LanguageSettings::GetStyle(int i) const
{
  return reinterpret_cast<StyleSettings *>(styleSettings.GetAt(i));
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LanguageSettings::AddKeywordSet(KeywordSet *set)
{
  if (FindKeywordSet(set) == -1 && FindKeywordSet(set->setId) == -1)
  {
    keywordSets.Add(set);
    return true;
  }
  return false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LanguageSettings::RemoveKeywordSet(KeywordSet *set, bool deleteData)
{
  int count = GetKeywordSetCount();
  for (int i = 0; i < count; ++i)
  {
    KeywordSet *ks = GetKeywordSet(i);
    if (ks == set)
    {
      keywordSets.RemoveAt(i);
      if (deleteData)
        delete ks;
      return true;
    }
  }

  return false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LanguageSettings::RemoveKeywordSet(int setId, bool deleteData)
{
  int count = GetKeywordSetCount();
  for (int i = 0; i < count; ++i)
  {
    KeywordSet *ks = GetKeywordSet(i);
    if (ks->setId == setId)
    {
      keywordSets.RemoveAt(i);
      if (deleteData)
        delete ks;
      return true;
    }
  }

  return false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int LanguageSettings::FindKeywordSet(KeywordSet *set) const
{
  int count = GetKeywordSetCount();
  for (int i = 0; i < count; ++i)
  {
    const KeywordSet *ks = GetKeywordSet(i);
    if (ks == set)
      return i;
  }

  return -1;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int LanguageSettings::FindKeywordSet(int setId) const
{
  int count = GetKeywordSetCount();
  for (int i = 0; i < count; ++i)
  {
    const KeywordSet *ks = GetKeywordSet(i);
    if (ks->setId == setId)
      return i;
  }

  return -1;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int LanguageSettings::GetKeywordSetCount() const
{
  return keywordSets.GetSize();
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeywordSet *LanguageSettings::GetKeywordSet(int i)
{
  return reinterpret_cast<KeywordSet *>(keywordSets.GetAt(i));
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const KeywordSet *LanguageSettings::GetKeywordSet(int i) const
{
  return reinterpret_cast<KeywordSet *>(keywordSets.GetAt(i));
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void LanguageSettings::Apply(ScintillaEdit &scintilla) const
{
  scintilla.SetLexer(lexerId);
  scintilla.SetStyleBits(styleBits);

  scintilla.SetTabWidth(tabWidth);
  scintilla.SetUseTabs(!tabsToSpaces);

  int i;
  int count = GetKeywordSetCount();
  for (i = 0; i < count; ++i)
  {
    const KeywordSet *set = GetKeywordSet(i);
    if (set->keywords)
      scintilla.SetKeywords(set->setId, set->keywords);
  }

//  scintilla.SetWhiteSpaceBGColour(true, defaultBackground);
//  scintilla.SetWhiteSpaceFGColour(true, defaultForeground);

  ApplyStyle(scintilla, defaultStyle);
  /// Copy default style to all.
  scintilla.StyleClearAll();

  scintilla.SetCaretColour(defaultStyle.foreground);
  scintilla.SetSelectionBGColour(true, defaultStyle.foreground);
  scintilla.SetSelectionFGColour(true, defaultStyle.background);

  count = GetStyleCount();
  for (i = 0; i < count; ++i)
  {
    const StyleSettings *style = GetStyle(i);
    ASSERT(style);
    ApplyStyle(scintilla, *style);
  }

  scintilla.Colourise(0, -1);
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void LanguageSettings::ApplyStyle(ScintillaEdit &scintilla, const StyleSettings &style) const
{
  if (!style.defaultForeground)
    scintilla.StyleSetFGColour(style.typeId, style.foreground);

  if (!style.defaultBackground)
    scintilla.StyleSetBGColour(style.typeId, style.background);

  if (style.size > 0) 
    scintilla.StyleSetSize(style.typeId, style.size); 
  if (style.font)
    scintilla.StyleSetFont(style.typeId, style.font);

  scintilla.StyleSetBold(style.typeId, style.bold);
  scintilla.StyleSetItalic(style.typeId, style.italic);
  scintilla.StyleSetUnderline(style.typeId, style.underline);
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
