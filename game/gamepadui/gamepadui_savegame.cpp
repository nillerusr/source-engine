#include "gamepadui_interface.h"
#include "gamepadui_image.h"
#include "gamepadui_util.h"
#include "gamepadui_genericconfirmation.h"
#include "gamepadui_scrollbar.h"

#include "ienginevgui.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "GameUI/IGameUI.h"

#include "vgui_controls/ComboBox.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/ScrollBar.h"
#include "savegame_version.h"

#include "KeyValues.h"
#include "filesystem.h"

#include "tier0/memdbgon.h"

ConVar gamepadui_savegame_use_delete_mode( "gamepadui_savegame_use_delete_mode", "1", FCVAR_NONE, "Causes the save game panel to use a \"delete mode\" when not using a controller, showing X buttons next to each save game" );
#ifdef GAMEPADUI_GAME_EZ2
ConVar gamepadui_savegame_wilson_thumb( "gamepadui_savegame_wilson_thumb", "1", FCVAR_NONE, "Shows a Wilson icon on save games that have Wilson" );
#endif

class GamepadUISaveButton;
struct SaveGameDescription_t;

class GamepadUISaveGamePanel : public GamepadUIFrame
{
    DECLARE_CLASS_SIMPLE( GamepadUISaveGamePanel, GamepadUIFrame );

public:
    GamepadUISaveGamePanel( vgui::Panel *pParent, const char* pPanelName, bool bIsSave );
	~GamepadUISaveGamePanel();

    void UpdateGradients();

	void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE;
	void OnThink() OVERRIDE;
    void Paint() OVERRIDE;
    void OnCommand( char const* pCommand ) OVERRIDE;
    void OnMouseWheeled( int nDelta ) OVERRIDE;

	bool InDeleteMode() { return m_pDeletePanels.Count() > 0; }

#ifdef GAMEPADUI_GAME_EZ2
	int IsSaveSuspect( const char *pszEZ2Version, const char *pszMapName, int nMapVersion, const char **ppszIncompatibleVersion, const char **ppszLastCompatibleBranch );

    GamepadUIImage &GetWilsonThumb( float &flSize, float &flOffsetX, float &flOffsetY )
    {
        flSize = m_flThumbSize; flOffsetX = m_flThumbOffsetX; flOffsetY = m_flThumbOffsetY;
        return m_WilsonThumb;
    }
#endif

    MESSAGE_FUNC_HANDLE( OnGamepadUIButtonNavigatedTo, "OnGamepadUIButtonNavigatedTo", button );

private:
    void ScanSavedGames();
    void LayoutSaveButtons();
    bool ParseSaveData( char const* pFileName, char const* pShortName, SaveGameDescription_t& save );
    static int SaveReadNameAndComment( FileHandle_t f, OUT_Z_CAP( nameSize ) char* name, int nameSize, OUT_Z_CAP( commentSize ) char* comment, int commentSize );
#ifdef GAMEPADUI_GAME_EZ2
	static int SaveReadCustomMetadata( const char *pSaveName, char *ez2version, int ez2versionSize, char *platform, int platformSize, int &nMapVersion, bool &bDeck, bool &bWilson );

	void LoadVersionHistory();
	void UnloadVersionHistory();
#endif
    void FindSaveSlot( OUT_Z_CAP( bufsize ) char* buffer, int bufsize );
    void DeleteSaveGame( const char* pFileName );

    void LoadGame( const SaveGameDescription_t* pSave );
    void SaveGame( const SaveGameDescription_t *pSave );

	GamepadUIString m_strNoSaveString;

    CUtlVector<GamepadUISaveButton*> m_pSavePanels;
    CUtlVector<SaveGameDescription_t> m_Saves;
	
    CUtlVector<GamepadUIButton*> m_pDeletePanels;

    GamepadUIScrollState m_ScrollState;

	GamepadUIScrollBar *m_pScrollBar = NULL;

    bool m_bIsSave;

#ifdef GAMEPADUI_GAME_EZ2
	KeyValues *m_pVersionHistory;

	GamepadUIImage m_WilsonThumb;
#endif

    GAMEPADUI_PANEL_PROPERTY( float, m_flSavesFade, "Saves.Fade", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flSavesOffsetX, "Saves.OffsetX", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flSavesOffsetY, "Saves.OffsetY", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flSavesSpacing, "Saves.Spacing", "0", SchemeValueTypes::ProportionalFloat );

	GAMEPADUI_PANEL_PROPERTY( float, m_flThumbSize, "Saves.Thumb.Size", "16", SchemeValueTypes::ProportionalFloat );
	GAMEPADUI_PANEL_PROPERTY( float, m_flThumbOffsetX, "Saves.Thumb.OffsetX", "4", SchemeValueTypes::ProportionalFloat );
	GAMEPADUI_PANEL_PROPERTY( float, m_flThumbOffsetY, "Saves.Thumb.OffsetY", "4", SchemeValueTypes::ProportionalFloat );
};

/* From GameUI */
struct SaveGameDescription_t;
#define SAVEGAME_MAPNAME_LEN 32
#define SAVEGAME_COMMENT_LEN 80
#define SAVEGAME_ELAPSED_LEN 32

#define TGA_IMAGE_PANEL_WIDTH 180
#define TGA_IMAGE_PANEL_HEIGHT 100
#define MAX_LISTED_SAVE_GAMES	128
#define NEW_SAVE_GAME_TIMESTAMP	0xFFFFFFFF

struct SaveGameDescription_t
{
	char szShortName[64];
	char szFileName[128];
	char szMapName[SAVEGAME_MAPNAME_LEN];
	char szComment[SAVEGAME_COMMENT_LEN];
	char szType[64];
	char szElapsedTime[SAVEGAME_ELAPSED_LEN];
	char szFileTime[32];
	unsigned int iTimestamp;
	unsigned int iSize;
#ifdef GAMEPADUI_GAME_EZ2
	char szEZ2Version[8];
	char szPlatform[16];
	int nMapVersion;
	bool bDeck;
	bool bWilson;
#endif
};
/* End from GameUI */

#ifdef GAMEPADUI_GAME_EZ2
enum
{
	SaveSuspectLevel_None,
	SaveSuspectLevel_Mismatch,		// Save is from a different version
	SaveSuspectLevel_Incompatible,	// Save is from an explicitly incompatible version
};
#endif

class GamepadUISaveButton : public GamepadUIButton
{
public:
    DECLARE_CLASS_SIMPLE( GamepadUISaveButton, GamepadUIButton );

    GamepadUISaveButton( vgui::Panel* pParent, vgui::Panel* pActionSignalTarget, const char *pSchemeFile, const char* pCommand, const SaveGameDescription_t *pSaveGame )
        : BaseClass( pParent, pActionSignalTarget, pSchemeFile, pCommand, pSaveGame->szComment, pSaveGame->szFileTime )
        , m_Image()
		, m_pSaveGame( pSaveGame )
    {
		char tga[_MAX_PATH];
		Q_strncpy( tga, pSaveGame->szFileName, sizeof( tga ) );
		char *ext = strstr( tga, ".sav" );
		if ( ext )
			strcpy( ext, ".tga" );

		char tga2[_MAX_PATH];
		Q_snprintf( tga2, sizeof( tga2 ), "//MOD/%s", tga );

		if ( g_pFullFileSystem->FileExists( tga2 ) )
		{
			m_Image.SetTGAImage( tga2 );
			m_bUseTGAImage = true;
		}
		else
		{
			m_Image.SetImage( "gamepadui/save_game.vmt" );
		}

#ifdef GAMEPADUI_GAME_EZ2
		m_strEZ2Version = GamepadUIString( pSaveGame->szEZ2Version );
#endif
    }

#ifdef GAMEPADUI_GAME_EZ2
	void RunAnimations( ButtonState state )
	{
		BaseClass::RunAnimations( state );

		GAMEPADUI_RUN_ANIMATION_COMMAND( m_colVersion, vgui::AnimationController::INTERPOLATOR_LINEAR );
	}

	void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE
	{
		BaseClass::ApplySchemeSettings( pScheme );

		char szVersion[8];
		V_UnicodeToUTF8( m_strEZ2Version.String(), szVersion, sizeof( szVersion ) );
		m_iSaveSuspectLevel = static_cast<GamepadUISaveGamePanel *>(GetParent())->IsSaveSuspect( szVersion, m_pSaveGame->szMapName, m_pSaveGame->nMapVersion, &m_pIncompatibleVersion, &m_pLastCompatibleBranch );

		if ( m_iSaveSuspectLevel != SaveSuspectLevel_None )
		{
			switch (m_iSaveSuspectLevel)
			{
				case SaveSuspectLevel_Mismatch:
					m_colVersionAnimationValue[ButtonStates::Out] = m_colVersionMismatch;
					break;

				case SaveSuspectLevel_Incompatible:
				{
					// For now, correspond suspect colors to over and out states
					m_colBackgroundColorAnimationValue[ButtonStates::Over] = m_colVersionIncompatible;
					m_colTextColorAnimationValue[ButtonStates::Out] = m_colVersionIncompatible;
					m_colDescriptionColorAnimationValue[ButtonStates::Out] = m_colVersionIncompatible;
					m_colVersionAnimationValue[ButtonStates::Out] = m_colVersionIncompatible;
					break;
				}
			}

			DoAnimations( true );
		}
	}
#endif

    void Paint() OVERRIDE
    {
        int x, y, w, t;
        GetBounds( x, y, w, t );

        PaintButton();

		// Save game icons are 180x100
		int imgW = (t * 180) / 100;

		if ( m_Image.IsValid() )
		{
			vgui::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
			vgui::surface()->DrawSetTexture( m_Image );
			int imgH = t;
			// Half pixel offset to avoid leaking into pink + black
			if ( m_bUseTGAImage )
			{
				const float flHalfPixelX = ( 0.5f / 180.0f );
				const float flHalfPixelY = ( 0.5f / 100.0f );
				vgui::surface()->DrawTexturedSubRect( 0, 0, imgW, imgH, 0.0f, 0.0f, 1.0f - flHalfPixelX, 1.0f - flHalfPixelY );
			}
			else
			{
				vgui::surface()->DrawTexturedRect( 0, 0, imgW, imgH );
			}
			vgui::surface()->DrawSetTexture( 0 );
		}
		else
		{
			vgui::surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );
			imgW = ( t * 180 ) / 100;
			int imgH = t;
			vgui::surface()->DrawFilledRect( 0, 0, imgW, imgH );
		}

        PaintText();

#ifdef GAMEPADUI_GAME_EZ2
		if ( m_pSaveGame->iTimestamp != NEW_SAVE_GAME_TIMESTAMP )
		{
			const wchar_t *pwszEZ2Version = m_strEZ2Version.String();
			int nEZ2VerLen = m_strEZ2Version.Length();
			if (m_strEZ2Version.IsEmpty())
			{
				// Display a question mark for unknown versions
				pwszEZ2Version = L"?";
				nEZ2VerLen = 1;
			}

			int nTextW, nTextH;
			vgui::surface()->GetTextSize( m_hDescriptionFont, pwszEZ2Version, nTextW, nTextH );

			int nTextX = m_flWidth - m_flTextOffsetX - nTextW + imgW;
			int nTextY = m_flHeight + m_flTextOffsetY - nTextH;

			vgui::surface()->DrawSetTextFont( m_hTextFont );
			vgui::surface()->DrawSetTextPos( nTextX, nTextY );
			vgui::surface()->DrawSetTextColor( m_colVersion );
			vgui::surface()->DrawPrintText( pwszEZ2Version, nEZ2VerLen );
		}

		if ( m_pSaveGame->bWilson && gamepadui_savegame_wilson_thumb.GetBool() )
		{
			float flSize, flOffsetX, flOffsetY;
			vgui::surface()->DrawSetColor( m_colDescriptionColor );
			vgui::surface()->DrawSetTexture( static_cast<GamepadUISaveGamePanel*>(GetParent())->GetWilsonThumb( flSize, flOffsetX, flOffsetY ) );
			//vgui::surface()->DrawTexturedSubRect( m_flWidth - flOffsetX - flSize, flOffsetY, m_flWidth - flOffsetX, flOffsetY + flSize, 0.28125f, 0.1875f, 0.703125f, 0.703125f );
			vgui::surface()->DrawTexturedSubRect( flOffsetX, flOffsetY, flOffsetX + flSize, flOffsetY + flSize, 0.28125f, 0.1875f, 0.703125f, 0.703125f );
			vgui::surface()->DrawSetTexture( 0 );
		}
#endif
    }

	const SaveGameDescription_t* GetSaveGame() const
	{
		return m_pSaveGame;
	}

#ifdef GAMEPADUI_GAME_EZ2
	int GetSaveSuspectLevel() const
	{
		return m_iSaveSuspectLevel;
	}

	const char *GetIncompatibleVersion() const
	{
		return m_pIncompatibleVersion;
	}

	const char *GetLastCompatibleBranch() const
	{
		return m_pLastCompatibleBranch;
	}
#endif

private:
	bool m_bUseTGAImage = false;
    GamepadUIImage m_Image;
	const SaveGameDescription_t *m_pSaveGame;
#ifdef GAMEPADUI_GAME_EZ2
	GamepadUIString m_strEZ2Version;

	int m_iSaveSuspectLevel;
	const char *m_pIncompatibleVersion = NULL;
	const char *m_pLastCompatibleBranch = NULL;

	GAMEPADUI_BUTTON_ANIMATED_PROPERTY( Color, m_colVersion, "Button.Version", "255 255 255 255", SchemeValueTypes::Color );
	GAMEPADUI_PANEL_PROPERTY( Color, m_colVersionMismatch, "Button.Version.Mismatch", "255 224 0 255", SchemeValueTypes::Color );
	GAMEPADUI_PANEL_PROPERTY( Color, m_colVersionIncompatible, "Button.Version.Incompatible", "255 128 0 255", SchemeValueTypes::Color );
#endif
};

GamepadUISaveGamePanel::GamepadUISaveGamePanel( vgui::Panel* pParent, const char* pPanelName, bool bIsSave )
	: BaseClass( pParent, pPanelName )
	, m_bIsSave( bIsSave )
{
    vgui::HScheme Scheme = vgui::scheme()->LoadSchemeFromFileEx( GamepadUI::GetInstance().GetSizingVPanel(), GAMEPADUI_DEFAULT_PANEL_SCHEME, "SchemePanel" );
    SetScheme( Scheme );

    GetFrameTitle() = GamepadUIString(m_bIsSave ? "#GameUI_SaveGame" : "#GameUI_LoadGame");

    Activate();

#ifdef GAMEPADUI_GAME_EZ2
	m_WilsonThumb.SetImage( "vgui/icons/icon_wilson" );

	LoadVersionHistory();
#endif
    ScanSavedGames();

    if ( m_pSavePanels.Count() )
        m_pSavePanels[0]->NavigateTo();

	UpdateGradients();
}

GamepadUISaveGamePanel::~GamepadUISaveGamePanel()
{
#ifdef GAMEPADUI_GAME_EZ2
	UnloadVersionHistory();
#endif
}

void GamepadUISaveGamePanel::UpdateGradients()
{
	const float flTime = GamepadUI::GetInstance().GetTime();
	GamepadUI::GetInstance().GetGradientHelper()->ResetTargets( flTime );
	GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Up, { 1.0f, 1.0f }, flTime );
	GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Down, { 1.0f, 1.0f }, flTime );
}

void GamepadUISaveGamePanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_bFooterButtonsStack = true;

	float flX, flY;
    if (GamepadUI::GetInstance().GetScreenRatio( flX, flY ))
    {
        m_flSavesOffsetX *= (flX);
    }

	int nX, nY;
	GamepadUI::GetInstance().GetSizingPanelOffset( nX, nY );
	if (nX > 0)
	{
		GamepadUI::GetInstance().GetSizingPanelScale( flX, flY );
		flX *= 0.4f;

		m_flSavesOffsetX += ((float)nX) * flX;
		m_flSavesFade += ((float)nX) * flX;
	}

	if ( m_pScrollBar )
	{
		m_pScrollBar->InitScrollBar( &m_ScrollState, m_flSavesOffsetX + m_pSavePanels[0]->GetWide() + m_flSavesSpacing, m_flSavesOffsetY );
	}
}

void GamepadUISaveGamePanel::OnThink()
{
	BaseClass::OnThink();

	LayoutSaveButtons();
}

void GamepadUISaveGamePanel::Paint()
{
    BaseClass::Paint();

	if ( !m_strNoSaveString.IsEmpty() )
	{
		int nParentW, nParentH;
		GetParent()->GetSize( nParentW, nParentH );

		vgui::surface()->DrawSetTextFont( m_hGenericFont );
		vgui::surface()->DrawSetTextColor( Color( 255, 255, 255, 255 ) );
		int nTextW, nTextH;
		vgui::surface()->GetTextSize( m_hGenericFont, m_strNoSaveString.String(), nTextW, nTextH );
		vgui::surface()->DrawSetTextPos( nParentW / 2 - nTextW / 2, m_flSavesOffsetY + nTextH );
		vgui::surface()->DrawPrintText( m_strNoSaveString.String(), m_strNoSaveString.Length() );
	}
}

/* Mostly from GameUI */
void GamepadUISaveGamePanel::ScanSavedGames()
{
	m_Saves.Purge();
	m_pSavePanels.PurgeAndDeleteElements();
	m_pDeletePanels.PurgeAndDeleteElements();

	if ( m_bIsSave )
		m_Saves.AddToTail( SaveGameDescription_t{ "NewSavedGame", "", "", "#GameUI_NewSaveGame", "", "", "Current", NEW_SAVE_GAME_TIMESTAMP } );

	// populate list box with all saved games on record:
	char	szDirectory[_MAX_PATH];
	Q_snprintf( szDirectory, sizeof( szDirectory ), "save/*.sav" );

	// iterate the saved files
	FileFindHandle_t handle;
	const char* pFileName = g_pFullFileSystem->FindFirst( szDirectory, &handle );
	while ( pFileName )
	{
		if ( !Q_strnicmp( pFileName, "HLSave", strlen( "HLSave" ) ) )
		{
			pFileName = g_pFullFileSystem->FindNext( handle );
			continue;
		}

		char szFileName[_MAX_PATH];
		Q_snprintf( szFileName, sizeof( szFileName ), "save/%s", pFileName );

		// Only load save games from the current mod's save dir
		if ( !g_pFullFileSystem->FileExists( szFileName, "MOD" ) )
		{
			pFileName = g_pFullFileSystem->FindNext( handle );
			continue;
		}

		SaveGameDescription_t save;
		if ( ParseSaveData( szFileName, pFileName, save ) )
		{
			m_Saves.AddToTail( save );
		}

		pFileName = g_pFullFileSystem->FindNext( handle );
	}

	g_pFullFileSystem->FindClose( handle );

	// sort the save list
	qsort( m_Saves.Base(), m_Saves.Count(), sizeof( SaveGameDescription_t ), []( const void* lhs, const void* rhs ) {
		const SaveGameDescription_t *s1 = ( const SaveGameDescription_t * )lhs;
		const SaveGameDescription_t *s2 = ( const SaveGameDescription_t * )rhs;

		if ( s1->iTimestamp < s2->iTimestamp )
			return 1;
		else if ( s1->iTimestamp > s2->iTimestamp )
			return -1;

		// timestamps are equal, so just sort by filename
		return strcmp( s1->szFileName, s2->szFileName );
	} );

	// add to the list
	for ( int saveIndex = 0; saveIndex < m_Saves.Count() && saveIndex < MAX_LISTED_SAVE_GAMES; saveIndex++ )
	{
		GamepadUISaveButton *button = new GamepadUISaveButton( this, this, GAMEPADUI_RESOURCE_FOLDER "schemesavebutton.res", "load_save", &m_Saves[saveIndex] );
		button->SetPriority( saveIndex );
		button->SetForwardToParent( true );
		m_pSavePanels.AddToTail( button );
	}

	// display a message if there are no save games
	if ( !m_Saves.Count() )
	{
		m_strNoSaveString = GamepadUIString( "#GameUI_NoSaveGamesToDisplay" );
		SetFooterButtons( FooterButtons::Back );
	}
	else
	{
		if ( m_bIsSave )
		{
			SetFooterButtons( FooterButtons::Back | FooterButtons::Select, FooterButtons::Select );
		}
		else
		{
			SetFooterButtons( FooterButtons::Back | FooterButtons::Delete | FooterButtons::Select, FooterButtons::Select );
		}
	}

	SetControlEnabled( "loadsave", false );
	SetControlEnabled( "delete", false );

    if ( m_pSavePanels.Count() )
	{
		if ( !m_pScrollBar )
		{
			m_pScrollBar = new GamepadUIScrollBar(
				this, this,
				GAMEPADUI_RESOURCE_FOLDER "schemescrollbar.res",
				NULL, false );

			m_pScrollBar->SetNavLeft( m_pSavePanels[0] );
		}
	}
	else if ( m_pScrollBar )
	{
		m_pScrollBar->MarkForDeletion();
		m_pScrollBar = NULL;
	}

    for ( int i = 1; i < m_pSavePanels.Count(); i++ )
    {
        m_pSavePanels[i]->SetNavUp( m_pSavePanels[i - 1] );
        m_pSavePanels[i - 1]->SetNavDown( m_pSavePanels[i] );
    }
}

bool GamepadUISaveGamePanel::ParseSaveData( char const* pFileName, char const* pShortName, SaveGameDescription_t& save )
{
	char szMapName[SAVEGAME_MAPNAME_LEN];
	char szComment[SAVEGAME_COMMENT_LEN];
	char szElapsedTime[SAVEGAME_ELAPSED_LEN];

	if ( !pFileName || !pShortName )
		return false;

	Q_strncpy( save.szShortName, pShortName, sizeof( save.szShortName ) );
	Q_strncpy( save.szFileName, pFileName, sizeof( save.szFileName ) );

	FileHandle_t fh = g_pFullFileSystem->Open( pFileName, "rb", "MOD" );
	if ( fh == FILESYSTEM_INVALID_HANDLE )
		return false;

	int readok = SaveReadNameAndComment( fh, szMapName, ARRAYSIZE( szMapName ), szComment, ARRAYSIZE( szComment ) );
	g_pFullFileSystem->Close( fh );

	if ( !readok )
	{
		return false;
	}

#ifdef GAMEPADUI_GAME_EZ2
	char szEZ2Version[8];
	char szPlatform[16];
	int nMapVersion = 0;
	bool bDeck = false;
	bool bWilson = false;
	SaveReadCustomMetadata( pFileName, szEZ2Version, sizeof(szEZ2Version), szPlatform, sizeof(szPlatform), nMapVersion, bDeck, bWilson );
#endif

	Q_strncpy( save.szMapName, szMapName, sizeof( save.szMapName ) );

	// Elapsed time is the last 6 characters in comment. ( mmm:ss )
	int i;
	i = strlen( szComment );
	Q_strncpy( szElapsedTime, "??", sizeof( szElapsedTime ) );
	if ( i >= 6 )
	{
		Q_strncpy( szElapsedTime, ( char* )&szComment[i - 6], 7 );
		szElapsedTime[6] = '\0';

		// parse out
		int minutes = atoi( szElapsedTime );
		int seconds = atoi( szElapsedTime + 4 );

		// reformat
		if ( minutes )
		{
			Q_snprintf( szElapsedTime, sizeof( szElapsedTime ), "%d %s %d seconds", minutes, minutes > 1 ? "minutes" : "minute", seconds );
		}
		else
		{
			Q_snprintf( szElapsedTime, sizeof( szElapsedTime ), "%d seconds", seconds );
		}

		// Chop elapsed out of comment.
		int n;

		n = i - 6;
		szComment[n] = '\0';

		n--;

		// Strip back the spaces at the end.
		while ( ( n >= 1 ) &&
			szComment[n] &&
			szComment[n] == ' ' )
		{
			szComment[n--] = '\0';
		}
	}

	// calculate the file name to print
	const char* pszType = "";
	if ( strstr( pFileName, "quick" ) )
	{
		pszType = "#GameUI_QuickSave";
	}
	else if ( strstr( pFileName, "autosave" ) )
	{
		pszType = "#GameUI_AutoSave";
	}

	Q_strncpy( save.szType, pszType, sizeof( save.szType ) );
	Q_strncpy( save.szComment, szComment, sizeof( save.szComment ) );
	Q_strncpy( save.szElapsedTime, szElapsedTime, sizeof( save.szElapsedTime ) );

	// Now get file time stamp.
	long fileTime = g_pFullFileSystem->GetFileTime( pFileName );
	char szFileTime[32];
	g_pFullFileSystem->FileTimeToString( szFileTime, sizeof( szFileTime ), fileTime );
	char* newline = strstr( szFileTime, "\n" );
	if ( newline )
	{
		*newline = 0;
	}
	Q_strncpy( save.szFileTime, szFileTime, sizeof( save.szFileTime ) );
	save.iTimestamp = fileTime;
#ifdef GAMEPADUI_GAME_EZ2
	Q_strncpy( save.szEZ2Version, szEZ2Version, sizeof( save.szEZ2Version ) );
	Q_strncpy( save.szPlatform, szPlatform, sizeof( save.szPlatform ) );
	save.nMapVersion = nMapVersion;
	save.bDeck = bDeck;
	save.bWilson = bWilson;
#endif
	return true;
}

int GamepadUISaveGamePanel::SaveReadNameAndComment( FileHandle_t f, OUT_Z_CAP( nameSize ) char* name, int nameSize, OUT_Z_CAP( commentSize ) char* comment, int commentSize )
{
	int i, tag, size, tokenSize, tokenCount;
	char* pSaveData, * pFieldName, ** pTokenList;

	name[0] = '\0';
	comment[0] = '\0';

	g_pFullFileSystem->Read( &tag, sizeof( int ), f );
	if ( tag != MAKEID( 'J', 'S', 'A', 'V' ) )
	{
		return 0;
	}

	g_pFullFileSystem->Read( &tag, sizeof( int ), f );
	if ( tag != SAVEGAME_VERSION )				// Enforce version for now
	{
		return 0;
	}

	g_pFullFileSystem->Read( &size, sizeof( int ), f );

	g_pFullFileSystem->Read( &tokenCount, sizeof( int ), f );	// These two ints are the token list
	g_pFullFileSystem->Read( &tokenSize, sizeof( int ), f );
	size += tokenSize;

	// Sanity Check.
	if ( tokenCount < 0 || tokenCount > 1024 * 1024 * 32 )
	{
		return 0;
	}

	if ( tokenSize < 0 || tokenSize > 1024 * 1024 * 32 )
	{
		return 0;
	}

	pSaveData = ( char* )new char[size];
	g_pFullFileSystem->Read( pSaveData, size, f );

	int nNumberOfFields;

	char* pData;
	int nFieldSize;

	pData = pSaveData;

	// Allocate a table for the strings, and parse the table
	if ( tokenSize > 0 )
	{
		pTokenList = new char* [tokenCount];

		// Make sure the token strings pointed to by the pToken hashtable.
		for ( i = 0; i < tokenCount; i++ )
		{
			pTokenList[i] = *pData ? pData : NULL;	// Point to each string in the pToken table
			while ( *pData++ );				// Find next token ( after next null )
		}
	}
	else
		pTokenList = NULL;

	// short, short ( size, index of field name )
	nFieldSize = *( short* )pData;
	pData += sizeof( short );
	pFieldName = pTokenList[*( short* )pData];

	if ( stricmp( pFieldName, "GameHeader" ) )
	{
		delete[] pSaveData;
		return 0;
	};

	// int ( fieldcount )
	pData += sizeof( short );
	nNumberOfFields = *( int* )pData;
	pData += nFieldSize;

	// Each field is a short ( size ), short ( index of name ), binary string of "size" bytes ( data )
	for ( i = 0; i < nNumberOfFields; i++ )
	{
		// Data order is:
		// Size
		// szName
		// Actual Data

		nFieldSize = *( short* )pData;
		pData += sizeof( short );

		pFieldName = pTokenList[*( short* )pData];
		pData += sizeof( short );

		if ( !stricmp( pFieldName, "comment" ) )
		{
			int copySize = MAX( commentSize, nFieldSize );
			Q_strncpy( comment, pData, copySize );
		}
		else if ( !stricmp( pFieldName, "mapName" ) )
		{
			int copySize = MAX( nameSize, nFieldSize );
			Q_strncpy( name, pData, copySize );
		};

		// Move to Start of next field.
		pData += nFieldSize;
	};

	// Delete the string table we allocated
	delete[] pTokenList;
	delete[] pSaveData;

	if ( strlen( name ) > 0 && strlen( comment ) > 0 )
		return 1;

	return 0;
}

#ifdef GAMEPADUI_GAME_EZ2
int GamepadUISaveGamePanel::SaveReadCustomMetadata( const char *pSaveName, char *ez2version, int ez2versionSize, char *platform, int platformSize, int &nMapVersion, bool &bDeck, bool &bWilson )
{
	char name[MAX_PATH];
	Q_strncpy( name, pSaveName, sizeof( name ) );
	Q_SetExtension( name, ".txt", sizeof( name ) );

	KeyValues *pCustomSaveMetadata = new KeyValues( "CustomSaveMetadata" );
	if (pCustomSaveMetadata->LoadFromFile( g_pFullFileSystem, name, "MOD" ))
	{
		Q_strncpy( ez2version, pCustomSaveMetadata->GetString( "ez2_version" ), ez2versionSize );
		Q_strncpy( platform, pCustomSaveMetadata->GetString( "platform" ), platformSize );
		nMapVersion = pCustomSaveMetadata->GetInt( "mapversion" );
		bDeck = pCustomSaveMetadata->GetBool( "is_deck" );
		bWilson = pCustomSaveMetadata->GetBool( "wilson" );

		pCustomSaveMetadata->deleteThis();
		return 1;
	}

	pCustomSaveMetadata->deleteThis();
	return 0;
}

// 0 = equal, negative = version 1 greater, positive = version 2 greater
// 
// Actual return number is based on which version place is greater/less
// For example:
// - '1' would mean version 1 is a major version greater than version 2
// - '-2' would mean version 2 is a minor version greater than version 1
static int CompareVersions( const char *pszVersion1, const char *pszVersion2 )
{
	if (!(pszVersion1 || *pszVersion1))
		return 1;
	if (!(pszVersion2 || *pszVersion2))
		return -1;

	// If the first character isn't a number, it's not a valid version
	if (pszVersion1[0] < '0' || pszVersion1[0] > '9')
		return 1;
	if (pszVersion2[0] < '0' || pszVersion2[0] > '9')
		return -1;

	CUtlStringList szVersionNums1;
	V_SplitString( pszVersion1, ".", szVersionNums1 );

	CUtlStringList szVersionNums2;
	V_SplitString( pszVersion2, ".", szVersionNums2 );

	Assert( szVersionNums1.Count() >= szVersionNums2.Count() );

	int nReturn = 0;
	for (int i = 0; i < szVersionNums1.Count(); i++)
	{
		int nV1 = atoi( szVersionNums1[i] );
		int nV2 = atoi( szVersionNums2[i] );
		if (nV1 > nV2)
		{
			nReturn = -(i+1);
			break;
		}
		else if (nV1 < nV2)
		{
			nReturn = i+1;
			break;
		}
	}

	szVersionNums1.PurgeAndDeleteElements();
	szVersionNums2.PurgeAndDeleteElements();

	return nReturn;
}

void GamepadUISaveGamePanel::LoadVersionHistory()
{
	m_pVersionHistory = new KeyValues( "VersionHistory" );
	m_pVersionHistory->LoadFromFile( g_pFullFileSystem, "scripts/ez2_version_history.txt", "MOD" );
}

void GamepadUISaveGamePanel::UnloadVersionHistory()
{
	m_pVersionHistory->deleteThis();
}

int GamepadUISaveGamePanel::IsSaveSuspect( const char *pszEZ2Version, const char *pszMapName, int nMapVersion, const char **ppszIncompatibleVersion, const char **ppszLastCompatibleBranch )
{
	if (!(pszEZ2Version && *pszEZ2Version))
	{
		// Placeholder version for updates which predate this system
		pszEZ2Version = "0.0.0";
		*ppszLastCompatibleBranch = "release-1.5";
	}

	if (m_pVersionHistory)
	{
		bool bNewerVersionExists = false;
		KeyValues *pVersionKey = m_pVersionHistory->GetFirstSubKey();
		while (pVersionKey)
		{
			int iVerCompare = CompareVersions( pVersionKey->GetName(), pszEZ2Version );
			if (iVerCompare <= -1)
			{
				// Newer version of E:Z2 than the save game's

				// Each major/minor version
				bNewerVersionExists = (iVerCompare >= -2);

				// Check if this version breaks all previous versions
				if (pVersionKey->GetBool( "universal" ))
				{
					*ppszIncompatibleVersion = pVersionKey->GetName();
					return SaveSuspectLevel_Incompatible;
				}

				// Check for incompatible maps
				KeyValues *pMaps = pVersionKey->FindKey( "maps" );
				if (pMaps)
				{
					KeyValues *pMapKey = pMaps->FindKey( pszMapName );
					if (pMapKey)
					{
						*ppszIncompatibleVersion = pVersionKey->GetName();
						return SaveSuspectLevel_Incompatible;
					}
				}
			}
			else if (iVerCompare == 0)
			{
				// Matching version of E:Z2
				*ppszLastCompatibleBranch = pVersionKey->GetString( "branch", *ppszLastCompatibleBranch );
			}

			pVersionKey = pVersionKey->GetNextKey();
		}

		if (bNewerVersionExists)
			return SaveSuspectLevel_Mismatch;
	}

	return SaveSuspectLevel_None;
}
#endif

void GamepadUISaveGamePanel::FindSaveSlot( OUT_Z_CAP( bufsize ) char* buffer, int bufsize )
{
	buffer[0] = 0;
	char szFileName[_MAX_PATH];
	for ( int i = 0; i < 1000; i++ )
	{
		Q_snprintf( szFileName, sizeof( szFileName ), "save/half-life-%03i.sav", i );

		FileHandle_t fp = g_pFullFileSystem->Open( szFileName, "rb" );
		if ( !fp )
		{
			// clean up name
			Q_strncpy( buffer, szFileName + 5, bufsize );
			char* ext = strstr( buffer, ".sav" );
			if ( ext )
			{
				*ext = 0;
			}
			return;
		}
		g_pFullFileSystem->Close( fp );
	}

	AssertMsg( false, "Could not generate new save game file" );
}

void GamepadUISaveGamePanel::DeleteSaveGame( const char* pFileName )
{
	if ( !pFileName || !pFileName[0] )
		return;

	// delete the save game file
	g_pFullFileSystem->RemoveFile( pFileName, "MOD" );

	// delete the associated tga
	char tga[_MAX_PATH];
	Q_strncpy( tga, pFileName, sizeof( tga ) );
	char* ext = strstr( tga, ".sav" );
	if ( ext )
	{
		strcpy( ext, ".tga" );
	}
	g_pFullFileSystem->RemoveFile( tga, "MOD" );

#ifdef GAMEPADUI_GAME_EZ2
	// delete the associated txt
	Q_SetExtension( tga, ".txt", sizeof( tga ) );
	g_pFullFileSystem->RemoveFile( tga, "MOD" );
#endif
}
/* End Mostly from GameUI */

void GamepadUISaveGamePanel::OnGamepadUIButtonNavigatedTo( vgui::VPANEL button )
{
    GamepadUIButton *pButton = dynamic_cast< GamepadUIButton * >( vgui::ipanel()->GetPanel( button, GetModuleName() ) );
    if ( pButton )
    {
        if ( pButton->GetAlpha() != 255 )
        {
            int nParentW, nParentH;
			GetParent()->GetSize( nParentW, nParentH );

            int nX, nY;
            pButton->GetPos( nX, nY );

            int nTargetY = pButton->GetPriority() * ( pButton->m_flHeightAnimationValue[ButtonStates::Out] + m_flSavesSpacing );

            if ( nY < nParentH / 2 )
            {
                nTargetY += nParentH - m_flSavesOffsetY;
                // Add a bit of spacing to make this more visually appealing :)
                nTargetY -= m_flSavesSpacing;
            }
            else
            {
                nTargetY += pButton->m_flHeightAnimationValue[ButtonStates::Over];
                // Add a bit of spacing to make this more visually appealing :)
                nTargetY += (pButton->m_flHeightAnimationValue[ButtonStates::Over] / 2) + m_flSavesSpacing;
            }


            m_ScrollState.SetScrollTarget( nTargetY - ( nParentH - m_flSavesOffsetY), GamepadUI::GetInstance().GetTime() );
        }
    }
}

void GamepadUISaveGamePanel::LayoutSaveButtons()
{
    int nParentW, nParentH;
	GetParent()->GetSize( nParentW, nParentH );

    float scrollClamp = 0.0f;
    for ( int i = 0; i < ( int )m_pSavePanels.Count(); i++ )
    {
        int size = ( m_pSavePanels[i]->GetTall() + m_flSavesSpacing );

        if ( i < ( ( int )m_pSavePanels.Count() ) - 3 )
            scrollClamp += size;
    }

    m_ScrollState.UpdateScrollBounds( 0.0f, scrollClamp );

    if (m_pSavePanels.Count() > 0)
    {
        m_pScrollBar->UpdateScrollBounds( 0.0f, scrollClamp,
            ((m_pSavePanels[0]->GetTall() + m_flSavesSpacing) * 3), nParentH - m_flFooterButtonsOffsetY - m_nFooterButtonHeight - m_flSavesOffsetY );
    }

    int previousSizes = 0;
    for ( int i = 0; i < ( int )m_pSavePanels.Count(); i++ )
    {
        int tall = m_pSavePanels[i]->GetTall();
        int size = ( tall + m_flSavesSpacing );

        int y = m_flSavesOffsetY + previousSizes - m_ScrollState.GetScrollProgress();
        int fade = 255;
        if ( y < m_flSavesOffsetY )
            fade = ( 1.0f - clamp( -( y - m_flSavesOffsetY ) / m_flSavesFade, 0.0f, 1.0f ) ) * 255.0f;
        if ( y > nParentH - m_flSavesFade )
            fade = ( 1.0f - clamp( ( y - ( nParentH - m_flSavesFade - size ) ) / m_flSavesFade, 0.0f, 1.0f ) ) * 255.0f;
        if ( m_pSavePanels[i]->HasFocus() && fade != 0 )
            fade = 255;
        m_pSavePanels[i]->SetAlpha( fade );
        m_pSavePanels[i]->SetPos( m_flSavesOffsetX, y );
        m_pSavePanels[i]->SetVisible( true );
        previousSizes += size;

		if (m_pDeletePanels.Count() > i)
		{
			m_pDeletePanels[i]->SetPos( m_flSavesOffsetX - m_pDeletePanels[i]->GetWide() - m_flSavesSpacing, y );
			m_pDeletePanels[i]->SetAlpha( fade );
			m_pDeletePanels[i]->SetVisible( true );
		}
    }

    m_ScrollState.UpdateScrolling( 2.0f, GamepadUI::GetInstance().GetTime() );
}

void GamepadUISaveGamePanel::LoadGame( const SaveGameDescription_t* pSave )
{
	const char* shortName = pSave->szShortName;
	if ( shortName && shortName[0] )
	{
		// Load the game, return to top and switch to engine
		char sz[256];
		Q_snprintf( sz, sizeof( sz ), "progress_enable\nload %s\n", shortName );

		GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( sz );

		// Close this dialog
		Close();
	}
}

void GamepadUISaveGamePanel::SaveGame( const SaveGameDescription_t* pSave )
{
	// delete any existing save
	DeleteSaveGame( pSave->szFileName );

	// save to a new name
	char saveName[128];
	FindSaveSlot( saveName, sizeof( saveName ) );
	if ( saveName && saveName[0] )
	{
		// Load the game, return to top and switch to engine
		char sz[256];
		Q_snprintf( sz, sizeof( sz ), "save %s\n", saveName );

		GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( sz );
		Close();
		GamepadUI::GetInstance().GetGameUI()->SendMainMenuCommand( "resumegame" );
	}
}

void GamepadUISaveGamePanel::OnCommand( char const* pCommand )
{
    if ( !V_strcmp( pCommand, "action_back" ) )
    {
        Close();
    }
	else if ( !V_strcmp( pCommand, "action_delete_mode_button" ) )
    {
		for ( auto& panel : m_pDeletePanels )
		{
			if ( panel->HasFocus() )
			{
				new GamepadUIGenericConfirmationPanel( this, "SaveDeleteConfirmationPanel", "#GameUI_ConfirmDeleteSaveGame_Title", "#GameUI_ConfirmDeleteSaveGame_Info",
				[this, panel]()
				{
					DeleteSaveGame( panel->GetName() );
					ScanSavedGames();
				} );
				break;
			}
		}
	}
	else if ( !V_strcmp( pCommand, "action_delete" ) )
    {
#ifdef STEAM_INPUT
        const bool bController = GamepadUI::GetInstance().GetSteamInput()->IsEnabled();
#elif defined(HL2_RETAIL) // Steam input and Steam Controller are not supported in SDK2013 (Madi)
        const bool bController = g_pInputSystem->IsSteamControllerActive();
#else
        const bool bController = ( g_pInputSystem->GetJoystickCount() >= 1 );
#endif
		if (InDeleteMode())
		{
			m_pDeletePanels.PurgeAndDeleteElements();
		}
		else if (!bController && gamepadui_savegame_use_delete_mode.GetBool())
		{
			// Add delete panels
			for (int i = 0; i < m_pSavePanels.Count(); i++)
			{
				GamepadUIButton *button = new GamepadUIButton( this, this, GAMEPADUI_RESOURCE_FOLDER "schemedeletesavebutton.res", "action_delete_mode_button", "X", "");
				button->SetName( m_pSavePanels[i]->GetSaveGame()->szFileName );
				button->SetPriority( m_pSavePanels[i]->GetPriority() );
				button->SetForwardToParent( true );
				m_pDeletePanels.AddToTail( button );
			}
		}
		else
		{
			// Delete directly if using a controller
			for ( auto& panel : m_pSavePanels )
			{
				if ( panel->HasFocus() )
				{
					new GamepadUIGenericConfirmationPanel( this, "SaveDeleteConfirmationPanel", "#GameUI_ConfirmDeleteSaveGame_Title", "#GameUI_ConfirmDeleteSaveGame_Info",
					[this, panel]()
					{
						int i = m_pSavePanels.Find( panel );
						DeleteSaveGame( panel->GetSaveGame()->szFileName );
						ScanSavedGames();

						if ( i > 0 && m_pSavePanels.Count() >= i )
							m_pSavePanels[i-1]->NavigateTo();
						else if ( m_pSavePanels.Count() )
							m_pSavePanels[0]->NavigateTo();
					} );
					break;
				}
			}
		}
    }
	else if ( !V_strcmp( pCommand, "load_save" ) )
	{
		for ( auto& panel : m_pSavePanels )
		{
			if ( panel->HasFocus() )
			{
				auto* pSave = panel->GetSaveGame();
				if ( m_bIsSave )
				{
					if ( panel->GetSaveGame()->iTimestamp != NEW_SAVE_GAME_TIMESTAMP )
					{
						new GamepadUIGenericConfirmationPanel( this, "SaveOverwriteConfirmationPanel", "#GameUI_ConfirmOverwriteSaveGame_Title", "#GameUI_ConfirmOverwriteSaveGame_Info",
						[this, pSave]()
						{
							SaveGame( pSave );
						} );
					}
					else
						SaveGame( pSave );
				}
				else
				{
#ifdef GAMEPADUI_GAME_EZ2
					if (panel->GetSaveSuspectLevel() != SaveSuspectLevel_None)
					{
						wchar_t wszLastCompatibleBranch[ 16 ];
						V_UTF8ToUnicode( panel->GetLastCompatibleBranch(), wszLastCompatibleBranch, sizeof( wszLastCompatibleBranch ) );
						
						const char *pszFrameTitle = NULL;
						wchar_t buf[ 512 ];
						switch (panel->GetSaveSuspectLevel())
						{
							case SaveSuspectLevel_Mismatch:
								g_pVGuiLocalize->ConstructString( buf, sizeof( buf ), g_pVGuiLocalize->Find( "#GameUI_DifferentVersion_Info" ), 1, wszLastCompatibleBranch );
								pszFrameTitle = "#GameUI_DifferentVersion_Title";
								break;
							case SaveSuspectLevel_Incompatible:
								g_pVGuiLocalize->ConstructString( buf, sizeof( buf ), g_pVGuiLocalize->Find( "#GameUI_IncompatibleVersion_Info" ), 1, wszLastCompatibleBranch );
								pszFrameTitle = "#GameUI_IncompatibleVersion_Title";
								break;
						}
						
						new GamepadUIGenericConfirmationPanel( this, "IncompatibleVersionConfirmationPanel", g_pVGuiLocalize->Find( pszFrameTitle ), buf,
						[this, pSave]()
						{
							LoadGame( pSave );
						}, true );
					}
					else
#endif
					LoadGame( pSave );
				}
				break;
			}
		}
	}
    else
    {
        BaseClass::OnCommand( pCommand );
    }
}

void GamepadUISaveGamePanel::OnMouseWheeled( int delta )
{
    m_ScrollState.OnMouseWheeled( delta * 200.0f, GamepadUI::GetInstance().GetTime() );
}

CON_COMMAND( gamepadui_opensavegamedialog, "" )
{
    new GamepadUISaveGamePanel( GamepadUI::GetInstance().GetBasePanel(), "", true );
}

CON_COMMAND( gamepadui_openloadgamedialog, "" )
{
	new GamepadUISaveGamePanel( GamepadUI::GetInstance().GetBasePanel(), "", false );
}
