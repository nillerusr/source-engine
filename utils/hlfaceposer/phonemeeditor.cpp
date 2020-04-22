//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include <Assert.h>
#include <stdio.h>
#include <math.h>
#include "hlfaceposer.h"
#include "PhonemeEditor.h"
#include "PhonemeEditorColors.h"
#include "snd_audio_source.h"
#include "snd_wave_source.h"
#include "ifaceposersound.h"
#include "choreowidgetdrawhelper.h"
#include "mxBitmapButton.h"
#include "phonemeproperties.h"
#include "tier2/riff.h"
#include "StudioModel.h"
#include "expressions.h"
#include "expclass.h"
#include "InputProperties.h"
#include "phonemeextractor/PhonemeExtractor.h"
#include "PhonemeConverter.h"
#include "choreoevent.h"
#include "choreoscene.h"
#include "ChoreoView.h"
#include "filesystem.h"
#include "UtlBuffer.h"
#include "AudioWaveOutput.h"
#include "StudioModel.h"
#include "viewerSettings.h"
#include "ControlPanel.h"
#include "faceposer_models.h"
#include "tier1/strtools.h"
#include "tabwindow.h"
#include "MatSysWin.h"
#include "soundflags.h"
#include "mdlviewer.h"
#include "filesystem_init.h"
#include "WaveBrowser.h"
#include "tier2/p4helpers.h"
#include "vstdlib/random.h"

extern IUniformRandomStream *random;

float SnapTime( float input, float granularity );

#define MODE_TAB_OFFSET 20

// 10x magnification
#define MAX_TIME_ZOOM 1000
// 10% per step
#define TIME_ZOOM_STEP 2

#define SCRUBBER_HEIGHT	15

#define TAG_TOP ( 25 + SCRUBBER_HEIGHT )
#define TAG_BOTTOM ( TAG_TOP + 20 )

#define PLENTY_OF_TIME 99999.9
#define MINIMUM_WORD_GAP 0.02f
#define MINIMUM_PHONEME_GAP 0.01f
#define DEFAULT_WORD_LENGTH 0.25f
#define DEFAULT_PHONEME_LENGTH 0.1f

#define WORD_DATA_EXTENSION	".txt"

// #define ITEM_GAP_EPSILON 0.0025f
struct PhonemeEditorColor
{
	int				color_number; // For readability
	int				mode_number; // -1 for all
	COLORREF		root_color;
	COLORREF		gray_color;  // if mode is wrong...
};

static PhonemeEditorColor g_PEColors[ NUM_COLORS ] =
{
	{ COLOR_PHONEME_BACKGROUND,					-1, RGB( 240, 240, 220 ) },
	{ COLOR_PHONEME_TEXT,						-1,	RGB( 63, 63, 63 ) },
	{ COLOR_PHONEME_LIGHTTEXT,					0,	RGB( 180, 180, 120 ) },
	{ COLOR_PHONEME_PLAYBACKTICK,				0,	RGB( 255, 0, 0 ) },
	{ COLOR_PHONEME_WAVDATA,					0,	RGB( 128, 31, 63 ) },
	{ COLOR_PHONEME_TIMELINE,					0,	RGB( 31, 31, 127 ) },
	{ COLOR_PHONEME_TIMELINE_MAJORTICK,			0,	RGB( 200, 200, 255 ) },
	{ COLOR_PHONEME_TIMELINE_MINORTICK,			0,	RGB( 210, 210, 240 ) },
	{ COLOR_PHONEME_EXTRACTION_RESULT_FAIL,		0,	RGB( 180, 180, 0 ) },
	{ COLOR_PHONEME_EXTRACTION_RESULT_SUCCESS,	0,	RGB( 100, 180, 100 ) },
	{ COLOR_PHONEME_EXTRACTION_RESULT_ERROR,	0,	RGB( 255, 31, 31 ) },
	{ COLOR_PHONEME_EXTRACTION_RESULT_OTHER,	0,	RGB( 63, 63, 63 ) },
	{ COLOR_PHONEME_TAG_BORDER,					0,	RGB( 160, 100, 100 ) },
	{ COLOR_PHONEME_TAG_BORDER_SELECTED,		0,	RGB( 255, 40, 60 ) },
	{ COLOR_PHONEME_TAG_FILLER_NORMAL,			0,	RGB( 210, 210, 190 ) },
	{ COLOR_PHONEME_TAG_SELECTED,				0,	RGB( 200, 130, 130 ) },
	{ COLOR_PHONEME_TAG_TEXT,					0,	RGB( 63, 63, 63 ) },
	{ COLOR_PHONEME_TAG_TEXT_SELECTED,			0,	RGB( 250, 250, 250 ) },
	{ COLOR_PHONEME_WAV_ENDPOINT,				0,	RGB( 0, 0, 200 ) },
	{ COLOR_PHONEME_AB,							0,	RGB( 63, 190, 210 ) },
	{ COLOR_PHONEME_AB_LINE,					0,	RGB( 31, 150, 180 ) },
	{ COLOR_PHONEME_AB_TEXT,					0,	RGB( 100, 120, 120 ) },
	{ COLOR_PHONEME_ACTIVE_BORDER,				0,	RGB( 150, 240, 180 ) },
	{ COLOR_PHONEME_SELECTED_BORDER,			0,	RGB( 255, 0, 0 ) },
	{ COLOR_PHONEME_TIMING_TAG,					-1,	RGB( 0, 100, 200 ) },

	{ COLOR_PHONEME_EMPHASIS_BG,				1,	RGB( 230, 230, 200 ) },
	{ COLOR_PHONEME_EMPHASIS_BG_STRONG,			1,	RGB( 163, 201, 239 ) },
	{ COLOR_PHONEME_EMPHASIS_BG_WEAK,			1,	RGB( 237, 239, 163 ) },
	{ COLOR_PHONEME_EMPHASIS_BORDER,			1,	RGB( 200, 200, 200 ) },
	{ COLOR_PHONEME_EMPHASIS_LINECOLOR,			1,	RGB( 0, 0, 255 ) },
	{ COLOR_PHONEME_EMPHASIS_DOTCOLOR,			1,	RGB( 0, 0, 255 ) },
	{ COLOR_PHONEME_EMPHASIS_DOTCOLOR_SELECTED,	1,	RGB( 240, 80, 20 ) },
	{ COLOR_PHONEME_EMPHASIS_TEXT,				1,	RGB( 0, 0, 0 ) },
	{ COLOR_PHONEME_EMPHASIS_MIDLINE,			1,	RGB( 100, 150, 200 ) },
};

struct Extractor
{
	PE_APITYPE			apitype;
	CSysModule			*module;
	IPhonemeExtractor	*extractor;
};

CUtlVector< Extractor >	g_Extractors;


bool DoesExtractorExistFor( PE_APITYPE type )
{
	for ( int i=0; i < g_Extractors.Count(); i++ )
	{
		if ( g_Extractors[i].apitype == type )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Implements the RIFF i/o interface on stdio
//-----------------------------------------------------------------------------
class StdIOReadBinary : public IFileReadBinary
{
public:
	int open( const char *pFileName )
	{
		return (int)filesystem->Open( pFileName, "rb" );
	}

	int read( void *pOutput, int size, int file )
	{
		if ( !file )
			return 0;

		return filesystem->Read( pOutput, size, (FileHandle_t)file );
	}

	void seek( int file, int pos )
	{
		if ( !file )
			return;

		filesystem->Seek( (FileHandle_t)file, pos, FILESYSTEM_SEEK_HEAD );
	}

	unsigned int tell( int file )
	{
		if ( !file )
			return 0;

		return filesystem->Tell( (FileHandle_t)file );
	}

	unsigned int size( int file )
	{
		if ( !file )
			return 0;

		return filesystem->Size( (FileHandle_t)file );
	}

	void close( int file )
	{
		if ( !file )
			return;

		filesystem->Close( (FileHandle_t)file );
	}
};

class StdIOWriteBinary : public IFileWriteBinary
{
public:
	int create( const char *pFileName )
	{
		MakeFileWriteable( pFileName );
		return (int)filesystem->Open( pFileName, "wb" );
	}

	int write( void *pData, int size, int file )
	{
		return filesystem->Write( pData, size, (FileHandle_t)file );
	}

	void close( int file )
	{
		filesystem->Close( (FileHandle_t)file );
	}

	void seek( int file, int pos )
	{
		filesystem->Seek( (FileHandle_t)file, pos, FILESYSTEM_SEEK_HEAD );
	}

	unsigned int tell( int file )
	{
		return filesystem->Tell( (FileHandle_t)file );
	}
};

// Interface objects
static StdIOWriteBinary io_out;
static StdIOReadBinary io_in;

class CPhonemeModeTab : public CTabWindow
{
public:
	typedef CTabWindow BaseClass;

	CPhonemeModeTab( mxWindow *parent, int x, int y, int w, int h, int id = 0, int style = 0 ) :
		CTabWindow( parent, x, y, w, h, id, style )
	{
		SetInverted( true );
	}

	virtual void ShowRightClickMenu( int mx, int my )
	{
		// Nothing
	}

	void	Init( void )
	{
		add( "Phonemes" );
		add( "Emphasis" );
		select( 0 );
	}
};

PhonemeEditor * g_pPhonemeEditor = 0;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
PhonemeEditor::PhonemeEditor( mxWindow *parent ) : 
	IFacePoserToolWindow( "PhonemeEditor", "Phoneme Editor" ), 
	mxWindow( parent, 0, 0, 0, 0 )
{
	SetAutoProcess( false );

	m_flPlaybackRate	= 1.0f;

	m_flScrub			= 0.0f;
	m_flScrubTarget		= 0.0f;

	m_CurrentMode = MODE_PHONEMES;
	Emphasis_Init();
	SetupPhonemeEditorColors();

	m_bRedoPending = false;
	m_nUndoLevel = 0;

	m_bWordsActive = false;

	m_pWaveFile = NULL;
	m_pMixer	= NULL;
	m_pEvent	= NULL;
	m_nClickX = 0;

	m_WorkFile.m_bDirty = false;
	m_WorkFile.m_szWaveFile[ 0 ] = 0;
	m_WorkFile.m_szWorkingFile[ 0 ] = 0;
	m_WorkFile.m_szBasePath[ 0 ] = 0;

	m_nTickHeight = 20;

	m_flPixelsPerSecond = 500.0f;
	m_nTimeZoom = 100;
	m_nTimeZoomStep = TIME_ZOOM_STEP;

	m_pHorzScrollBar = new mxScrollbar( this, 0, 0, 18, 100, IDC_PHONEME_SCROLL, mxScrollbar::Horizontal );


	m_hPrevCursor		= 0;
	m_nStartX			= 0;
	m_nStartY			= 0;
	m_nLastX			= 0;
	m_nLastY			= 0;
	m_nDragType			= DRAGTYPE_NONE;

	SetClickedPhoneme( -1, -1 );

	m_nSelection[ 0 ] = m_nSelection[ 1 ] = 0;
	m_bSelectionActive = false;

	m_nSelectedPhonemeCount = 0;
	m_nSelectedWordCount = 0;

	m_btnSave					= new mxButton( this, 0, 0, 16, 16, "Save (Ctrl+S)", IDC_SAVE_LINGUISTIC );
	m_btnRedoPhonemeExtraction	= new mxButton( this, 38, 14, 80, 16, "Re-extract (Ctrl+R)", IDC_REDO_PHONEMEEXTRACTION );

	m_btnLoad					= new mxButton( this, 0, 0, 0, 0, "Load (Ctrl+O)", IDC_LOADWAVEFILE );
	m_btnPlay					= new mxButton( this, 0, 0, 16, 16, "Play (Spacebar)", IDC_PLAYBUTTON );

	m_pPlaybackRate				= new mxSlider( this, 0, 0, 16, 16, IDC_PLAYBACKRATE );
	m_pPlaybackRate->setRange( 0.0, 2.0, 40 );
	m_pPlaybackRate->setValue( m_flPlaybackRate );

	m_pModeTab					= new CPhonemeModeTab( this, 0, 0, 500, 20, IDC_MODE_TAB );
	m_pModeTab->Init();

	m_nLastExtractionResult		= SR_RESULT_NORESULT;

	ClearDragLimit();

	SetSuffix( " - Normal" );
	m_flScrubberTimeOffset = 0.0f;

	LoadPhonemeConverters();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::OnDelete()
{
	if ( m_pWaveFile )
	{
		char fn[ 512 ];
		Q_snprintf( fn, sizeof( fn ), "%s%s", m_WorkFile.m_szBasePath, m_WorkFile.m_szWorkingFile );
		filesystem->RemoveFile( fn, "GAME" );
	}

	delete m_pWaveFile;
	m_pWaveFile = NULL;

	m_Tags.Reset();
	m_TagsExt.Reset();

	UnloadPhonemeConverters();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhonemeEditor::CanClose()
{
	if ( !GetDirty() )
		return true;

	int retval = mxMessageBox( this, va( "Save current changes to %s", m_WorkFile.m_szWaveFile ),
		"Phoneme Editor", MX_MB_QUESTION | MX_MB_YESNOCANCEL );

	// Cancel
	if ( retval == 2 )
	{
		return false;
	}

	// Yes
	if ( retval == 0 )
	{
		CommitChanges();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
PhonemeEditor::~PhonemeEditor( void )
{
}

void PhonemeEditor::SetupPhonemeEditorColors( void )
{
	int i;
	for ( i = 0; i < NUM_COLORS; i++ )
	{
		PhonemeEditorColor *p = &g_PEColors[ i ];
		Assert( p->color_number == i );

		if ( p->mode_number == -1 )
		{
			p->gray_color = p->root_color;
		}
		else
		{
			COLORREF bgColor = g_PEColors[ COLOR_PHONEME_BACKGROUND ].root_color;

			int bgr, bgg, bgb;

			bgr = GetRValue( bgColor );
			bgg = GetGValue( bgColor );
			bgb = GetBValue( bgColor );
			
			int r, g, b;

			r = GetRValue( p->root_color );
			g = GetGValue( p->root_color );
			b = GetBValue( p->root_color );

			int avg = ( r + g + b ) / 3;
			int bgavg = ( bgr + bgg + bgb ) / 3;

			// Bias toward bg color
			avg += ( bgavg - avg ) / 2.5;

			p->gray_color = RGB( avg, avg, avg );
		}
	}
}

COLORREF PhonemeEditor::PEColor( int colornum )
{
	COLORREF clr = RGB( 0, 0, 0 );
	if ( colornum < 0 || colornum >= NUM_COLORS )
	{
		Assert( 0 );
		return clr;
	}

	PhonemeEditorColor *p = &g_PEColors[ colornum ];

	if ( p->mode_number == -1 )
	{
		return p->root_color;
	}

	int modenum = (int)GetMode();

	if ( p->mode_number == modenum )
	{
		return p->root_color;
	}
	
	return p->gray_color;
}

void PhonemeEditor::EditWord( CWordTag *pWord, bool positionDialog /*= false*/ )
{
	if ( !pWord )
	{
		Con_Printf( "PhonemeEditor::EditWord:  pWord == NULL\n" );
		return;
	}

	CInputParams params;
	memset( &params, 0, sizeof( params ) );
	strcpy( params.m_szDialogTitle, "Edit Word" );
	strcpy( params.m_szPrompt, "Current Word:" );
	V_strcpy_safe( params.m_szInputText, pWord->GetWord() );

	params.m_nLeft = -1;
	params.m_nTop = -1;

	params.m_bPositionDialog = positionDialog;
	if ( params.m_bPositionDialog )
	{
		RECT rcWord;
		GetWordRect( pWord, rcWord );

		// Convert to screen coords
		POINT pt;
		pt.x = rcWord.left;
		pt.y = rcWord.top;

		ClientToScreen( (HWND)getHandle(), &pt );

		params.m_nLeft	= pt.x;
		params.m_nTop	= pt.y;
	}

	int iret = InputProperties( &params );
	SetFocus( (HWND)getHandle() );
	if ( !iret )
	{
		return;
	}

	// Validate parameters
	if ( CSentence::CountWords( params.m_szInputText ) != 1 )
	{
		Con_ErrorPrintf( "Edit word:  %s has multiple words in it!!!\n", params.m_szInputText );
		return;
	}

	SetFocus( (HWND)getHandle() );

	SetDirty( true );

	PushUndo();

	// Set the word and clear out the phonemes
	// ->m_nPhonemeCode = TextToPhoneme( params.m_szName );
	pWord->SetWord( params.m_szInputText );

	PushRedo();

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPhoneme - 
//			positionDialog - 
//-----------------------------------------------------------------------------
void PhonemeEditor::EditPhoneme( CPhonemeTag *pPhoneme, bool positionDialog /*= false*/ )
{
	if ( !pPhoneme )
	{
		Con_Printf( "PhonemeEditor::EditPhoneme:  pPhoneme == NULL\n" );
		return;
	}

	CPhonemeParams params;
	memset( &params, 0, sizeof( params ) );
	strcpy( params.m_szDialogTitle, "Phoneme/Viseme Properties" );
	V_strcpy_safe( params.m_szName, ConvertPhoneme( pPhoneme->GetPhonemeCode() ) );

	params.m_nLeft = -1;
	params.m_nTop = -1;

	params.m_bPositionDialog = positionDialog;
	if ( params.m_bPositionDialog )
	{
		RECT rcPhoneme;
		GetPhonemeRect( pPhoneme, rcPhoneme );

		// Convert to screen coords
		POINT pt;
		pt.x = rcPhoneme.left;
		pt.y = rcPhoneme.top;

		ClientToScreen( (HWND)getHandle(), &pt );

		params.m_nLeft	= pt.x;
		params.m_nTop	= pt.y;
	}

	int iret = PhonemeProperties( &params );
	SetFocus( (HWND)getHandle() );

	if ( !iret )
	{
		return;
	}

	SetDirty( true );

	PushUndo();

	pPhoneme->SetPhonemeCode( TextToPhoneme( params.m_szName ) );

	PushRedo();

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::EditPhoneme( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CPhonemeTag *pPhoneme = GetClickedPhoneme();
	if ( !pPhoneme )
		return;

	EditPhoneme( pPhoneme, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::EditWord( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CWordTag *pWord = GetClickedWord();
	if ( !pWord )
		return;

	EditWord( pWord, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dragtype - 
//			startx - 
//			cursor - 
//-----------------------------------------------------------------------------
void PhonemeEditor::StartDragging( int dragtype, int startx, int starty, HCURSOR cursor )
{
	m_nDragType = dragtype;
	m_nStartX	= startx;
	m_nLastX	= startx;
	m_nStartY	= starty;
	m_nLastY	= starty;
	
	if ( m_hPrevCursor )
	{
		SetCursor( m_hPrevCursor );
		m_hPrevCursor = NULL;
	}
	m_hPrevCursor = SetCursor( cursor );

	m_FocusRects.Purge();

	RECT rc;
	GetWorkspaceRect( rc );

	RECT rcStart;
	rcStart.left = startx;
	rcStart.right = startx;

	bool addrect = true;
	switch ( dragtype )
	{
	default:
	case DRAGTYPE_SCRUBBER:
		{
			RECT rcScrub;
			GetScrubHandleRect( rcScrub, true );

			rcStart = rcScrub;
			rcStart.left = ( rcScrub.left + rcScrub.right ) / 2;
			rcStart.right = rcStart.left;
			rcStart.bottom = h2() - 18 - MODE_TAB_OFFSET;
		}
		break;
	case DRAGTYPE_EMPHASIS_SELECT:
		{
			RECT rcEmphasis;
			Emphasis_GetRect( rc, rcEmphasis );

			rcStart.top = starty;
			rcStart.bottom = starty;
		}
		break;
	case DRAGTYPE_EMPHASIS_MOVE:
		{
			SetDirty( true );

			PushUndo();

			Emphasis_MouseDrag( startx, starty );
			m_Tags.Resort();

			addrect = false;
		}
		break;
	case DRAGTYPE_SELECTSAMPLES:
	case DRAGTYPE_MOVESELECTIONSTART:
	case DRAGTYPE_MOVESELECTIONEND:
		rcStart.top = rc.top;
		rcStart.bottom = rc.bottom;
		break;
	case DRAGTYPE_MOVESELECTION:
		{
			rcStart.top = rc.top;
			rcStart.bottom = rc.bottom;

			// Compute left/right pixels for selection
			rcStart.left = GetPixelForSample( m_nSelection[ 0 ] );
			rcStart.right = GetPixelForSample( m_nSelection[ 1 ] );
		}
		break;
	case DRAGTYPE_PHONEME:
		{
			GetPhonemeTrayTopBottom( rcStart );
			m_bWordsActive = false;
		}
		break;
	case DRAGTYPE_WORD:
		{
			GetWordTrayTopBottom( rcStart );
			m_bWordsActive = true;
		}
		break;
	case DRAGTYPE_MOVEWORD:
		{
			TraverseWords( &PhonemeEditor::ITER_AddFocusRectSelectedWords, 0.0f );
			addrect = false;
			m_bWordsActive = true;
		}
		break;
	case DRAGTYPE_MOVEPHONEME:
		{
			TraversePhonemes( &PhonemeEditor::ITER_AddFocusRectSelectedPhonemes, 0.0f );
			addrect = false;
			m_bWordsActive = false;
		}
		break;
	case DRAGTYPE_EVENTTAG_MOVE:
		{
			rcStart.top = TAG_TOP;
			rcStart.bottom = TAG_BOTTOM;
			rcStart.left -= 10;
			rcStart.right += 10;
		}
		break;
	}

	if ( addrect )
	{
		AddFocusRect( rcStart );
	}
	
	DrawFocusRect( "start" );

	SetDragLimit( m_nDragType );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
// Output : int
//-----------------------------------------------------------------------------
int PhonemeEditor::handleEvent( mxEvent *event )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	int iret = 0;
	
	if ( HandleToolEvent( event ) )
	{
		return iret;
	}

	switch ( event->event )
	{
	case mxEvent::Action:
		{
			iret = 1;
			switch ( event->action )
			{
			case IDC_EXPORT_SENTENCE:
				{
					OnExport();
				}
				break;
			case IDC_IMPORT_SENTENCE:
				{
					OnImport();
				}
				break;
			case IDC_PLAYBACKRATE:
				{
					m_flPlaybackRate = m_pPlaybackRate->getValue();
					redraw();
				}
				break;
			case IDC_MODE_TAB:
				{
					// The mode changed, so reset stuff here
					EditorMode newMode = (EditorMode)m_pModeTab->getSelectedIndex();
					bool needpaint = ( m_CurrentMode != newMode );
					m_CurrentMode = newMode;
					if ( needpaint )
					{
						switch ( GetMode() )
						{
						default:
						case MODE_PHONEMES:
							SetSuffix( " - Normal" );
							break;
						case MODE_EMPHASIS:
							SetSuffix( " - Emphasis Track" );
							break;
						}

						OnModeChanged();
						redraw();
					}
				}
				break;
			case IDC_EMPHASIS_DELETE:
				Emphasis_Delete();
				break;
			case IDC_EMPHASIS_DESELECT:
				Emphasis_DeselectAll();
				break;
			case IDC_EMPHASIS_SELECTALL:
				Emphasis_SelectAll();
				break;
			case IDC_API_SAPI:
				OnSAPI();
				break;
			case IDC_API_LIPSINC:
				OnLipSinc();
				break;
			case IDC_PLAYBUTTON:
				Play();
				break;
			case IDC_UNDO:
				Undo();
				break;
			case IDC_REDO:
				Redo();
				break;
			case IDC_CLEARUNDO:
				ClearUndo();
				break;
			case IDC_ADDTAG:
				AddTag();
				break;
			case IDC_DELETETAG:
				DeleteTag();
				break;
			case IDC_COMMITEXTRACTED:
				CommitExtracted();
				SetFocus( (HWND)getHandle() );
				break;
			case IDC_CLEAREXTRACTED:
				ClearExtracted();
				break;
			case IDC_SEPARATEPHONEMES:
				SeparatePhonemes();
				break;
			case IDC_SNAPPHONEMES:
				SnapPhonemes();
				break;
			case IDC_SEPARATEWORDS:
				SeparateWords();
				break;
			case IDC_SNAPWORDS:
				SnapWords();
				break;
			case IDC_EDITWORDLIST:
				EditWordList();
				break;
			case IDC_EDIT_PHONEME:
				EditPhoneme();
				break;
			case IDC_EDIT_WORD:
				EditWord();
				break;
			case IDC_EDIT_INSERTPHONEMEBEFORE:
				EditInsertPhonemeBefore();
				break;
			case IDC_EDIT_INSERTPHONEMEAFTER:
				EditInsertPhonemeAfter();
				break;
			case IDC_EDIT_INSERTWORDBEFORE:
				EditInsertWordBefore();
				break;
			case IDC_EDIT_INSERTWORDAFTER:
				EditInsertWordAfter();
				break;
			case IDC_EDIT_DELETEPHONEME:
				EditDeletePhoneme();
				break;
			case IDC_EDIT_DELETEWORD:
				EditDeleteWord();
				break;
			case IDC_EDIT_INSERTFIRSTPHONEMEOFWORD:
				EditInsertFirstPhonemeOfWord();
				break;
			case IDC_PHONEME_PLAY_ORIG:
				{
					StopPlayback();
					if ( m_pWaveFile )
					{
						// Make sure phonemes are loaded
						FacePoser_EnsurePhonemesLoaded();

						sound->PlaySound( m_pWaveFile, VOL_NORM, &m_pMixer );
					}
				}
				break;
			case IDC_PHONEME_SCROLL:
				if (event->modifiers == SB_THUMBTRACK)
				{
					MoveTimeSliderToPos( event->height );
				}
				else if ( event->modifiers == SB_PAGEUP )
				{
					int offset = m_pHorzScrollBar->getValue();
					
					offset -= 10;
					offset = max( offset, m_pHorzScrollBar->getMinValue() );

					MoveTimeSliderToPos( offset );
				}
				else if ( event->modifiers == SB_PAGEDOWN )
				{
					int offset = m_pHorzScrollBar->getValue();
					
					offset += 10;
					offset = min( offset, m_pHorzScrollBar->getMaxValue() );

					MoveTimeSliderToPos( offset );
				}
				break;	
			case IDC_REDO_PHONEMEEXTRACTION:
				if ( m_Tags.m_Words.Size() <= 0 )
				{
					// This calls redo LISET if some words are actually entered
					EditWordList();
				}
				else
				{
					RedoPhonemeExtraction();
				}
				SetFocus( (HWND)getHandle() );
				break;
			case IDC_REDO_PHONEMEEXTRACTION_SELECTION:
				{
					RedoPhonemeExtractionSelected();
				}
				SetFocus( (HWND)getHandle() );
				break;
			case IDC_DESELECT:
				Deselect();
				redraw();
				break;
			case IDC_PLAY_EDITED:
				PlayEditedWave( false );
				SetFocus( (HWND)getHandle() );
				break;
			case IDC_PLAY_EDITED_SELECTION:
				PlayEditedWave( true );
				SetFocus( (HWND)getHandle() );
				break;
			case IDC_SAVE_LINGUISTIC:
				CommitChanges();
				SetFocus( (HWND)getHandle() );
				break;
			case IDC_LOADWAVEFILE:
				LoadWaveFile();
				SetFocus( (HWND)getHandle() );
				break;
			case IDC_CANCELPLAYBACK:
				StopPlayback();
				SetFocus( (HWND)getHandle() );
				break;
			case IDC_SELECT_WORDSRIGHT:
				SelectWords( true );
				break;
			case IDC_SELECT_WORDSLEFT:
				SelectWords( false );
				break;
			case IDC_SELECT_PHONEMESRIGHT:
				SelectPhonemes( true );
				break;
			case IDC_SELECT_PHONEMESLEFT:
				SelectPhonemes( false );
				break;
			case IDC_DESELECT_PHONEMESANDWORDS:
				DeselectPhonemes();
				DeselectWords();
				redraw();
				break;
			case IDC_CLEANUP:
				CleanupWordsAndPhonemes( true );
				redraw();
				break;
			case IDC_REALIGNPHONEMES:
				RealignPhonemesToWords( true );
				redraw();
				break;
			case IDC_REALIGNWORDS:
				RealignWordsToPhonemes( true );
				redraw();
				break;
			case IDC_TOGGLE_VOICEDUCK:
				OnToggleVoiceDuck();
				break;
			}

			if ( iret == 1 )
			{
				SetActiveTool( this );
				SetFocus( (HWND)getHandle() );
			}
		}
		break;
	case mxEvent::MouseWheeled:
		{
			// Zoom time in  / out
			if ( event->height > 0 )
			{
				m_nTimeZoom = min( m_nTimeZoom + m_nTimeZoomStep, MAX_TIME_ZOOM );
			}
			else
			{
				m_nTimeZoom = max( m_nTimeZoom - m_nTimeZoomStep, m_nTimeZoomStep );
			}
			RepositionHSlider();
			iret = 1;
		}
		break;
	case mxEvent::Size:
		{
			int bw	= 100;
			int x	= 5;
			int by = h2() - 18 - MODE_TAB_OFFSET;

			m_pModeTab->setBounds( 0, h2() - MODE_TAB_OFFSET, w2(), MODE_TAB_OFFSET );

			m_btnRedoPhonemeExtraction->setBounds( x, by, bw, 16 );
			x += bw;
			m_btnSave->setBounds( x, by, bw, 16 );
			x += bw;
			m_btnLoad->setBounds( x, by, bw, 16 );
			x += bw;
			m_btnPlay->setBounds( x, by, bw, 16 );
			x += bw;

			m_pPlaybackRate->setBounds( x, by, 100, 16 );

			RepositionHSlider();
			iret = 1;
		}
		break;
	case mxEvent::MouseDown:
		{
			iret = 1;

			CPhonemeTag *pt;
			CWordTag *wt;
			
			pt = GetPhonemeTagUnderMouse( (short)event->x, (short)event->y );
			wt = GetWordTagUnderMouse( (short)event->x, (short)event->y );

			bool ctrldown = ( event->modifiers & mxEvent::KeyCtrl ) ? true : false;
			bool shiftdown = ( event->modifiers & mxEvent::KeyShift ) ? true : false;
			
			if ( event->buttons & mxEvent::MouseRightButton )
			{
				RECT rc;
				GetWorkspaceRect( rc );

				if ( IsMouseOverWordRow( (short)event->y ) )
				{
					ShowWordMenu( wt,  (short)event->x, (short)event->y );
				}
				else if ( IsMouseOverPhonemeRow( (short)event->y ) )
				{
					ShowPhonemeMenu( pt, (short)event->x, (short)event->y );
				}
				else if ( IsMouseOverTagRow( (short)event->y ) )
				{
					ShowTagMenu( (short)event->x, (short)event->y );
				}
				else if ( IsMouseOverScrubArea( event ) )
				{
					float t = GetTimeForPixel( (short)event->x );

					ClampTimeToSelectionInterval( t );

					SetScrubTime( t );
					SetScrubTargetTime( t );

					redraw();
				}
				else
				{
					ShowContextMenu( (short)event->x, (short)event->y );
				}
				return iret;
			}
			
			if ( m_nDragType == DRAGTYPE_NONE )
			{
				CountSelected();

				int type = IsMouseOverBoundary( event );

				if ( IsMouseOverScrubArea( event ) )
				{
					if ( IsMouseOverScrubHandle( event ) )
					{
						StartDragging( DRAGTYPE_SCRUBBER, 
							(short)event->x, 
							(short)event->y, 
							LoadCursor( NULL, IDC_SIZEWE ) );

						float t = GetTimeForPixel( (short)event->x );
						m_flScrubberTimeOffset = m_flScrub - t;
						float maxoffset = 0.5f * (float)SCRUBBER_HANDLE_WIDTH / GetPixelsPerSecond();
						m_flScrubberTimeOffset = clamp( m_flScrubberTimeOffset, -maxoffset, maxoffset );
						t += m_flScrubberTimeOffset;
						ClampTimeToSelectionInterval( t );

						SetScrubTime( t );
						SetScrubTargetTime( t );

						DrawScrubHandle();
						iret = true;
					}
					else
					{
						float t = GetTimeForPixel( (short)event->x );

						ClampTimeToSelectionInterval( t );

						SetScrubTargetTime( t );

						iret = true;

					}
					return iret;
				}
				else if ( GetMode() == MODE_EMPHASIS )
				{
					CEmphasisSample *sample = Emphasis_GetSampleUnderMouse( event );
					if ( sample )
					{
						if  ( shiftdown ) 
						{
							sample->selected = !sample->selected;
							redraw();
						}
						else if ( sample->selected )
						{
							StartDragging( DRAGTYPE_EMPHASIS_MOVE, (short)event->x, (short)event->y, LoadCursor( NULL, IDC_SIZEALL ) );
						}
						else
						{
							if  ( !shiftdown ) 
							{
								Emphasis_DeselectAll();
								redraw();
							}

							StartDragging( DRAGTYPE_EMPHASIS_SELECT, (short)event->x, (short)event->y, NULL );
						}
						return true;
					}
					else if ( ctrldown )
					{
						// Add a sample point
						float t = GetTimeForPixel( (short)event->x );

						RECT rcWork;
						GetWorkspaceRect( rcWork );
						RECT rcEmphasis;
						Emphasis_GetRect( rcWork, rcEmphasis );

						int eh = rcEmphasis.bottom - rcEmphasis.top;
						int dy = (short)event->y - rcEmphasis.top;

						CEmphasisSample sample;
						sample.time = t;
						Assert( eh >= 0 );
						sample.value = (float)( dy ) / ( float ) eh;
						sample.value = 1.0f - clamp( sample.value, 0.0f, 1.0f );
						sample.selected = false;

						Emphasis_AddSample( sample );

						redraw();

						return true;
					}
					else
					{
						if  ( !shiftdown ) 
						{
							Emphasis_DeselectAll();
							redraw();
						}

						StartDragging( DRAGTYPE_EMPHASIS_SELECT, (short)event->x, (short)event->y, NULL );
						return true;
					}
				}
				else
				{
					if ( type == BOUNDARY_PHONEME && m_nSelectedPhonemeCount <= 1 )
					{
						StartDragging( DRAGTYPE_PHONEME, (short)event->x, (short)event->y, LoadCursor( NULL, IDC_SIZEWE ) );
						return true;
					}			
					else if ( type == BOUNDARY_WORD && m_nSelectedWordCount <= 1 )
					{
						StartDragging( DRAGTYPE_WORD, (short)event->x, (short)event->y, LoadCursor( NULL, IDC_SIZEWE ) );
						return true;
					}
					else if ( IsMouseOverSamples( (short)event->x, (short)event->y ) )
					{
						if ( !m_bSelectionActive )
						{
							StartDragging( DRAGTYPE_SELECTSAMPLES, (short)event->x, (short)event->y, LoadCursor( NULL, IDC_SIZEWE ) );
						}
						else
						{
							// Either move, move edge if ctrl key is held, or deselect
							if ( IsMouseOverSelection( (short)event->x, (short)event->y ) )
							{
								if ( IsMouseOverSelectionStartEdge( event ) )
								{
									StartDragging( DRAGTYPE_MOVESELECTIONSTART, (short)event->x, (short)event->y, LoadCursor( NULL, IDC_SIZEWE ) );
								}
								else if ( IsMouseOverSelectionEndEdge( event ) )
								{
									StartDragging( DRAGTYPE_MOVESELECTIONEND, (short)event->x, (short)event->y, LoadCursor( NULL, IDC_SIZEWE ) );
								}
								else
								{
									if ( shiftdown )
									{
										StartDragging( DRAGTYPE_MOVESELECTION, (short)event->x, (short)event->y, LoadCursor( NULL, IDC_SIZEALL ) );
									}
								}
							}
							else
							{
								Deselect();
								redraw();
								return iret;
							}
						}
						return true;
					}
				}
				
				if ( IsMouseOverTag( (short)event->x, (short)event->y ) )
				{
					StartDragging( DRAGTYPE_EVENTTAG_MOVE, (short)event->x, (short)event->y, LoadCursor( NULL, IDC_SIZEALL ) );
					return true;
				}
				else
				{
					if ( pt )
					{
						// Can only move when holding down shift key
						if ( shiftdown )
						{
							pt->m_bSelected = true;
							StartDragging( DRAGTYPE_MOVEPHONEME,
								(short)event->x, (short)event->y, LoadCursor( NULL, IDC_SIZEALL ) );
						}
						else
						{
							// toggle the selection
							pt->m_bSelected = !pt->m_bSelected;
						}


						m_bWordsActive = false;

						redraw();
						return true;
					}
					else if ( wt )
					{

						// Can only move when holding down shift key
						if ( shiftdown )
						{
							wt->m_bSelected = true;
							StartDragging( DRAGTYPE_MOVEWORD,
								(short)event->x, (short)event->y, LoadCursor( NULL, IDC_SIZEALL ) );
						}
						else
						{
							// toggle the selection
							wt->m_bSelected = !wt->m_bSelected;
						}

						m_bWordsActive = true;

						redraw();
						return true;
					}
					else if ( type == BOUNDARY_NONE )
					{
						DeselectPhonemes();
						DeselectWords();
						redraw();
						return true;
					}
				}
			}
		}
		break;
	case mxEvent::MouseMove:
	case mxEvent::MouseDrag:
		{
			OnMouseMove( event );
			iret = 1;
		}
		break;
	case mxEvent::MouseUp:
		{
			if ( m_nDragType != DRAGTYPE_NONE )
			{
				int mx = (short)event->x;

				LimitDrag( mx );

				event->x = (short)mx;

				DrawFocusRect( "finish" );

				if ( m_hPrevCursor )
				{
					SetCursor( m_hPrevCursor );
					m_hPrevCursor = 0;
				}

				switch ( m_nDragType )
				{
				case DRAGTYPE_WORD:
					FinishWordMove( m_nStartX, (short)event->x );
					break;
				case DRAGTYPE_PHONEME:
					FinishPhonemeMove( m_nStartX, (short)event->x );
					break;
				case DRAGTYPE_SELECTSAMPLES:
					FinishSelect( m_nStartX, (short)event->x );
					break;
				case DRAGTYPE_MOVESELECTION:
					FinishMoveSelection( m_nStartX, (short)event->x );
					break;
				case DRAGTYPE_MOVESELECTIONSTART:
					FinishMoveSelectionStart( m_nStartX, (short)event->x );
					break;
				case DRAGTYPE_MOVESELECTIONEND:
					FinishMoveSelectionEnd( m_nStartX, (short)event->x );
					break;
				case DRAGTYPE_MOVEWORD:
					FinishWordDrag( m_nStartX, (short)event->x );
					break;
				case DRAGTYPE_MOVEPHONEME:
					FinishPhonemeDrag( m_nStartX, (short)event->x );
					break;
				case DRAGTYPE_EVENTTAG_MOVE:
					FinishEventTagDrag( m_nStartX, (short)event->x );
					break;
				case DRAGTYPE_EMPHASIS_MOVE:
					{
						Emphasis_MouseDrag( (short)event->x, (short)event->y );
						m_Tags.Resort();

						PushRedo();

						redraw();
					}
					break;
				case DRAGTYPE_EMPHASIS_SELECT:
					{
						Emphasis_SelectPoints();
						redraw();
					}
					break;
				case DRAGTYPE_SCRUBBER:
					{
						float t = GetTimeForPixel( (short)event->x );
						t += m_flScrubberTimeOffset;
						m_flScrubberTimeOffset = 0.0f;

						ClampTimeToSelectionInterval( t );

						SetScrubTime( t );
						SetScrubTargetTime( t );

						sound->Flush(); 

						DrawScrubHandle();
					}
					break;
				default:
					break;
				}

				m_nDragType = DRAGTYPE_NONE;
			}
			iret = 1;
		}
		break;
	case mxEvent::KeyUp:
		{
			bool shiftDown = GetAsyncKeyState( VK_SHIFT ) ? true : false;
			bool ctrlDown = GetAsyncKeyState( VK_CONTROL ) ? true : false;

			switch( event->key )
			{
				case VK_TAB:
					{
						int direction = shiftDown ? -1 : 1;
						SelectNextWord( direction );
					}
					break;
				case VK_NEXT:
				case VK_PRIOR:
					{
						m_bWordsActive = event->key == VK_PRIOR ? true : false;
						redraw();
					}
					break;
				case VK_UP:
				case VK_RETURN:
					if ( m_bWordsActive )
					{
						if ( event->key == VK_UP ||
							ctrlDown )
						{
							CountSelected();

							if ( m_nSelectedWordCount == 1 )
							{
								// Find the selected one
								for ( int i = 0; i < m_Tags.m_Words.Size(); i++ )
								{
									CWordTag *word = m_Tags.m_Words[ i ];
									if ( !word || !word->m_bSelected )
										continue;

									EditWord( word, true );
								}
							}
						}
					}
					else
					{
						if ( event->key == VK_UP ||
							ctrlDown )
						{
							CountSelected();

							if ( m_nSelectedPhonemeCount == 1 )
							{
								// Find the selected one
								for ( int i = 0; i < m_Tags.m_Words.Size(); i++ )
								{
									CWordTag *word = m_Tags.m_Words[ i ];
									if ( !word )
										continue;

									for ( int j = 0; j < word->m_Phonemes.Size(); j++ )
									{
										CPhonemeTag *phoneme = word->m_Phonemes[ j ];
										if ( !phoneme )
											continue;

										if ( !phoneme->m_bSelected )
											continue;
								
										EditPhoneme( phoneme, true );
									}
								}
							}
						}
					}
					break;
				case VK_DELETE:
					if ( GetMode() == MODE_EMPHASIS )
					{
						Emphasis_Delete();
					}
					else
					{
						if ( m_bWordsActive )
						{
							EditDeleteWord();
						}
						else
						{
							EditDeletePhoneme();
						}
					}
					break;
				case VK_INSERT:
					if ( m_bWordsActive )
					{
						if ( shiftDown )
						{
							EditInsertWordBefore();
						}
						else
						{
							EditInsertWordAfter();
						}
					}
					else
					{
						if ( shiftDown )
						{
							EditInsertPhonemeBefore();
						}
						else
						{
							EditInsertPhonemeAfter();
						}
					}
					break;
				case VK_SPACE:
					if ( m_pWaveFile && sound->IsSoundPlaying( m_pMixer ) )
					{
						Con_Printf( "Stopping playback\n" );
						m_btnPlay->setLabel( "Play (Spacebar)" );
						StopPlayback();	
					}
					else
					{
						Con_Printf( "Playing .wav\n" );
						m_btnPlay->setLabel( "Stop[ (Spacebar)" );
						PlayEditedWave( m_bSelectionActive );
					}
					break;
				case VK_SHIFT:
				case VK_CONTROL:
					{
						// Force mouse move
						POINT pt;
						GetCursorPos( &pt );
						SetCursorPos( pt.x, pt.y );
						return 0;
					}
					break;
				case VK_ESCAPE:
					{
						// If playing sound, stop it, otherwise, deselect all
						if ( !StopPlayback() )
						{
							Deselect();
							DeselectPhonemes();
							DeselectWords();
							Emphasis_DeselectAll();
							redraw();
						}
					}
					break;
				case 'O':
					{
						if ( ctrlDown )
						{
							LoadWaveFile();
						}
					}
					break;
				case 'S':
					{
						if ( ctrlDown )
						{
							CommitChanges();
						}
					}
					break;
				case 'T':
					{
						if ( ctrlDown )
						{
							// Edit sentence text
							EditWordList();
						}
					}
					break;
				case 'G':
					{
						if ( ctrlDown )
						{
							// Commit extraction
							CommitExtracted();
						}
					}
					break;
				case 'R':
					{
						if ( ctrlDown )
						{
							RedoPhonemeExtraction();
						}
					}
					break;
				default:
					break;
			}

			SetFocus( (HWND)getHandle() );
			iret = 1;
		}
		break;
	case mxEvent::KeyDown:
		{
			switch ( event->key )
			{
			case 'Z':
				if ( GetAsyncKeyState( VK_CONTROL ) )
				{
					Undo();
				}
				break;
			case 'Y':
				if ( GetAsyncKeyState( VK_CONTROL ) )
				{
					Redo();
				}
				break;


			case VK_RIGHT:
			case VK_LEFT:
				{
					int direction = event->key == VK_LEFT ? -1 : 1;

					if ( !m_bWordsActive )
					{
						if ( GetAsyncKeyState( VK_CONTROL ) )
						{
							ExtendSelectedPhonemeEndTime( direction );
						}
						else if ( GetAsyncKeyState( VK_SHIFT ) )
						{
							ShiftSelectedPhoneme( direction );
						}
						else
						{
							SelectNextPhoneme( direction );
						}
					}
					else
					{
						if ( GetAsyncKeyState( VK_CONTROL ) )
						{
							ExtendSelectedWordEndTime( direction );
						}
						else if ( GetAsyncKeyState( VK_SHIFT ) )
						{
							ShiftSelectedWord( direction );
						}
						else
						{
							SelectNextWord( direction );
						}
					}
				}
				break;
			case VK_RETURN:
				{
				}
				break;
			case VK_SHIFT:
			case VK_CONTROL:
				{
					// Force mouse move
					POINT pt;
					GetCursorPos( &pt );
					//SetCursorPos( pt.x -1, pt.y );
					SetCursorPos( pt.x, pt.y );
					return 0;
				}
				break;
			default:
				break;
			}
			iret = 1;
		}
		break;
	}
	return iret;
}

void PhonemeEditor::DrawWords( CChoreoWidgetDrawHelper& drawHelper, RECT& rcWorkSpace, CSentence& sentence, int type, bool showactive /* = true */ )
{
	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float endtime = w2() / GetPixelsPerSecond() + starttime;

	int ypos = rcWorkSpace.top + m_nTickHeight + 2;

	if ( type == 1 )
	{
		ypos += m_nTickHeight + 5;
	}

	const char *fontName = "Arial";

	bool drawselected;
	for ( int pass = 0; pass < 2 ; pass++ )
	{
		drawselected = pass == 0 ? false : true;

		for (int k = 0; k < sentence.m_Words.Size(); k++)
		{
			CWordTag *word = sentence.m_Words[ k ];
			if ( !word )
				continue;

			if ( word->m_bSelected != drawselected )
				continue;

			bool hasselectedphonemes = false;
			for ( int p = 0; p < word->m_Phonemes.Size() && !hasselectedphonemes; p++ )
			{
				CPhonemeTag *t = word->m_Phonemes[ p ];
				if ( t->m_bSelected )
				{
					hasselectedphonemes = true;
				}
			}

			float t1 = word->m_flStartTime;
			float t2 = word->m_flEndTime;

			// Tag it
			float frac = ( t1 - starttime ) / ( endtime - starttime );

			int xpos = ( int )( frac * rcWorkSpace.right );

			if ( frac <= 0.0 )
				xpos = 0;

			// Draw duration
			float frac2  = ( t2 - starttime ) / ( endtime - starttime );
			if ( frac2 < 0.0 )
				continue;

			int xpos2 = ( int )( frac2 * rcWorkSpace.right );

			// Draw line and vertical ticks
			RECT rcWord;
			rcWord.left = xpos;
			rcWord.right = xpos2;
			rcWord.top = ypos - m_nTickHeight + 1;
			rcWord.bottom = ypos;

			drawHelper.DrawFilledRect( 
				PEColor( word->m_bSelected ? COLOR_PHONEME_TAG_SELECTED : COLOR_PHONEME_TAG_FILLER_NORMAL ), 
				rcWord );

			COLORREF border = PEColor( word->m_bSelected ? COLOR_PHONEME_TAG_BORDER_SELECTED : COLOR_PHONEME_TAG_BORDER );

			if ( showactive && m_bWordsActive )
			{
				drawHelper.DrawFilledRect( PEColor( COLOR_PHONEME_ACTIVE_BORDER ), xpos, ypos - m_nTickHeight, xpos2, ypos - m_nTickHeight + 4 );
			}

			drawHelper.DrawColoredLine( border, PS_SOLID, 1, xpos, ypos, xpos2, ypos );
			drawHelper.DrawColoredLine( border, PS_SOLID, 1, xpos, ypos, xpos, ypos - m_nTickHeight );
			drawHelper.DrawColoredLine( border, PS_SOLID, 1, xpos2, ypos, xpos2, ypos - m_nTickHeight );
			drawHelper.DrawColoredLine( border, PS_SOLID, 1, xpos, ypos - m_nTickHeight, xpos2, ypos - m_nTickHeight );

			if ( hasselectedphonemes )
			{
				drawHelper.DrawFilledRect( PEColor( COLOR_PHONEME_SELECTED_BORDER ), xpos, ypos - 3, xpos2, ypos );
			}
		
			//if ( frac >= 0.0 && frac <= 1.0 )
			{
				int fontsize = 9;

				RECT rcText;
				rcText.left = xpos;
				rcText.right = xpos + 500;
				rcText.top = ypos - m_nTickHeight + 4;
				rcText.bottom = rcText.top + fontsize + 2;

				int length = drawHelper.CalcTextWidth( fontName, fontsize, FW_NORMAL, "%s", word->GetWord() );

				rcText.right = max( (LONG)xpos2 - 2, rcText.left + length + 1 );

				int w = rcText.right - rcText.left;
				if ( w > length )
				{
					rcText.left += ( w - length ) / 2;
				}

				drawHelper.DrawColoredText( 
					fontName, 
					fontsize, 
					FW_NORMAL, 
					PEColor( word->m_bSelected ? COLOR_PHONEME_TAG_TEXT_SELECTED : COLOR_PHONEME_TAG_TEXT ), 
					rcText,
					"%s", word->GetWord() );
			}

		}
	}
}

void PhonemeEditor::DrawPhonemes( CChoreoWidgetDrawHelper& drawHelper, RECT& rcWorkSpace, CSentence& sentence, int type, bool showactive /* = true */ )
{
	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float endtime = w2() / GetPixelsPerSecond() + starttime;

	int ypos = rcWorkSpace.bottom - m_nTickHeight - 2;

	if ( type == 1 )
	{
		ypos -= ( m_nTickHeight + 5 );
	}

	const char *fontName = "Arial";

	bool drawselected;
	for ( int pass = 0; pass < 2 ; pass++ )
	{
		drawselected = pass == 0 ? false : true;

		for ( int i = 0; i < sentence.m_Words.Size(); i++ )
		{
			CWordTag *w = sentence.m_Words[ i ];
			if ( !w )
				continue;

			if ( w->m_bSelected != drawselected )
				continue;

			for ( int k = 0; k < w->m_Phonemes.Size(); k++ )
			{
				CPhonemeTag *pPhoneme = w->m_Phonemes[ k ];

				float t1 = pPhoneme->GetStartTime();
				float t2 = pPhoneme->GetEndTime();

				// Tag it
				float frac = ( t1 - starttime ) / ( endtime - starttime );

				int xpos = ( int )( frac * rcWorkSpace.right );
				if ( frac <= 0.0 )
				{
					xpos = 0;
				}

				// Draw duration
				float frac2  = ( t2 - starttime ) / ( endtime - starttime );
				if ( frac2 < 0.0 )
				{
					continue;
				}

				int xpos2 = ( int )( frac2 * rcWorkSpace.right );

				RECT rcFrame;
				rcFrame.left = xpos;
				rcFrame.right = xpos2;
				rcFrame.top = ypos - m_nTickHeight + 1;
				rcFrame.bottom = ypos;

				drawHelper.DrawFilledRect( 
					PEColor( pPhoneme->m_bSelected ? COLOR_PHONEME_TAG_SELECTED : COLOR_PHONEME_TAG_FILLER_NORMAL ),
					rcFrame );

				COLORREF border = PEColor( pPhoneme->m_bSelected ? COLOR_PHONEME_TAG_BORDER_SELECTED : COLOR_PHONEME_TAG_BORDER );

				if ( showactive && !m_bWordsActive )
				{
					drawHelper.DrawFilledRect( PEColor( COLOR_PHONEME_ACTIVE_BORDER ), xpos, ypos - 3, xpos2, ypos );
				}

				drawHelper.DrawColoredLine( border, PS_SOLID, 1, xpos, ypos - m_nTickHeight, xpos2, ypos - m_nTickHeight );
				drawHelper.DrawColoredLine( border, PS_SOLID, 1, xpos, ypos, xpos, ypos - m_nTickHeight );
				drawHelper.DrawColoredLine( border, PS_SOLID, 1, xpos2, ypos, xpos2, ypos - m_nTickHeight );
				drawHelper.DrawColoredLine( border, PS_SOLID, 1, xpos, ypos, xpos2, ypos );

				if ( w->m_bSelected )
				{
					drawHelper.DrawFilledRect( PEColor( COLOR_PHONEME_SELECTED_BORDER ), xpos, ypos - m_nTickHeight + 1, xpos2, ypos - m_nTickHeight + 4 );
				}

				//if ( frac >= 0.0 && frac <= 1.0 )
				{

					int fontsize = 9;

					RECT rcText;
					rcText.left = xpos;
					rcText.right = xpos + 500;
					rcText.top = ypos - m_nTickHeight + 4;
					rcText.bottom = rcText.top + fontsize + 2;

					int length = drawHelper.CalcTextWidth( fontName, fontsize, FW_NORMAL, "%s", ConvertPhoneme( pPhoneme->GetPhonemeCode() ) );

					rcText.right = max( (LONG)xpos2 - 2, rcText.left + length + 1 );

					int w = rcText.right - rcText.left;
					if ( w > length )
					{
						rcText.left += ( w - length ) / 2;
					}

					drawHelper.DrawColoredText( 
						fontName, 
						fontsize, 
						FW_NORMAL, 
						PEColor( pPhoneme->m_bSelected ? COLOR_PHONEME_TAG_TEXT_SELECTED : COLOR_PHONEME_TAG_TEXT ), 
						rcText,
						"%s", ConvertPhoneme( pPhoneme->GetPhonemeCode() ) );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rc - 
//-----------------------------------------------------------------------------
void PhonemeEditor::DrawRelativeTags( CChoreoWidgetDrawHelper& drawHelper, RECT& rc )
{
	if ( !m_pEvent || !m_pWaveFile )
		return;

	drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, PEColor( COLOR_PHONEME_TIMING_TAG ), rc, "Timing Tags:" );

	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float endtime = w2() / GetPixelsPerSecond() + starttime;

	for ( int i = 0; i < m_pEvent->GetNumRelativeTags(); i++ )
	{
		CEventRelativeTag *tag = m_pEvent->GetRelativeTag( i );
		if ( !tag )
			continue;

		// 
		float tagtime = tag->GetPercentage() * m_pWaveFile->GetRunningLength();
		if ( tagtime < starttime || tagtime > endtime )
			continue;

		float frac = ( tagtime - starttime ) / ( endtime - starttime );

		int left = rc.left + (int)( frac * ( float )( rc.right - rc.left ) + 0.5f );

		RECT rcMark;
		rcMark = rc;
		rcMark.top = rc.bottom - 8;
		rcMark.bottom = rc.bottom;
		rcMark.left = left - 4;
		rcMark.right = left + 4;

		drawHelper.DrawTriangleMarker( rcMark, PEColor( COLOR_PHONEME_TIMING_TAG ) );

		RECT rcText;
		rcText = rc;
		rcText.bottom = rc.bottom - 10;
		rcText.top = rcText.bottom - 10;
	
		int len = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, tag->GetName() );
		rcText.left = left - len / 2;
		rcText.right = rcText.left + len + 2;

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, PEColor( COLOR_PHONEME_TIMING_TAG ), rcText, tag->GetName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::redraw( void )
{
	if ( !ToolCanDraw() )
		return;

	CChoreoWidgetDrawHelper drawHelper( this );
	HandleToolRedraw( drawHelper );

	if ( !m_pWaveFile )
		return;

	HDC dc = drawHelper.GrabDC();

	RECT rc;
	GetWorkspaceRect( rc );

	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float endtime = w2() / GetPixelsPerSecond() + starttime;

	// Now draw the time legend
	RECT rcLabel;
	float granularity = 0.5f; 

	drawHelper.DrawColoredLine( PEColor( COLOR_PHONEME_TIMELINE ), PS_SOLID, 1, rc.left, rc.bottom - m_nTickHeight, rc.right, rc.bottom - m_nTickHeight );

	if ( GetMode() != MODE_EMPHASIS )
	{
		Emphasis_Redraw( drawHelper, rc );
	}

	sound->RenderWavToDC( 
		dc, 
		rc, 
		PEColor( COLOR_PHONEME_WAVDATA ), 
		starttime, 
		endtime, 
		m_pWaveFile,
		m_bSelectionActive,
		m_nSelection[ 0 ],
		m_nSelection[ 1 ] );

	float f = SnapTime( starttime, granularity );
	while ( f <= endtime )
	{
		float frac = ( f - starttime ) / ( endtime - starttime );
		if ( frac >= 0.0f && frac <= 1.0f )
		{
			drawHelper.DrawColoredLine( ( COLOR_PHONEME_TIMELINE_MAJORTICK ), PS_SOLID, 1, (int)( frac * rc.right ), rc.top, (int)( frac * rc.right ), rc.bottom - m_nTickHeight );

			rcLabel.left = (int)( frac * rc.right );
			rcLabel.bottom = rc.bottom;
			rcLabel.top = rcLabel.bottom - 10;

			char sz[ 32 ];
			sprintf( sz, "%.2f", f );
			int textWidth = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, sz );
			rcLabel.right = rcLabel.left + textWidth;
			OffsetRect( &rcLabel, -textWidth / 2, 0 );
			drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, PEColor( COLOR_PHONEME_TEXT ), rcLabel, sz );
		}
		f += granularity;
	}

	HBRUSH br = CreateSolidBrush( PEColor( COLOR_PHONEME_TEXT ) );

	FrameRect( dc, &rc, br );

	DeleteObject( br );

	RECT rcTags = rc;
	rcTags.top = TAG_TOP;
	rcTags.bottom = TAG_BOTTOM;

	DrawRelativeTags( drawHelper, rcTags );

	int fontsize = 9;
	RECT rcText = rc;
	rcText.top = rcText.bottom + 5;
	rcText.left += 5;
	rcText.bottom = rcText.top + fontsize + 1;
	rcText.right -= 5;

	int fontweight = FW_NORMAL;

	const char *font = "Arial";

	if ( m_nLastExtractionResult != SR_RESULT_NORESULT )
	{
		COLORREF clr = PEColor( COLOR_PHONEME_EXTRACTION_RESULT_OTHER );
		switch ( m_nLastExtractionResult )
		{
		case SR_RESULT_ERROR:
			clr = PEColor( COLOR_PHONEME_EXTRACTION_RESULT_ERROR );
			break;
		case SR_RESULT_SUCCESS:
			clr = PEColor( COLOR_PHONEME_EXTRACTION_RESULT_SUCCESS );
			break;
		case SR_RESULT_FAILED:
			clr = PEColor( COLOR_PHONEME_EXTRACTION_RESULT_FAIL );
			break;
		default:
			break;
		}

		drawHelper.DrawColoredText( font, fontsize, fontweight, clr, rcText,
			"Last Extraction Result:  %s", GetExtractionResultString( m_nLastExtractionResult ) );
	
		OffsetRect( &rcText, 0, fontsize + 1 );
	}

	if ( m_pEvent && !Q_stristr( m_pEvent->GetParameters(), ".wav" ) )
	{
		drawHelper.DrawColoredText( font, fontsize, fontweight, PEColor( COLOR_PHONEME_TEXT ), rcText,
			"Sound: '%s', file: %s, length %.2f seconds", 
			m_pEvent->GetParameters(),
			m_WorkFile.m_szWaveFile, 
			m_pWaveFile->GetRunningLength() );
	}
	else
	{
		drawHelper.DrawColoredText( font, fontsize, fontweight, PEColor( COLOR_PHONEME_TEXT ), rcText,
			"File: %s, length %.2f seconds", m_WorkFile.m_szWaveFile, m_pWaveFile->GetRunningLength() );
	}

	OffsetRect( &rcText, 0, fontsize + 1 );

	drawHelper.DrawColoredText( font, fontsize, fontweight, PEColor( COLOR_PHONEME_TEXT ), rcText,
		"Number of samples %i at %ikhz (%i bits/sample) %s", (int) (m_pWaveFile->GetRunningLength() * m_pWaveFile->SampleRate() ), m_pWaveFile->SampleRate(), (m_pWaveFile->SampleSize()<<3), m_Tags.GetVoiceDuck() ? "duck other audio" : "no ducking" );

	OffsetRect( &rcText, 0, fontsize + 1 );

	drawHelper.DrawColoredText( font, fontsize, fontweight, PEColor( COLOR_PHONEME_TEXT ), rcText,
		"[ %i ] Words [ %i ] Phonemes / Zoom %i %%", m_Tags.m_Words.Size(), m_Tags.CountPhonemes(), m_nTimeZoom );

	if ( m_pEvent )
	{
		OffsetRect( &rcText, 0, fontsize + 1 );

		drawHelper.DrawColoredText( font, fontsize, fontweight, PEColor( COLOR_PHONEME_TEXT ), rcText,
			"Event %s", m_pEvent->GetName() );
	}

	OffsetRect( &rcText, 0, fontsize + 1 );

	drawHelper.DrawColoredText( font, fontsize, fontweight, PEColor( COLOR_PHONEME_TEXT ), rcText,
		"Using:  %s", GetSpeechAPIName() );


	char text[ 4096 ];
	sprintf( text, "Sentence Text:  %s", m_Tags.GetText() );

	int halfwidth = ( rc.right - rc.left ) / 2;

	rcText = rc;
	rcText.left = halfwidth;
	rcText.top = rcText.bottom + 5;
	rcText.right = rcText.left + halfwidth * 0.6;

	drawHelper.CalcTextRect( font, fontsize, fontweight, halfwidth, rcText, text );

	drawHelper.DrawColoredTextMultiline( font, fontsize, fontweight, PEColor( COLOR_PHONEME_TEXT ), rcText,
		text );

	CWordTag *cw = GetSelectedWord();
	if ( cw )
	{
		char wordInfo[ 512 ];
		sprintf( wordInfo, "Word:  %s, start %.2f end %.2f, duration %.2f ms phonemes %i",
			cw->GetWord(), cw->m_flStartTime, cw->m_flEndTime, 1000.0f * ( cw->m_flEndTime - cw->m_flStartTime ),
			cw->m_Phonemes.Size() );

		int length = drawHelper.CalcTextWidth( font, fontsize, fontweight, wordInfo );

		OffsetRect( &rcText, 0, ( rcText.bottom - rcText.top ) + 2 );

		rcText.left = rcText.right - length - 10;
		rcText.bottom = rcText.top + fontsize + 1;

		drawHelper.DrawColoredText( font, fontsize, fontweight, PEColor( COLOR_PHONEME_TEXT ), rcText, wordInfo );
	}

	CPhonemeTag *cp = GetSelectedPhoneme();
	if ( cp )
	{
		char phonemeInfo[ 512 ];
		sprintf( phonemeInfo, "Phoneme:  %s, start %.2f end %.2f, duration %.2f ms",
			ConvertPhoneme( cp->GetPhonemeCode() ), cp->GetStartTime(), cp->GetEndTime(), 1000.0f * ( cp->GetEndTime() - cp->GetStartTime() ) );

		int length = drawHelper.CalcTextWidth( font, fontsize, fontweight, phonemeInfo );

		OffsetRect( &rcText, 0, ( rcText.bottom - rcText.top ) + 2 );

		rcText.left = rcText.right - length - 10;
		rcText.bottom = rcText.top + fontsize + 1;

		drawHelper.DrawColoredText( font, fontsize, fontweight, PEColor( COLOR_PHONEME_TEXT ), rcText, phonemeInfo );
	}

	// Draw playback rate
	{
		char sz[ 48 ];
		sprintf( sz, "Speed: %.2fx", m_flPlaybackRate );

		int length = drawHelper.CalcTextWidth( font, fontsize, fontweight, sz);
		
		rcText = rc;
		rcText.top = rc.bottom + 60;
		rcText.bottom = rcText.top + fontsize + 1;
		rcText.left = m_pPlaybackRate->x() + m_pPlaybackRate->w() - x();
		rcText.right = rcText.left + length + 2;

		drawHelper.DrawColoredText( font, fontsize, fontweight, 
			PEColor( COLOR_PHONEME_TEXT ), rcText, sz );
	}

	if ( m_UndoStack.Size() > 0 )
	{
		int length = drawHelper.CalcTextWidth( font, fontsize, fontweight, 
			"Undo levels:  %i/%i", m_nUndoLevel, m_UndoStack.Size() );

		rcText = rc;
		rcText.top = rc.bottom + 60;
		rcText.bottom = rcText.top + fontsize + 1;
		rcText.right -= 5;
		rcText.left = rcText.right - length - 10;

		drawHelper.DrawColoredText( font, fontsize, fontweight, PEColor( COLOR_PHONEME_EXTRACTION_RESULT_SUCCESS ), rcText,
			"Undo levels:  %i/%i", m_nUndoLevel, m_UndoStack.Size() );
	}

	float endfrac = ( m_pWaveFile->GetRunningLength() - starttime ) / ( endtime - starttime );
	if ( endfrac >= 0.0f && endfrac <= 1.0f )
	{
		int endpos = ( int ) ( rc.right * endfrac );

		drawHelper.DrawColoredLine( PEColor( COLOR_PHONEME_WAV_ENDPOINT ), PS_DOT, 2, endpos, rc.top, endpos, rc.bottom - m_nTickHeight );
	}

	DrawPhonemes( drawHelper, rc, m_Tags, 0 );

	DrawPhonemes( drawHelper, rc, m_TagsExt, 1, false );

	DrawWords( drawHelper, rc, m_Tags, 0 );

	DrawWords( drawHelper, rc, m_TagsExt, 1, false );

	if ( GetMode() == MODE_EMPHASIS )
	{
		Emphasis_Redraw( drawHelper, rc );
	}

	DrawScrubHandle( drawHelper );
}

#define MOTION_RANGE 3000
#define MOTION_MAXSTEP 500
//-----------------------------------------------------------------------------
// Purpose: Brown noise simulates brownian motion centered around 127.5 but we cap the walking
//  to just a couple of units
// Input  : *buffer - 
//			count - 
// Output : static void
//-----------------------------------------------------------------------------
static void WriteBrownNoise( void *buffer, int count )
{
	int currentValue = 127500;
	int maxValue = currentValue + ( MOTION_RANGE / 2 );
	int minValue = currentValue - ( MOTION_RANGE / 2 );

	unsigned char *pos = ( unsigned char *)buffer;

	while ( --count >= 0 )
	{
		currentValue += random->RandomInt( -MOTION_MAXSTEP, MOTION_MAXSTEP );
		currentValue = min( maxValue, currentValue );
		currentValue = max( minValue, currentValue );

		// Downsample to 0-255 range
		*pos++ = (unsigned char)( ( (float)currentValue / 1000.0f ) + 0.5f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Replace with brownian noice parts of the wav file that we dont' want processed by the
//  speech recognizer
// Input  : store - 
//			*format - 
//			chunkname - 
//			*buffer - 
//			buffersize - 
//-----------------------------------------------------------------------------
void PhonemeEditor::ResampleChunk( IterateOutputRIFF& store, void *format, int chunkname, char *buffer, int buffersize, int start_silence /*=0*/, int end_silence /*=0*/ )
{
	WAVEFORMATEX *pFormat = ( WAVEFORMATEX * )format;
	Assert( pFormat );

	if ( pFormat->wFormatTag == WAVE_FORMAT_PCM )
	{
		int silience_time = start_silence + end_silence;

		// Leave room for silence at start + end
		int resamplesize = buffersize + silience_time * pFormat->nSamplesPerSec;
		char *resamplebuffer = new char[ resamplesize + 4 ];
		memset( resamplebuffer, (unsigned char)128, resamplesize + 4 );

		int startpos = (int)( start_silence * pFormat->nSamplesPerSec );

		if ( startpos > 0 )
		{
			WriteBrownNoise( resamplebuffer, startpos );
		}

		if ( startpos + buffersize < resamplesize )
		{
			WriteBrownNoise( &resamplebuffer[ startpos + buffersize ], resamplesize - ( startpos + buffersize ) );
		}

		memcpy( &resamplebuffer[ startpos ], buffer, buffersize );

		store.ChunkWriteData( resamplebuffer, resamplesize );
		return;
	}

	store.ChunkWriteData( buffer, buffersize );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::ReadLinguisticTags( void )
{
	if ( !m_pWaveFile )
		return;

	CAudioSource *wave = sound->LoadSound( m_WorkFile.m_szWorkingFile );
	if ( !wave )
		return;

	m_Tags.Reset();

	CSentence *sentence = wave->GetSentence();
	if ( sentence )
	{
		// Copy data from sentence to m_Tags
		m_Tags.Reset();
		m_Tags = *sentence;
	}

	delete wave;
}

//-----------------------------------------------------------------------------
// Purpose: Switch wave files
// Input  : *wavefile - 
//			force - 
//-----------------------------------------------------------------------------
void PhonemeEditor::SetCurrentWaveFile( const char *wavefile, bool force /*=false*/, CChoreoEvent *event /*=NULL*/ )
{
	// No change?
	if ( !force && !stricmp( m_WorkFile.m_szWaveFile, wavefile ) )
		return;

	StopPlayback();

	if ( GetDirty() )
	{
		int retval = mxMessageBox( this, va( "Save current changes to %s", m_WorkFile.m_szWaveFile ),
			"Phoneme Editor", MX_MB_QUESTION | MX_MB_YESNOCANCEL );

		// Cancel
		if ( retval == 2 )
			return;

		// Yes
		if ( retval == 0 )
		{
			CommitChanges();
		}
	}

	ClearExtracted();

	m_Tags.Reset();
	m_TagsExt.Reset();

	Deselect();

	if ( m_pWaveFile )
	{
		char fn[ 512 ];
		Q_snprintf( fn, sizeof( fn ), "%s%s", m_WorkFile.m_szBasePath, m_WorkFile.m_szWorkingFile );
		filesystem->RemoveFile( fn, "GAME" );
	}

	delete m_pWaveFile;
	m_pWaveFile = NULL;

	SetDirty( false );

	// Set up event and scene
	m_pEvent = event;

	// Try an dload new sound
	m_pWaveFile = sound->LoadSound( wavefile );
	Q_strncpy( m_WorkFile.m_szWaveFile, wavefile, sizeof( m_WorkFile.m_szWaveFile ) );

	char fullpath[ 512 ];
	filesystem->RelativePathToFullPath( wavefile, "GAME", fullpath, sizeof( fullpath ) );
	int len = Q_strlen( fullpath );
	int charstocopy = len - Q_strlen( wavefile ) + 1;
	m_WorkFile.m_szBasePath[ 0 ] = 0;
	if ( charstocopy >= 0 )
	{
		Q_strncpy( m_WorkFile.m_szBasePath, fullpath, charstocopy );
		m_WorkFile.m_szBasePath[ charstocopy ] = 0;
	}
	Q_StripExtension( wavefile, m_WorkFile.m_szWorkingFile, sizeof( m_WorkFile.m_szWorkingFile ) );
	Q_strncat( m_WorkFile.m_szWorkingFile, "_work.wav", sizeof( m_WorkFile.m_szWorkingFile ), COPY_ALL_CHARACTERS );

	Q_FixSlashes( m_WorkFile.m_szWaveFile );
	Q_FixSlashes( m_WorkFile.m_szWorkingFile );
	Q_FixSlashes( m_WorkFile.m_szBasePath );

	if ( !m_pWaveFile )
	{
		Con_ErrorPrintf( "Couldn't set current .wav file to %s\n", m_WorkFile.m_szWaveFile );
		return;
	}

	Con_Printf( "Current .wav file set to %s\n", m_WorkFile.m_szWaveFile );

	g_pWaveBrowser->SetCurrent( m_WorkFile.m_szWaveFile );

	// Copy over and overwrite file
	FPCopyFile( m_WorkFile.m_szWaveFile, m_WorkFile.m_szWorkingFile, false );
	// Make it writable
	MakeFileWriteable( m_WorkFile.m_szWorkingFile );

	ReadLinguisticTags();

	Deselect();

	RepositionHSlider();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//-----------------------------------------------------------------------------
void PhonemeEditor::MoveTimeSliderToPos( int x )
{
	m_nLeftOffset = x;
	m_pHorzScrollBar->setValue( m_nLeftOffset );
	InvalidateRect( (HWND)m_pHorzScrollBar->getHandle(), NULL, TRUE );
	redraw();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int PhonemeEditor::ComputeHPixelsNeeded( void )
{
	int pixels = 0;

	if ( m_pWaveFile )
	{
		float maxtime = m_pWaveFile->GetRunningLength();
		maxtime += 1.0f;
		pixels = (int)( maxtime * GetPixelsPerSecond() );
	}

	return pixels;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::RepositionHSlider( void )
{
	int pixelsneeded = ComputeHPixelsNeeded();

	if ( pixelsneeded <= w2() )
	{
		m_pHorzScrollBar->setVisible( false );
	}
	else
	{
		m_pHorzScrollBar->setVisible( true );
	}

	m_pHorzScrollBar->setBounds( 0, GetCaptionHeight(), w2(), 12 );

	m_pHorzScrollBar->setRange( 0, pixelsneeded );
	m_pHorzScrollBar->setValue( 0 );
	m_nLeftOffset = 0;

	m_pHorzScrollBar->setPagesize( w2() );

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float PhonemeEditor::GetPixelsPerSecond( void )
{
	return m_flPixelsPerSecond * GetTimeZoomScale();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float PhonemeEditor::GetTimeZoomScale( void )
{
	return ( float )m_nTimeZoom / 100.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : scale - 
//-----------------------------------------------------------------------------
void PhonemeEditor::SetTimeZoomScale( int scale )
{
	m_nTimeZoom = scale;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void PhonemeEditor::Think( float dt )
{
	if ( !m_pWaveFile )
		return;

	bool scrubbing = ( m_nDragType == DRAGTYPE_SCRUBBER ) ? true : false;
	ScrubThink( dt, scrubbing );

	if ( m_pMixer && !sound->IsSoundPlaying( m_pMixer ) )
	{
		m_pMixer = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int PhonemeEditor::IsMouseOverBoundary( mxEvent *event )
{
	int mx, my;

	mx = (short)event->x;
	my = (short)event->y;

	// Deterime if phoneme boundary is under the cursor
	//
	if ( !m_pWaveFile )
		return BOUNDARY_NONE;

	if ( !(event->modifiers & mxEvent::KeyCtrl ) ) 
	{
		return BOUNDARY_NONE;
	}

	RECT rc;
	GetWorkspaceRect( rc );

	if ( IsMouseOverPhonemeRow( my ) )
	{
		float starttime = m_nLeftOffset / GetPixelsPerSecond();
		float endtime = w2() / GetPixelsPerSecond() + starttime;

		int		mouse_tolerance = 3;

		for ( int i = 0; i < m_Tags.m_Words.Size(); i++ )
		{
			CWordTag *word = m_Tags.m_Words[ i ];

			for ( int k = 0; k < word->m_Phonemes.Size(); k++ )
			{
				CPhonemeTag *pPhoneme = word->m_Phonemes[ k ];

				float t1 = pPhoneme->GetStartTime();
				float t2 = pPhoneme->GetEndTime();

				// Tag it
				float frac1 = ( t1 - starttime ) / ( endtime - starttime );
				float frac2 = ( t2 - starttime ) / ( endtime - starttime );

				int xpos1 = ( int )( frac1 * w2() );
				int xpos2 = ( int )( frac2 * w2() );
				if ( abs( xpos1 - mx ) <= mouse_tolerance ||
					 abs( xpos2 - mx ) <= mouse_tolerance )
				{
					return BOUNDARY_PHONEME;
				}
			}
		}
	}

	if ( IsMouseOverWordRow( my ) )
	{
		float starttime = m_nLeftOffset / GetPixelsPerSecond();
		float endtime = w2() / GetPixelsPerSecond() + starttime;

		int		mouse_tolerance = 3;

		for ( int k = 0; k < m_Tags.m_Words.Size(); k++ )
		{
			CWordTag *word = m_Tags.m_Words[ k ];

			float t1 = word->m_flStartTime;
			float t2 = word->m_flEndTime;

			// Tag it
			float frac1 = ( t1 - starttime ) / ( endtime - starttime );
			float frac2 = ( t2 - starttime ) / ( endtime - starttime );

			int xpos1 = ( int )( frac1 * w2() );
			int xpos2 = ( int )( frac2 * w2() );
			if ( ( abs( xpos1 - mx ) <= mouse_tolerance ) ||
				 ( abs( xpos2 - mx ) <= mouse_tolerance ) )
			{
				return BOUNDARY_WORD;
			}
		}
	}

	return BOUNDARY_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::DrawFocusRect( char *reason )
{
	HDC dc = GetDC( NULL );

	for ( int i = 0; i < m_FocusRects.Size(); i++ )
	{
		RECT rc = m_FocusRects[ i ].m_rcFocus;

		::DrawFocusRect( dc, &rc );
	}

	ReleaseDC( NULL, dc );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &rc - 
//-----------------------------------------------------------------------------
void PhonemeEditor::GetWorkspaceRect( RECT &rc )
{
	GetClientRect( (HWND)getHandle(), &rc );
	
	rc.top += TAG_BOTTOM;
	rc.bottom = rc.bottom - 75 - MODE_TAB_OFFSET;

	InflateRect( &rc, -1, -1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
void PhonemeEditor::ShowWordMenu( CWordTag *word, int mx, int my )
{
	CountSelected();

	mxPopupMenu *pop = new mxPopupMenu();
	Assert( pop );

	pop->add( va( "Edit sentence text..." ), IDC_EDITWORDLIST );
	
	if ( m_nSelectedWordCount > 0 && word )
	{
		pop->addSeparator();

		pop->add( va( "Delete %s", m_nSelectedWordCount > 1 ? "words" : va( "'%s'", word->GetWord() ) ), IDC_EDIT_DELETEWORD );

		if ( m_nSelectedWordCount == 1 )
		{
			int index = IndexOfWord( word );
			bool valid = false;
			if ( index != -1 )
			{
				SetClickedPhoneme( index, -1 );
				valid = true;
			}

			if ( valid )
			{
				pop->add( va( "Edit word '%s'...", word->GetWord() ), IDC_EDIT_WORD );

				float nextGap = GetTimeGapToNextWord( true, word );
				float prevGap = GetTimeGapToNextWord( false, word );

				if ( nextGap > MINIMUM_WORD_GAP ||
					 prevGap > MINIMUM_WORD_GAP )
				{
					pop->addSeparator();
					if ( prevGap > MINIMUM_WORD_GAP )
					{
						pop->add( va( "Insert word before '%s'...", word->GetWord() ), IDC_EDIT_INSERTWORDBEFORE );
					}
					if ( nextGap > MINIMUM_WORD_GAP )
					{
						pop->add( va( "Insert word after '%s'...", word->GetWord() ), IDC_EDIT_INSERTWORDAFTER );
					}
				}

				if ( word->m_Phonemes.Size() == 0 )
				{
					pop->addSeparator();
					pop->add( va( "Add phoneme to '%s'...", word->GetWord() ), IDC_EDIT_INSERTFIRSTPHONEMEOFWORD );
				}

				pop->addSeparator();
				pop->add( va( "Select all words after '%s'", word->GetWord() ), IDC_SELECT_WORDSRIGHT );
				pop->add( va( "Select all words before '%s'", word->GetWord() ), IDC_SELECT_WORDSLEFT );
			}
		}
	}

	if ( AreSelectedWordsContiguous() && m_nSelectedWordCount > 1 )
	{
		pop->addSeparator();
		pop->add( va( "Merge words" ), IDC_SNAPWORDS );
		
		if ( m_nSelectedWordCount == 2 )
		{
			pop->add( va( "Separate words" ), IDC_SEPARATEWORDS );
		}
	}

	if ( m_nSelectedWordCount > 0 )
	{
		pop->addSeparator();

		pop->add( va( "Deselect all" ), IDC_DESELECT_PHONEMESANDWORDS );
	}

	if ( m_Tags.m_Words.Size() > 0 )
	{
		pop->addSeparator();
		pop->add( va( "Cleanup words/phonemes" ), IDC_CLEANUP );
	}

	if ( m_Tags.m_Words.Size() > 0 )
	{
		pop->addSeparator();
		pop->add( va( "Realign phonemes to words" ), IDC_REALIGNPHONEMES );
	}


	pop->popup( this, mx, my );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
void PhonemeEditor::ShowPhonemeMenu( CPhonemeTag *pho, int mx, int my )
{
	CountSelected();

	SetClickedPhoneme( -1, -1 );

	if ( !pho )
		return;

	if ( m_Tags.CountPhonemes() == 0 )
	{
		Con_Printf( "No phonemes, try extracting from .wav first\n" );
		return;
	}

	mxPopupMenu *pop = new mxPopupMenu();
	bool valid = false;
	CWordTag *tag = m_Tags.GetWordForPhoneme( pho );
	if ( tag )
	{
		int wordNum = IndexOfWord( tag );
		int pi = tag->IndexOfPhoneme( pho );

		SetClickedPhoneme( wordNum, pi );
		valid = true;
	}

	if ( valid )
	{
		if ( m_nSelectedPhonemeCount == 1 )
		{
			pop->add( va( "Edit '%s'...", ConvertPhoneme( pho->GetPhonemeCode() ) ), IDC_EDIT_PHONEME );

			float nextGap = GetTimeGapToNextPhoneme( true, pho );
			float prevGap = GetTimeGapToNextPhoneme( false, pho );

			if ( nextGap > MINIMUM_PHONEME_GAP ||
				 prevGap > MINIMUM_PHONEME_GAP )
			{
				pop->addSeparator();
				if ( prevGap > MINIMUM_PHONEME_GAP )
				{
					pop->add( va( "Insert phoneme before '%s'...", ConvertPhoneme( pho->GetPhonemeCode() ) ), IDC_EDIT_INSERTPHONEMEBEFORE );
				}
				if ( nextGap > MINIMUM_PHONEME_GAP )
				{
					pop->add( va( "Insert phoneme after '%s'...", ConvertPhoneme( pho->GetPhonemeCode() ) ), IDC_EDIT_INSERTPHONEMEAFTER );
				}
			}

			pop->addSeparator();
			pop->add( va( "Select all phonemes after '%s'", ConvertPhoneme( pho->GetPhonemeCode() ) ), IDC_SELECT_PHONEMESRIGHT );
			pop->add( va( "Select all phonemes before '%s'",ConvertPhoneme( pho->GetPhonemeCode() ) ), IDC_SELECT_PHONEMESLEFT );

			pop->addSeparator();
		}
				
		if ( AreSelectedPhonemesContiguous() && m_nSelectedPhonemeCount > 1 )
		{
			pop->add( va( "Merge phonemes" ), IDC_SNAPPHONEMES );
			if ( m_nSelectedPhonemeCount == 2 )
			{
				pop->add( va( "Separate phonemes" ), IDC_SEPARATEPHONEMES );
			}

			pop->addSeparator();
		}
		
		if ( m_nSelectedPhonemeCount >= 1 )
		{
			pop->add( va( "Delete %s", 
				m_nSelectedPhonemeCount == 1 ? va( "'%s'", ConvertPhoneme( pho->GetPhonemeCode() ) ) : "phonemes" ), IDC_EDIT_DELETEPHONEME );

			pop->addSeparator();
			pop->add( va( "Deselect all" ), IDC_DESELECT_PHONEMESANDWORDS );
		}
	}


	if ( m_Tags.m_Words.Size() > 0 )
	{
		pop->addSeparator();
		pop->add( va( "Cleanup words/phonemes" ), IDC_CLEANUP );
	}

	if ( m_Tags.m_Words.Size() > 0 )
	{
		pop->addSeparator();
		pop->add( va( "Realign words to phonemes" ), IDC_REALIGNWORDS );
	}

	pop->popup( this, mx, my );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
// Output : float
//-----------------------------------------------------------------------------
float PhonemeEditor::GetTimeForPixel( int mx )
{
	RECT rc;
	GetWorkspaceRect( rc );

	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float time = (float)mx / GetPixelsPerSecond() + starttime;

	return time;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : time - 
//			**pp1 - 
//			**pp2 - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhonemeEditor::FindSpanningPhonemes( float time, CPhonemeTag **pp1, CPhonemeTag **pp2 )
{
	Assert( pp1 && pp2 );

	*pp1 = NULL;
	*pp2 = NULL;

	// Three pixels
	double time_epsilon = ( 1.0f / GetPixelsPerSecond() ) * 3;

	CPhonemeTag *previous = NULL;

	for ( int w = 0; w < m_Tags.m_Words.Size(); w++ )
	{
		CWordTag *word = m_Tags.m_Words[ w ];

		for ( int i = 0; i < word->m_Phonemes.Size(); i++ )
		{
			CPhonemeTag *current = word->m_Phonemes[ i ];
			double dt;

			if ( !previous )
			{
				dt = fabs( current->GetStartTime() - time );
				if ( dt < time_epsilon )
				{
					*pp2 = current;
					return true;
				}
			}
			else
			{
				int found = 0;

				dt = fabs( previous->GetEndTime() - time );
				if ( dt < time_epsilon )
				{
					*pp1 = previous;
					found++;
				}

				dt = fabs( current->GetStartTime() - time );
				if ( dt < time_epsilon )
				{
					*pp2 = current;
					found++;
				}

				if ( found != 0 )
				{
					return true;
				}
			}
		
			previous = current;
		}
	}

	if ( m_Tags.m_Words.Size() > 0 )
	{
		// Check last word, but only if it has some phonemes
		CWordTag *lastWord = m_Tags.m_Words[ m_Tags.m_Words.Size() - 1 ];
		if ( lastWord && 
			( lastWord->m_Phonemes.Size() > 0 ) )
		{

			CPhonemeTag *last = lastWord->m_Phonemes[ lastWord->m_Phonemes.Size() - 1 ];
			float dt;
			dt = fabs( last->GetEndTime() - time );
			if ( dt < time_epsilon )
			{
				*pp1 = last;
				return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : time - 
//			**pp1 - 
//			**pp2 - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhonemeEditor::FindSpanningWords( float time, CWordTag **pp1, CWordTag **pp2 )
{
	Assert( pp1 && pp2 );

	*pp1 = NULL;
	*pp2 = NULL;

	// Three pixels
	double time_epsilon = ( 1.0f / GetPixelsPerSecond() ) * 3;

	CWordTag *previous = NULL;
	for ( int i = 0; i < m_Tags.m_Words.Size(); i++ )
	{
		CWordTag *current = m_Tags.m_Words[ i ];
		double dt;

		if ( !previous )
		{
			dt = fabs( current->m_flStartTime - time );
			if ( dt < time_epsilon )
			{
				*pp2 = current;
				return true;
			}
		}
		else
		{
			int found = 0;

			dt = fabs( previous->m_flEndTime - time );
			if ( dt < time_epsilon )
			{
				*pp1 = previous;
				found++;
			}

			dt = fabs( current->m_flStartTime - time );
			if ( dt < time_epsilon )
			{
				*pp2 = current;
				found++;
			}

			if ( found != 0 )
			{
				return true;
			}
		}
	
		previous = current;
	}

	if ( m_Tags.m_Words.Size() > 0 )
	{
		CWordTag *last = m_Tags.m_Words[ m_Tags.m_Words.Size() - 1 ];
		float dt;
		dt = fabs( last->m_flEndTime - time );
		if ( dt < time_epsilon )
		{
			*pp1 = last;
			return true;
		}
	}

	return false;
}

int	PhonemeEditor::FindWordForTime( float time )
{
	for ( int i = 0; i < m_Tags.m_Words.Size(); i++ )
	{
		CWordTag *pCurrent = m_Tags.m_Words[ i ];

		if ( time < pCurrent->m_flStartTime )
			continue;

		if ( time > pCurrent->m_flEndTime )
			continue;

		return i;
	}

	return -1;
}

void PhonemeEditor::FinishWordDrag( int startx, int endx )
{
	float clicktime	= GetTimeForPixel( startx );
	float endtime	= GetTimeForPixel( endx );

	float dt = endtime - clicktime;

	SetDirty( true );

	PushUndo();

	TraverseWords( &PhonemeEditor::ITER_MoveSelectedWords, dt );

	RealignPhonemesToWords( false );
	CleanupWordsAndPhonemes( false );

	PushRedo();

	redraw();
}

void PhonemeEditor::FinishWordMove( int startx, int endx )
{
	float clicktime	= GetTimeForPixel( startx );
	float endtime	= GetTimeForPixel( endx );

	// Find the phonemes who have the closest start/endtime to the starting click time
	CWordTag *current, *next;

	if ( !FindSpanningWords( clicktime, &current, &next ) )
	{
		return;
	}

	SetDirty( true );

	PushUndo();

	if ( current && !next )
	{
		// cap movement
		current->m_flEndTime += ( endtime - clicktime );
	}
	else if ( !current && next )
	{
		// cap movement
		next->m_flStartTime += ( endtime - clicktime );
	}
	else
	{
		// cap movement
		endtime = min( endtime, next->m_flEndTime - 1.0f / GetPixelsPerSecond() );
		endtime = max( endtime, current->m_flStartTime + 1.0f / GetPixelsPerSecond() );

		current->m_flEndTime = endtime;
		next->m_flStartTime = endtime;
	}

	RealignPhonemesToWords( false );
	CleanupWordsAndPhonemes( false );

	PushRedo();

	redraw();
}

CPhonemeTag *PhonemeEditor::FindPhonemeForTime( float time )
{
	for ( int w = 0 ; w < m_Tags.m_Words.Size(); w++ )
	{
		CWordTag *word = m_Tags.m_Words[ w ];


		for ( int i = 0; i < word->m_Phonemes.Size(); i++ )
		{
			CPhonemeTag *pCurrent = word->m_Phonemes[ i ];

			if ( time < pCurrent->GetStartTime() )
				continue;

			if ( time > pCurrent->GetEndTime() )
				continue;

			return pCurrent;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : phoneme - 
//			startx - 
//			endx - 
//-----------------------------------------------------------------------------
void PhonemeEditor::FinishPhonemeDrag( int startx, int endx )
{
	float clicktime	= GetTimeForPixel( startx );
	float endtime	= GetTimeForPixel( endx );

	float dt = endtime - clicktime;

	SetDirty( true );

	PushUndo();

	TraversePhonemes( &PhonemeEditor::ITER_MoveSelectedPhonemes, dt );

	RealignWordsToPhonemes( false );
	CleanupWordsAndPhonemes( false );

	PushRedo();

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : phoneme - 
//			startx - 
//			endx - 
//-----------------------------------------------------------------------------
void PhonemeEditor::FinishPhonemeMove( int startx, int endx )
{
	float clicktime	= GetTimeForPixel( startx );
	float endtime	= GetTimeForPixel( endx );

	// Find the phonemes who have the closest start/endtime to the starting click time
	CPhonemeTag *current, *next;

	if ( !FindSpanningPhonemes( clicktime, &current, &next ) )
	{
		return;
	}

	SetDirty( true );

	PushUndo();

	if ( current && !next )
	{
		// cap movement
		current->AddEndTime( endtime - clicktime );
	}
	else if ( !current && next )
	{
		// cap movement
		next->AddStartTime( endtime - clicktime );
	}
	else
	{
		// cap movement
		endtime = min( endtime, next->GetEndTime() - 1.0f / GetPixelsPerSecond() );
		endtime = max( endtime, current->GetStartTime() + 1.0f / GetPixelsPerSecond() );

		current->SetEndTime( endtime );
		next->SetStartTime( endtime );
	}

	RealignWordsToPhonemes( false );
	CleanupWordsAndPhonemes( false );

	PushRedo();

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dirty - 
//-----------------------------------------------------------------------------
void PhonemeEditor::SetDirty( bool dirty, bool clearundo /*=true*/ )
{
	m_WorkFile.m_bDirty = dirty;

	if ( !dirty && clearundo )
	{
		WipeUndo();
		redraw();
	}

	SetPrefix( dirty ? "* " : "" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhonemeEditor::GetDirty( void )
{
	return m_WorkFile.m_bDirty;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::EditInsertPhonemeBefore( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CPhonemeTag *cp = GetSelectedPhoneme();
	if ( !cp )
		return;

	float gap = GetTimeGapToNextPhoneme( false, cp );
	if ( gap < MINIMUM_PHONEME_GAP )
	{
		Con_Printf( "Can't insert before, gap of %.2f ms is too small\n", 1000.0f * gap );
		return;
	}

	// Don't have really long phonemes
	gap = min( gap, DEFAULT_PHONEME_LENGTH );

	CWordTag *word = m_Tags.GetWordForPhoneme( cp );
	if ( !word )
	{
		Con_Printf( "EditInsertPhonemeBefore:  phoneme not a member of any known word!!!\n" );
		return;
	}

	int clicked = word->IndexOfPhoneme( cp );
	if ( clicked < 0 )
	{
		Con_Printf( "EditInsertPhonemeBefore:  phoneme not a member of any specified word!!!\n" );
		Assert( 0 );
		return;
	}

	CPhonemeTag phoneme;

	CPhonemeParams params;
	memset( &params, 0, sizeof( params ) );
	strcpy( params.m_szDialogTitle, "Phoneme/Viseme Properties" );
	strcpy( params.m_szName, "" );

	int iret = PhonemeProperties( &params );
	SetFocus( (HWND)getHandle() );
	if ( !iret )
	{
		return;
	}

	SetDirty( true );

	PushUndo();

	phoneme.SetPhonemeCode( TextToPhoneme( params.m_szName ) );
	phoneme.SetTag( params.m_szName );

	phoneme.SetEndTime( cp->GetStartTime() );
	phoneme.SetStartTime( cp->GetStartTime() - gap );
	phoneme.m_bSelected = true;
	cp->m_bSelected = false;

	word->m_Phonemes.InsertBefore( clicked, new CPhonemeTag( phoneme ) );

	PushRedo();

	// Add it
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::EditInsertPhonemeAfter( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CPhonemeTag *cp = GetSelectedPhoneme();
	if ( !cp )
		return;

	float gap = GetTimeGapToNextPhoneme( true, cp );
	if ( gap < MINIMUM_PHONEME_GAP )
	{
		Con_Printf( "Can't insert after, gap of %.2f ms is too small\n", 1000.0f * gap );
		return;
	}

	// Don't have really long phonemes
	gap = min( gap, DEFAULT_PHONEME_LENGTH );

	CWordTag *word = m_Tags.GetWordForPhoneme( cp );
	if ( !word )
	{
		Con_Printf( "EditInsertPhonemeAfter:  phoneme not a member of any known word!!!\n" );
		return;
	}

	int clicked = word->IndexOfPhoneme( cp );
	if ( clicked < 0 )
	{
		Con_Printf( "EditInsertPhonemeAfter:  phoneme not a member of any specified word!!!\n" );
		Assert( 0 );
		return;
	}

	CPhonemeTag phoneme;

	CPhonemeParams params;
	memset( &params, 0, sizeof( params ) );
	strcpy( params.m_szDialogTitle, "Phoneme/Viseme Properties" );
	strcpy( params.m_szName, "" );

	int iret = PhonemeProperties( &params );
	SetFocus( (HWND)getHandle() );

	if ( !iret )
	{
		return;
	}

	SetDirty( true );

	PushUndo();

	phoneme.SetPhonemeCode( TextToPhoneme( params.m_szName ) );
	phoneme.SetTag( params.m_szName );

	phoneme.SetEndTime( cp->GetEndTime() + gap );
	phoneme.SetStartTime( cp->GetEndTime() );
	phoneme.m_bSelected = true;
	cp->m_bSelected = false;
	
	word->m_Phonemes.InsertAfter( clicked, new CPhonemeTag( phoneme ) );

	PushRedo();

	// Add it
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::EditInsertWordBefore( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CWordTag *cw = GetSelectedWord();
	if ( !cw )
		return;

	float gap = GetTimeGapToNextWord( false, cw );
	if ( gap < MINIMUM_WORD_GAP )
	{
		Con_Printf( "Can't insert before, gap of %.2f ms is too small\n", 1000.0f * gap );
		return;
	}

	// Don't have really long words
	gap = min( gap, DEFAULT_WORD_LENGTH );

	int clicked = IndexOfWord( cw );
	if ( clicked < 0 )
	{
		Con_Printf( "EditInsertWordBefore:  word not in sentence!!!\n" );
		Assert( 0 );
		return;
	}

	CInputParams params;
	memset( &params, 0, sizeof( params ) );
	strcpy( params.m_szDialogTitle, "Insert Word" );
	strcpy( params.m_szPrompt, "Word:" );
	strcpy( params.m_szInputText, "" );

	params.m_nLeft = -1;
	params.m_nTop = -1;

	params.m_bPositionDialog = true;
	if ( params.m_bPositionDialog )
	{
		RECT rcWord;
		GetWordRect( cw, rcWord );

		// Convert to screen coords
		POINT pt;
		pt.x = rcWord.left;
		pt.y = rcWord.top;

		ClientToScreen( (HWND)getHandle(), &pt );

		params.m_nLeft	= pt.x;
		params.m_nTop	= pt.y;
	}

	int iret = InputProperties( &params );
	SetFocus( (HWND)getHandle() );
	if ( !iret )
	{
		return;
	}

	if ( strlen( params.m_szInputText ) <= 0 )
	{
		return;
	}

	int wordCount = CSentence::CountWords( params.m_szInputText );
	if ( wordCount > 1 )
	{
		Con_Printf( "Can only insert one word at a time, %s has %i words in it!\n",
			params.m_szInputText, wordCount );
		return;
	}

	SetDirty( true );

	PushUndo();

	CWordTag newword;

	newword.SetWord( params.m_szInputText );

	newword.m_flEndTime		= cw->m_flStartTime;
	newword.m_flStartTime	= cw->m_flStartTime - gap;
	newword.m_bSelected = true;
	cw->m_bSelected = false;

	m_Tags.m_Words.InsertBefore( clicked, new CWordTag( newword ) );

	PushRedo();

	// Add it
	redraw();

	// Jump to phoneme insertion UI
	EditInsertFirstPhonemeOfWord();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::EditInsertWordAfter( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CWordTag *cw = GetSelectedWord();
	if ( !cw )
		return;

	float gap = GetTimeGapToNextWord( true, cw );
	if ( gap < MINIMUM_WORD_GAP )
	{
		Con_Printf( "Can't insert after, gap of %.2f ms is too small\n", 1000.0f * gap );
		return;
	}

	// Don't have really long words
	gap = min( gap, DEFAULT_WORD_LENGTH );

	int clicked = IndexOfWord( cw );
	if ( clicked < 0 )
	{
		Con_Printf( "EditInsertWordBefore:  word not in sentence!!!\n" );
		Assert( 0 );
		return;
	}

	CInputParams params;
	memset( &params, 0, sizeof( params ) );
	strcpy( params.m_szDialogTitle, "Insert Word" );
	strcpy( params.m_szPrompt, "Word:" );
	strcpy( params.m_szInputText, "" );

	params.m_nLeft = -1;
	params.m_nTop = -1;

	params.m_bPositionDialog = true;
	if ( params.m_bPositionDialog )
	{
		RECT rcWord;
		GetWordRect( cw, rcWord );

		// Convert to screen coords
		POINT pt;
		pt.x = rcWord.left;
		pt.y = rcWord.top;

		ClientToScreen( (HWND)getHandle(), &pt );

		params.m_nLeft	= pt.x;
		params.m_nTop	= pt.y;
	}

	int iret = InputProperties( &params );
	SetFocus( (HWND)getHandle() );
	if ( !iret )
	{
		return;
	}

	if ( strlen( params.m_szInputText ) <= 0 )
	{
		return;
	}

	int wordCount = CSentence::CountWords( params.m_szInputText );
	if ( wordCount > 1 )
	{
		Con_Printf( "Can only insert one word at a time, %s has %i words in it!\n",
			params.m_szInputText, wordCount );
		return;
	}

	SetDirty( true );

	PushUndo();

	CWordTag newword;

	newword.SetWord( params.m_szInputText );

	newword.m_flEndTime		= cw->m_flEndTime + gap;
	newword.m_flStartTime	= cw->m_flEndTime;
	newword.m_bSelected = true;
	cw->m_bSelected = false;
	
	CWordTag *w = new CWordTag( newword );
	Assert( w );
	if ( w )
	{
		m_Tags.m_Words.InsertAfter( clicked, w );
	}

	PushRedo();

	// Add it
	redraw();

	EditInsertFirstPhonemeOfWord();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::EditDeletePhoneme( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CountSelected();

	if ( m_nSelectedPhonemeCount < 1 )
	{
		return;
	}

	SetDirty( true );

	PushUndo();

	for ( int i = m_Tags.m_Words.Size() - 1; i >= 0; i-- )
	{
		CWordTag *word = m_Tags.m_Words[ i ];
		if ( !word )
			continue;

		for ( int j = word->m_Phonemes.Size() - 1; j >= 0; j-- )
		{
			CPhonemeTag *p = word->m_Phonemes[ j ];
			if ( !p || !p->m_bSelected )
				continue;

			// Delete it
			word->m_Phonemes.Remove( j );
		}
	}

	PushRedo();

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::EditDeleteWord( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CountSelected();

	if ( m_nSelectedWordCount < 1 )
	{
		return;
	}

	SetDirty( true );

	PushUndo();

	for ( int i = m_Tags.m_Words.Size() - 1; i >= 0; i-- )
	{
		CWordTag *word = m_Tags.m_Words[ i ];
		if ( !word || !word->m_bSelected )
			continue;

		m_Tags.m_Words.Remove( i );
	}

	PushRedo();

	redraw();
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::PlayEditedWave( bool selection /* = false */ )
{
	StopPlayback();

	if ( !m_pWaveFile )
		return;

	// Make sure phonemes are loaded
	FacePoser_EnsurePhonemesLoaded();

	SaveLinguisticData();

	SetScrubTime( 0.0f );
	SetScrubTargetTime( m_pWaveFile->GetRunningLength() );
}

typedef struct channel_s
{
	int		leftvol;
	int		rightvol;
	int		rleftvol;
	int		rrightvol;
	float	pitch;
} channel_t;

bool PhonemeEditor::CreateCroppedWave( char const *filename, int startsample, int endsample )
{
	Assert( sound );

	CAudioWaveOutput *pWaveOutput = ( CAudioWaveOutput * )sound->GetAudioOutput();
	if ( !pWaveOutput )
		return false;

	CAudioSource *wave = sound->LoadSound( m_WorkFile.m_szWaveFile );
	if ( !wave )
		return false;

	CAudioMixer *pMixer = wave->CreateMixer();
	if ( !pMixer )
		return false;

	// Create out put file
	OutFileRIFF riffout( filename, io_out );
	// Create output iterator
	IterateOutputRIFF store( riffout );

	WAVEFORMATEX format;
	format.cbSize = sizeof( format );

	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nAvgBytesPerSec = (int)wave->SampleRate();
	format.nChannels = 1;
	format.wBitsPerSample = 8;
	format.nSamplesPerSec = (int)wave->SampleRate();
	format.nBlockAlign = 1; // (int)wave->SampleSize();

	store.ChunkWrite( WAVE_FMT, &format, sizeof( format ) );

	// Pull in data and write it out

	int currentsample = 0;

	store.ChunkStart( WAVE_DATA );

	// need a bit of space
	short samples[ 2 ];
	channel_t channel;
	channel.leftvol = 255;
	channel.rightvol = 255;
	channel.pitch = 1.0;

	while ( 1 )
	{
		pWaveOutput->m_audioDevice.MixBegin();

		if ( !pMixer->MixDataToDevice( &pWaveOutput->m_audioDevice, &channel, currentsample, 1, wave->SampleRate(), true ) )
			break;

		pWaveOutput->m_audioDevice.TransferBufferStereo16( samples, 1 );

		currentsample = pMixer->GetSamplePosition();

		if ( currentsample >= startsample && currentsample <= endsample )
		{
			// left + right (2 channels ) * 16 bits
			float s1 = (float)( samples[ 0 ] >> 8 );
			float s2 = (float)( samples[ 1 ] >> 8 );

			float avg = ( s1 + s2 ) / 2.0f;
			unsigned char chopped = (unsigned char)( avg + 127.0f );

			store.ChunkWriteData( &chopped, sizeof( byte ) );
		}
	}

	store.ChunkFinish();

	delete pMixer;
	delete wave;

	return true;
}

void PhonemeEditor::SentenceFromString( CSentence& sentence, char const *str )
{
	sentence.Reset();

	if ( !str || !str[0] || CSentence::CountWords( str ) == 0 )
	{
		return;
	}

	char word[ 256 ];
	unsigned char const *in = (unsigned char *)str;
	char *out = word;
	
	while ( *in )
	{
		if ( *in > 32 )
		{
			*out++ = *in++;
		}
		else
		{
			*out = 0;

			while ( *in && *in <= 32 )
			{
				in++;
			}
			
			if ( strlen( word ) > 0 )
			{
				CWordTag *w = new CWordTag( (char *)word );
				Assert( w );
				if ( w )
				{
					sentence.m_Words.AddToTail( w );
				}
			}
			
			out = word;
		}
	}
	
	*out = 0;
	if ( strlen( word ) > 0 )
	{
		CWordTag *w = new CWordTag( (char *)word );
		Assert( w );
		if ( w )
		{
			sentence.m_Words.AddToTail( w );
		}
	}
	
	sentence.SetText( str );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::RedoPhonemeExtractionSelected( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	if ( !CheckSpeechAPI() )
		return;

	if ( !m_pWaveFile )
	{
		Con_Printf( "Can't redo extraction, no wavefile loaded!\n" );
		Assert( 0 );
		return;
	}

	if ( !m_bSelectionActive )
	{
		Con_Printf( "Please select a portion of the .wav from which to re-extract phonemes\n" );
		return;
	}

	// Now copy data back into original list, offsetting by samplestart time
	float numsamples = m_pWaveFile->GetRunningLength() * m_pWaveFile->SampleRate();
	float selectionstarttime = 0.0f;
	if ( numsamples > 0.0f )
	{
		// Convert sample #'s to time
		selectionstarttime = ( m_nSelection[ 0 ] / numsamples ) * m_pWaveFile->GetRunningLength();
		selectionstarttime = max( 0.0f, selectionstarttime );
	}
	else
	{
		Con_Printf( "Original .wav file %s has no samples!!!\n", m_WorkFile.m_szWaveFile );
		return;
	}

	int i;
	// Create input array of just selected words
	CSentence m_InputWords;
	CSentence m_Results;

	CountSelected();
	
	bool usingselection = true;

	if ( m_nSelectedWordCount == 0 )
	{
		// Allow user to type in text
		// Build word string
		char wordstring[ 1024 ];
		strcpy( wordstring, "" );

		CInputParams params;
		memset( &params, 0, sizeof( params ) );
		strcpy( params.m_szDialogTitle, "Phrase Word List" );
		strcpy( params.m_szPrompt, "Phrase" );

		strcpy( params.m_szInputText, wordstring );

		if ( !InputProperties( &params ) )
			return;

		if ( strlen( params.m_szInputText ) <= 0 )
		{
			Con_ErrorPrintf( "Edit word list:  No words entered!\n" );
			return;
		}

		SentenceFromString( m_InputWords, params.m_szInputText );

		if ( m_InputWords.m_Words.Size() == 0 )
		{
			Con_Printf( "You must either select words, or type in a set of words in order to extract phonemes!\n" );
			return;
		}

		usingselection = false;
	}
	else
	{
		if ( !AreSelectedWordsContiguous() )
		{
			Con_Printf( "Can only redo extraction on a contiguous subset of words\n" );
			return;
		}

		char temp[ 4096 ];
		bool killspace = false;
		Q_strncpy( temp, m_InputWords.GetText(), sizeof( temp ) );

		// Iterate existing words, looking for contiguous selected words
		for ( i = 0; i < m_Tags.m_Words.Size(); i++ )
		{
			CWordTag *word = m_Tags.m_Words[ i ];
			if ( !word || !word->m_bSelected )
				continue;

			// Now add "clean slate" to input list
			m_InputWords.m_Words.AddToTail( new CWordTag( *word ) );

			Q_strncat( temp, word->GetWord(), sizeof( temp ), COPY_ALL_CHARACTERS );
			Q_strncat( temp, " ", sizeof( temp ), COPY_ALL_CHARACTERS );
			killspace = true;
		}

		// Kill terminal space character
		int len = Q_strlen( temp );
		if ( killspace && ( len >= 1 ) )
		{
			Assert( temp[ len -1 ] == ' ' );
			temp[ len - 1 ] = 0;
		}

		m_InputWords.SetText( temp );
	}

	m_nLastExtractionResult		= SR_RESULT_NORESULT;

	char szCroppedFile[ 512 ];
	char szBaseFile[ 512 ];
	Q_StripExtension( m_WorkFile.m_szWaveFile, szBaseFile, sizeof( szBaseFile ) );
	Q_snprintf( szCroppedFile, sizeof( szCroppedFile ), "%s%s_work1.wav", m_WorkFile.m_szBasePath, szBaseFile );

	filesystem->RemoveFile( szCroppedFile, "GAME" );

	if ( !CreateCroppedWave( szCroppedFile, m_nSelection[ 0 ], m_nSelection[ 1 ] ) )
	{
		Con_Printf( "Unable to create cropped wave file %s from samples %i to %i\n",
			szCroppedFile,
			m_nSelection[ 0 ],
			m_nSelection[ 1 ] );
		return;
	}

	CAudioSource *m_pCroppedWave = sound->LoadSound( szCroppedFile );
	if ( !m_pCroppedWave )
	{
		Con_Printf( "Unable to load cropped wave file %s from samples %i to %i\n",
			szCroppedFile,
			m_nSelection[ 0 ],
			m_nSelection[ 1 ] );
		return;
	}

	// Save any pending stuff
	SaveLinguisticData();

	// Store off copy of complete sentence
	m_TagsExt = m_Tags;

	char filename[ 512 ];
	Q_snprintf( filename, sizeof( filename ), "%s%s", m_WorkFile.m_szBasePath, szCroppedFile );

	m_nLastExtractionResult = m_pPhonemeExtractor->Extract( 
		filename,
		(int)( m_pCroppedWave->GetRunningLength() * m_pCroppedWave->SampleRate() * m_pCroppedWave->TrueSampleSize() ),
		Con_Printf,
		m_InputWords,
		m_Results );

	if ( m_InputWords.m_Words.Size() != m_Results.m_Words.Size() )
	{
		Con_Printf( "Extraction returned %i words, source had %i, try adjusting selection\n",
			m_Results.m_Words.Size(), m_InputWords.m_Words.Size() );

		filesystem->RemoveFile( filename, "GAME" );

		redraw();
		return;
	}

	float bytespersecond = m_pCroppedWave->SampleRate() * m_pCroppedWave->TrueSampleSize();

	// Tracker 57389:
	// Total hack to fix a bug where the Lipsinc extractor is messing up the # channels on 16 bit stereo waves
	if ( m_pPhonemeExtractor->GetAPIType() == SPEECH_API_LIPSINC &&
		m_pCroppedWave->IsStereoWav() && 
		m_pCroppedWave->SampleSize() == 16 )
	{
		bytespersecond *= 2.0f;
	}

	// Now convert byte offsets to times
	for ( i = 0; i < m_Results.m_Words.Size(); i++ )
	{
		CWordTag *tag = m_Results.m_Words[ i ];
		Assert( tag );
		if ( !tag )
			continue;

		tag->m_flStartTime = ( float )(tag->m_uiStartByte ) / bytespersecond;
		tag->m_flEndTime = ( float )(tag->m_uiEndByte ) / bytespersecond;

		for ( int j = 0; j < tag->m_Phonemes.Size(); j++ )
		{
			CPhonemeTag *ptag = tag->m_Phonemes[ j ];
			Assert( ptag );
			if ( !ptag )
				continue;

			ptag->SetStartTime( ( float )(ptag->m_uiStartByte ) / bytespersecond );
			ptag->SetEndTime( ( float )(ptag->m_uiEndByte ) / bytespersecond );
		}
	}

	if ( usingselection )
	{
		// Copy data into m_TagsExt, offseting times by selectionstarttime
		CWordTag *from;
		CWordTag *to;

		int fromWord = 0;

		for ( i = 0; i < m_TagsExt.m_Words.Size() ; i++ )
		{
			to = m_TagsExt.m_Words[ i ];
			if ( !to || !to->m_bSelected )
				continue;

			// Found start of contiguous run
			if ( fromWord >= m_Results.m_Words.Size() )
				break;

			from = m_Results.m_Words[ fromWord++ ];
			Assert( from );
			if ( !from )
				continue;

			// Remove all phonemes from destination
			while ( to->m_Phonemes.Size() > 0 )
			{
				CPhonemeTag *p = to->m_Phonemes[ 0 ];
				Assert( p );
				to->m_Phonemes.Remove( 0 );
				delete p;
			}

			// Now copy phonemes from source
			for ( int j = 0; j < from->m_Phonemes.Size(); j++ )
			{
				CPhonemeTag *fromPhoneme = from->m_Phonemes[ j ];
				Assert( fromPhoneme );
				if ( !fromPhoneme )
					continue;

				CPhonemeTag newPhoneme( *fromPhoneme );
				// Offset start time
				newPhoneme.AddStartTime( selectionstarttime );
				newPhoneme.AddEndTime( selectionstarttime );

				// Add it back in with corrected timing data
				CPhonemeTag *p = new CPhonemeTag( newPhoneme );
				Assert( p );
				if ( p )
				{
					to->m_Phonemes.AddToTail( p );
				}
			}

			// Done
			if ( fromWord >= m_Results.m_Words.Size() )
				break;
		}

	}
	else
	{
		// Find word just before starting point of selection and
		//  place input words into list starting that that point

		int startWord = 0;

		CWordTag *firstWordOfPhrase = m_Results.m_Words[ 0 ];
		Assert( firstWordOfPhrase );
		
		for ( ; startWord < m_TagsExt.m_Words.Size(); startWord++ )
		{
			CWordTag *w = m_TagsExt.m_Words[ startWord ];
			Assert( w );
			if ( !w )
				continue;
			
			if ( w->m_flStartTime > firstWordOfPhrase->m_flStartTime + selectionstarttime )
				break;
		}

		for ( i = 0; i < m_Results.m_Words.Size(); i++ )
		{
			CWordTag *from = m_Results.m_Words[ i ];
			Assert( from );
			if ( !from )
				continue;

			CWordTag *to = new CWordTag( *from );
			Assert( to );

			to->m_flStartTime	+= selectionstarttime;
			to->m_flEndTime		+= selectionstarttime;

			// Now adjust phoneme times
			for ( int j = 0; j < to->m_Phonemes.Size(); j++ )
			{
				CPhonemeTag *toPhoneme = to->m_Phonemes[ j ];
				Assert( toPhoneme );
				if ( !toPhoneme )
					continue;

				// Offset start time
				toPhoneme->AddStartTime( selectionstarttime );
				toPhoneme->AddEndTime( selectionstarttime );
			}

			m_TagsExt.m_Words.InsertBefore( startWord++, to );
		}
	}

	Con_Printf( "Cleaning up...\n" );
	filesystem->RemoveFile( filename, "GAME" );

	SetFocus( (HWND)getHandle() );

	redraw();
}

void PhonemeEditor::RedoPhonemeExtraction( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	if ( !CheckSpeechAPI() )
		return;

	m_nLastExtractionResult		= SR_RESULT_NORESULT;

	if ( !m_pWaveFile )
		return;

	SaveLinguisticData();

	// Send m_WorkFile.m_szWorkingFile to extractor and retrieve resulting data
	// 

	m_TagsExt.Reset();

	Assert( m_pPhonemeExtractor );

	char filename[ 512 ];
	Q_snprintf( filename, sizeof( filename ), "%s%s", m_WorkFile.m_szBasePath, m_WorkFile.m_szWorkingFile );

	m_nLastExtractionResult = m_pPhonemeExtractor->Extract( 
		filename,
		(int)( m_pWaveFile->GetRunningLength() * m_pWaveFile->SampleRate() * m_pWaveFile->TrueSampleSize() ),
		Con_Printf,
		m_Tags,
		m_TagsExt );

	float bytespersecond = m_pWaveFile->SampleRate() * m_pWaveFile->TrueSampleSize();

	// Now convert byte offsets to times
	int i;
	for ( i = 0; i < m_TagsExt.m_Words.Size(); i++ )
	{
		CWordTag *tag = m_TagsExt.m_Words[ i ];
		Assert( tag );
		if ( !tag )
			continue;

		tag->m_flStartTime = ( float )(tag->m_uiStartByte ) / bytespersecond;
		tag->m_flEndTime = ( float )(tag->m_uiEndByte ) / bytespersecond;

		for ( int j = 0; j < tag->m_Phonemes.Size(); j++ )
		{
			CPhonemeTag *ptag = tag->m_Phonemes[ j ];
			Assert( ptag );
			if ( !ptag )
				continue;

			ptag->SetStartTime( ( float )(ptag->m_uiStartByte ) / bytespersecond );
			ptag->SetEndTime( ( float )(ptag->m_uiEndByte ) / bytespersecond );
		}
	}

	SetFocus( (HWND)getHandle() );

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::Deselect( void )
{
	m_nSelection[ 0 ] = m_nSelection[ 1 ] = 0;
	m_bSelectionActive = false;
}

void PhonemeEditor::ITER_SelectSpanningWords( CWordTag *word, float amount )
{
	Assert( word );
	word->m_bSelected = false;

	if ( !m_bSelectionActive )
		return;

	if ( !m_pWaveFile )
		return;

	float numsamples = m_pWaveFile->GetRunningLength() * m_pWaveFile->SampleRate();
	if ( numsamples > 0.0f )
	{
		// Convert sample #'s to time
		float starttime = ( m_nSelection[ 0 ] / numsamples ) * m_pWaveFile->GetRunningLength();
		float endtime	= ( m_nSelection[ 1 ] / numsamples ) * m_pWaveFile->GetRunningLength();

		if ( word->m_flEndTime >= starttime &&
			 word->m_flStartTime <= endtime )
		{
			word->m_bSelected = true;

			m_bWordsActive = true;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : start - 
//			end - 
//-----------------------------------------------------------------------------
void PhonemeEditor::SelectSamples( int start, int end )
{
	if ( !m_pWaveFile )
		return;

	// Make sure order is correct
	if ( end < start )
	{
		int temp = end;
		end = start;
		start = temp;
	}

	Deselect();

	m_nSelection[ 0 ] = start;
	m_nSelection[ 1 ] = end;
	m_bSelectionActive = true;

	// Select any words that span the selection
	//
	TraverseWords( &PhonemeEditor::ITER_SelectSpanningWords, 0.0f );

	redraw();
}

void PhonemeEditor::FinishMoveSelection( int startx, int mx )
{
	if ( !m_pWaveFile )
		return;

	int sampleStart = GetSampleForMouse( startx );
	int sampleEnd = GetSampleForMouse( mx );

	int delta = sampleEnd - sampleStart;

	for ( int i = 0; i < 2; i++ )
	{
		m_nSelection[ i ] += delta;
	}

	// Select any words that span the selection
	//
	TraverseWords( &PhonemeEditor::ITER_SelectSpanningWords, 0.0f );

	redraw();
}

void PhonemeEditor::FinishMoveSelectionStart( int startx, int mx )
{
	if ( !m_pWaveFile )
		return;

	int sampleStart = GetSampleForMouse( startx );
	int sampleEnd = GetSampleForMouse( mx );

	int delta = sampleEnd - sampleStart;

	m_nSelection[ 0 ] += delta;

	if ( m_nSelection[ 0 ] >= m_nSelection[ 1 ] )
	{
		Deselect();
	}

	// Select any words that span the selection
	//
	TraverseWords( &PhonemeEditor::ITER_SelectSpanningWords, 0.0f );

	redraw();
}

void PhonemeEditor::FinishMoveSelectionEnd( int startx, int mx )
{
	if ( !m_pWaveFile )
		return;

	int sampleStart = GetSampleForMouse( startx );
	int sampleEnd = GetSampleForMouse( mx );

	int delta = sampleEnd - sampleStart;

	m_nSelection[ 1 ] += delta;

	if ( m_nSelection[ 1 ] <= m_nSelection[ 0 ] )
	{
		Deselect();
	}

	// Select any words that span the selection
	//
	TraverseWords( &PhonemeEditor::ITER_SelectSpanningWords, 0.0f );

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : startx - 
//			mx - 
//-----------------------------------------------------------------------------
void PhonemeEditor::FinishSelect( int startx, int mx )
{
	if ( !m_pWaveFile )
		return;

	// Don't select really small areas
	if ( abs( startx - mx ) < 2 )
		return;

	int sampleStart = GetSampleForMouse( startx );
	int sampleEnd = GetSampleForMouse( mx );

	SelectSamples( sampleStart, sampleEnd );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhonemeEditor::IsMouseOverSamples( int mx, int my )
{
	if ( GetMode() != MODE_PHONEMES )
		return false;

	// Deterime if phoneme boundary is under the cursor
	//
	if ( !m_pWaveFile )
		return false;

	RECT rc;
	GetWorkspaceRect( rc );

	// Over tag
	if ( my >= TAG_TOP && my <= TAG_BOTTOM )
		return false;

	if ( IsMouseOverPhonemeRow( my ) )
		return false;

	if ( IsMouseOverWordRow( my ) )
		return false;

	RECT rcWord;
	GetWordTrayTopBottom( rcWord );
	RECT rcPhoneme;
	GetPhonemeTrayTopBottom( rcPhoneme );

	if ( my < rcWord.bottom )
		return false;

	if ( my > rcPhoneme.top )
		return false;

	return true;
}

void PhonemeEditor::GetScreenStartAndEndTime( float &starttime, float& endtime )
{
	starttime = m_nLeftOffset / GetPixelsPerSecond();
	endtime = w2() / GetPixelsPerSecond() + starttime;
}

float PhonemeEditor::GetTimePerPixel( void )
{
	RECT rc;
	GetWorkspaceRect( rc );

	float starttime, endtime;
	GetScreenStartAndEndTime( starttime, endtime );

	if ( rc.right - rc.left <= 0 )
	{
		return ( endtime - starttime );
	}

	float timeperpixel = ( endtime - starttime ) / (float)( rc.right - rc.left );
	return timeperpixel;
}

int PhonemeEditor::GetPixelForSample( int sample )
{
	RECT rc;
	GetWorkspaceRect( rc );

	if ( !m_pWaveFile )
		return rc.left;

	// Determine start/stop positions
	int totalsamples = (int)( m_pWaveFile->GetRunningLength() * m_pWaveFile->SampleRate() );
	if ( totalsamples <= 0 )
	{
		return rc.left;
	}

	float starttime, endtime;
	GetScreenStartAndEndTime( starttime, endtime );

	float sampleFrac = (float)sample / (float)totalsamples;
	float sampleTime = sampleFrac * (float)m_pWaveFile->GetRunningLength();

	if ( endtime - starttime < 0.0f )
	{
		return rc.left;
	}

	float windowFrac = ( sampleTime - starttime ) / ( endtime - starttime );

	return rc.left + (int)( windowFrac * ( rc.right - rc.left ) );
}

int PhonemeEditor::GetSampleForMouse( int mx )
{
	if ( !m_pWaveFile )
		return 0;

	RECT rc;
	GetWorkspaceRect( rc );

	// Determine start/stop positions
	int totalsamples = (int)( m_pWaveFile->GetRunningLength() * m_pWaveFile->SampleRate() );

	float starttime, endtime;
	GetScreenStartAndEndTime( starttime, endtime );

	if ( GetPixelsPerSecond() <= 0 )
		return 0;

	// Start and end times
	float clickTime	= (float)mx / GetPixelsPerSecond() + starttime;

	// What sample do these correspond to
	if ( (float)m_pWaveFile->GetRunningLength() <= 0.0f )
		return 0;

	int sampleNumber	= (int) ( (float)totalsamples * clickTime / (float)m_pWaveFile->GetRunningLength() );

	return sampleNumber;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhonemeEditor::IsMouseOverSelection( int mx, int my )
{
	if ( GetMode() != MODE_PHONEMES )
		return false;

	if ( !m_pWaveFile )
		return false;

	if ( !m_bSelectionActive )
		return false;

	if ( !IsMouseOverSamples( mx, my ) )
		return false;

	int sampleNumber = GetSampleForMouse( mx );

	if ( sampleNumber >= m_nSelection[ 0 ] - 3 &&
		 sampleNumber <= m_nSelection[ 1 ] + 3 )
	{
		return true;
	}

	return false;
}

bool PhonemeEditor::IsMouseOverSelectionStartEdge( mxEvent *event )
{
	if ( GetMode() != MODE_PHONEMES )
		return false;

	if ( !m_pWaveFile )
		return false;

	int mx, my;
	mx = (short)event->x;
	my = (short)event->y;

	if ( !(event->modifiers & mxEvent::KeyCtrl ) )
		return false;

	if ( !IsMouseOverSelection( mx, my ) )
		return false;

	int sample = GetSampleForMouse( mx );

	int		mouse_tolerance = 5;

	RECT rc;
	GetWorkspaceRect( rc );

	// Determine start/stop positions
	float timeperpixel = GetTimePerPixel();

	int samplesperpixel = (int)( timeperpixel * m_pWaveFile->SampleRate() );

	if ( abs( sample - m_nSelection[ 0 ] ) < mouse_tolerance * samplesperpixel )
	{
		return true;
	}
	return false;
}

bool PhonemeEditor::IsMouseOverSelectionEndEdge( mxEvent *event )
{
	if ( GetMode() != MODE_PHONEMES )
		return false;

	if ( !m_pWaveFile )
		return false;

	int mx, my;
	mx = (short)event->x;
	my = (short)event->y;

	if ( !(event->modifiers & mxEvent::KeyCtrl ) )
		return false;

	if ( !IsMouseOverSelection( mx, my ) )
		return false;

	int sample = GetSampleForMouse( mx );

	int		mouse_tolerance = 5;

	RECT rc;
	GetWorkspaceRect( rc );

	if ( GetPixelsPerSecond() <= 0.0f )
		return false;

	if ( ( rc.right - rc.left ) <= 0 )
		return false;

	// Determine start/stop positions
	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float endtime = w2() / GetPixelsPerSecond() + starttime;

	float timeperpixel = ( endtime - starttime ) / (float)( rc.right - rc.left );
	int samplesperpixel = (int)( timeperpixel * m_pWaveFile->SampleRate() );

	if ( abs( sample - m_nSelection[ 1 ] ) < mouse_tolerance * samplesperpixel )
	{
		return true;
	}
	return false;
}

void PhonemeEditor::OnImport()
{
	char filename[ 512 ];
	if ( !FacePoser_ShowOpenFileNameDialog( filename, sizeof( filename ), "sound", "*" WORD_DATA_EXTENSION ) )
	{
		return;
	}

	ImportValveDataChunk( filename );
}

void PhonemeEditor::OnExport()
{
	if ( !m_pWaveFile )
		return;

	char filename[ 512 ];
	if ( !FacePoser_ShowSaveFileNameDialog( filename, sizeof( filename ), "sound", "*" WORD_DATA_EXTENSION ) )
	{
		return;
	}

	Q_SetExtension( filename, WORD_DATA_EXTENSION, sizeof( filename ) );

	ExportValveDataChunk( filename );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : store - 
//-----------------------------------------------------------------------------
void PhonemeEditor::StoreValveDataChunk( IterateOutputRIFF& store )
{
	// Buffer and dump data
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	m_Tags.SaveToBuffer( buf );

	// Copy into store
	store.ChunkWriteData( buf.Base(), buf.TellPut() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *tempfile - 
//-----------------------------------------------------------------------------
void PhonemeEditor::ExportValveDataChunk( char const *tempfile )
{
	if ( m_Tags.m_Words.Count() <= 0 )
	{
		Con_ErrorPrintf( "PhonemeEditor::ExportValveDataChunk:  Sentence has no word data\n" );
		return;
	}

	FileHandle_t fh = filesystem->Open( tempfile, "wb" );
	if ( !fh )
	{
		Con_ErrorPrintf( "PhonemeEditor::ExportValveDataChunk:  Unable to write to %s (read-only?)\n", tempfile );
		return;
	}
	else
	{
		// Buffer and dump data
		CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

		m_Tags.SaveToBuffer( buf );

		filesystem->Write( buf.Base(), buf.TellPut(), fh );
		filesystem->Close(fh);

		Con_Printf( "Exported %i words to %s\n", m_Tags.m_Words.Count(), tempfile );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *tempfile - 
//-----------------------------------------------------------------------------
void PhonemeEditor::ImportValveDataChunk( char const *tempfile )
{
	FileHandle_t fh = filesystem->Open( tempfile, "rb" );
	if ( !fh )
	{
		Con_ErrorPrintf( "PhonemeEditor::ImportValveDataChunk:  Unable to read from %s\n", tempfile );
		return;
	}

	int len = filesystem->Size( fh );
	if ( len <= 4 )
	{
		Con_ErrorPrintf( "PhonemeEditor::ImportValveDataChunk:  File %s has length 0\n", tempfile );
		return;
	}

	ClearExtracted();

	unsigned char *buf = new unsigned char[ len + 1 ];

	filesystem->Read( buf, len, fh );
	filesystem->Close( fh );

	m_TagsExt.InitFromDataChunk( (void *)( buf ), len );

	delete[] buf;

	Con_Printf( "Imported %i words from %s\n", m_TagsExt.m_Words.Count(), tempfile );

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: Copy file over, but update phoneme lump with new data
//-----------------------------------------------------------------------------
void PhonemeEditor::SaveLinguisticData( void )
{
	if ( !m_pWaveFile )
		return;

	InFileRIFF riff( m_WorkFile.m_szWaveFile, io_in );
	Assert( riff.RIFFName() == RIFF_WAVE );

	// set up the iterator for the whole file (root RIFF is a chunk)
	IterateRIFF walk( riff, riff.RIFFSize() );

	char fullout[ 512 ];
	Q_snprintf( fullout, sizeof( fullout ), "%s%s", m_WorkFile.m_szBasePath, m_WorkFile.m_szWorkingFile );

	OutFileRIFF riffout( fullout, io_out );

	IterateOutputRIFF store( riffout );

	bool formatset = false;
	WAVEFORMATEX format;

	bool wordtrackwritten = false;

	// Walk input chunks and copy to output
	while ( walk.ChunkAvailable() )
	{
		unsigned int originalPos = store.ChunkGetPosition();

		store.ChunkStart( walk.ChunkName() );

		bool skipchunk = false;

		switch ( walk.ChunkName() )
		{
		case WAVE_VALVEDATA:
			// Overwrite data
			StoreValveDataChunk( store );
			wordtrackwritten = true;
			break;
		case WAVE_FMT:
			{
				formatset = true;
				
				char *buffer = new char[ walk.ChunkSize() ];
				Assert( buffer );
				walk.ChunkRead( buffer );

				format = *(WAVEFORMATEX *)buffer;

				store.ChunkWriteData( buffer, walk.ChunkSize() );

				delete[] buffer;
			}
			break;
		case WAVE_DATA:
			{
				Assert( formatset );
				
				char *buffer = new char[ walk.ChunkSize() ];
				Assert( buffer );
				
				walk.ChunkRead( buffer );
				// Resample it
				ResampleChunk( store, (void *)&format, walk.ChunkName(), buffer, walk.ChunkSize() );

				delete[] buffer;
			}
			break;
		default:
			store.CopyChunkData( walk );
			break;
		}

		store.ChunkFinish();
		if ( skipchunk )
		{
			store.ChunkSetPosition( originalPos );
		}

		walk.ChunkNext();
	}

	if ( !wordtrackwritten )
	{
		store.ChunkStart( WAVE_VALVEDATA );
		StoreValveDataChunk( store );
		store.ChunkFinish();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Copy phoneme data in from wave file we sent for resprocessing
//-----------------------------------------------------------------------------
void PhonemeEditor::RetrieveLinguisticData( void )
{
	if ( !m_pWaveFile )
		return;

	m_Tags.Reset();

	ReadLinguisticTags();

	redraw();
}

bool PhonemeEditor::StopPlayback( void )
{
	bool bret = false;
	if ( m_pWaveFile )
	{
		SetScrubTargetTime( m_flScrub );

		if ( sound->IsSoundPlaying( m_pMixer ) )
		{
			sound->StopAll();
			bret = true;
		}
	}

	sound->Flush();

	return bret;
}

CPhonemeTag *PhonemeEditor::GetPhonemeTagUnderMouse( int mx, int my )
{
	if ( GetMode() != MODE_PHONEMES )
		return NULL;

	if ( !m_pWaveFile )
		return NULL;

	// FIXME:  Don't read from file, read from arrays after LISET finishes
	// Deterime if phoneme boundary is under the cursor
	//
	RECT rc;
	GetWorkspaceRect( rc );

	if ( !IsMouseOverPhonemeRow( my ) )
		return NULL;

	if ( GetPixelsPerSecond() <= 0 )
		return NULL;

	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float endtime = w2() / GetPixelsPerSecond() + starttime;

	if ( endtime - starttime <= 0.0f )
		return NULL;

	for ( int i = 0; i < m_Tags.m_Words.Size(); i++ )
	{
		CWordTag *word = m_Tags.m_Words[ i ];
		Assert( word );
		if ( !word )
			continue;

		for ( int k = 0; k < word->m_Phonemes.Size(); k++ )
		{
			CPhonemeTag *pPhoneme = word->m_Phonemes[ k ];
			Assert( pPhoneme );
			if ( !pPhoneme )
				continue;

			float t1 = pPhoneme->GetStartTime();
			float t2 = pPhoneme->GetEndTime();

			float frac1 = ( t1 - starttime ) / ( endtime - starttime );
			float frac2 = ( t2 - starttime ) / ( endtime - starttime );

			frac1 = min( 1.0f, frac1 );
			frac1 = max( 0.0f, frac1 );
			frac2 = min( 1.0f, frac2 );
			frac2 = max( 0.0f, frac2 );

			if ( frac1 == frac2 )
				continue;

			int x1 = ( int )( frac1 * w2() );
			int x2 = ( int )( frac2 * w2() );

			if ( mx >= x1 && mx <= x2 )
			{
				return pPhoneme;
			}
		}
	}

	return NULL;
}

CWordTag *PhonemeEditor::GetWordTagUnderMouse( int mx, int my )
{
	if ( GetMode() != MODE_PHONEMES )
		return NULL;

	// Deterime if phoneme boundary is under the cursor
	//
	if ( !m_pWaveFile )
		return NULL;

	RECT rc;
	GetWorkspaceRect( rc );

	if ( !IsMouseOverWordRow( my ) )
		return NULL;

	if ( GetPixelsPerSecond() <= 0 )
		return NULL;

	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float endtime = w2() / GetPixelsPerSecond() + starttime;

	if ( endtime - starttime <= 0.0f )
		return NULL;

	for ( int k = 0; k < m_Tags.m_Words.Size(); k++ )
	{
		CWordTag *word = m_Tags.m_Words[ k ];
		Assert( word );
		if ( !word )
			continue;

		float t1 = word->m_flStartTime;
		float t2 = word->m_flEndTime;

		float frac1 = ( t1 - starttime ) / ( endtime - starttime );
		float frac2 = ( t2 - starttime ) / ( endtime - starttime );

		frac1 = min( 1.0f, frac1 );
		frac1 = max( 0.0f, frac1 );
		frac2 = min( 1.0f, frac2 );
		frac2 = max( 0.0f, frac2 );

		if ( frac1 == frac2 )
			continue;

		int x1 = ( int )( frac1 * w2() );
		int x2 = ( int )( frac2 * w2() );

		if ( mx >= x1 && mx <= x2 )
		{
			return word;
		}
	}

	return NULL;
}

void PhonemeEditor::DeselectWords( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	for ( int i = 0 ; i < m_Tags.m_Words.Size(); i++ )
	{
		CWordTag *w = m_Tags.m_Words[ i ];
		Assert( w );
		if ( !w )
			continue;
		w->m_bSelected = false;
	}

}

void PhonemeEditor::DeselectPhonemes( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	for ( int w = 0 ; w < m_Tags.m_Words.Size(); w++ )
	{
		CWordTag *word = m_Tags.m_Words[ w ];
		Assert( word );
		if ( !word )
			continue;

		for ( int i = 0 ; i < word->m_Phonemes.Size(); i++ )
		{
			CPhonemeTag *pt = word->m_Phonemes[ i ];
			Assert( pt );
			if ( !pt )
				continue;
			pt->m_bSelected = false;
		}
	}
}

void PhonemeEditor::SnapWords( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	if ( m_Tags.m_Words.Size() < 2 )
	{
		Con_Printf( "Can't snap, need at least two contiguous selected words\n" );
		return;
	}

	SetDirty( true );

	PushUndo();

	for ( int i = 0; i < m_Tags.m_Words.Size() - 1; i++ )
	{
		CWordTag *current = m_Tags.m_Words[ i ];
		CWordTag *next = m_Tags.m_Words[ i + 1 ];

		Assert( current && next );

		if ( !current->m_bSelected || !next->m_bSelected )
			continue;

		// Move next word to end of current
		next->m_flStartTime = current->m_flEndTime;
	}

	PushRedo();

	redraw();
}

void PhonemeEditor::SeparateWords( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	if ( GetPixelsPerSecond() <= 0.0f )
		return;

	if ( m_Tags.m_Words.Size() < 2 )
	{
		Con_Printf( "Can't separate, need at least two contiguous selected words\n" );
		return;
	}

	// Three pixels
	double time_epsilon = ( 1.0f / GetPixelsPerSecond() ) * 6;

	SetDirty( true );

	PushUndo();

	for ( int i = 0; i < m_Tags.m_Words.Size() - 1; i++ )
	{
		CWordTag *current = m_Tags.m_Words[ i ];
		CWordTag *next = m_Tags.m_Words[ i + 1 ];

		Assert( current && next );

		if ( !current->m_bSelected || !next->m_bSelected )
			continue;

		// Close enough?
		if ( fabs( current->m_flEndTime - next->m_flStartTime ) > time_epsilon )
		{
			Con_Printf( "Can't split %s and %s, already split apart\n",
				current->GetWord(), next->GetWord() );
			continue;
		}

		// Offset next word start time a bit
		next->m_flStartTime += time_epsilon;

		break;
	}

	PushRedo();

	redraw();
}

void PhonemeEditor::CreateEvenWordDistribution( const char *wordlist )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	if( !m_pWaveFile )
		return;

	Assert( wordlist );
	if ( !wordlist )
		return;

	m_Tags.CreateEventWordDistribution( wordlist, m_pWaveFile->GetRunningLength() );

	redraw();
}

void PhonemeEditor::EditWordList( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	if ( !m_pWaveFile )
		return;

	// Build word string
	char wordstring[ 1024 ];
	V_strcpy_safe( wordstring, m_Tags.GetText() );

	CInputParams params;
	memset( &params, 0, sizeof( params ) );
	strcpy( params.m_szDialogTitle, "Word List" );
	strcpy( params.m_szPrompt, "Sentence:" );

	strcpy( params.m_szInputText, wordstring );

	if ( !InputProperties( &params ) )
		return;

	if ( strlen( params.m_szInputText ) <= 0 )
	{
		// Could be foreign language...
		Warning( "Edit word list:  No words entered!\n" );
	}

	SetDirty( true );

	PushUndo();

	// Clear any current LISET results
	ClearExtracted();

	// Force text
	m_Tags.SetText( params.m_szInputText );

	if ( m_Tags.m_Words.Size() == 0 )
	{
		// First text we've seen, just distribute words evenly
		CreateEvenWordDistribution( params.m_szInputText );
	
		// Redo liset
		RedoPhonemeExtraction();
	}
	
	PushRedo();

	SetFocus( (HWND)getHandle() );

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: Overwrite original wave with changes
//-----------------------------------------------------------------------------
void PhonemeEditor::CommitChanges( void )
{
	SaveLinguisticData();

	// Make it writable - if possible
	MakeFileWriteable( m_WorkFile.m_szWaveFile );

	//Open a message box to warn the user if the file was unable to be made non-read only
	if ( !IsFileWriteable( m_WorkFile.m_szWaveFile ) )
	{
		mxMessageBox( NULL, va( "Unable to save file '%s'. File is read-only or in use.",	
				m_WorkFile.m_szWaveFile ), g_appTitle, MX_MB_OK );
	}
	else
	{
		// Copy over and overwrite file
		FPCopyFile( m_WorkFile.m_szWorkingFile, m_WorkFile.m_szWaveFile, true );
		Msg( "Changes saved to '%s'\n", m_WorkFile.m_szWaveFile );
		SetDirty( false, false );
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::LoadWaveFile( void )
{
	char filename[ 512 ];
	if ( !FacePoser_ShowOpenFileNameDialog( filename, sizeof( filename ), "sound", "*.wav" ) )
	{
		return;
	}

	StopPlayback();

	// Strip out the game directory
	SetCurrentWaveFile( filename );
}

void PhonemeEditor::SnapPhonemes( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	SetDirty( true );

	PushUndo();

	CPhonemeTag *prev = NULL;

	for ( int w = 0; w < m_Tags.m_Words.Size(); w++ )
	{
		CWordTag *word = m_Tags.m_Words[ w ];
		Assert( word );
		if ( !word )
			continue;

		for ( int i = 0; i < word->m_Phonemes.Size(); i++ )
		{
			CPhonemeTag *current = word->m_Phonemes[ i ];
			
			Assert( current );

			if ( current->m_bSelected )
			{
				if (prev)
				{
					// More start of next to end of previous
					prev->SetEndTime( current->GetStartTime() );
				}
				prev = current;
			}
			else
			{
				prev = NULL;
			}
		}
	}

	PushRedo();

	redraw();
}

void PhonemeEditor::SeparatePhonemes( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	SetDirty( true );

	PushUndo();

	// Three pixels
	double time_epsilon = ( 1.0f / GetPixelsPerSecond() ) * 6;

	CPhonemeTag *prev = NULL;

	for ( int w = 0; w < m_Tags.m_Words.Size(); w++ )
	{
		CWordTag *word = m_Tags.m_Words[ w ];
		Assert( word );
		if ( !word )
			continue;

		for ( int i = 0; i < word->m_Phonemes.Size(); i++ )
		{
			CPhonemeTag *current = word->m_Phonemes[ i ];

			Assert( current );

			if ( current->m_bSelected )
			{
				if ( prev )
				{
					// Close enough?
					if ( fabs( prev->GetEndTime() - current->GetStartTime() ) > time_epsilon )
					{
						Con_Printf( "Can't split already split apart\n" );
						continue;
					}

					current->AddStartTime( time_epsilon );
				}

				prev = current;
			}
			else
			{
				prev = NULL;
			}
		}
	}

	PushRedo();

	redraw();
}

bool PhonemeEditor::IsMouseOverWordRow( int my )
{
	if ( GetMode() != MODE_PHONEMES )
		return false;

	RECT rc;

	GetWordTrayTopBottom( rc );

	if ( my < rc.top )
		return false;
	
	if ( my > rc.bottom )
		return false;
	
	return true;
}

bool PhonemeEditor::IsMouseOverPhonemeRow( int my )
{
	if ( GetMode() != MODE_PHONEMES )
		return false;

	RECT rc;

	GetPhonemeTrayTopBottom( rc );

	if ( my < rc.top )
		return false;
	
	if ( my > rc.bottom )
		return false;
	
	return true;
}

void PhonemeEditor::GetPhonemeTrayTopBottom( RECT& rc )
{
	RECT wkrc;
	GetWorkspaceRect( wkrc );

	rc.top  = wkrc.bottom - 2 * m_nTickHeight;
	rc.bottom = wkrc.bottom - m_nTickHeight;
}

void PhonemeEditor::GetWordTrayTopBottom( RECT& rc )
{
	RECT wkrc;
	GetWorkspaceRect( wkrc );

	rc.top  = wkrc.top;
	rc.bottom = wkrc.top + m_nTickHeight;
}

int PhonemeEditor::GetMouseForTime( float time )
{
	RECT rc;
	GetWorkspaceRect( rc );

	if ( GetPixelsPerSecond() < 0.0f )
		return rc.left;

	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float endtime = w2() / GetPixelsPerSecond() + starttime;

	if ( endtime - starttime <= 0.0f )
		return rc.left;

	float frac;

	frac = ( time - starttime ) / ( endtime - starttime );

	return rc.left + ( int )( rc.right * frac );
}

void PhonemeEditor::GetWordRect( const CWordTag *tag, RECT& rc )
{
	Assert( tag );
	GetWordTrayTopBottom( rc );
	
	rc.left = GetMouseForTime( tag->m_flStartTime );
	rc.right = GetMouseForTime( tag->m_flEndTime );

}

void PhonemeEditor::GetPhonemeRect( const CPhonemeTag *tag, RECT& rc )
{
	Assert( tag );

	GetPhonemeTrayTopBottom( rc );
	rc.left = GetMouseForTime( tag->GetStartTime() );
	rc.right = GetMouseForTime( tag->GetEndTime() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::CommitExtracted( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	m_nLastExtractionResult		= SR_RESULT_NORESULT;

	if ( !m_TagsExt.m_Words.Size() )
		return;

	SetDirty( true );

	PushUndo();

	m_Tags.Reset();
	m_Tags = m_TagsExt;

	PushRedo();

	ClearExtracted();

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::ClearExtracted( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	m_nLastExtractionResult		= SR_RESULT_NORESULT;

	m_TagsExt.Reset();

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : resultCode - 
// Output : const char
//-----------------------------------------------------------------------------
const char *PhonemeEditor::GetExtractionResultString( int resultCode )
{
	switch ( resultCode )
	{
	case SR_RESULT_NORESULT:
		return "no extraction info.";
	case SR_RESULT_ERROR:
		return "an error occurred during extraction.";
	case SR_RESULT_SUCCESS:
		return "successful.";
	case SR_RESULT_FAILED:
		return "results retrieved, but full recognition failed.";
	default:
		break;
	}

	return "unknown result code.";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
// Output : CEventRelativeTag
//-----------------------------------------------------------------------------
CEventRelativeTag *PhonemeEditor::GetTagUnderMouse( int mx )
{
	if ( GetMode() != MODE_PHONEMES )
		return NULL;

	// Figure out tag positions
	if ( !m_pEvent || !m_pWaveFile )
		return NULL;
	
	RECT rc;
	GetWorkspaceRect( rc );
	RECT rcTags = rc;

	if ( GetPixelsPerSecond() <= 0.0f )
		return NULL;

	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float endtime = w2() / GetPixelsPerSecond() + starttime;

	if ( endtime - starttime < 0 )
		return NULL;

	for ( int i = 0; i < m_pEvent->GetNumRelativeTags(); i++ )
	{
		CEventRelativeTag *tag = m_pEvent->GetRelativeTag( i );
		if ( !tag )
			continue;

		// 
		float tagtime = tag->GetPercentage() * m_pWaveFile->GetRunningLength();
		if ( tagtime < starttime || tagtime > endtime )
			continue;

		float frac = ( tagtime - starttime ) / ( endtime - starttime );

		int left = rcTags.left + (int)( frac * ( float )( rcTags.right - rcTags.left ) + 0.5f );

		if ( abs( mx - left ) < 10 )
			return tag;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhonemeEditor::IsMouseOverTag( int mx, int my )
{
	if ( GetMode() != MODE_PHONEMES )
		return false;

	if ( !IsMouseOverTagRow( my ) )
		return false;

	CEventRelativeTag *tag = GetTagUnderMouse( mx );
	if ( !tag )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : startx - 
//			endx - 
//-----------------------------------------------------------------------------
void PhonemeEditor::FinishEventTagDrag( int startx, int endx )
{
	if ( !m_pWaveFile )
		return;

	if ( !m_pWaveFile->GetRunningLength() )
		return;

	// Find starting tag
	CEventRelativeTag *tag = GetTagUnderMouse( startx );
	if ( !tag )
		return;

	if ( GetPixelsPerSecond() <= 0 )
		return;

	// Convert mouse position to time
	float starttime = m_nLeftOffset / GetPixelsPerSecond();

	float clicktime = (float)endx / GetPixelsPerSecond() + starttime;

	float percent = clicktime / m_pWaveFile->GetRunningLength();
	percent = clamp( percent, 0.0f, 1.0f );
	
	tag->SetPercentage( percent );

	redraw();

	if ( g_pChoreoView )
	{
		g_pChoreoView->InvalidateLayout();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : my - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhonemeEditor::IsMouseOverTagRow( int my )
{
	if ( GetMode() != MODE_PHONEMES )
		return false;

	if ( my < TAG_TOP || my > TAG_BOTTOM )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
void PhonemeEditor::ShowTagMenu( int mx, int my )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	// Figure out tag positions
	if ( !m_pEvent || !m_pWaveFile )
		return;

	if ( !IsMouseOverTagRow( my ) )
		return;

	CEventRelativeTag *tag = GetTagUnderMouse( mx );

	mxPopupMenu *pop = new mxPopupMenu();

	if ( tag )
	{
		pop->add( va( "Delete tag '%s'", tag->GetName() ), IDC_DELETETAG );
	}
	else
	{
		pop->add( va( "Add tag..." ), IDC_ADDTAG );
	}

	m_nClickX = mx;

	pop->popup( this, mx, my );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::DeleteTag( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	// Figure out tag positions
	if ( !m_pEvent )
		return;

	CEventRelativeTag *tag = GetTagUnderMouse( m_nClickX );
	if ( !tag )
		return;

	// Remove it
	m_pEvent->RemoveRelativeTag( tag->GetName() );

	g_pChoreoView->InvalidateLayout();
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::AddTag( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	// Figure out tag positions
	if ( !m_pEvent || !m_pWaveFile )
		return;

	CInputParams params;
	memset( &params, 0, sizeof( params ) );
	strcpy( params.m_szDialogTitle, "Event Tag Name" );
	strcpy( params.m_szPrompt, "Name:" );
	strcpy( params.m_szInputText, "" );

	if ( !InputProperties( &params ) )
		return;

	if ( strlen( params.m_szInputText ) <= 0 )
	{
		Con_ErrorPrintf( "Event Tag Name:  No name entered!\n" );
		return;
	}

	// Convert mouse position to time
	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float clicktime = (float)m_nClickX / GetPixelsPerSecond() + starttime;

	float percent = clicktime / m_pWaveFile->GetRunningLength();
	percent = min( 1.0f, percent );
	percent = max( 0.0f, percent );

	m_pEvent->AddRelativeTag( params.m_szInputText, percent );

	g_pChoreoView->InvalidateLayout();

	SetFocus( (HWND)getHandle() );

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::ClearEvent( void )
{
	m_pEvent = NULL;
	redraw();
}

void PhonemeEditor::TraverseWords( PEWORDITERFUNC pfn, float fparam )
{
	for ( int i = 0; i < m_Tags.m_Words.Size(); i++ )
	{
		CWordTag *word = m_Tags.m_Words[ i ];
		if ( !word )
			continue;

		(this->*pfn)( word, fparam );
	}
}

void PhonemeEditor::TraversePhonemes( PEPHONEMEITERFUNC pfn, float fparam )
{
	for ( int i = 0; i < m_Tags.m_Words.Size(); i++ )
	{
		CWordTag *word = m_Tags.m_Words[ i ];
		if ( !word )
			continue;

		for ( int j = 0; j < word->m_Phonemes.Size(); j++ )
		{
			CPhonemeTag *phoneme = word->m_Phonemes[ j ];
			if ( !phoneme )
				continue;

			(this->*pfn)( phoneme, word, fparam );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : amount - 
//-----------------------------------------------------------------------------
void PhonemeEditor::ITER_MoveSelectedWords( CWordTag *word, float amount )
{
	if ( !word->m_bSelected )
		return;

	word->m_flStartTime += amount;
	word->m_flEndTime += amount;
}

void PhonemeEditor::ITER_MoveSelectedPhonemes( CPhonemeTag *phoneme, CWordTag *word, float amount )
{
	if ( !phoneme->m_bSelected )
		return;

	phoneme->AddStartTime( amount );
	phoneme->AddEndTime( amount );

}

void PhonemeEditor::ITER_ExtendSelectedPhonemeEndTimes( CPhonemeTag *phoneme, CWordTag *word, float amount )
{
	if ( !phoneme->m_bSelected )
		return;

	if ( phoneme->GetEndTime() + amount <= phoneme->GetStartTime() )
		return;

	phoneme->AddEndTime( amount );

	// Fixme, check for extending into next phoneme
}


void PhonemeEditor::ITER_ExtendSelectedWordEndTimes( CWordTag *word, float amount )
{
	if ( !word->m_bSelected )
		return;

	if ( word->m_flEndTime + amount <= word->m_flStartTime )
		return;

	word->m_flEndTime += amount;

	// Fixme, check for extending into next word
}

void PhonemeEditor::ITER_AddFocusRectSelectedWords( CWordTag *word, float amount )
{
	if ( !word->m_bSelected )
		return;

	RECT wordRect;
	GetWordRect( word, wordRect );

	AddFocusRect( wordRect );
}

void PhonemeEditor::ITER_AddFocusRectSelectedPhonemes( CPhonemeTag *phoneme, CWordTag *word, float amount )
{
	if ( !phoneme->m_bSelected )
		return;

	RECT phonemeRect;
	GetPhonemeRect( phoneme, phonemeRect );

	AddFocusRect( phonemeRect );
}


void PhonemeEditor::AddFocusRect( RECT& rc )
{
	RECT rcFocus = rc;

	POINT offset;
	offset.x = 0;
	offset.y = 0;
	ClientToScreen( (HWND)getHandle(), &offset );
	OffsetRect( &rcFocus, offset.x, offset.y );

	// Convert to screen space?
	CFocusRect fr;
	fr.m_rcFocus = rcFocus;
	fr.m_rcOrig = rcFocus;

	m_FocusRects.AddToTail( fr );
}

void PhonemeEditor::CountSelected( void )
{
	m_nSelectedPhonemeCount = 0;
	m_nSelectedWordCount = 0;

	TraverseWords( &PhonemeEditor::ITER_CountSelectedWords, 0.0f );
	TraversePhonemes( &PhonemeEditor::ITER_CountSelectedPhonemes, 0.0f );
}

void PhonemeEditor::ITER_CountSelectedWords( CWordTag *word, float amount )
{
	if ( !word->m_bSelected )
		return;

	m_nSelectedWordCount++;

}

void PhonemeEditor::ITER_CountSelectedPhonemes( CPhonemeTag *phoneme, CWordTag *word, float amount )
{
	if ( !phoneme->m_bSelected )
		return;

	m_nSelectedPhonemeCount++;
}

// Undo/Redo
void PhonemeEditor::Undo( void )
{
	if ( m_UndoStack.Size() > 0 && m_nUndoLevel > 0 )
	{
		m_nUndoLevel--;
		PEUndo *u = m_UndoStack[ m_nUndoLevel ];
		Assert( u->undo );
		m_Tags = *(u->undo);

		SetClickedPhoneme( -1, -1 );
	}

	redraw();
}

void PhonemeEditor::Redo( void )
{
	if ( m_UndoStack.Size() > 0 && m_nUndoLevel <= m_UndoStack.Size() - 1 )
	{
		PEUndo *u = m_UndoStack[ m_nUndoLevel ];
		Assert( u->redo );
		m_Tags = *(u->redo);
		m_nUndoLevel++;

		SetClickedPhoneme( -1, -1 );
	}

	redraw();
}

void PhonemeEditor::PushUndo( void )
{
	Assert( !m_bRedoPending );
	m_bRedoPending = true;
	WipeRedo();

	// Copy current data
	CSentence *u = new CSentence();
	*u = m_Tags;
	PEUndo *undo = new PEUndo;
	undo->undo = u;
	undo->redo = NULL;
	m_UndoStack.AddToTail( undo );
	m_nUndoLevel++;
}

void PhonemeEditor::PushRedo( void )
{
	Assert( m_bRedoPending );
	m_bRedoPending = false;

	// Copy current data
	CSentence *r = new CSentence();
	*r = m_Tags;
	PEUndo *undo = m_UndoStack[ m_nUndoLevel - 1 ];
	undo->redo = r;
}

void PhonemeEditor::WipeUndo( void )
{
	while ( m_UndoStack.Size() > 0 )
	{
		PEUndo *u = m_UndoStack[ 0 ];
		delete u->undo;
		delete u->redo;
		delete u;
		m_UndoStack.Remove( 0 );
	}
	m_nUndoLevel = 0;
}

void PhonemeEditor::WipeRedo( void )
{
	// Wipe everything above level
	while ( m_UndoStack.Size() > m_nUndoLevel )
	{
		PEUndo *u = m_UndoStack[ m_nUndoLevel ];
		delete u->undo;
		delete u->redo;
		delete u;
		m_UndoStack.Remove( m_nUndoLevel );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : word - 
//			phoneme - 
//-----------------------------------------------------------------------------
void PhonemeEditor::SetClickedPhoneme( int word, int phoneme )
{
	m_nClickedPhoneme = phoneme;
	m_nClickedWord = word;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CPhonemeTag
//-----------------------------------------------------------------------------
CPhonemeTag *PhonemeEditor::GetClickedPhoneme( void )
{
	if ( m_nClickedPhoneme < 0 || m_nClickedWord < 0 )
		return NULL;

	if ( m_nClickedWord >= m_Tags.m_Words.Size() )
		return NULL;

	CWordTag *word = m_Tags.m_Words[ m_nClickedWord ];
	if ( !word )
		return NULL;

	if ( m_nClickedPhoneme >= word->m_Phonemes.Size() )
		return NULL;

	CPhonemeTag *phoneme = word->m_Phonemes[ m_nClickedPhoneme ];
	return phoneme;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CWordTag
//-----------------------------------------------------------------------------
CWordTag *PhonemeEditor::GetClickedWord( void )
{
	if ( m_nClickedWord < 0 )
		return NULL;

	if ( m_nClickedWord >= m_Tags.m_Words.Size() )
		return NULL;

	CWordTag *word = m_Tags.m_Words[ m_nClickedWord ];
	return word;
}

void PhonemeEditor::ShowContextMenu_Phonemes( int mx, int my )
{
	CountSelected();

	// Construct main
	mxPopupMenu *pop = new mxPopupMenu();

	if ( m_pWaveFile )
	{
		mxPopupMenu *play = new mxPopupMenu;
		play->add( va( "Original" ), IDC_PHONEME_PLAY_ORIG );
		play->add( va( "Edited" ), IDC_PLAY_EDITED );
		if ( m_bSelectionActive )
		{
			play->add( va( "Selection" ), IDC_PLAY_EDITED_SELECTION );
		}

		pop->addMenu( "Play", play );

		if ( sound->IsSoundPlaying( m_pMixer ) )
		{
			pop->add( va( "Cancel playback" ), IDC_CANCELPLAYBACK );
		}

		pop->addSeparator();
	}

	pop->add( va( "Load..." ), IDC_LOADWAVEFILE );

	if ( m_pWaveFile )
	{
		pop->add( va( "Save" ), IDC_SAVE_LINGUISTIC );
	}

	if ( m_bSelectionActive )
	{
		pop->addSeparator();
		pop->add( va( "Deselect" ), IDC_DESELECT );
	}

	if ( m_pWaveFile )
	{
		pop->addSeparator();
		pop->add( va( "Redo Extraction" ), IDC_REDO_PHONEMEEXTRACTION );

		if ( m_nSelectedWordCount < 1 || AreSelectedWordsContiguous() )
		{
			pop->add( va( "Redo Extraction of selected words" ), IDC_REDO_PHONEMEEXTRACTION_SELECTION );
		}
	}

	if ( m_pWaveFile && m_TagsExt.m_Words.Size() )
	{
		pop->addSeparator();
		pop->add( va( "Commit extraction" ) , IDC_COMMITEXTRACTED );
		pop->add( va( "Clear extraction" ), IDC_CLEAREXTRACTED );
	}

	if ( m_nUndoLevel != 0 || m_nUndoLevel != m_UndoStack.Size()  )
	{
		pop->addSeparator();
		if ( m_nUndoLevel != 0 )
		{
			pop->add( va( "Undo" ), IDC_UNDO );
		}
		if ( m_nUndoLevel != m_UndoStack.Size() )
		{
			pop->add( va( "Redo" ), IDC_REDO );
		}
		pop->add( va( "Clear Undo Info" ), IDC_CLEARUNDO );
	}

	if ( m_Tags.m_Words.Size() > 0 )
	{
		pop->addSeparator();
		pop->add( va( "Cleanup words/phonemes" ), IDC_CLEANUP );
	}

	// Show hierarchical options menu
	{
		mxPopupMenu *api = 0;
		
		if ( DoesExtractorExistFor( SPEECH_API_SAPI ) )
		{
			api = new mxPopupMenu();
			api->add( "Microsoft Speech API", IDC_API_SAPI );
			if ( g_viewerSettings.speechapiindex == SPEECH_API_SAPI )
			{
				api->setChecked( IDC_API_SAPI, true );
			}
		}

		if ( DoesExtractorExistFor( SPEECH_API_LIPSINC ) )
		{
			if ( !api )
				api = new mxPopupMenu();

			api->add( "Lipsinc Speech API", IDC_API_LIPSINC );
			if ( g_viewerSettings.speechapiindex == SPEECH_API_LIPSINC )
			{
				api->setChecked( IDC_API_LIPSINC, true );
			}
		}

		pop->addSeparator();
		pop->addMenu( "Change Speech API", api );
	}

	// Import export menu
	if ( m_pWaveFile )
	{
		pop->addSeparator();
		if ( m_Tags.m_Words.Count() > 0 )
		{
			pop->add( "Export word data to " WORD_DATA_EXTENSION "...", IDC_EXPORT_SENTENCE );
		}
		pop->add( "Import word data from " WORD_DATA_EXTENSION "...", IDC_IMPORT_SENTENCE );
		pop->add( va("%s Voice Duck", m_Tags.GetVoiceDuck() ? "Disable" : "Enable" ), IDC_TOGGLE_VOICEDUCK );
	}

	pop->popup( this, mx, my );
}

void PhonemeEditor::ShowContextMenu_Emphasis( int mx, int my )
{
	Emphasis_CountSelected();

	// Construct main
	mxPopupMenu *pop = new mxPopupMenu();

	pop->add( va( "Select All" ), IDC_EMPHASIS_SELECTALL );
	if ( m_nNumSelected > 0 )
	{
		pop->add( va( "Deselect All" ), IDC_EMPHASIS_DESELECT );
	}

	if ( m_nUndoLevel != 0 || m_nUndoLevel != m_UndoStack.Size()  )
	{
		pop->addSeparator();

		if ( m_nUndoLevel != 0 )
		{
			pop->add( va( "Undo" ), IDC_UNDO );
		}
		if ( m_nUndoLevel != m_UndoStack.Size() )
		{
			pop->add( va( "Redo" ), IDC_REDO );
		}
		pop->add( va( "Clear Undo Info" ), IDC_CLEARUNDO );
	}
	pop->popup( this, mx, my );
}

void PhonemeEditor::ShowContextMenu( int mx, int my )
{
	switch ( GetMode() )
	{
	default:
	case MODE_PHONEMES:
		ShowContextMenu_Phonemes( mx, my );
		break;
	case MODE_EMPHASIS:
		ShowContextMenu_Emphasis( mx, my );
		break;
	}
}

void PhonemeEditor::ShiftSelectedPhoneme( int direction )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CountSelected();

	switch ( m_nSelectedPhonemeCount )
	{
	case 1:
		break;
	case 0:
		Con_Printf( "Can't shift phonemes, none selected\n" );
		return;
	default:
		Con_Printf( "Can only shift one phoneme at a time via keyboard\n" );
		return;
	}

	RECT rc;
	GetWorkspaceRect( rc );

	// Determine start/stop positions
	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float endtime = w2() / GetPixelsPerSecond() + starttime;

	float timeperpixel = ( endtime - starttime ) / (float)( rc.right - rc.left );

	float movetime = timeperpixel * (float)direction;
	
	float maxmove = ComputeMaxPhonemeShift( direction > 0 ? true : false, false );

	if ( direction > 0 )
	{
		if ( movetime > maxmove )
		{
			movetime = maxmove;
			Con_Printf( "Further shift is blocked on right\n" );
		}
	}
	else
	{
		if ( movetime < -maxmove )
		{
			movetime = -maxmove;
			Con_Printf( "Further shift is blocked on left\n" );
		}
	}

	if ( fabs( movetime ) < 0.0001f )
		return;

	SetDirty( true );

	PushUndo();

	TraversePhonemes( &PhonemeEditor::ITER_MoveSelectedPhonemes, movetime );

	PushRedo();

	m_bWordsActive = false;

	redraw();

	Con_Printf( "Shift phoneme %s\n", direction == -1 ? "left" : "right" );
}

void PhonemeEditor::ExtendSelectedPhonemeEndTime( int direction )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CountSelected();

	if ( m_nSelectedPhonemeCount != 1 )
		return;

	RECT rc;
	GetWorkspaceRect( rc );

	// Determine start/stop positions
	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float endtime = w2() / GetPixelsPerSecond() + starttime;

	float timeperpixel = ( endtime - starttime ) / (float)( rc.right - rc.left );

	float movetime = timeperpixel * (float)direction;

	SetDirty( true );

	PushUndo();

	TraversePhonemes( &PhonemeEditor::ITER_ExtendSelectedPhonemeEndTimes, movetime );

	PushRedo();

	m_bWordsActive = false;

	redraw();

	Con_Printf( "Extend phoneme end %s\n", direction == -1 ? "left" : "right" );
}

void PhonemeEditor::SelectNextPhoneme( int direction )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CountSelected();

	if ( m_nSelectedPhonemeCount != 1 )
	{
		if ( m_nSelectedWordCount == 1 )
		{
			CWordTag *word = GetSelectedWord();
			if ( word && word->m_Phonemes.Size() > 0 )
			{
				m_nSelectedPhonemeCount = 1;
				CPhonemeTag *p = word->m_Phonemes[ direction ? word->m_Phonemes.Size() - 1 : 0 ];
				p->m_bSelected = true;
			}
			else
			{
				return;
			}
		}
		else
		{
			return;
		}
	}

	Con_Printf( "Move to next phoneme %s\n", direction == -1 ? "left" : "right" );

	for ( int i = 0; i < m_Tags.m_Words.Size(); i++ )
	{
		CWordTag *word = m_Tags.m_Words[ i ];
		if ( !word )
			continue;

		for ( int j = 0; j < word->m_Phonemes.Size(); j++ )
		{
			CPhonemeTag *phoneme = word->m_Phonemes[ j ];
			if ( !phoneme )
				continue;

			if ( !phoneme->m_bSelected )
				continue;

			// Deselect this one and move 
			int nextindex = j + direction;
			if ( nextindex < 0 )
			{
				nextindex = word->m_Phonemes.Size() - 1;
			}
			else if ( nextindex >= word->m_Phonemes.Size() )
			{
				nextindex = 0;
			}

			phoneme->m_bSelected = false;

			phoneme = word->m_Phonemes[ nextindex ];

			phoneme->m_bSelected = true;

			m_bWordsActive = false;

			redraw();
			return;
		}
	}
}

bool PhonemeEditor::IsPhonemeSelected( CWordTag *word )
{
	for ( int i = 0 ; i < word->m_Phonemes.Size(); i++ )
	{
		CPhonemeTag *p = word->m_Phonemes[ i ];
		if ( !p || !p->m_bSelected )
			continue;

		return true;
	}
	return false;
}

void PhonemeEditor::SelectNextWord( int direction )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CountSelected();

	if ( m_nSelectedWordCount != 1 &&
		 m_nSelectedPhonemeCount != 1 )
	{
		// Selected first word then
		if ( m_nSelectedWordCount == 0 && m_Tags.m_Words.Size() > 0 )
		{
			CWordTag *word = m_Tags.m_Words[ direction ? m_Tags.m_Words.Size() - 1 : 0 ];
			word->m_bSelected = true;
			m_nSelectedWordCount = 1;
		}
		else
		{
			return;
		}
	}

	Con_Printf( "Move to next word %s\n", direction == -1 ? "left" : "right" );

	for ( int i = 0; i < m_Tags.m_Words.Size(); i++ )
	{
		CWordTag *word = m_Tags.m_Words[ i ];
		if ( !word )
			continue;

		if ( m_nSelectedWordCount == 1 )
		{
			if ( !word->m_bSelected )
				continue;
		}
		else 
		{
			if ( !IsPhonemeSelected( word ) )
				continue;
		}

		// Deselect word
		word->m_bSelected = false;

		for ( int j = 0; j < word->m_Phonemes.Size(); j++ )
		{
			CPhonemeTag *phoneme = word->m_Phonemes[ j ];
			if ( !phoneme )
				continue;

			if ( !phoneme->m_bSelected )
				continue;

			phoneme->m_bSelected = false;
		}

		// Deselect this one and move 
		int nextword = i + direction;
		if ( nextword < 0 )
		{
			nextword = m_Tags.m_Words.Size() - 1;
		}
		else if ( nextword >= m_Tags.m_Words.Size() )
		{
			nextword = 0;
		}

		word = m_Tags.m_Words[ nextword ];
		word->m_bSelected = true;

		if ( word->m_Phonemes.Size() > 0 )
		{
			CPhonemeTag *phoneme = NULL;

			if ( direction > 0 )
			{
				phoneme = word->m_Phonemes[ 0 ];
			}
			else
			{
				phoneme = word->m_Phonemes[ word->m_Phonemes.Size() - 1 ];
			}

			phoneme->m_bSelected = true;
		}

		m_bWordsActive = true;

		redraw();
		return;
	}
}

void PhonemeEditor::ShiftSelectedWord( int direction )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CountSelected();

	switch ( m_nSelectedWordCount )
	{
	case 1:
		break;
	case 0:
		Con_Printf( "Can't shift words, none selected\n" );
		return;
	default:
		Con_Printf( "Can only shift one word at a time via keyboard\n" );
		return;
	}

	RECT rc;
	GetWorkspaceRect( rc );

	// Determine start/stop positions
	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float endtime = w2() / GetPixelsPerSecond() + starttime;

	float timeperpixel = ( endtime - starttime ) / (float)( rc.right - rc.left );

	float movetime = timeperpixel * (float)direction;

	float maxmove = ComputeMaxWordShift( direction > 0 ? true : false, false );

	if ( direction > 0 )
	{
		if ( movetime > maxmove )
		{
			movetime = maxmove;
			Con_Printf( "Further shift is blocked on right\n" );
		}
	}
	else
	{
		if ( movetime < -maxmove )
		{
			movetime = -maxmove;
			Con_Printf( "Further shift is blocked on left\n" );
		}
	}

	if ( fabs( movetime ) < 0.0001f )
		return;

	SetDirty( true );

	PushUndo();

	TraverseWords( &PhonemeEditor::ITER_MoveSelectedWords, movetime );

	PushRedo();

	m_bWordsActive = true;

	redraw();

	Con_Printf( "Shift word %s\n", direction == -1 ? "left" : "right" );
}

void PhonemeEditor::ExtendSelectedWordEndTime( int direction )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CountSelected();

	if ( m_nSelectedWordCount != 1 )
		return;

	RECT rc;
	GetWorkspaceRect( rc );

	// Determine start/stop positions
	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float endtime = w2() / GetPixelsPerSecond() + starttime;

	float timeperpixel = ( endtime - starttime ) / (float)( rc.right - rc.left );

	float movetime = timeperpixel * (float)direction;

	SetDirty( true );

	PushUndo();

	TraverseWords( &PhonemeEditor::ITER_ExtendSelectedWordEndTimes, movetime );

	PushRedo();

	m_bWordsActive = true;

	redraw();

	Con_Printf( "Extend word end %s\n", direction == -1 ? "left" : "right" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *word - 
// Output : int
//-----------------------------------------------------------------------------
int PhonemeEditor::IndexOfWord( CWordTag *word )
{
	for ( int i = 0 ; i < m_Tags.m_Words.Size(); i++ )
	{
		if ( m_Tags.m_Words[ i ] == word )
			return i;
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : forward - 
//			*currentWord - 
//			**nextWord - 
// Output : float
//-----------------------------------------------------------------------------
float PhonemeEditor::GetTimeGapToNextWord( bool forward, CWordTag *currentWord, CWordTag **ppNextWord /* = NULL */ )
{
	if ( ppNextWord )
	{
		*ppNextWord = NULL;
	}

	if ( !currentWord )
		return 0.0f;

	int wordnum = IndexOfWord( currentWord );
	if ( wordnum == -1 )
		return 0.0f;

	// Go in correct direction
	int newwordnum = wordnum + ( forward ? 1 : -1 );

	// There is no next word
	if ( newwordnum >= m_Tags.m_Words.Size() )
	{
		return PLENTY_OF_TIME;
	}

	// There is no previous word
	if ( newwordnum < 0 )
	{
		return PLENTY_OF_TIME;
	}

	if ( ppNextWord )
	{
		*ppNextWord = m_Tags.m_Words[ newwordnum ];
	}

	// Otherwise, figure out time gap
	if ( forward )
	{
		float currentEnd = currentWord->m_flEndTime;
		float nextStart = m_Tags.m_Words[ newwordnum ]->m_flStartTime;

		return ( nextStart - currentEnd );
	}
	else
	{
		float previousEnd = m_Tags.m_Words[ newwordnum ]->m_flEndTime;
		float currentStart = currentWord->m_flStartTime;

		return ( currentStart - previousEnd );
	}

	
	Assert( 0 );
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : forward - 
//			*currentPhoneme - 
//			**word - 
//			**phoneme - 
// Output : float
//-----------------------------------------------------------------------------
float PhonemeEditor::GetTimeGapToNextPhoneme( bool forward, CPhonemeTag *currentPhoneme, 
	CWordTag **ppword /* = NULL */, CPhonemeTag **ppphoneme /* = NULL */ )
{
	if ( ppword )
	{
		*ppword = NULL;
	}
	if ( ppphoneme )
	{
		*ppphoneme = NULL;
	}

	if ( !currentPhoneme )
		return 0.0f;

	CWordTag *word = m_Tags.GetWordForPhoneme( currentPhoneme );
	if ( !word )
		return 0.0f;

	int wordnum = IndexOfWord( word );
	Assert( wordnum != -1 );

	int phonemenum = word->IndexOfPhoneme( currentPhoneme );
	if ( phonemenum < 0  )
		return 0.0f;

	CPhonemeTag *nextPhoneme = NULL;

	int nextphoneme = phonemenum + ( forward ? 1 : -1 );

	// Try last phoneme of previous word
	if ( nextphoneme < 0 )
	{
		wordnum--;
		while ( wordnum >= 0 )
		{
			if ( ppword )
			{
				*ppword = m_Tags.m_Words[ wordnum ];
			}
			if ( m_Tags.m_Words.Size() > 0 )
			{
				if ( m_Tags.m_Words[ wordnum ]->m_Phonemes.Size() > 0 )
				{
					nextPhoneme = m_Tags.m_Words[ wordnum ]->m_Phonemes[ m_Tags.m_Words[ wordnum ]->m_Phonemes.Size() - 1 ];
					break;
				}
			}
			wordnum--;
		}
	}
	// Try first phoneme of next word, if there is one
	else if ( nextphoneme >= word->m_Phonemes.Size() )
	{
		wordnum++;
		while ( wordnum < m_Tags.m_Words.Size() )
		{
			if ( ppword )
			{
				*ppword = m_Tags.m_Words[ wordnum ];
			}
			// Really it can't be zero, but check anyway
			if ( m_Tags.m_Words.Size() > 0 )
			{
				if ( m_Tags.m_Words[ wordnum ]->m_Phonemes.Size() > 0 )
				{
					nextPhoneme = m_Tags.m_Words[ wordnum ]->m_Phonemes[ 0 ];
					break;
				}
			}
			wordnum++;
		}
	}
	else
	{
		nextPhoneme = word->m_Phonemes[ nextphoneme ];
	}

	if ( !nextPhoneme )
		return PLENTY_OF_TIME;

	if ( ppphoneme )
	{
		*ppphoneme = nextPhoneme;
	}

	// Now compute time delta
	float dt = 0.0f;
	if ( forward )
	{
		dt = nextPhoneme->GetStartTime() - currentPhoneme->GetEndTime();
	}
	else
	{
		dt = currentPhoneme->GetStartTime() - nextPhoneme->GetEndTime();
	}

	return dt;
}

CPhonemeTag *PhonemeEditor::GetSelectedPhoneme( void )
{
	CountSelected();

	if ( m_nSelectedPhonemeCount != 1 )
		return NULL;

	for ( int i = 0; i < m_Tags.m_Words.Size(); i++ )
	{
		CWordTag *w = m_Tags.m_Words[ i ];
		if ( !w )
			continue;

		for ( int j = 0; j < w->m_Phonemes.Size() ; j++ )
		{
			CPhonemeTag *p = w->m_Phonemes[ j ];
			if ( !p || !p->m_bSelected )
				continue;

			return p;
		}
	}
	return NULL;
}

CWordTag *PhonemeEditor::GetSelectedWord( void )
{
	CountSelected();

	if ( m_nSelectedWordCount != 1 )
		return NULL;

	for ( int i = 0; i < m_Tags.m_Words.Size(); i++ )
	{
		CWordTag *w = m_Tags.m_Words[ i ];
		if ( !w || !w->m_bSelected )
			continue;

		return w;
	}
	return NULL;
}

void PhonemeEditor::OnMouseMove( mxEvent *event )
{
	int mx = (short)event->x;

	LimitDrag( mx );

	event->x = (short)mx;

	if ( m_nDragType != DRAGTYPE_NONE )
	{
		DrawFocusRect( "moving old" );

		for ( int i = 0; i < m_FocusRects.Size(); i++ )
		{
			CFocusRect *f = &m_FocusRects[ i ];
			f->m_rcFocus = f->m_rcOrig;

			switch ( m_nDragType )
			{
			default:
				{
					// Only X Shifts supported
					OffsetRect( &f->m_rcFocus, ( (short)event->x - m_nStartX ),
						0 );
				}
				break;
			case DRAGTYPE_EMPHASIS_SELECT:
				{
					RECT rcWork;
					GetWorkspaceRect( rcWork );
					RECT rcEmphasis;
					Emphasis_GetRect( rcWork, rcEmphasis );

					RECT rcFocus;

					rcFocus = f->m_rcOrig;

					rcFocus.left = m_nStartX < (short)event->x ? m_nStartX : (short)event->x;
					rcFocus.right = m_nStartX < (short)event->x ? (short)event->x : m_nStartX;
					
					rcFocus.top = m_nStartY < (short)event->y ? m_nStartY : (short)event->y;
					rcFocus.bottom = m_nStartY < (short)event->y ? (short)event->y : m_nStartY;

					rcFocus.top = clamp( rcFocus.top, rcEmphasis.top, rcEmphasis.bottom );
					rcFocus.bottom = clamp( rcFocus.bottom, rcEmphasis.top, rcEmphasis.bottom );

					//OffsetRect( &rcFocus, 0, -rcEmphasis.top );

					POINT offset;
					offset.x = 0;
					offset.y = 0;
					ClientToScreen( (HWND)getHandle(), &offset );
					OffsetRect( &rcFocus, offset.x, offset.y );

					f->m_rcFocus = rcFocus;
				}
				break;
			}
		}

		if ( m_nDragType == DRAGTYPE_EMPHASIS_MOVE )
		{
			redraw();
		}

		DrawFocusRect( "moving new" );
	}
	else
	{
		if ( m_hPrevCursor )
		{
			SetCursor( m_hPrevCursor );
			m_hPrevCursor = NULL;
		}

		CountSelected();

		int overhandle = IsMouseOverBoundary( event );
		if ( overhandle == BOUNDARY_PHONEME && m_nSelectedPhonemeCount <= 1 )
		{
			m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
		}
		else if ( overhandle == BOUNDARY_WORD && m_nSelectedWordCount <= 1 )
		{
			m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
		}
		else if ( IsMouseOverSelection( (short)event->x, (short)event->y ) )
		{
			if ( IsMouseOverSelectionStartEdge( event ) )
			{
				m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
			}
			else if ( IsMouseOverSelectionEndEdge( event ) )
			{
				m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
			}
			else
			{
				if ( event->modifiers & mxEvent::KeyShift )
				{
					m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEALL ) );
				}
			}
		}
		else
		{
			if ( IsMouseOverTag( (short)event->x, (short)event->y ) )
			{
				m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEALL ) );
			}
			else
			{
				CPhonemeTag *pt = GetPhonemeTagUnderMouse( (short)event->x, (short)event->y );
				CWordTag *wt = GetWordTagUnderMouse( (short)event->x, (short)event->y );
				if ( wt || pt )
				{
					if ( pt )
					{
						// Select it
						SelectExpression( pt );
					}
					if ( event->modifiers & mxEvent::KeyShift )
					{
						m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEALL ) );
					}
				}
			}
		}
	}

	switch ( m_nDragType )
	{
	default:
		break;
	case DRAGTYPE_EMPHASIS_MOVE:
		{
			Emphasis_MouseDrag( (short)event->x, (short)event->y );
			m_Tags.Resort();
		}
		break;
	case DRAGTYPE_SCRUBBER:
		{
			float t = GetTimeForPixel( (short)event->x );
			t += m_flScrubberTimeOffset;

			ClampTimeToSelectionInterval( t );

			float dt = t - m_flScrub;

			SetScrubTargetTime( t );

			ScrubThink( dt, true );

			SetScrubTime( t );

			DrawScrubHandle();
		}
		break;
	}

	m_nLastX = (short)event->x;
	m_nLastY = (short)event->y;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::EditInsertFirstPhonemeOfWord( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CWordTag *cw = GetSelectedWord();
	if ( !cw )
		return;

	if ( cw->m_Phonemes.Size() != 0 )
	{
		Con_Printf( "Can't insert first phoneme into %s, already has phonemes\n", cw->GetWord() );
		return;
	}

	CPhonemeParams params;
	memset( &params, 0, sizeof( params ) );
	strcpy( params.m_szDialogTitle, "Phoneme/Viseme Properties" );
	strcpy( params.m_szName, "" );

	params.m_nLeft = -1;
	params.m_nTop = -1;

	params.m_bPositionDialog = true;
	params.m_bMultiplePhoneme = true;

	if ( params.m_bPositionDialog )
	{
		RECT rcWord;
		GetWordRect( cw, rcWord );

		// Convert to screen coords
		POINT pt;
		pt.x = rcWord.left;
		pt.y = rcWord.top;

		ClientToScreen( (HWND)getHandle(), &pt );

		params.m_nLeft	= pt.x;
		params.m_nTop	= pt.y;
	}

	int iret = PhonemeProperties( &params );
	SetFocus( (HWND)getHandle() );
	if ( !iret )
	{
		return;
	}

	int phonemeCount = CSentence::CountWords( params.m_szName );
	if ( phonemeCount <= 0 )
	{
		return;
	}

	float wordLength = cw->m_flEndTime - cw->m_flStartTime;
	float timePerPhoneme = wordLength / (float)phonemeCount;

	float currentTime = cw->m_flStartTime;

	SetDirty( true );

	PushUndo();

	unsigned char *in;
	char *out;

	char phonemeName[ 128 ];

	in = (unsigned char *)params.m_szName;

	do
	{
		out = phonemeName;

		while ( *in > 32 )
		{
			*out++ = *in++;
		}
		*out = 0;

		CPhonemeTag phoneme;

		phoneme.SetPhonemeCode( TextToPhoneme( phonemeName ) );
		phoneme.SetTag( phonemeName );

		phoneme.SetStartTime( currentTime );
		phoneme.SetEndTime( currentTime + timePerPhoneme );
		phoneme.m_bSelected = false;

		cw->m_Phonemes.AddToTail( new CPhonemeTag( phoneme ) );

		currentTime += timePerPhoneme;

		if ( !*in )
			break;

		// Skip whitespace
		in++;

	} while ( 1 );

	cw->m_Phonemes[ 0 ]->m_bSelected = true;

	PushRedo();

	// Add it
	redraw();
}

void PhonemeEditor::SelectPhonemes( bool forward )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CountSelected();

	if ( m_nSelectedPhonemeCount != 1 )
		return;

	CPhonemeTag *phoneme = GetSelectedPhoneme();
	if ( !phoneme )
		return;

	// Figure out it's word and index
	CWordTag *word = m_Tags.GetWordForPhoneme( phoneme );
	if ( !word )
		return;

	int wordNum = IndexOfWord( word );
	if ( wordNum == -1 )
		return;

	// Select remaining phonemes in current word
	int i;

	i = word->IndexOfPhoneme( phoneme );
	if ( i == -1 )
		return;

	if ( forward )
	{
		// Start at next one
		i++;

		for ( ; i < word->m_Phonemes.Size(); i++ )
		{
			phoneme = word->m_Phonemes[ i ];
			phoneme->m_bSelected = true;
		}

		// Now start at next word
		wordNum++;

		for ( ; wordNum < m_Tags.m_Words.Size(); wordNum++ )
		{
			word = m_Tags.m_Words[ wordNum ];

			for ( int j = 0; j < word->m_Phonemes.Size(); j++ )
			{
				phoneme = word->m_Phonemes[ j ];
				phoneme->m_bSelected = true;
			}
		}
	}
	else
	{
		// Start at previous
		i--;

		for ( ; i >= 0; i-- )
		{
			phoneme = word->m_Phonemes[ i ];
			phoneme->m_bSelected = true;
		}

		// Now start at previous word
		wordNum--;

		for ( ; wordNum >= 0 ; wordNum-- )
		{
			word = m_Tags.m_Words[ wordNum ];

			for ( int j = 0; j < word->m_Phonemes.Size(); j++ )
			{
				phoneme = word->m_Phonemes[ j ];
				phoneme->m_bSelected = true;
			}
		}
	}

	redraw();
}

void PhonemeEditor::SelectWords( bool forward )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	CountSelected();

	if ( m_nSelectedWordCount != 1 )
		return;

	// Figure out it's word and index
	CWordTag *word = GetSelectedWord();
	if ( !word )
		return;

	int wordNum = IndexOfWord( word );
	if ( wordNum == -1 )
		return;

	if ( forward )
	{
		wordNum++;

		for ( ; wordNum < m_Tags.m_Words.Size(); wordNum++ )
		{
			word = m_Tags.m_Words[ wordNum ];
			word->m_bSelected = true;
		}
	}
	else
	{
		wordNum--;

		for ( ; wordNum >= 0; wordNum-- )
		{
			word = m_Tags.m_Words[ wordNum ];
			word->m_bSelected = true;
		}

	}

	redraw();
}

bool PhonemeEditor::AreSelectedWordsContiguous( void )
{
	CountSelected();

	if ( m_nSelectedWordCount < 1 )
		return false;

	if ( m_nSelectedWordCount == 1 )
		return true;

	// Find first selected word
	int runcount = 0;
	bool parity = false;

	for ( int i = 0 ; i < m_Tags.m_Words.Size() ; i++ )
	{
		CWordTag *word = m_Tags.m_Words[ i ];
		if ( !word )
			continue;

		if ( word->m_bSelected )
		{
			if ( !parity )
			{
				parity = true;
				runcount++;
			}
		}
		else
		{
			if ( parity )
			{
				parity = false;
			}
		}
	}

	if ( runcount == 1 )
		return true;

	return false;
}

bool PhonemeEditor::AreSelectedPhonemesContiguous( void )
{
	CountSelected();

	if ( m_nSelectedPhonemeCount < 1 )
		return false;

	if ( m_nSelectedPhonemeCount == 1 )
		return true;

	// Find first selected word
	int runcount = 0;
	bool parity = false;

	for ( int i = 0 ; i < m_Tags.m_Words.Size() ; i++ )
	{
		CWordTag *word = m_Tags.m_Words[ i ];
		if ( !word )
			continue;

		for ( int j = 0 ; j < word->m_Phonemes.Size(); j++ )
		{
			CPhonemeTag *phoneme = word->m_Phonemes[ j ];
			if ( !phoneme )
				continue;

			if ( phoneme->m_bSelected )
			{
				if ( !parity )
				{
					parity = true;
					runcount++;
				}
			}
			else
			{
				if ( parity )
				{
					parity = false;
				}
			}
		}
	}

	if ( runcount == 1 )
		return true;

	return false;

}

void PhonemeEditor::SortWords( bool prepareundo )
{
	if ( prepareundo )
	{
		SetDirty( true );
		PushUndo();
	}

	// Just bubble sort by start time
	int c = m_Tags.m_Words.Count();

	int i;

	// check for start > end
	for ( i = 0; i < c; i++ )
	{
		CWordTag *p1 = m_Tags.m_Words[ i ];
		if (p1->m_flStartTime > p1->m_flEndTime )
		{
			float swap = p1->m_flStartTime;			
			p1->m_flStartTime = p1->m_flEndTime;
			p1->m_flEndTime = swap;
		}
	}

	for ( i = 0; i < c; i++ )
	{
		for ( int j = i + 1; j < c; j++ )
		{
			CWordTag *p1 = m_Tags.m_Words[ i ];
			CWordTag *p2 = m_Tags.m_Words[ j ];

			if ( p1->m_flStartTime < p2->m_flStartTime )
				continue;

			// Swap them
			m_Tags.m_Words[ i ] = p2;
			m_Tags.m_Words[ j ] = p1;
		}
	}

	if ( prepareundo )
	{
		PushRedo();
	}
}

void PhonemeEditor::SortPhonemes( bool prepareundo )
{
	if ( prepareundo )
	{
		SetDirty( true );
		PushUndo();
	}

	// Just bubble sort by start time
	int wc = m_Tags.m_Words.Count();
	for ( int w = 0; w < wc; w++ )
	{
		CWordTag *word = m_Tags.m_Words[ w ];
		Assert( word );

		int c = word->m_Phonemes.Count();
		int i;

		// check for start > end
		for ( i = 0; i < c; i++ )
		{
			CPhonemeTag *p1 = word->m_Phonemes[ i ];

			if (p1->GetStartTime() > p1->GetEndTime() )
			{
				float swap = p1->GetStartTime();			
				p1->SetStartTime( p1->GetEndTime() );
				p1->SetEndTime( swap );
			}
		}

		for ( i = 0; i < c; i++ )
		{
			for ( int j = i + 1; j < c; j++ )
			{
				CPhonemeTag *p1 = word->m_Phonemes[ i ];
				CPhonemeTag *p2 = word->m_Phonemes[ j ];

				if ( p1->GetStartTime() < p2->GetStartTime() )
					continue;

				// Swap them
				word->m_Phonemes[ i ] = p2;
				word->m_Phonemes[ j ] = p1;
			}
		}
	}

	if ( prepareundo )
	{
		PushRedo();
	}
}

void PhonemeEditor::CleanupWordsAndPhonemes( bool prepareundo )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	// 2 pixel gap
	float snap_epsilon = 2.49f / GetPixelsPerSecond();

	if ( prepareundo )
	{
		SetDirty( true );
		PushUndo();
	}

	SortWords( false );
	SortPhonemes( false );

	for ( int i = 0 ; i < m_Tags.m_Words.Size() ; i++ )
	{
		CWordTag *word = m_Tags.m_Words[ i ];
		if ( !word )
			continue;

		CWordTag *next = NULL;
		if ( i < m_Tags.m_Words.Size() - 1 )
		{
			next = m_Tags.m_Words[ i + 1 ];
		}

		if ( word && next )
		{
			// Check for words close enough
			float eps = next->m_flStartTime - word->m_flEndTime;
			if ( eps && eps <= snap_epsilon )
			{
				float t = (word->m_flEndTime + next->m_flStartTime) * 0.5;
				word->m_flEndTime = t;
				next->m_flStartTime = t;
			}
		}

		for ( int j = 0 ; j < word->m_Phonemes.Size(); j++ )
		{
			CPhonemeTag *phoneme = word->m_Phonemes[ j ];
			if ( !phoneme )
				continue;

			CPhonemeTag *next = NULL;
			if ( j < word->m_Phonemes.Size() - 1 )
			{
				next = word->m_Phonemes[ j + 1 ];
			}

			if ( phoneme && next )
			{
				float eps = next->GetStartTime() - phoneme->GetEndTime();
				if ( eps && eps <= snap_epsilon )
				{
					float t = (phoneme->GetEndTime() + next->GetStartTime() ) * 0.5;
					phoneme->SetEndTime( t );
					next->SetStartTime( t );
				}
			}
		}
	}

	if ( prepareundo )
	{
		PushRedo();
	}

	// NOTE: Caller must call "redraw()" to get screen to update
}



void PhonemeEditor::RealignPhonemesToWords( bool prepareundo )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	if ( prepareundo )
	{
		SetDirty( true );
		PushUndo();
	}

	SortWords( false );
	SortPhonemes( false );

	for ( int i = 0 ; i < m_Tags.m_Words.Size() ; i++ )
	{
		CWordTag *word = m_Tags.m_Words[ i ];
		if ( !word )
			continue;

		CWordTag *next = NULL;
		if ( i < m_Tags.m_Words.Size() - 1 )
		{
			next = m_Tags.m_Words[ i + 1 ];
		}

		float word_dt = word->m_flEndTime - word->m_flStartTime;

		CPhonemeTag *FirstPhoneme = word->m_Phonemes[ 0 ];
		if ( !FirstPhoneme )
			continue;

		CPhonemeTag *LastPhoneme = word->m_Phonemes[ word->m_Phonemes.Size() - 1 ];
		if ( !LastPhoneme )
			continue;

		float phoneme_dt = LastPhoneme->GetEndTime() - FirstPhoneme->GetStartTime();

		float phoneme_shift = FirstPhoneme->GetStartTime();

		for ( int j = 0 ; j < word->m_Phonemes.Size(); j++ )
		{
			CPhonemeTag *phoneme = word->m_Phonemes[ j ];
			if ( !phoneme )
				continue;

			CPhonemeTag *next = NULL;
			if ( j < word->m_Phonemes.Size() - 1 )
			{
				next = word->m_Phonemes[ j + 1 ];
			}

			if (j == 0)
			{
				float t = (phoneme->GetStartTime() - phoneme_shift ) * (word_dt / phoneme_dt) + word->m_flStartTime;
				phoneme->SetStartTime( t );
			}

			float t = (phoneme->GetEndTime() - phoneme_shift ) * (word_dt / phoneme_dt) + word->m_flStartTime;
			phoneme->SetEndTime( t );
			if (next)
			{
				next->SetStartTime( t );
			}
		}
	}

	if ( prepareundo )
	{
		PushRedo();
	}

	// NOTE: Caller must call "redraw()" to get screen to update
}


void PhonemeEditor::RealignWordsToPhonemes( bool prepareundo )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	if ( prepareundo )
	{
		SetDirty( true );
		PushUndo();
	}

	SortWords( false );
	SortPhonemes( false );

	for ( int i = 0 ; i < m_Tags.m_Words.Size() ; i++ )
	{
		CWordTag *word = m_Tags.m_Words[ i ];
		if ( !word )
			continue;

		CPhonemeTag *FirstPhoneme = word->m_Phonemes[ 0 ];
		if ( !FirstPhoneme )
			continue;

		CPhonemeTag *LastPhoneme = word->m_Phonemes[ word->m_Phonemes.Size() - 1 ];
		if ( !LastPhoneme )
			continue;

		word->m_flStartTime = FirstPhoneme->GetStartTime();
		word->m_flEndTime = LastPhoneme->GetEndTime();
	}

	if ( prepareundo )
	{
		PushRedo();
	}

	// NOTE: Caller must call "redraw()" to get screen to update
}



float PhonemeEditor::ComputeMaxWordShift( bool forward, bool allowcrop )
{
	// skipping selected words, figure out max time shift of words before they selection touches any
	// unselected words
	// if allowcrop is true, then the maximum extends up to end of next word
	float maxshift = PLENTY_OF_TIME;

	if ( forward )
	{
		for ( int i = 0; i < m_Tags.m_Words.Size(); i++ )
		{
			CWordTag *w1 = m_Tags.m_Words[ i ];
			if ( !w1 || !w1->m_bSelected )
				continue;

			CWordTag *w2 = NULL;
			for ( int search = i + 1; search < m_Tags.m_Words.Size() ; search++ )
			{
				CWordTag *check = m_Tags.m_Words[ search ];
				if ( !check || check->m_bSelected )
					continue;

				w2 = check;
				break;
			}

			if ( w2 )
			{
				float shift;
				if ( allowcrop )
				{
					shift = w2->m_flEndTime - w1->m_flEndTime;
				}
				else
				{
					shift = w2->m_flStartTime - w1->m_flEndTime;
				}

				if ( shift < maxshift )
				{
					maxshift = shift;
				}
			}
		}
	}
	else
	{
		for ( int i = m_Tags.m_Words.Size() -1; i >= 0; i-- )
		{
			CWordTag *w1 = m_Tags.m_Words[ i ];
			if ( !w1 || !w1->m_bSelected )
				continue;

			CWordTag *w2 = NULL;
			for ( int search = i - 1; search >= 0 ; search-- )
			{
				CWordTag *check = m_Tags.m_Words[ search ];
				if ( !check || check->m_bSelected )
					continue;

				w2 = check;
				break;
			}

			if ( w2 )
			{
				float shift;
				if ( allowcrop )
				{
					shift = w1->m_flStartTime - w2->m_flStartTime;
				}
				else
				{
					shift = w1->m_flStartTime - w2->m_flEndTime;
				}

				if ( shift < maxshift )
				{
					maxshift = shift;
				}
			}
		}
	}

	return maxshift;
}

float PhonemeEditor::ComputeMaxPhonemeShift( bool forward, bool allowcrop )
{
	// skipping selected phonemes, figure out max time shift of phonemes before they selection touches any
	// unselected words
	// if allowcrop is true, then the maximum extends up to end of next word
	float maxshift = PLENTY_OF_TIME;

	if ( forward )
	{
		for ( int i = 0; i < m_Tags.m_Words.Size(); i++ )
		{
			CWordTag *word = m_Tags.m_Words[ i ];
			if ( !word )
				continue;

			for ( int j = 0; j < word->m_Phonemes.Size(); j++ )
			{
				CPhonemeTag *p1 = word->m_Phonemes[ j ];
				if ( !p1 || !p1->m_bSelected )
					continue;

				// Find next unselected phoneme
				CPhonemeTag *p2 = NULL;

				CPhonemeTag *start = p1;
				do
				{
					CPhonemeTag *test = NULL;
					GetTimeGapToNextPhoneme( forward, start, NULL, &test );
					if ( !test )
						break;

					if ( test->m_bSelected )
					{
						start = test;
						continue;
					}

					p2 = test;
					break;
				} while ( 1 );

				if ( p2 )
				{
					float shift;
					if ( allowcrop )
					{
						shift = p2->GetEndTime() - p1->GetEndTime();
					}
					else
					{
						shift = p2->GetStartTime() - p1->GetEndTime();
					}

					if ( shift < maxshift )
					{
						maxshift = shift;
					}
				}
			}
		}
	}
	else
	{
		for ( int i = m_Tags.m_Words.Size() -1; i >= 0; i-- )
		{
			CWordTag *word = m_Tags.m_Words[ i ];
			if ( !word )
				continue;

			for ( int j = word->m_Phonemes.Size() - 1; j >= 0; j-- )
			{
				CPhonemeTag *p1 = word->m_Phonemes[ j ];
				if ( !p1 || !p1->m_bSelected )
					continue;

				// Find previous unselected phoneme
				CPhonemeTag *p2 = NULL;

				CPhonemeTag *start = p1;
				do
				{
					CPhonemeTag *test = NULL;
					GetTimeGapToNextPhoneme( forward, start, NULL, &test );
					if ( !test )
						break;

					if ( test->m_bSelected )
					{
						start = test;
						continue;
					}

					p2 = test;
					break;
				} while ( 1 );

				if ( p2 )
				{
					float shift;
					if ( allowcrop )
					{
						shift = p1->GetStartTime() - p2->GetStartTime();
					}
					else
					{
						shift = p1->GetStartTime() - p2->GetEndTime();
					}

					if ( shift < maxshift )
					{
						maxshift = shift;
					}
				}
			}
		}
	}

	return maxshift;
}

int PhonemeEditor::PixelsForDeltaTime( float dt )
{
	if ( !dt )
		return 0;

	RECT rc;
	GetWorkspaceRect( rc );

	float starttime = m_nLeftOffset / GetPixelsPerSecond();
	float endtime = w2() / GetPixelsPerSecond() + starttime;

	float timeperpixel = ( endtime - starttime ) / (float)( rc.right - rc.left );

	float pixels = dt / timeperpixel;

	return abs( (int)pixels );
}

void PhonemeEditor::ClearDragLimit( void )
{
	m_bLimitDrag = false;
	m_nLeftLimit = -1;
	m_nRightLimit = -1;
}

void PhonemeEditor::SetDragLimit( int dragtype )
{
	ClearDragLimit();

	float nextW, nextP;
	float prevW, prevP;

	nextW = ComputeMaxWordShift( true, false );
	prevW = ComputeMaxWordShift( false, false );
	nextP = ComputeMaxPhonemeShift( true, false );
	prevP = ComputeMaxPhonemeShift( false, false );

	/*
	Con_Printf( "+w %f -w %f +p %f -p %f\n",
		1000.0f * nextW,
		1000.0f * prevW,
		1000.0f * nextP,
		1000.0f * prevP );
	*/

	switch ( dragtype )
	{
	case DRAGTYPE_MOVEWORD:
		m_bLimitDrag = true;
		m_nLeftLimit = PixelsForDeltaTime( prevW );
		m_nRightLimit = PixelsForDeltaTime( nextW );
		break;
	case DRAGTYPE_MOVEPHONEME:
		m_bLimitDrag = true;
		m_nLeftLimit = PixelsForDeltaTime( prevP );
		m_nRightLimit = PixelsForDeltaTime( nextP );
		break;
	default:
		ClearDragLimit();
		break;
	}
}

void PhonemeEditor::LimitDrag( int& mousex )
{
	if ( m_nDragType == DRAGTYPE_NONE )
		return;

	if ( !m_bLimitDrag )
		return;

	int delta = mousex - m_nStartX;
	if ( delta > 0 )
	{
		if ( m_nRightLimit >= 0 )
		{
			if ( delta > m_nRightLimit )
			{
				mousex = m_nStartX + m_nRightLimit;
			}
		}
	}
	else if ( delta < 0 )
	{
		if ( m_nLeftLimit >= 0 )
		{
			if ( abs( delta ) > abs( m_nLeftLimit ) )
			{
				mousex = m_nStartX - m_nLeftLimit;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Wipe undo/redo data
//-----------------------------------------------------------------------------
void PhonemeEditor::ClearUndo( void )
{
	WipeUndo();
	WipeRedo();

	SetDirty( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *tag - 
//-----------------------------------------------------------------------------
void PhonemeEditor::SelectExpression( CPhonemeTag *tag )
{
	if ( !models->GetActiveStudioModel() )
		return;

	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( !hdr )
		return;

	// Make sure phonemes are loaded
	FacePoser_EnsurePhonemesLoaded();

	CExpClass *cl = expressions->FindClass( "phonemes", true );
	if ( !cl )
	{
		Con_Printf( "Couldn't load expressions/phonemes.txt!\n" );
		return;
	}

	if ( expressions->GetActiveClass() != cl )
	{
		expressions->ActivateExpressionClass( cl );
	}

	CExpression *exp = cl->FindExpression( ConvertPhoneme( tag->GetPhonemeCode() ) );
	if ( !exp )
	{
		Con_Printf( "Couldn't find phoneme '%s'\n", ConvertPhoneme( tag->GetPhonemeCode() )  );
		return;
	}

	float *settings = exp->GetSettings();
	for (LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++)
	{
		int j = hdr->pFlexcontroller( i )->localToGlobal;

		models->GetActiveStudioModel()->SetFlexController( i, settings[j] );
	}
}

void PhonemeEditor::OnSAPI( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	g_viewerSettings.speechapiindex = SPEECH_API_SAPI;

	m_pPhonemeExtractor = NULL;

	CheckSpeechAPI();

	redraw();
}

void PhonemeEditor::OnLipSinc( void )
{
	if ( GetMode() != MODE_PHONEMES )
		return;

	g_viewerSettings.speechapiindex = SPEECH_API_LIPSINC;

	m_pPhonemeExtractor = NULL;

	CheckSpeechAPI();

	redraw();
}

void PhonemeEditor::LoadPhonemeConverters()
{
	m_pPhonemeExtractor = NULL;

	// Enumerate modules under bin folder of exe
	FileFindHandle_t findHandle;
	const char *pFilename = filesystem->FindFirstEx( "phonemeextractors/*.dll", "EXECUTABLE_PATH", &findHandle );
	while( pFilename )
	{	
		char fullpath[ 512 ];
		Q_snprintf( fullpath, sizeof( fullpath ), "phonemeextractors/%s", pFilename );

		Con_Printf( "Loading extractor from %s\n", fullpath );

		Extractor e;
		e.module = Sys_LoadModule( fullpath );
		if ( !e.module )
		{
			pFilename = filesystem->FindNext( findHandle );
			continue;
		}

		CreateInterfaceFn factory = Sys_GetFactory( e.module );
		if ( !factory )
		{
			pFilename = filesystem->FindNext( findHandle );
			continue;
		}

		e.extractor = ( IPhonemeExtractor * )factory( VPHONEME_EXTRACTOR_INTERFACE, NULL );
		if ( !e.extractor )
		{
			Warning( "Unable to get IPhonemeExtractor interface version %s from %s\n", VPHONEME_EXTRACTOR_INTERFACE, fullpath );
			pFilename = filesystem->FindNext( findHandle );
			continue;
		}

		e.apitype = e.extractor->GetAPIType();

		g_Extractors.AddToTail( e );
		pFilename = filesystem->FindNext( findHandle );
	}

	filesystem->FindClose( findHandle );
}

void PhonemeEditor::ValidateSpeechAPIIndex()
{
	if ( !DoesExtractorExistFor( (PE_APITYPE)g_viewerSettings.speechapiindex ) )
	{
		if ( g_Extractors.Count() > 0 )
			g_viewerSettings.speechapiindex = g_Extractors[0].apitype;
	}
}

void PhonemeEditor::UnloadPhonemeConverters()
{
	int c = g_Extractors.Count();
	for ( int i = c - 1; i >= 0; i-- )
	{
		Extractor *e = &g_Extractors[ i ];
		Sys_UnloadModule( e->module );
	}

	g_Extractors.RemoveAll();

	m_pPhonemeExtractor = NULL;
}

bool PhonemeEditor::CheckSpeechAPI( void )
{
	if ( GetMode() != MODE_PHONEMES )
	{
		return false;
	}

	if ( !m_pPhonemeExtractor )
	{
		int c = g_Extractors.Count();
		for ( int i = 0; i < c; i++ )
		{
			Extractor *e = &g_Extractors[ i ];
			if ( e->apitype == (PE_APITYPE)g_viewerSettings.speechapiindex )
			{
				m_pPhonemeExtractor = e->extractor;
				break;
			}
		}

		if ( !m_pPhonemeExtractor )
		{
			Con_ErrorPrintf( "Couldn't find phoneme extractor %i\n",
				g_viewerSettings.speechapiindex );
		}
	}

	return m_pPhonemeExtractor != NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *PhonemeEditor::GetSpeechAPIName( void )
{
	CheckSpeechAPI();

	if ( m_pPhonemeExtractor )
	{
		return m_pPhonemeExtractor->GetName();
	}

	return "Unknown Speech API";
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhonemeEditor::PaintBackground( void )
{
	redraw();
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : PhonemeEditor::EditorMode
//-----------------------------------------------------------------------------
PhonemeEditor::EditorMode PhonemeEditor::GetMode( void ) const
{
	return m_CurrentMode;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rcWorkSpace - 
//			rcEmphasis - 
//-----------------------------------------------------------------------------
void PhonemeEditor::Emphasis_GetRect( RECT const & rcWorkSpace, RECT& rcEmphasis )
{
	rcEmphasis = rcWorkSpace;

	int ybottom = rcWorkSpace.bottom - 2 * m_nTickHeight - 2;
	int workspaceheight = rcWorkSpace.bottom - rcWorkSpace.top;
	
	// Just past midpoint
	rcEmphasis.top		= rcWorkSpace.top + workspaceheight / 2 + 2;
	// 60 units or 
	rcEmphasis.bottom = clamp( rcEmphasis.top + 60, rcEmphasis.top + 20, ybottom );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::OnModeChanged( void )
{
	// Show/hide controls as necessary
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
void PhonemeEditor::Emphasis_Init( void )
{
	m_nNumSelected = 0;
}

CEmphasisSample *PhonemeEditor::Emphasis_GetSampleUnderMouse( mxEvent *event )
{
	if ( GetMode() != MODE_EMPHASIS )
		return NULL;

	if ( !m_pWaveFile )
		return NULL;

	if ( w2() <= 0 )
		return NULL;

	if ( GetPixelsPerSecond() <= 0 )
		return NULL;

	float timeperpixel = 1.0f / GetPixelsPerSecond();
	float closest_dist = 999999.0f;
	CEmphasisSample *bestsample = NULL;

	int samples = m_Tags.GetNumSamples();

	float clickTime = GetTimeForPixel( (short)event->x );

	for ( int i = 0; i < samples; i++ )
	{
		CEmphasisSample *sample = m_Tags.GetSample( i );

		float dist = fabs( sample->time - clickTime );
		if ( dist < closest_dist )
		{
			bestsample = sample;
			closest_dist = dist;
		}

	}

	// Not close to any of them!!!
	if ( closest_dist > ( 5.0f * timeperpixel ) )
	{
		return NULL;
	}

	return bestsample;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::Emphasis_DeselectAll( void )
{
	if ( GetMode() != MODE_EMPHASIS )
		return;

	for ( int i = 0; i < m_Tags.GetNumSamples(); i++ )
	{
		CEmphasisSample *sample = m_Tags.GetSample( i );
		sample->selected = false;
	}
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::Emphasis_SelectAll( void )
{
	if ( GetMode() != MODE_EMPHASIS )
		return;

	for ( int i = 0; i <  m_Tags.GetNumSamples(); i++ )
	{
		CEmphasisSample *sample = m_Tags.GetSample( i );
		sample->selected = true;
	}
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::Emphasis_Delete( void )
{
	if ( GetMode() != MODE_EMPHASIS )
		return;

	SetDirty( true );

	PushUndo();

	for ( int i = m_Tags.GetNumSamples() - 1; i >= 0 ; i-- )
	{
		CEmphasisSample *sample = m_Tags.GetSample( i );
		if ( !sample->selected )
			continue;

		m_Tags.m_EmphasisSamples.Remove( i );

		SetDirty( true );
	}

	PushRedo();

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sample - 
//-----------------------------------------------------------------------------
void PhonemeEditor::Emphasis_AddSample( CEmphasisSample const& sample )
{
	if ( GetMode() != MODE_EMPHASIS )
		return;

	SetDirty( true );

	PushUndo();

	m_Tags.m_EmphasisSamples.AddToTail( sample );
	m_Tags.Resort();

	PushRedo();

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::Emphasis_CountSelected( void )
{
	m_nNumSelected = 0;

	for ( int i = 0; i < m_Tags.GetNumSamples(); i++ )
	{
		CEmphasisSample *sample = m_Tags.GetSample( i );
		if ( !sample || !sample->selected )
			continue;

		m_nNumSelected++;
	}
}

void PhonemeEditor::Emphasis_ShowContextMenu( mxEvent *event )
{
	if ( GetMode() != MODE_EMPHASIS )
		return;

	CountSelected();

	// Construct main menu
	mxPopupMenu *pop = new mxPopupMenu();

	if ( m_nNumSelected > 0 )
	{
		pop->add( va( "Delete" ), IDC_EMPHASIS_DELETE );
		pop->add( "Deselect all", IDC_EMPHASIS_DESELECT );
	}
	pop->add( "Select all", IDC_EMPHASIS_SELECTALL );

	pop->popup( this, (short)event->x, (short)event->y );
}

void PhonemeEditor::Emphasis_MouseDrag( int x, int y )
{
	if ( m_nDragType != DRAGTYPE_EMPHASIS_MOVE )
		return;

	RECT rcWork;
	GetWorkspaceRect( rcWork );

	RECT rc;
	Emphasis_GetRect( rcWork, rc );

	int height = rc.bottom - rc.top;

	int dx = x - m_nLastX;
	int dy = y - m_nLastY;

	float dfdx = (float)dx * GetTimePerPixel();
	float dfdy = (float)dy / (float)height;

	for ( int i = 0; i < m_Tags.GetNumSamples(); i++ )
	{
		CEmphasisSample *sample = m_Tags.GetSample( i );
		if ( !sample || !sample->selected )
			continue;

		sample->time += dfdx;
		//sample->time = clamp( sample->time, 0.0f, 1.0f );

		sample->value -= dfdy;
		sample->value = clamp( sample->value, 0.0f, 1.0f );
	}
}

void PhonemeEditor::Emphasis_Redraw( CChoreoWidgetDrawHelper& drawHelper, RECT& rcWorkSpace )
{
	if ( GetMode() != MODE_EMPHASIS &&
		 GetMode() != MODE_PHONEMES )
		return;

	bool fullmode = GetMode() == MODE_EMPHASIS;
	RECT rcClient;

	Emphasis_GetRect( rcWorkSpace, rcClient );

	RECT rcText;
	rcText = rcClient;

	InflateRect( &rcText, -15, 0 );

	OffsetRect( &rcText, 0, -20 );
	rcText.bottom = rcText.top + 20;

	if ( fullmode )
	{
		drawHelper.DrawColoredText( "Arial", 15, FW_BOLD, PEColor( COLOR_PHONEME_EMPHASIS_TEXT ), rcText, "Emphasis..." );
	}

	{
		int h = rcClient.bottom - rcClient.top;
		int offset = h/3;
		RECT rcSpot = rcClient;
		rcSpot.bottom = rcSpot.top + offset;
		
		drawHelper.DrawGradientFilledRect( 
			rcSpot, 
			PEColor( COLOR_PHONEME_EMPHASIS_BG_STRONG ), 
			PEColor( COLOR_PHONEME_EMPHASIS_BG ), 
			true );

		OffsetRect( &rcSpot, 0, offset );

		drawHelper.DrawFilledRect( PEColor( COLOR_PHONEME_EMPHASIS_BG ), rcSpot );

		OffsetRect( &rcSpot, 0, offset );

		drawHelper.DrawGradientFilledRect( 
			rcSpot, 
			PEColor( COLOR_PHONEME_EMPHASIS_BG ), 
			PEColor( COLOR_PHONEME_EMPHASIS_BG_WEAK ), 
			true );
	}

	COLORREF gray = PEColor( COLOR_PHONEME_EMPHASIS_MIDLINE );

	drawHelper.DrawOutlinedRect( PEColor( COLOR_PHONEME_EMPHASIS_BORDER ), PS_SOLID, 1, rcClient );

	COLORREF lineColor = PEColor( COLOR_PHONEME_EMPHASIS_LINECOLOR );
	COLORREF dotColor = PEColor( COLOR_PHONEME_EMPHASIS_DOTCOLOR );
	COLORREF dotColorSelected = PEColor( COLOR_PHONEME_EMPHASIS_DOTCOLOR_SELECTED );

	int midy = ( rcClient.bottom + rcClient.top ) / 2;

	drawHelper.DrawColoredLine( gray, PS_SOLID, 1, rcClient.left, midy,
		rcClient.right, midy );
	int height = rcClient.bottom - rcClient.top;
	int bottom = rcClient.bottom - 1;

	if ( !m_pWaveFile )
		return;

	float running_length = m_pWaveFile->GetRunningLength();

	// FIXME: adjust this based on framerate....
	float timeperpixel = GetTimePerPixel();

	float starttime, endtime;
	GetScreenStartAndEndTime( starttime, endtime );

	int prevx = 0;
	float prev_t = starttime;
	float prev_value = m_Tags.GetIntensity( prev_t, running_length );

	int dx = 5;

	for ( int x = 0; x < ( w2() + dx ); x += dx )
	{
		float t = GetTimeForPixel( x );

		float value = m_Tags.GetIntensity( t, running_length );

		// Draw segment
		drawHelper.DrawColoredLine( lineColor, PS_SOLID, 1,
			prevx, 
			bottom - prev_value * height,
			x, 
			bottom - value * height );

		prev_t = t;
		prev_value = value;
		prevx = x;

	}

	int numsamples = m_Tags.GetNumSamples();

	for ( int sample = 0; sample < numsamples; sample++ )
	{
		CEmphasisSample *start = m_Tags.GetSample( sample );

		int x = ( start->time - starttime ) / timeperpixel;

		float value = m_Tags.GetIntensity( start->time, running_length );
		int y = bottom - value * height;

		int dotsize = 4;
		int dotSizeSelected = 5;

		COLORREF clr = dotColor;
		COLORREF clrSelected = dotColorSelected;

		drawHelper.DrawCircle( 
			start->selected ? clrSelected : clr, 
			x, y, 
			start->selected ? dotSizeSelected : dotsize,
			true );

	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhonemeEditor::Emphasis_IsValid( void )
{
	if ( m_Tags.GetNumSamples() > 0 )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhonemeEditor::Emphasis_SelectPoints( void )
{
	if ( GetMode() != MODE_EMPHASIS )
		return;

	RECT rcWork, rcEmphasis;
	GetWorkspaceRect( rcWork );

	Emphasis_GetRect( rcWork, rcEmphasis );

	RECT rcSelection;
	
	rcSelection.left = m_nStartX < m_nLastX ? m_nStartX : m_nLastX;
	rcSelection.right = m_nStartX < m_nLastX ? m_nLastX : m_nStartX;

	rcSelection.top = m_nStartY < m_nLastY ? m_nStartY : m_nLastY;
	rcSelection.bottom = m_nStartY < m_nLastY ? m_nLastY : m_nStartY;

	rcSelection.top = max( rcSelection.top, rcEmphasis.top );
	rcSelection.bottom = min( rcSelection.bottom, rcEmphasis.bottom );

	int eh, ew;

	eh = rcEmphasis.bottom - rcEmphasis.top;
	ew = rcEmphasis.right - rcEmphasis.left;

	InflateRect( &rcSelection, 5, 5 );

	if ( !w2() || !h2() )
		return;

	float fleft = GetTimeForPixel( rcSelection.left );
	float fright = GetTimeForPixel( rcSelection.right );

	float ftop = (float)( rcSelection.top - rcEmphasis.top ) / (float)eh;
	float fbottom = (float)( rcSelection.bottom- rcEmphasis.top ) / (float)eh;

	//fleft = clamp( fleft, 0.0f, 1.0f );
	//fright = clamp( fright, 0.0f, 1.0f );
	ftop = clamp( ftop, 0.0f, 1.0f );
	fbottom = clamp( fbottom, 0.0f, 1.0f );

	float eps = 0.005;

	for ( int i = 0; i < m_Tags.GetNumSamples(); i++ )
	{
		CEmphasisSample *sample = m_Tags.GetSample( i );
		
		if ( sample->time + eps < fleft )
			continue;

		if ( sample->time - eps > fright )
			continue;

		if ( (1.0f - sample->value ) + eps < ftop )
			continue;

		if ( (1.0f - sample->value ) - eps > fbottom )
			continue;

		sample->selected = true;
	}

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rcHandle - 
//-----------------------------------------------------------------------------
void PhonemeEditor::GetScrubHandleRect( RECT& rcHandle, bool clipped )
{
	float pixel = 0.0f;

	if ( m_pWaveFile )
	{
		float currenttime = m_flScrub;
		float starttime, endtime;
		GetScreenStartAndEndTime( starttime, endtime );

		float screenfrac = ( currenttime - starttime ) / ( endtime - starttime );

		pixel = screenfrac * w2();

		if ( clipped )
		{
			pixel = clamp( pixel, SCRUBBER_HANDLE_WIDTH/2, w2() - SCRUBBER_HANDLE_WIDTH/2 );
		}
	}

	rcHandle.left = pixel-SCRUBBER_HANDLE_WIDTH/2;
	rcHandle.right = pixel + SCRUBBER_HANDLE_WIDTH/2;
	rcHandle.top = 2 + GetCaptionHeight() + 12;
	rcHandle.bottom = rcHandle.top + SCRUBBER_HANDLE_HEIGHT;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rcArea - 
//-----------------------------------------------------------------------------
void PhonemeEditor::GetScrubAreaRect( RECT& rcArea )
{
	rcArea.left = 0;
	rcArea.right = w2();
	rcArea.top = 2 + GetCaptionHeight() + 12;
	rcArea.bottom = rcArea.top + SCRUBBER_HEIGHT - 4;
}

void PhonemeEditor::DrawScrubHandle()
{
	RECT rcHandle;
	GetScrubHandleRect( rcHandle, true );
	rcHandle.left = 0;
	rcHandle.right = w2();

	CChoreoWidgetDrawHelper drawHelper( this, rcHandle );

	DrawScrubHandle( drawHelper );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rcHandle - 
//-----------------------------------------------------------------------------
void PhonemeEditor::DrawScrubHandle( CChoreoWidgetDrawHelper& drawHelper )
{
	RECT rcHandle;
	GetScrubHandleRect( rcHandle, true );

	HBRUSH br = CreateSolidBrush( RGB( 0, 150, 100 ) );

	COLORREF areaBorder = RGB( 230, 230, 220 );

	drawHelper.DrawColoredLine( areaBorder,
		PS_SOLID, 1, 0, rcHandle.top, w2(), rcHandle.top );
	drawHelper.DrawColoredLine( areaBorder,
		PS_SOLID, 1, 0, rcHandle.bottom, w2(), rcHandle.bottom );

	drawHelper.DrawFilledRect( br, rcHandle );

	// 
	char sz[ 32 ];
	sprintf( sz, "%.3f", m_flScrub );

	int len = drawHelper.CalcTextWidth( "Arial", 9, 500, sz );

	RECT rcText = rcHandle;
	int textw = rcText.right - rcText.left;

	rcText.left += ( textw - len ) / 2;

	drawHelper.DrawColoredText( "Arial", 9, 500, RGB( 255, 255, 255 ), rcText, sz );

	DeleteObject( br );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhonemeEditor::IsMouseOverScrubHandle( mxEvent *event )
{
	RECT rcHandle;
	GetScrubHandleRect( rcHandle, true );
	InflateRect( &rcHandle, 2, 2 );

	POINT pt;
	pt.x = (short)event->x;
	pt.y = (short)event->y;
	if ( PtInRect( &rcHandle, pt ) )
	{
		return true;
	}
	return false;
}

bool PhonemeEditor::IsMouseOverScrubArea( mxEvent *event )
{
	RECT rcArea;

	rcArea.left = 0;
	rcArea.right = w2();
	rcArea.top = 2 + GetCaptionHeight() + 12;
	rcArea.bottom = rcArea.top + 10;

	InflateRect( &rcArea, 2, 2 );

	POINT pt;
	pt.x = (short)event->x;
	pt.y = (short)event->y;
	if ( PtInRect( &rcArea, pt ) )
	{
		return true;
	}

	return false;
}

float PhonemeEditor::GetTimeForSample( int sample )
{
	if ( !m_pWaveFile )
	{
		return 0.0f;
	}

	float duration = m_pWaveFile->GetRunningLength();
	int sampleCount = m_pWaveFile->SampleCount();
	if ( sampleCount <= 0 )
		return 0.0f;

	float frac = (float)sample / (float)sampleCount;

	return frac * duration;
}

void PhonemeEditor::ClampTimeToSelectionInterval( float& timeval )
{
	if ( !m_pWaveFile )
	{
		return;
	}
	if ( !m_pMixer || !sound->IsSoundPlaying( m_pMixer ) )
	{
		return;
	}

	if ( !m_bSelectionActive )
		return;

	float starttime = GetTimeForSample( m_nSelection[ 0 ] );
	float endtime = GetTimeForSample( m_nSelection[ 1 ] );

	Assert( starttime <= endtime );

	timeval = clamp( timeval, starttime, endtime );
}

void PhonemeEditor::ScrubThink( float dt, bool scrubbing )
{
	ClampTimeToSelectionInterval( m_flScrub );
	ClampTimeToSelectionInterval( m_flScrubTarget );

	if ( m_flScrubTarget == m_flScrub && !scrubbing )
	{
		if ( sound->IsSoundPlaying( m_pMixer ) )
		{
			sound->StopSound( m_pMixer );
		}
		return;
	}

	if ( !m_pWaveFile )
		return;

	bool newmixer = false;
	if ( !m_pMixer || !sound->IsSoundPlaying( m_pMixer ) )
	{
		m_pMixer = NULL;
		SaveLinguisticData();

		StudioModel *model = NULL;//models->GetActiveStudioModel();

		sound->PlaySound( model, VOL_NORM, m_WorkFile.m_szWorkingFile, &m_pMixer );
		newmixer = true;
	}

	if ( !m_pMixer )
	{
		return;
	}

	if ( m_flScrub > m_flScrubTarget )
	{
		m_pMixer->SetDirection( false );
	}
	else
	{
		m_pMixer->SetDirection( true );
	}

	float duration = m_pWaveFile->GetRunningLength();
	if ( !duration )
		return;

	float d = m_flScrubTarget - m_flScrub;
	int sign = d > 0.0f ? 1 : -1;

	float maxmove = dt * m_flPlaybackRate;

	if ( sign > 0 )
	{
		if ( d < maxmove )
		{
			m_flScrub = m_flScrubTarget;
		}
		else
		{
			m_flScrub += maxmove;
		}
	}
	else
	{
		if ( -d < maxmove )
		{
			m_flScrub = m_flScrubTarget;
		}
		else
		{
			m_flScrub -= maxmove;
		}
	}

	int sampleCount = m_pMixer->GetSource()->SampleCount();

	int cursample = sampleCount * ( m_flScrub / duration );

	int realsample = m_pMixer->GetSamplePosition();

	int dsample = cursample - realsample;

	int onehundredth = m_pMixer->GetSource()->SampleRate() * 0.01f;

	if ( abs( dsample ) > onehundredth )
	{
		m_pMixer->SetSamplePosition( cursample, true );
	}
	m_pMixer->SetActive( true );

	RECT rcArea;
	GetScrubAreaRect( rcArea );

	CChoreoWidgetDrawHelper drawHelper( this, rcArea );
	DrawScrubHandle( drawHelper );

	if ( scrubbing )
	{
		g_pMatSysWindow->Frame();
	}
}

void PhonemeEditor::SetScrubTime( float t )
{
	m_flScrub = t;
	ClampTimeToSelectionInterval( m_flScrub );
}

void PhonemeEditor::SetScrubTargetTime( float t )
{
	m_flScrubTarget = t;
	ClampTimeToSelectionInterval( m_flScrubTarget );
}


void PhonemeEditor::OnToggleVoiceDuck()
{
	SetDirty( true );
	PushUndo();
	m_Tags.SetVoiceDuck( !m_Tags.GetVoiceDuck() );
	PushRedo();
	redraw();
}

void PhonemeEditor::Play()
{
	PlayEditedWave( m_bSelectionActive );
}





