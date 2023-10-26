#ifndef __SCINTILLAEDIT_H
#define __SCINTILLAEDIT_H

#include "include/Scintilla.h"

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
/// Utility class for simplifying communication with a Scintilla edit control. Usage is to create
/// the Scintilla editor using CreateWindowEx (see Scintilla documentation) then to call
/// Attach(HWND) to associate this class with the window. Don't forget to call Detach() when done.
///
/// The utility functions herein are wrappers for Scintilla messages. Consult Scintilla
/// doccumentation for further information on the utility functions.
class ScintillaEdit : public CWnd
{
public:
  /// Constructor. Calls CWnd contructor.
  ScintillaEdit();

  /// Attaches this class to the given window handle. This version checks that the given handle is
  /// for a Scintilla control. See MFC doccumentation for further details.
  /// \return TRUE on successful attachment.
  BOOL Attach(HWND hwnd);

  //----------------------------------------------
  // Scintilla text message functions.
  //----------------------------------------------

  /// Gets the window text. Sends SCI_GETTEXT. Retrieves maxChars-1 characters.
  /// \param  maxChars      maximum number of characters +1 to get.
  /// \param  text          text buffer to retrieve text into.
  void GetText(int maxChars, char *text);

  /// Sets the window text. Sends SCI_SETTEXT.
  void SetText(const char *text);

  /// Sets the changed status of the doccument to false.
  void SetSavePoint();

  /// Gets the a single line of text. Sends SCI_GETLINE. Buffer must be long enough to hold line.
  /// Use GetLineLength to determine the required buffer size.
  /// \param  line          line number of text to retrieve. Zero based.
  /// \param  text          text buffer to retrieve line into. Must be long enough. Will not be
  ///                       null terminated.
  void GetLine(int line, char *text);

  /// Replaces the selected text. Text is replaced between the current anchor and current position.
  /// Inserts text at the caret position if the anchor and current position are equal.
  /// Uses SCI_REPLACESEL message.
  /// \param  text          text to replace/insert with. Must be NULL terminated.
  void ReplaceSelection(const char *text);

  /// Returns true if in read only mode. Uses SCI_GETREADONLY message.
  /// \return true if read only.
  bool GetReadOnly();

  /// Sets the read only state of the control. Uses SCI_SETREADONLY message.
  /// \param  readOnly      new read only state.
  void SetReadOnly(bool readOnly);

  /// Collects text between cpMin and cpMax in textRange. Text is copied into lpstrText in
  /// textRange, which must be large enough to hold the text and a NULL character. Retrieves to the
  /// end of the doccument if cpMax is -1.
  /// Uses SCI_GETTEXTRANGE message.
  /// \param  textRange     details of text to retrieve and the buffer to retrieve into.
  void GetTextRange(TextRange &textRange);

  /// Collects text with style information. Text is collected in a similar fashion to GetTextRange.
  /// However, two bytes are used per character with the lower byte being the character and the
  /// upper byte the style information. Two 0 bytes are used to terminate the string. Once again
  /// the lpstrText buffer must be large enough.
  /// Uses the SCI_GETSTYLEDTEXT message.
  /// \param  textRange     details of text to retrieve and the buffer to retrieve into.
  void GetStyledText(TextRange &textRange);

  /// Adds text at the caret position. Adds length characters regarless of whether the text buffer
  /// is shorter that this value. Does not scroll text into view.
  /// Uses the SCI_ADDTEXT message.
  /// \param  length      number of characters to insert.
  /// \param  text        text to insert. Can this be a const char *???
  void AddText(int length, char *text);

  /// Adds styled text at the caret position. Behaves similarly to AddText, but uses the two byte
  /// styled format (low byte character, high byte style).
  /// Uses the SCI_ADDSTYLEDTEXT message.
  /// \param  length      number of characters to insert.
  /// \param  text        text to insert. Can this be a const char *???
  void AddStyledText(int length, char *text);

  /// Adds text to the end of the document without affecting the selection.
  /// Uses SCI_APPENDTEXT.
  /// \param  length      number of characters to insert.
  /// \param  text        text to append. Can this be a const char *???
  void AppendText(int length, char *text);

  /// Inserts the NULL terminated string at the given position. Adds to current position if pos is'
  /// -1.
  /// Uses SCI_INSERTTEXT.
  /// \param  pos         position to insert at. -1 to insert at current position.
  /// \param  text        text to insert.
  void InsertText(int pos, const char *text);

  /// Deletes doccument text, but only if not read only. Uses SCI_CLEARALL.
  void ClearAll();

  /// Clears current style information and resets folding state. May be used when changing styles.
  /// Uses SCI_CLEARDOCUMENTSTYLE message.
  void ClearDocumentStyle();

  /// Retrieves a single character. Uses SCI_GETCHARAT message.
  /// \param  pos       position of character to retrieve.
  /// \return   the character at this position or 0 if pos is out of range.
  char GetCharAt(int pos);

  /// Retrieves a single character's style. Uses SCI_GETSTYLEAT message.
  /// \param  pos       position of character to retrieve.
  /// \return   the style at this position or 0 if pos is out of range.
  char GetStyleAt(int pos);

  /// Sets the number of bits to use for style information. A maximum of 7 bits may be used. Other
  /// bits may still be used as indicators. Set to 5 by default.
  /// Uses SCI_SETSTYLEBITS message.
  /// \param  bits      number of style bits to use.
  void SetStyleBits(int bits);

  /// Gets the number of bits to use for style information.
  /// Uses SCI_GETSTYLEBITS message.
  /// \return number of style bits in use. Maximum of 7, 5 by default.
  int GetStyleBits();

  //----------------------------------------------
  // Scintilla search message functions.
  // See Scintilla documentation for search flags.
  //----------------------------------------------

  /// Searches for particular text within the document. Returns -1 on failure to find the text or
  /// the position of the start of the text when found.
  ///
  /// Search flags are explained below as taken from Scintilla documentation:
  ///
  /// Several of the search routines use flag options which include a simple regular expression
  /// search. Combine the flag options by adding them:
  ///
  /// SCFIND_MATCHCASE A match only occurs with text that matches the case of the search string.
  /// SCFIND_WHOLEWORD A match only occurs if the characters before and after are not word characters.
  /// SCFIND_WORDSTART A match only occurs if the character before is not a word character.
  /// SCFIND_REGEXP The search string should be interpreted as a regular expression.
  ///
  /// If SCFIND_REGEXP is not included in the searchFlags, you can search backwards to find the
  /// previous occurance of a search string by setting the end of the search range before the start.
  /// If SCFIND_REGEXP is included searches are always from a lower position to a higher position,
  /// even if the search range is backwards.
  ///
  /// In a regular expression, special characters interpreted are:
  ///
  /// . Matches any character
  /// \( This marks the start of a region for tagging a match.
  /// \) This marks the end of a tagged region.
  /// \n Where n is 1 through 9 refers to the first through ninth tagged region when replacing.
  ///   For example if the search string was Fred\([1-9]\)XXX and the replace string was Sam\1YYY
  ///   applied to Fred2XXX this would generate Sam2YYY.
  /// \< This matches the start of a word using Scintilla's definitions of words.
  /// \> This matches the end of a word using Scintilla's definition of words.
  /// \x This allows you to use a character x that would otherwise have a special meaning. For
  ///   example, \[ would be interpreted as [ and not as the start of a character set.
  /// [...] This indicates a set of characters, for example [abc] means any of the characters a, b
  ///   or c. You can also use ranges, for example [a-z] for any lower case character.
  /// [^...] The complement of the characters in the set. For example, [^A-Za-z] means any
  ///   character except an alphabetic character.
  /// ^ This matches the start of a line (unless used inside a set, see above).
  /// $ This matches the end of a line.
  /// * This matches 0 or more times. For example Sa*m matches Sm, Sam, Saam, Saaam and so on.
  /// + This matches 1 or more times. For example Sa+m matches Sam, Saam, Saaam and so on.
  ///
  /// Uses SCI_FINDTEXT message.
  /// \param  searchFlags   flags to search with.
  /// \param  ttf           search text details. Use chrg.cpMin and max to set search range.
  /// \return the position of the found text or -1 if not found.
  int FindText(int searchFlags, TextToFind &ttf);

  /// Sets the search anchor for SearchNext and SearchPrev to the start of the current selection.
  /// Uses SCI_SEARCHANCHOR message.
  void SetSearchAnchor();

  /// Searches for the next occurence of the given text.
  /// Uses SCI_SEARCHNEXT message.
  /// \param  searchFlags   flags to search with.
  /// \param  ttf           search text details. Use chrg.cpMin and max to set search range.
  /// \return the position of the found text or -1 if not found.
  int SearchNext(int searchFlags, const char *forText);

  /// Searches for the previous occurence of the given text.
  /// Uses SCI_SEARCHPREV message.
  /// \param  searchFlags   flags to search with.
  /// \param  ttf           search text details. Use chrg.cpMin and max to set search range.
  /// \return the position of the found text or -1 if not found.
  int SearchPrev(int searchFlags, const char *forText);

  // Note: there are too many functions for me to bother redocumenting all of them. Consult
  // Scintilla documentation. Generally, the name of the function directly corresponds to an
  // equivalent SCI_message.

  //----------------------------------------------
  // Scintilla search and replace message functions.
  //----------------------------------------------
  int GetTargetStart();
  void SetTargetStart(int pos);

  int GetTargetEnd();
  void SetTargetEnd(int pos);

  int ReplaceTarget(int length, const char *text);
  int ReplaceTargetRegularExpression(int length, const char *text);

  int GetSearchFlags();
  void SetSearchFlags(int flags);

  int SearchInTarget(int length, const char *text);

  //----------------------------------------------
  // Scintilla over type message functions.
  //----------------------------------------------
  bool GetOverType();
  void SetOvertType(bool overType);

  //----------------------------------------------
  // Scintilla cut & paste message functions.
  //----------------------------------------------
  void Cut();
  void Copy();
  void Paste();
  void Clear();
  bool CanPaste();

  //----------------------------------------------
  // Scintilla error handling message functions.
  // TODO - not supported yet.
  //----------------------------------------------

  //----------------------------------------------
  // Scintilla undo/redo message functions.
  // TODO
  //----------------------------------------------

  //----------------------------------------------
  // Scintilla selection message functions.
  //----------------------------------------------
  int GetTextLength();
  int GetLineCount();
  int GetFirstVisibleLine();
  int GetLinesOnScreen();
  bool GetModified();
  void SetSelection(int anchorPos, int currentPos);
  void GotoPos(int pos);
  void GotoLine(int line);

  int GetCurrentPos();
  void SetCurrentPos(int pos);
  int GetAnchor();
  void SetAnchor(int pos);

  int GetSelectionStart();
  void SetSelectionStart(int pos);
  int GetSelectionEnd();
  void SetSelectionEnd(int pos);

  void SelectAll();

  int LineFromPosition(int pos);
  int PositionFromLine(int line);
  int GetLineEndPosition(int line);
  int GetLineLength(int line);

  void GetSelectedText(char *text);

  void GetCurrentLine(int maxChars, char *text);

  bool SelectionIsRectangle();
  void MoveCaretInsideView();
  int WordEndPosition(int position, bool onlyWordCharacters = true);
  int WordStartPosition(int position, bool onlyWordCharacters = true);

  int TextWidth(int styleNumber, char *text);
  int TextHeight(int line);
  int GetColumn(int pos);

  int PositionFromPoint(int x, int y);
  int PositionFromPointClose(int x, int y);
  int PointXFromPosition(int pos);
  int PointYFromPosition(int pos);

  bool HideSelection(bool hide);

  //----------------------------------------------
  // Scintilla scrolling message functions.
  //----------------------------------------------
  void LineScroll(int column, int line);
  void ScrollToCaret();

  void SetXCaretPolicy(int caretPolicy, int caretStop);
  void SetYCaretPolicy(int caretPolicy, int caretStop);

  void SetVisiblePolicy(int caretPolicy, int caretStop);

  bool GetHScrollBar();
  void SetHScrollBar(bool visible);

  int GetXOffset();
  void SetXOffset(int offset);

  int GetScrollWidth();
  void SetScrollWidth(int pixelWidth);

  bool GetEndAtLastLine();
  void SetEndAtLastLine(bool end);

  //----------------------------------------------
  // Scintilla white space message functions.
  //----------------------------------------------
  int GetWhiteSpaceMode();
  void SetWhiteSpaceMode(int mode);

  void SetWhiteSpaceFGColour(bool useWhiteSpaceFGColour, int colour);
  void SetWhiteSpaceBGColour(bool useWhiteSpaceBGColour, int colour);
  void SetWhiteSpaceColour(bool useWhiteSpaceColour, int fgColour, int bgColour);

  //----------------------------------------------
  // Scintilla cursor and mouse capture message functions.
  //----------------------------------------------
  int GetCursorType();
  void SetCursorType(int type);

  bool GetMouseDownCaptures();
  void SetMouseDownCaptures(bool captures);

  //----------------------------------------------
  // Scintilla line endings capture message functions.
  //----------------------------------------------
  enum EOLMode
  {
    EOL_CrLf    = SC_EOL_CRLF,
    EOL_Cr      = SC_EOL_CR,
    EOL_Lf      = SC_EOL_LF,
  };

  EOLMode GetEOLMode();
  void SetEOLMode(EOLMode mode);

  void ConvertEOLs(int eolMode);

  bool GetViewEOL();
  void SetViewEOL(bool view);

  //----------------------------------------------
  // Scintilla styling endings capture message functions.
  //----------------------------------------------
  bool GetEndStyled();

  void StartStyling(int pos, int mask);
  void SetStyling(int length, int style);
  void SetStylingEx(int length, const char *styles);

  int GetLineState(int line);
  void SetLineState(int line, int state);

  int GetMaxLineState();

  //----------------------------------------------
  // Scintilla caret and selection style message functions.
  //----------------------------------------------
  void StyleResetDefault();
  void StyleClearAll();

  void StyleSetFont(int styleNumber, char *fontName);
  void StyleSetSize(int styleNumber, int pointSize);
  void StyleSetBold(int styleNumber, bool bold);
  void StyleSetItalic(int styleNumber, bool italic);
  void StyleSetUnderline(int styleNumber, bool underline);
  void StyleSetFont(int styleNumber, char *fontName, int pointSize, bool bold, bool italic, bool underline);

  void StyleSetFGColour(int styleNumber, int colour);
  void StyleSetBGColour(int styleNumber, int colour);
  void StyleSetColour(int styleNumber, int fgColour, int bgColour);

  void StyleSetEOLFilled(int styleNumber, bool eolFilled);

  enum StyleCharSet
  {
    SCS_Ansi        = SC_CHARSET_ANSI,
    SCS_Arabic      = SC_CHARSET_ARABIC,
    SCS_Baltic      = SC_CHARSET_BALTIC,
    SCS_ChineseBig5 = SC_CHARSET_CHINESEBIG5,
    SCS_Default     = SC_CHARSET_DEFAULT,
    SCS_EastEurope  = SC_CHARSET_EASTEUROPE,
    SCS_GB2312      = SC_CHARSET_GB2312,
    SCS_Greek       = SC_CHARSET_GREEK,
    SCS_Hangul      = SC_CHARSET_HANGUL,
    SCS_Hebrew      = SC_CHARSET_HEBREW,
    SCS_Johab       = SC_CHARSET_JOHAB,
    SCS_Mac         = SC_CHARSET_MAC,
    SCS_OEM         = SC_CHARSET_OEM,
    SCS_ShiftJis    = SC_CHARSET_SHIFTJIS,
    SCS_Symbol      = SC_CHARSET_SYMBOL,
    SCS_Thai        = SC_CHARSET_THAI,
    SCS_Turkish     = SC_CHARSET_TURKISH,
    SCS_Vietnamese  = SC_CHARSET_VIETNAMESE,
  };

  void StyleSetCharacterSet(int styleNumber, StyleCharSet charSet);

  enum StyleCaseMode
  {
    SCM_Mixed       = SC_CASE_MIXED,
    SCM_Upper       = SC_CASE_UPPER,
    SCM_Lower       = SC_CASE_LOWER,
  };

  void StyleSetCase(int styleNumber, StyleCaseMode mode);

  void StyleSetVisible(int styleNumber, bool visible);

  void StyleSetChangeable(int styleNumber, bool changeable);

  //----------------------------------------------
  // Scintilla caret and selection style message functions.
  //----------------------------------------------
  void SetSelectionFGColour(bool useSelectionFGColour, int colour);
  void SetSelectionBGColour(bool useSelectionBGColour, int colour);

  int GetCaretColour();
  void SetCaretColour(int colour);

  bool GetCaretLineVisible();
  void SetCaretLineVisible(bool show);
  int GetCaretLineColour();
  void SetCaretLineColour(int colour);

  int GetCaretPeriod();
  void SetCaretPeriod(int ms);
  int GetCaretWidth();
  void SetCaretWidth(int pixels);
  int GetControlCharSymbol();
  void SetControlCharSymbol(int symbol);

  //----------------------------------------------
  // Scintilla margin message functions.
  //----------------------------------------------
  enum Margin
  {
    M_Number    = 0,
    M_Symbol1,
    M_Symbol2,
  };

  int GetMarginType(Margin margin);
  void SetMarginType(Margin margin, int iType);

  int GetMarginWidth(Margin margin);
  void SetMarginWidth(Margin margin, int pixelWidth);

  int GetMarginMask(Margin margin);
  void SetMarginMask(Margin margin, int mask);

  bool GetMarginSensitive(Margin margin);
  void SetMarginSensitive(Margin margin, bool sensitive);

  int GetMarginLeft();
  int GetMarginRight();
  void SetMarginLeft(int pixels);
  void SetMarginRight(int pixels);

  //----------------------------------------------
  // Scintilla other settings message functions.
  //----------------------------------------------
  bool GetUsePalette();
  void SetUszePalette(bool allowPalette);

  bool GetBufferedDraw();
  void SetBufferedDraw(bool buffer);

  int GetCodePage();
  void SetCodePage(int codePage);

  void SetWordChars(const char *chars);

  void GrabFocus();
  void GetFocus();
  void SetFocus(bool focus);

  //----------------------------------------------
  // Scintilla brace highlighting message functions.
  //----------------------------------------------
  void BraceHighlight(int pos1, int pos2);
  void BraceBadLight(int pos);
  int BraceMatch(int pos, int maxReStyle);

  //----------------------------------------------
  // Scintilla tab and indent message functions.
  //----------------------------------------------
  int  GetTabWidth();
  void SetTabWidth(int numChars);
  bool GetUseTabs();
  void SetUseTabs(bool useTabs);
  int  GetIndent();
  void SetIndent(int numChars);

  bool GetTabIndents();
  void SetTabIndents(bool tabIndents);
  bool GetBackSpaceUnindents();
  void SetBackSpaceUnindents(bool unindents);
  int  GetLineIndentation(int line);
  void SetLineIndentation(int line, int indentation);
  int  GetLineIndentPosition(int line);

  bool GetIndentationGuides();
  void SetIndentationGuides(bool view);
  int  GetHighlightGuide();
  void SetHighlightGuide(int column);

  //----------------------------------------------
  // Scintilla marker message functions.
  //----------------------------------------------
  enum MarkerSymbol
  {
    MS_Circle               = SC_MARK_CIRCLE,
    MS_RoundRect            = SC_MARK_ROUNDRECT,
    MS_Arrow                = SC_MARK_ARROW,
    MS_SmallRect            = SC_MARK_SMALLRECT,
    MS_ShortArrow           = SC_MARK_SHORTARROW,
    MS_Empty                = SC_MARK_EMPTY,
    MS_ArrowDown            = SC_MARK_ARROWDOWN,
    MS_Minus                = SC_MARK_MINUS,
    MS_Plus                 = SC_MARK_PLUS,
    MS_VLine                = SC_MARK_VLINE,
    MS_LCorner              = SC_MARK_LCORNER,
    MS_TCorner              = SC_MARK_TCORNER,
    MS_BoxPlus              = SC_MARK_BOXPLUS,
    MS_BoxPlusConnected     = SC_MARK_BOXPLUSCONNECTED,
    MS_BoxMinus             = SC_MARK_BOXMINUS,
    MS_BoxMinusConncted     = SC_MARK_BOXMINUSCONNECTED,
    MS_LCornerCurve         = SC_MARK_LCORNERCURVE,
    MS_TCornerCurve         = SC_MARK_TCORNERCURVE,
    MS_CirclePlus           = SC_MARK_CIRCLEPLUS,
    MS_CirclePlusConnected  = SC_MARK_CIRCLEPLUSCONNECTED,
    MS_CircleMinus          = SC_MARK_CIRCLEMINUS,
    MS_CircleMinusConnected = SC_MARK_CIRCLEMINUSCONNECTED,
    MS_MarkBackground       = SC_MARK_BACKGROUND,
    MS_Ellipsis             = SC_MARK_DOTDOTDOT,
    MS_Arrows               = SC_MARK_ARROWS,
    MS_PixMap               = SC_MARK_PIXMAP,
    MS_Character            = SC_MARK_CHARACTER,
  };

  void MarkerDefine(int markerNumber, MarkerSymbol marker);
  void MarkerSetFGColour(int markerNumber, int colour);
  void MarkerSetGGColour(int markerNumber, int colour);

  int  MarkerAdd(int line, int markerNumber);
  void MarkerDelete(int line, int markerNumber);
  void MarkerDeleteAll(int markerNumber);

  int  MarkerGet(int line);
  int  MarkerNext(int lineStart, int markerMask);
  int  MarkerPrev(int lineStart, int markerMask);

  int  MarkerLineFromHandle(int markerHandle);
  void MarkerDeleteFromHandle(int markerHandle);

  void MarkerDefinePixMap(int markerNumber, const char *xpm);

  //----------------------------------------------
  // Scintilla indicator message functions.
  //----------------------------------------------
  void IndicatorSetStyle(int indicatorNumber, int indicatorStyle);
  int  IndicatorGetStyle(int indicatorNumber);
  int  GetIndicatorFGColour(int indicatorNumber);
  void SetIndicatorFGColour(int indicatorNumber, int colour);

  //----------------------------------------------
  // Scintilla auto completion message functions.
  //----------------------------------------------
  void AutoCompleteShow(int lenEntered, const char *list);
  void AutoCompleteCancel();
  bool AutoCompleteActive();
  int  AutoCompletePosStart();
  void AutoComplete();
  void AutoCompleteStops(const char *chars);

  char AutoCompleteGetSeparator();
  void AutoCompleteSetSeparator(char separator);

  void AutoCompleteSelect(const char *select);

  bool AutoCompleteGetCancelAtStart();
  void AutoCompleteSetCancelAtStart(bool cancel);

  void AutoCompleteSetFillUps(const char *chars);

  bool AutoCompleteGetChooseSingle();
  void AutoCompleteSetChooseSingle(bool single);
  bool AutoCompleteGetIgnoreCase();
  void AutoCompleteSetIgnoreCase(bool ignore);
  bool AutoCompleteGetAutoHide();
  void AutoCompleteSetAutoHide(bool autoHide);
  bool AutoCompleteGetDropRestOfWord();
  void AutoCompleteSetDropRestOfWord(bool drop);

  //----------------------------------------------
  // Scintilla use listmessage functions.
  //----------------------------------------------
  void UserListShow(int listType, const char *list);

  //----------------------------------------------
  // Scintilla call tip message functions.
  //----------------------------------------------
  void CallTipShow(int posStart, const char *definition);
  void CallTipCancel();
  bool CallTipActive();
  int  CallTipPosStart();
  void CallTipSetHighlight(int hlStart, int hlEnd);
  void CallTipSetBGColour(int colour);

  //----------------------------------------------
  // Scintilla key binding message functions.
  //----------------------------------------------
  void AssignCommandKey(int keyDefinition, int sciCommand);
  void ClearCommandKey(int keyDefinition);
  void ClearAllCommandKeys();

  //----------------------------------------------
  // Scintilla popup edit message functions.
  //----------------------------------------------
  void UsePopup(bool enable);

  //----------------------------------------------
  // Scintilla macro message functions.
  //----------------------------------------------
  void MarcroStartRecord();
  void MarcoStopRecord();

  //----------------------------------------------
  // Scintilla printing message functions.
  //----------------------------------------------
  void PrintFormatRange();
  int  GetPrintMagnification();
  void SetPrintMagnification(int magnification);
  int  GetPrintColourMode();
  void SetPrintColourMode(int mode);

  //----------------------------------------------
  // Scintilla multiple view message functions.
  // TODO
  //----------------------------------------------

  //----------------------------------------------
  // Scintilla folding message functions.
  // TODO
  //----------------------------------------------

  //----------------------------------------------
  // Scintilla line wrapping message functions.
  //----------------------------------------------
  enum WrapMode
  {
    WM_None = SC_WRAP_NONE,
    WM_Word = SC_WRAP_WORD,
  };

  enum LayoutCache
  {
    LC_None     = SC_CACHE_NONE,
    LC_Caret    = SC_CACHE_CARET,
    LC_Page     = SC_CACHE_PAGE,
    LC_Document = SC_CACHE_DOCUMENT,
  };

  WrapMode GetWrapMode();
  void SetWrapMode(WrapMode mode);

  LayoutCache GetLayoutCache();
  void SetLayoutCache(LayoutCache cache);

  //----------------------------------------------
  // Scintilla zooming message functions.
  //----------------------------------------------
  void ZoomIn();
  void ZoomOut();
  int  GetZoom();
  void SetZoom(int zoom);

  //----------------------------------------------
  // Scintilla long line message functions.
  //----------------------------------------------
  enum EdgeMode
  {
    EM_None       = EDGE_NONE,
    EM_Line       = EDGE_LINE,
    EM_Background = EDGE_BACKGROUND,
  };

  int  GetEdgeColumn();
  void SetEdgeColumn(int column);
  EdgeMode GetEdgeMode();
  void SetEdgeMode(EdgeMode mode);
  int  GetEdgeColour();
  void SetEdgeColour(int colour);

  //----------------------------------------------
  // Scintilla lexer message functions.
  //----------------------------------------------
  int  GetLexer();
  void SetLexer(int lexer);
  void SetLexerLanguage(char *name);
  void Colourise(int start, int end);
  void SetProperty(char *key, char *value);
  void SetKeywords(int keywordSet, char *keywordList);

protected:
  /// Hidden to prevent misuse.
  virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL)
  {
    return CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext);
  }

  /// Hidden to prevent misuse.
  BOOL CreateEx(DWORD dwExStyle, LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hwndParent, HMENU nIDorHMenu, LPVOID lpParam = NULL)
  {
    return CWnd::CreateEx(dwExStyle, lpszClassName, lpszWindowName, dwStyle, x, y, nWidth, nHeight, hwndParent, nIDorHMenu, lpParam);
  }

  /// Hidden to prevent misuse.
  BOOL CreateEx(DWORD dwExStyle, LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, LPVOID lpParam = NULL)
  {
    return CreateEx(dwExStyle, lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, lpParam);
  }
};

#endif // !__SCINTILLAEDIT_H
