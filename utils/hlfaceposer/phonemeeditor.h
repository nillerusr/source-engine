//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHONEEDITOR_H
#define PHONEEDITOR_H
#ifdef _WIN32
#pragma once
#endif

#include <mxtk/mx.h>
class CAudioSource;
class CAudioMixer;
class mxBitmapButton;
class mxButton;

#include "utlvector.h"
#include "faceposertoolwindow.h"

#define IDC_PHONEME_SCROLL				1001
#define IDC_PHONEME_PLAY_ORIG			1002
#define IDC_EDIT_PHONEME				1004
#define IDC_EDIT_INSERTPHONEMEBEFORE	1005
#define IDC_EDIT_INSERTPHONEMEAFTER		1006
#define IDC_EDIT_DELETEPHONEME			1007

#define IDC_PLAY_EDITED_SELECTION		1008

#define IDC_REDO_PHONEMEEXTRACTION		1009
#define IDC_REDO_PHONEMEEXTRACTION_SELECTION 1010
#define IDC_DESELECT					1011
#define IDC_PLAY_EDITED					1012
#define IDC_SAVE_LINGUISTIC				1013
#define IDC_CANCELPLAYBACK				1014

#define IDC_EDITWORDLIST				1015
#define IDC_SNAPWORDS					1016
#define IDC_SEPARATEWORDS				1017
#define IDC_LOADWAVEFILE				1018
#define IDC_SNAPPHONEMES				1019
#define IDC_SEPARATEPHONEMES			1020

#define IDC_COMMITEXTRACTED				1021
#define IDC_CLEAREXTRACTED				1022

#define IDC_ADDTAG						1023
#define IDC_DELETETAG					1024

#define IDC_CVUNDO						1025
#define IDC_CVREDO						1026

#define IDC_EDIT_DELETEWORD				1027
#define IDC_EDIT_WORD					1028
#define IDC_EDIT_INSERTWORDBEFORE		1029
#define IDC_EDIT_INSERTWORDAFTER		1030
#define IDC_EDIT_INSERTFIRSTPHONEMEOFWORD 1031

#define IDC_SELECT_WORDSRIGHT			1032
#define IDC_SELECT_WORDSLEFT			1033
#define IDC_SELECT_PHONEMESRIGHT		1034
#define IDC_SELECT_PHONEMESLEFT			1035

#define IDC_DESELECT_PHONEMESANDWORDS	1036
#define IDC_CLEANUP						1037
#define IDC_CLEARUNDO					1038

#define IDC_PLAYBUTTON					1039

#define IDC_MODE_TAB					1040

#define IDC_EMPHASIS_DELETE				1041
#define IDC_EMPHASIS_DESELECT			1042
#define IDC_EMPHASIS_SELECTALL			1043

#define IDC_PLAYBACKRATE				1044

#define IDC_REALIGNPHONEMES				1045
#define IDC_REALIGNWORDS				1046

// Support for multiple speech api's
#define IDC_API_SAPI					1050
#define IDC_API_LIPSINC					1051

#define IDC_EXPORT_SENTENCE				1075
#define IDC_IMPORT_SENTENCE				1076
#define IDC_TOGGLE_VOICEDUCK			1077

#define		IDC_PE_LANGUAGESTART		1100
// #define  IDC_PE_LANGUAGEEND			1106 or so

class IterateOutputRIFF;
class IterateRIFF;
class CChoreoWidgetDrawHelper;
class CChoreoEvent;
class CEventRelativeTag;
class CChoreoView;
class IPhonemeExtractor;
class CPhonemeModeTab;
class mxPopupMenu;

#include "sentence.h"

enum
{
	COLOR_PHONEME_BACKGROUND = 0,
	COLOR_PHONEME_TEXT,
	COLOR_PHONEME_LIGHTTEXT,
	COLOR_PHONEME_PLAYBACKTICK,
	COLOR_PHONEME_WAVDATA,
	COLOR_PHONEME_TIMELINE,
	COLOR_PHONEME_TIMELINE_MAJORTICK,
	COLOR_PHONEME_TIMELINE_MINORTICK,
	COLOR_PHONEME_EXTRACTION_RESULT_FAIL,
	COLOR_PHONEME_EXTRACTION_RESULT_SUCCESS,
	COLOR_PHONEME_EXTRACTION_RESULT_ERROR,
	COLOR_PHONEME_EXTRACTION_RESULT_OTHER,
	COLOR_PHONEME_TAG_BORDER,
	COLOR_PHONEME_TAG_BORDER_SELECTED,
	COLOR_PHONEME_TAG_FILLER_NORMAL,
	COLOR_PHONEME_TAG_SELECTED,
	COLOR_PHONEME_TAG_TEXT,
	COLOR_PHONEME_TAG_TEXT_SELECTED,	
	COLOR_PHONEME_WAV_ENDPOINT,		
	COLOR_PHONEME_AB,
	COLOR_PHONEME_AB_LINE,
	COLOR_PHONEME_AB_TEXT,

	COLOR_PHONEME_ACTIVE_BORDER,
	COLOR_PHONEME_SELECTED_BORDER,
	COLOR_PHONEME_TIMING_TAG,

	COLOR_PHONEME_EMPHASIS_BG,
	COLOR_PHONEME_EMPHASIS_BG_STRONG,
	COLOR_PHONEME_EMPHASIS_BG_WEAK,

	COLOR_PHONEME_EMPHASIS_BORDER,
	COLOR_PHONEME_EMPHASIS_LINECOLOR,
	COLOR_PHONEME_EMPHASIS_DOTCOLOR,
	COLOR_PHONEME_EMPHASIS_DOTCOLOR_SELECTED,
	COLOR_PHONEME_EMPHASIS_TEXT,
	COLOR_PHONEME_EMPHASIS_MIDLINE,

	NUM_COLORS,
};

//-----------------------------------------------------------------------------
// Purpose: Shows WAV data and allows blanking it out and tweaking phoneme tags
//-----------------------------------------------------------------------------
class PhonemeEditor : public mxWindow, public IFacePoserToolWindow
{
public:
	enum
	{
		BOUNDARY_NONE = 0,
		BOUNDARY_PHONEME,
		BOUNDARY_WORD,
	};

	typedef enum
	{
		MODE_PHONEMES = 0,
		MODE_EMPHASIS
	} EditorMode;

	// Construction
						PhonemeEditor( mxWindow *parent );
						~PhonemeEditor( void );

	virtual void		Think( float dt );

	virtual void		OnDelete();
	virtual bool		CanClose();

	void				ValidateSpeechAPIIndex();

	virtual int			handleEvent( mxEvent *event );
	virtual void		redraw( void );
	virtual bool		PaintBackground( void );

	EditorMode			GetMode( void ) const;
	void				SetupPhonemeEditorColors( void );
	COLORREF			PEColor( int colornum );
	void				OnModeChanged( void );

	// Change wave file being edited
	void				SetCurrentWaveFile( const char *wavefile, bool force = false, CChoreoEvent *event = NULL );

	// called when scene is unloaded in choreview or when event/channel/actor gets deleted
	// so we don't have dangling pointers to tags, events, scene
	void				ClearEvent( void );

	void				Play();

private:
	void				DrawWords( CChoreoWidgetDrawHelper& drawHelper, RECT& rcWorkSpace, CSentence &sentence, int type, bool showactive = true );
	void				DrawPhonemes( CChoreoWidgetDrawHelper& drawHelper, RECT& rcWorkSpace, CSentence &sentence, int type, bool showactive = true );
	void				DrawRelativeTags( CChoreoWidgetDrawHelper& drawHelper, RECT& rc );

	void				Emphasis_Redraw( CChoreoWidgetDrawHelper& drawHelper, RECT& rcWorkSpace );
	void				Emphasis_GetRect( RECT const & rcWorkSpace, RECT& rcEmphasis );

	void				Emphasis_Init( void );
	CEmphasisSample		*Emphasis_GetSampleUnderMouse( mxEvent *event );
	void				Emphasis_DeselectAll( void );
	void				Emphasis_SelectAll( void );
	void				Emphasis_Delete( void );
	void				Emphasis_AddSample( CEmphasisSample const& sample );
	void				Emphasis_CountSelected( void );
	void				Emphasis_ShowContextMenu( mxEvent *event );
	void				Emphasis_MouseDrag( int x, int y );
	bool				Emphasis_IsValid( void );
	void				Emphasis_SelectPoints( void );
	
	// Data
	int					m_nNumSelected;

	// Readjust slider
	void				MoveTimeSliderToPos( int x );

	// Handle scrollbar
	void				SetTimeZoomScale( int scale );
	float				GetTimeZoomScale( void );
	float				GetPixelsPerSecond( void );
	// Adjust scroll bars
	void				RepositionHSlider( void );

	// Edit commands
	void				EditPhoneme( CPhonemeTag *pPhoneme, bool positionDialog = false );
	void				EditPhoneme( void );
	void				EditInsertPhonemeBefore( void );
	void				EditInsertPhonemeAfter( void );
	void				EditDeletePhoneme( void );

	void				SelectPhonemes( bool forward );

	void				EditInsertFirstPhonemeOfWord( void );

	void				EditWord( CWordTag *pWord, bool positionDialog = false );
	void				EditWord( void );
	void				EditInsertWordBefore( void );
	void				EditInsertWordAfter( void );
	void				EditDeleteWord( void );

	void				SelectWords( bool forward );

	// Edit word list
	void				EditWordList( void );
	void				SentenceFromString( CSentence& sentence, char const *str );


	// Wav processing commands
	void				RedoPhonemeExtraction( void );
	// Redo extraction of selected words only
	void				RedoPhonemeExtractionSelected( void );
	void				Deselect( void );

	void				PlayEditedWave( bool selection = false );
	void				CommitChanges( void );

	// Context menu
	void				ShowPhonemeMenu( CPhonemeTag *pho, int mx, int my );
	void				ShowWordMenu( CWordTag *word, int mx, int my );

	void				ShowContextMenu( int mx, int my );
	void				ShowContextMenu_Phonemes( int mx, int my );
	void				ShowContextMenu_Emphasis( int mx, int my );

	// UI helpers
	void				GetWorkspaceRect( RECT &rc );


	bool				IsMouseOverWordRow( int my );
	bool				IsMouseOverPhonemeRow( int my );

	int					IsMouseOverBoundary( mxEvent *event );
	int					GetWordUnderMouse( int mx, int my );
	int					ComputeHPixelsNeeded( void );
	void				DrawFocusRect( char *reason );
	void				StartDragging( int dragtype, int startx, int starty, HCURSOR cursor );

	void				FinishPhonemeMove( int startx, int endx );
	void				FinishPhonemeDrag( int startx, int endx );
	void				FinishWordMove( int startx, int endx );
	void				FinishWordDrag( int startx, int endx );

	float				GetTimeForPixel( int mx );
	void				GetScreenStartAndEndTime( float &starttime, float& endtime );
	float				GetTimePerPixel( void );
	int					GetSampleForMouse( int mx );
	int					GetPixelForSample( int sample );

	bool				FindSpanningPhonemes( float time, CPhonemeTag **pp1, CPhonemeTag **pp2 );
	bool				FindSpanningWords( float time, CWordTag **pp1, CWordTag **pp2 );
	int					FindWordForTime( float time );
	CPhonemeTag			*FindPhonemeForTime( float time );
	void				DeselectWords( void );
	void				SnapWords( void );
	void				SeparateWords( void );

	void				DeselectPhonemes( void );
	void				SnapPhonemes( void );
	void				SeparatePhonemes( void );

	void				CreateEvenWordDistribution( const char *wordlist );

	// Dirty flag
	void				SetDirty( bool dirty, bool clearundo = true );
	bool				GetDirty( void );

	// FIXME:  Do something else here
	void				ResampleChunk( IterateOutputRIFF& store, void *format, int chunkname, char *buffer, int buffersize, int start_silence = 0, int end_silence = 0 );

	// Mouse control over selected samples
	void				SelectSamples( int start, int end );
	void				FinishSelect( int startx, int mx );
	void				FinishMoveSelection( int startx, int mx );
	void				FinishMoveSelectionStart( int startx, int mx );
	void				FinishMoveSelectionEnd( int startx, int mx );

	bool				IsMouseOverSamples( int mx, int my );
	bool				IsMouseOverSelection( int mx, int my );
	bool				IsMouseOverSelectionStartEdge( mxEvent *event );
	bool				IsMouseOverSelectionEndEdge( mxEvent *event );

	bool				IsMouseOverTag( int mx, int my );
	void				FinishEventTagDrag( int startx, int endx );
	CEventRelativeTag	*GetTagUnderMouse( int mx );
	bool				IsMouseOverTagRow( int my );
	void				ShowTagMenu( int mx, int my );
	void				AddTag( void );
	void				DeleteTag( void );

	// After running liset/sapi, retrieve phoneme tag data from stream
	void				RetrieveLinguisticData( void );

	// Copy current phoneme chunk over existing data chunk of .wav file
	void				SaveLinguisticData( void );
	void				StoreValveDataChunk( IterateOutputRIFF& store );

	void				ExportValveDataChunk( char const *tempfile );
	void				ImportValveDataChunk( char const *tempfile );

	void				OnImport();
	void				OnExport();

	// Playback (returns true if sound had been playing)
	bool				StopPlayback( void );

	CPhonemeTag			*GetPhonemeTagUnderMouse( int mx, int my );
	CWordTag			*GetWordTagUnderMouse( int mx, int my );

	void				ReadLinguisticTags( void );

	void				LoadWaveFile( void );

	void				GetPhonemeTrayTopBottom( RECT& rc );
	void				GetWordTrayTopBottom( RECT& rc );

	void				GetWordRect( const CWordTag *tag, RECT& rc );
	void				GetPhonemeRect( const CPhonemeTag *tag, RECT& rc );
	int					GetMouseForTime( float time );

	void				CommitExtracted( void );
	void				ClearExtracted( void );

	const char *		GetExtractionResultString( int resultCode );

	void				AddFocusRect( RECT& rc );

	void				CountSelected( void );

	typedef void (PhonemeEditor::*PEWORDITERFUNC)( CWordTag *word, float fparam );
	typedef void (PhonemeEditor::*PEPHONEMEITERFUNC)( CPhonemeTag *phoneme, CWordTag *word, float fparam );

	void				TraverseWords( PEWORDITERFUNC pfn, float fparam );
	void				TraversePhonemes( PEPHONEMEITERFUNC pfn, float fparam );

	// Iteration functions
	void				ITER_MoveSelectedWords( CWordTag *word, float amount );
	void				ITER_MoveSelectedPhonemes( CPhonemeTag *phoneme, CWordTag *word, float amount );

	void				ITER_ExtendSelectedPhonemeEndTimes( CPhonemeTag *phoneme, CWordTag *word, float amount );
	void				ITER_ExtendSelectedWordEndTimes( CWordTag *word, float amount );

	void				ITER_AddFocusRectSelectedWords( CWordTag *word, float amount );
	void				ITER_AddFocusRectSelectedPhonemes( CPhonemeTag *phoneme, CWordTag *word, float amount );

	void				ITER_CountSelectedWords( CWordTag *word, float amount );
	void				ITER_CountSelectedPhonemes( CPhonemeTag *phoneme, CWordTag *word, float amount );

	void				ITER_SelectSpanningWords( CWordTag *word, float amount );

// Undo/Redo
	void				Undo( void );
	void				Redo( void );
	void				ClearUndo( void );

	// Do push before changes
	void				PushUndo( void );
	// Do this push after changes, must match pushundo 1for1
	void				PushRedo( void );

	void				WipeUndo( void );
	void				WipeRedo( void );

	CPhonemeTag			*GetClickedPhoneme( void );
	CWordTag			*GetClickedWord( void );
	void				SetClickedPhoneme( int word, int phoneme );

	void				ShiftSelectedPhoneme( int direction );
	void				ExtendSelectedPhonemeEndTime( int direction );
	void				SelectNextPhoneme( int direction );
	void				SelectNextWord( int direction );
	bool				IsPhonemeSelected( CWordTag *word );
	void				ShiftSelectedWord( int direction );
	void				ExtendSelectedWordEndTime( int direction );

	float				GetTimeGapToNextWord( bool forward, CWordTag *currentWord, CWordTag **ppNextWord = NULL );
	float				GetTimeGapToNextPhoneme( bool forward, CPhonemeTag *currentPhoneme, CWordTag **ppword = NULL, CPhonemeTag **phoneme = NULL );
	int					IndexOfWord( CWordTag *word );
	CPhonemeTag			*GetSelectedPhoneme( void );
	CWordTag			*GetSelectedWord( void );

	void				OnMouseMove( mxEvent *event );

	bool				AreSelectedWordsContiguous( void );
	bool				AreSelectedPhonemesContiguous( void );

	bool				CreateCroppedWave( char const *filename, int startsample, int endsample );
	void				CleanupWordsAndPhonemes( bool prepareundo );
	void				RealignPhonemesToWords( bool prepareundo );
	void				RealignWordsToPhonemes( bool prepareundo );
	void				SortWords( bool prepareundo );
	void				SortPhonemes( bool prepareundo );

	float				ComputeMaxWordShift( bool forward, bool allowcrop );
	float				ComputeMaxPhonemeShift( bool forward, bool allowcrop );

	int					PixelsForDeltaTime( float dt );

	void				ClearDragLimit( void );
	void				SetDragLimit( int dragtype );
	void				LimitDrag( int& mousex );

	void				SelectExpression( CPhonemeTag *tag );

	void				OnSAPI( void );
	void				OnLipSinc( void );

	bool				CheckSpeechAPI( void );
	char const			*GetSpeechAPIName( void );

	void				LoadPhonemeConverters();
	void				UnloadPhonemeConverters();

	bool				IsMouseOverScrubHandle( mxEvent *event );
	bool				IsMouseOverScrubArea( mxEvent *event );
	void				GetScrubHandleRect( RECT& rcHandle, bool clipped = false );
	void				GetScrubAreaRect( RECT& rcArea );
	void				DrawScrubHandle();

	void				DrawScrubHandle( CChoreoWidgetDrawHelper& drawHelper );
	void				ScrubThink( float dt, bool scrubbing );

	void				SetScrubTime( float t );
	void				SetScrubTargetTime( float t );

	float				GetTimeForSample( int sample );
	void				ClampTimeToSelectionInterval( float& timeval );
	void				OnToggleVoiceDuck();

	// Data
private:
	// Type of mouse movement
	enum
	{
		DRAGTYPE_NONE = 0,
		DRAGTYPE_PHONEME ,
		DRAGTYPE_WORD,
		DRAGTYPE_SELECTSAMPLES,
		DRAGTYPE_MOVESELECTION,
		DRAGTYPE_MOVESELECTIONSTART,
		DRAGTYPE_MOVESELECTIONEND,
		DRAGTYPE_MOVEWORD,
		DRAGTYPE_MOVEPHONEME,
		DRAGTYPE_EVENTTAG_MOVE,
		DRAGTYPE_EMPHASIS_SELECT,
		DRAGTYPE_EMPHASIS_MOVE,
		DRAGTYPE_SCRUBBER
	};

	float				m_flScrub;
	float				m_flScrubTarget;

	EditorMode			m_CurrentMode;
	// Graph scale
	float				m_flPixelsPerSecond;
	// Graph scale
	int					m_nTimeZoom;
	int					m_nTimeZoomStep;

	int					m_nTickHeight;

	// Current wave file
	CAudioSource		*m_pWaveFile;
	CAudioMixer			*m_pMixer;
	CChoreoEvent		*m_pEvent;
	int					m_nClickX;

	struct CWorkFile
	{
	public:
		char				m_szWaveFile[ 256 ];
		char				m_szWorkingFile[ 256 ];
		char				m_szBasePath[ 256 ];
		bool				m_bDirty;
	};
	CWorkFile			m_WorkFile;

	mxScrollbar			*m_pHorzScrollBar;
	// Current sb value
	int					m_nLeftOffset;

	CPhonemeModeTab		*m_pModeTab;

	mxSlider			*m_pPlaybackRate;
	float				m_flPlaybackRate;

	mxButton			*m_btnRedoPhonemeExtraction;
	mxButton			*m_btnSave;
	mxButton			*m_btnLoad;

	mxButton			*m_btnPlay; // selection or full depending

	// Mouse dragging
	HCURSOR				m_hPrevCursor;

	int					m_nStartX;
	int					m_nStartY;
	int					m_nLastX;
	int					m_nLastY;
	int					m_nDragType;
	struct CFocusRect
	{
		RECT	m_rcOrig;
		RECT	m_rcFocus;
	};
	CUtlVector < CFocusRect >	m_FocusRects;

	int					m_nClickedPhoneme;
	int					m_nClickedWord;

	// Current set of tags
	CSentence			m_Tags;

	CSentence			m_TagsExt;

	int					m_nSelection[ 2 ];
	bool				m_bSelectionActive;

	int					m_nLastExtractionResult;

	int					m_nSelectedPhonemeCount;
	int					m_nSelectedWordCount;

	bool				m_bWordsActive;

	struct PEUndo
	{
		CSentence *undo;
		CSentence *redo;
	};

	CUtlVector< PEUndo * >	m_UndoStack;
	int					m_nUndoLevel;
	bool				m_bRedoPending;

	bool				m_bLimitDrag;
	int					m_nLeftLimit;
	int					m_nRightLimit;

	IPhonemeExtractor	*m_pPhonemeExtractor;
	float				m_flScrubberTimeOffset;
};

extern PhonemeEditor	*g_pPhonemeEditor;

#endif // PHONEEDITOR_H
