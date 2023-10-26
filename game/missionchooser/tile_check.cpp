#include "tile_check.h"
#include "RoomTemplate.h"
#include "LevelTheme.h"
#include "TileGenDialog.h"
#include "filesystem.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/RichText.h"
#include "vgui_controls/ProgressBar.h"
#include <vgui/IVGui.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

TileCheck::TileCheck()
{
	m_iTotalTemplates = 1;
	m_iCheckedTemplates = 0;
}

TileCheck::~TileCheck()
{

}

void TileCheck::TileCheckError( const char *pMsg, ... )
{
	char msg[4096];
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( msg, sizeof( msg ), pMsg, marker );
	va_end( marker );

	m_TileCheckErrors.AddToTail( TileGenCopyString( msg ) );
}

void TileCheck::ClearTileCheckErrors()
{
	m_TileCheckErrors.PurgeAndDeleteElements();
}

bool TileCheck::ShowTileCheckErrors()
{
	if ( m_TileCheckErrors.Count() <= 0 )
	{
		TileCheckError( "No errors found!\n" );
	}
	char msg[4096];
	msg[0] = 0;
	for ( int i=0; i<m_TileCheckErrors.Count(); i++ )
	{
		Q_snprintf( msg, sizeof( msg ), "%s\n%s", msg, m_TileCheckErrors[i] );
	}
	VGUIMessageBox( g_pTileGenDialog, "Room Template check:", msg );
	return true;
}

void TileCheck::StartCheckingAllRoomTemplates()
{
	ClearTileCheckErrors();

	m_iCurrentTheme = 0;
	m_iCurrentRoomTemplate = 0;
	m_iCheckedTemplates = 0;

	m_iTotalTemplates = 0;
	int iThemes = CLevelTheme::s_LevelThemes.Count();
	for ( int i = 0; i < iThemes; i++ )
	{
		CLevelTheme* pTheme = CLevelTheme::s_LevelThemes[i];
		if ( !pTheme || pTheme->m_bSkipErrorCheck )
		{
			continue;
		}
		m_iTotalTemplates += pTheme->m_RoomTemplates.Count();
	}
}

bool TileCheck::ContinueCheckingAllRoomTemplates()
{
	int iThemes = CLevelTheme::s_LevelThemes.Count();
	if ( m_iCurrentTheme >= iThemes )
		return false;

	CLevelTheme* pTheme = CLevelTheme::s_LevelThemes[m_iCurrentTheme];
	if ( !pTheme || pTheme->m_bSkipErrorCheck )
	{
		m_iCurrentTheme++;
		m_iCurrentRoomTemplate = 0;
		return true;
	}

	if ( m_iCurrentRoomTemplate >= pTheme->m_RoomTemplates.Count() )
	{
		m_iCurrentTheme++;
		m_iCurrentRoomTemplate = 0;
		return true;
	}
	CRoomTemplate *pTemplate = pTheme->m_RoomTemplates[m_iCurrentRoomTemplate];
	if ( !pTemplate )
	{
		m_iCurrentRoomTemplate++;
		m_iCheckedTemplates++;
		return true;
	}

	CheckTemplate( pTemplate );
	m_iCurrentRoomTemplate++;
	m_iCheckedTemplates++;
	return true;	
}

void TileCheck::CheckTemplate( CRoomTemplate *pTemplate )
{
	if ( !pTemplate || !pTemplate->m_pLevelTheme )
		return;

	char szFullFileName[512];
	Q_snprintf(szFullFileName, sizeof(szFullFileName), "tilegen/roomtemplates/%s/%s.vmf", pTemplate->m_pLevelTheme->m_szName, pTemplate->GetFullName() );
	KeyValues *pRoomTemplateKeyValues = new KeyValues( pTemplate->GetFullName() );
	if ( !pRoomTemplateKeyValues->LoadFromFile( g_pFullFileSystem, szFullFileName, "GAME" ) )
	{
		pRoomTemplateKeyValues->deleteThis();
		return;
	}

	int iNodes = CountAINodes( pRoomTemplateKeyValues );
	if ( pTemplate->m_Exits.Count() > 0 && iNodes <= 0 )
	{
		TileCheckError( "%s/%s has no AI nodes.\n", pTemplate->m_pLevelTheme->m_szName, pTemplate->GetFullName() );
	}

	// VBSP2 converts brushes automatically, so we don't need this check currently
// 	if ( CountNonFuncDetailBrushes( pRoomTemplateKeyValues ) > 0 )
// 	{
// 		TileCheckError( "%s/%s has brushes that aren't func_detail.\n", pTemplate->m_pLevelTheme->m_szName, pTemplate->GetFullName() );
// 	}

	if ( CountDirectors( pRoomTemplateKeyValues ) > 0 )
	{
		TileCheckError( "%s/%s has an asw_director_control.\n", pTemplate->m_pLevelTheme->m_szName, pTemplate->GetFullName() );
	}

	if ( pTemplate->HasTag( "Chokepoint" ) )
	{
		// check we have at least one grow source and one non-grow source
		bool bGrowSource = false;
		bool bNonGrowSource = false;
		for ( int i = 0; i < pTemplate->m_Exits.Count(); i++ )
		{
			if ( pTemplate->m_Exits[i]->m_bChokepointGrowSource )
			{
				bGrowSource = true;
			}
			else
			{
				bNonGrowSource = true;
			}
		}

		if ( !bNonGrowSource )
		{
			TileCheckError( "%s/%s requires at least one exit that isn't marked 'chokepoint grow source'.\n", pTemplate->m_pLevelTheme->m_szName, pTemplate->GetFullName() );
		}
		if ( !bGrowSource )
		{
			TileCheckError( "%s/%s requires at least one exit marked 'chokepoint grow source'.\n", pTemplate->m_pLevelTheme->m_szName, pTemplate->GetFullName() );
		}
	}

	pRoomTemplateKeyValues->deleteThis();
}

int TileCheck::CountAINodes( KeyValues *pRoomKeys )
{
	int iNodes = 0;
	while ( pRoomKeys )
	{
		if ( !Q_stricmp( pRoomKeys->GetName(), "entity" ) )		// go through all entities in this room
		{
			KeyValues *pFieldKey = pRoomKeys->GetFirstSubKey();
			while ( pFieldKey )								// go through all properties of this entity
			{
				if ( !pFieldKey->GetFirstSubKey() )			// leaf
				{
					if ( !stricmp(pFieldKey->GetName(), "nodeid" ) )
					{
						iNodes++;
					}
				}		
				pFieldKey = pFieldKey->GetNextKey();
			}
		}
		pRoomKeys = pRoomKeys->GetNextKey();
	}
	return iNodes;
}

int TileCheck::CountNonFuncDetailBrushes( KeyValues *pRoomKeys )
{
	int iBrushes = 0;
	KeyValues *pKeys = pRoomKeys;
	while ( pKeys )
	{
		if ( !Q_stricmp( pKeys->GetName(), "world" ) )		// find world
		{
			KeyValues *pSubKey = pKeys->GetFirstSubKey();
			while ( pSubKey )
			{
				if ( !Q_stricmp( pSubKey->GetName(), "solid" ) )		// find solids
				{
					iBrushes++;
				}
				pSubKey = pSubKey->GetNextKey();
			}			
		}
		pKeys = pKeys->GetNextKey();
	}
	return iBrushes;
}

int TileCheck::CountDirectors( KeyValues *pRoomKeys )
{
	int iCount = 0;
	while ( pRoomKeys )
	{
		if ( !Q_stricmp( pRoomKeys->GetName(), "entity" ) )		// go through all entities in this room
		{
			const char *szClassname = pRoomKeys->GetString( "classname" );
			if ( szClassname && !Q_stricmp( szClassname, "asw_director_control" ) )
			{
				iCount++;
			}
		}
		pRoomKeys = pRoomKeys->GetNextKey();
	}
	return iCount;
}

// =====================================================================================================

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CTile_Check_Frame::CTile_Check_Frame( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile( "tilegen_scheme.res", "tilegen_scheme" );
	SetScheme( scheme );

	m_pCheckingLabel = new vgui::Label( this, "CheckingLabel", "" );
	m_pProgressBar = new vgui::ProgressBar( this, "ProgressBar" );
	m_pErrorTextBox = new vgui::RichText( this, "ErrorTextBox" );
	LoadControlSettings( "tilegen/TileCheck.res", "GAME" );

	SetMinimumSize( 384, 420 );

	SetSizeable( true );
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetMenuButtonVisible( false );
	m_bFirstPerformLayout = true;

	SetTitle( "Room Template Check", true );

	m_pTileCheck = new TileCheck;

	// make sure the window get a tick all the time (could change this to only tick on necessary states?)
	vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );

	m_bCheckingTiles = true;
	m_pTileCheck->StartCheckingAllRoomTemplates();
}

//-----------------------------------------------------------------------------
// Destructor 
//-----------------------------------------------------------------------------
CTile_Check_Frame::~CTile_Check_Frame()
{
	delete m_pTileCheck;
}

//-----------------------------------------------------------------------------
// Purpose: Kills the whole app on close
//-----------------------------------------------------------------------------
void CTile_Check_Frame::OnClose( void )
{
	BaseClass::OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: Parse commands coming in from the VGUI dialog
//-----------------------------------------------------------------------------
void CTile_Check_Frame::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );	
}

void CTile_Check_Frame::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CTile_Check_Frame::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
}

void CTile_Check_Frame::OnTick()
{
	if ( m_bCheckingTiles )
	{
		if ( !m_pTileCheck->ContinueCheckingAllRoomTemplates() )
		{
			m_bCheckingTiles = false;
		}
		// pull out any new messages
		for ( int i = 0; i < m_pTileCheck->m_TileCheckErrors.Count(); i++ )
		{
			m_pErrorTextBox->InsertString( m_pTileCheck->m_TileCheckErrors[i] );
		}
		m_pTileCheck->ClearTileCheckErrors();
		m_pProgressBar->SetProgress( m_pTileCheck->GetProgress() );
	}
}