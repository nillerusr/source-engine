#pragma once

#include "ScintillaEdit.h"

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
struct StyleSettings
{
  CString   name;
  int       typeId;
  COLORREF  foreground;
  COLORREF  background;
  bool      defaultForeground;
  bool      defaultBackground;
  int       size;
  char *    font;
  bool      bold;
  bool      italic;
  bool      underline;

  StyleSettings();
  StyleSettings(const CString &name, int typeId);
  StyleSettings(const CString &name, int typeId, COLORREF foreground);
  StyleSettings(const CString &name, int typeId, COLORREF foreground, COLORREF background);
  ~StyleSettings();

  void InitDefaults();

  void SetFont(const char *str);
};

struct KeywordSet
{
  int       setId;                                // 0 to 9.
  char *    keywords;                             // space, \t or \n separated.

  KeywordSet();
  virtual ~KeywordSet();

  void SetKeywords(const char *str);
};

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class LanguageSettings
{
public:
  LanguageSettings(void);
  virtual ~LanguageSettings(void);

  inline void SetLexerId(int id)                  { lexerId = id; }
  inline int GetLexerId() const                   { return lexerId; }

  inline void SetStyleBits(int bits)              { styleBits = bits; }
  inline int GetStyleBits() const                 { return styleBits; }

  StyleSettings &GetDefaultStyle()                { return defaultStyle; }
  const StyleSettings &GetDefaultStyle() const    { return defaultStyle; }

  bool AddStyle(StyleSettings *settings);

  bool RemoveStyle(StyleSettings *settings, bool deleteData = true);
  bool RemoveStyle(const CString &name, bool deleteData = true);
  bool RemoveStyle(int typeId, bool deleteData = true);

  int FindStyle(StyleSettings *settings) const;
  int FindStyle(const CString &name) const;
  int FindStyle(int typeId) const;

  int GetStyleCount() const;
  StyleSettings *GetStyle(int i);
  const StyleSettings *GetStyle(int i) const;

  bool AddKeywordSet(KeywordSet *set);

  bool RemoveKeywordSet(KeywordSet *set, bool deleteData = true);
  bool RemoveKeywordSet(int setId, bool deleteData = true);

  int FindKeywordSet(KeywordSet *set) const;
  int FindKeywordSet(int setId) const;

  int GetKeywordSetCount() const;
  KeywordSet *GetKeywordSet(int i);
  const KeywordSet *GetKeywordSet(int i) const;

  void Apply(ScintillaEdit &scintilla) const;

protected:
  int           lexerId;
  int           tabWidth;
  bool          tabsToSpaces;
  int           styleBits;
  CPtrArray     styleSettings;
  CPtrArray     keywordSets;
  StyleSettings defaultStyle;

  void ApplyStyle(ScintillaEdit &scintilla, const StyleSettings &style) const;
};

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
