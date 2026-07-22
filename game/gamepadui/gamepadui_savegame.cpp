#include "gamepadui_interface.h"
#include "gamepadui_image.h"
#include "gamepadui_util.h"
#include "gamepadui_genericconfirmation.h"
#include "gamepadui_scroll.h"

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

class GamepadUISaveButton;
struct SaveGameDescription_t;

class GamepadUISaveGamePanel : public GamepadUIFrame
{
    DECLARE_CLASS_SIMPLE( GamepadUISaveGamePanel, GamepadUIFrame );

public:
    GamepadUISaveGamePanel( vgui::Panel *pParent, const char* pPanelName, bool bIsSave );

    void UpdateGradients();

	void OnThink() OVERRIDE;
    void Paint() OVERRIDE;
    void OnCommand( char const* pCommand ) OVERRIDE;
    void OnMouseWheeled( int nDelta ) OVERRIDE;

    MESSAGE_FUNC_HANDLE( OnGamepadUIButtonNavigatedTo, "OnGamepadUIButtonNavigatedTo", button );

private:
    void ScanSavedGames();
    void LayoutSaveButtons();
    bool ParseSaveData( char const* pFileName, char const* pShortName, SaveGameDescription_t& save );
    static int SaveReadNameAndComment( FileHandle_t f, OUT_Z_CAP( nameSize ) char* name, int nameSize, OUT_Z_CAP( commentSize ) char* comment, int commentSize );
    void FindSaveSlot( OUT_Z_CAP( bufsize ) char* buffer, int bufsize );
    void DeleteSaveGame( const char* pFileName );

    void LoadGame( const SaveGameDescription_t* pSave );
    void SaveGame( const SaveGameDescription_t *pSave );

	GamepadUIString m_strNoSaveString;

    CUtlVector<GamepadUISaveButton*> m_pSavePanels;
    CUtlVector<SaveGameDescription_t> m_Saves;

    GamepadUIScrollState m_ScrollState;

    bool m_bIsSave;

    GAMEPADUI_PANEL_PROPERTY( float, m_flSavesFade, "Saves.Fade", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flSavesOffsetX, "Saves.OffsetX", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flSavesOffsetY, "Saves.OffsetY", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flSavesSpacing, "Saves.Spacing", "0", SchemeValueTypes::ProportionalFloat );
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
};
/* End from GameUI */

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
    }

    void Paint() OVERRIDE
    {
        int x, y, w, t;
        GetBounds( x, y, w, t );

        PaintButton();

		if ( m_Image.IsValid() )
		{
			vgui::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
			vgui::surface()->DrawSetTexture( m_Image );
			// Save game icons are 180x100
			int imgW = ( t * 180 ) / 100;
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
			int imgW = ( t * 180 ) / 100;
			int imgH = t;
			vgui::surface()->DrawFilledRect( 0, 0, imgW, imgH );
		}

        PaintText();
    }

	const SaveGameDescription_t* GetSaveGame() const
	{
		return m_pSaveGame;
	}

private:
	bool m_bUseTGAImage = false;
    GamepadUIImage m_Image;
	const SaveGameDescription_t *m_pSaveGame;
};

GamepadUISaveGamePanel::GamepadUISaveGamePanel( vgui::Panel* pParent, const char* pPanelName, bool bIsSave )
	: BaseClass( pParent, pPanelName )
	, m_bIsSave( bIsSave )
{
    vgui::HScheme Scheme = vgui::scheme()->LoadSchemeFromFile( GAMEPADUI_DEFAULT_PANEL_SCHEME, "SchemePanel" );
    SetScheme( Scheme );

    GetFrameTitle() = GamepadUIString(m_bIsSave ? "#GameUI_SaveGame" : "#GameUI_LoadGame");

    Activate();

	if ( m_bIsSave )
		m_Saves.AddToTail( SaveGameDescription_t{ "NewSavedGame", "", "", "#GameUI_NewSaveGame", "", "", "Current", NEW_SAVE_GAME_TIMESTAMP } );

    ScanSavedGames();

    if ( m_pSavePanels.Count() )
        m_pSavePanels[0]->NavigateTo();

    for ( int i = 1; i < m_pSavePanels.Count(); i++ )
    {
        m_pSavePanels[i]->SetNavUp( m_pSavePanels[i - 1] );
        m_pSavePanels[i - 1]->SetNavDown( m_pSavePanels[i] );
    }

	UpdateGradients();
}

void GamepadUISaveGamePanel::UpdateGradients()
{
	const float flTime = GamepadUI::GetInstance().GetTime();
	GamepadUI::GetInstance().GetGradientHelper()->ResetTargets( flTime );
	GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Up, { 1.0f, 1.0f }, flTime );
	GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Down, { 1.0f, 1.0f }, flTime );
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
		SetFooterButtons( FooterButtons::Back | FooterButtons::Select, FooterButtons::Select );
	}

	SetControlEnabled( "loadsave", false );
	SetControlEnabled( "delete", false );
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
					LoadGame( pSave );
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
