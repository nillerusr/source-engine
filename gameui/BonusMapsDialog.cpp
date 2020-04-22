//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "BonusMapsDialog.h"
#include "EngineInterface.h"

#include "vgui/ISystem.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include <vgui/ILocalize.h>
#include "filesystem.h"

#include "vgui_controls/Button.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/QueryBox.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/BitmapImagePanel.h"
#include "vgui_controls/FileOpenDialog.h"

#include "TGAImagePanel.h"
#include "MouseMessageForwardingPanel.h"

#include "BasePanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


#define MAX_LISTED_BONUS_MAPS	128


extern const char *COM_GetModDirectory( void );


bool ConstructFullImagePath( const char *pCurrentPath, const char *pchImageName, char *pchImageFileName )
{
	char *ext = Q_strstr( pchImageName , ".tga" );
	if ( ext )
	{
		// Use the specified image
		if ( pchImageName[ 0 ] != '.' )
			Q_snprintf( pchImageFileName, _MAX_PATH, "%s", pchImageName );
		else
			Q_snprintf( pchImageFileName, _MAX_PATH, "%s/%s", pCurrentPath, pchImageName );

		return true;
	}

	Q_strcpy( pchImageFileName, pchImageName );

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Describes the layout of a same game pic
//-----------------------------------------------------------------------------
class CBonusMapPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CBonusMapPanel, vgui::EditablePanel );
public:
	CBonusMapPanel( PanelListPanel *parent, const char *name, int bonusMapListItemID ) : BaseClass( parent, name )
	{
		m_iBonusMapListItemID = bonusMapListItemID;
		m_pParent = parent;
		m_pBonusMapTGAImage = new CTGAImagePanel( this, "BonusMapTGAImage" );
		m_pBonusMapImage = SETUP_PANEL( new ImagePanel( this, "BonusMapImage" ) );
		m_pBonusMapScreenshotBackground = SETUP_PANEL( new ImagePanel( this, "BonusMapScreenshotBackground" ) );
		m_pMapNameLabel = new Label( this, "MapNameLabel", "" );
		m_pLockIcon = new ImagePanel( this, "LockIcon" );
		m_pCompleteIcon = new ImagePanel( this, "CompleteIcon" );

		CMouseMessageForwardingPanel *panel = new CMouseMessageForwardingPanel(this, NULL);
		panel->SetZPos(2);

		SetSize( 200, 140 );

		LoadControlSettings( "resource/BonusMapPanel.res" );

		m_FillColor = m_pBonusMapScreenshotBackground->GetFillColor();
	}

	void SetBonusMapInfo( const char *pCurrentPath, BonusMapDescription_t &map )
	{
		// set the image to display
		char szImageFileName[_MAX_PATH];

		bool bIsTGA = false;

		if ( map.bIsFolder )
		{
			if ( map.szImageName[ 0 ] == '\0' )
			{
				// use associate bonus folder icon
				Q_snprintf( szImageFileName, sizeof(szImageFileName), "%s/foldericon.tga", map.szFileName );
				bIsTGA = true;

				// use default folder icon
				if( !g_pFullFileSystem->FileExists( szImageFileName, "MOD" ) )
				{
					V_strcpy_safe( szImageFileName, "bonusmaps/icon_bonus_map_folder" );
					bIsTGA = false;
				}
			}
			else
			{
				// Use the specified image
				bIsTGA = ConstructFullImagePath( pCurrentPath, map.szImageName, szImageFileName );
			}
		}
		else
		{
			if ( map.szImageName[ 0 ] == '\0' )
			{
				// Didn't specify an image name, so pair it with the name of this file
				char szImpliedTgaName[_MAX_PATH];
				Q_snprintf( szImpliedTgaName, sizeof( szImpliedTgaName ), "%s.tga", map.szMapFileName );
				bIsTGA = ConstructFullImagePath( pCurrentPath, szImpliedTgaName, szImageFileName );

				// if it doesn't exist use default bonus map icon
				if( !g_pFullFileSystem->FileExists( szImageFileName, "MOD" ) )
				{
					V_strcpy_safe( szImageFileName, "bonusmaps/icon_bonus_map_default" );
					bIsTGA = false;
				}
			}
			else
			{
				// Use the specified image
				bIsTGA = ConstructFullImagePath( pCurrentPath, map.szImageName, szImageFileName );
			}
		}

		if ( bIsTGA )
		{
			m_pBonusMapTGAImage->SetTGANonMod( szImageFileName );
			m_pBonusMapTGAImage->SetSize( 180, 100 );
			m_pBonusMapTGAImage->SetVisible( true );
			m_pBonusMapImage->SetVisible( false );
		}
		else
		{
			m_pBonusMapImage->SetImage( szImageFileName );
			m_pBonusMapImage->SetSize( 180, 100 );
			m_pBonusMapImage->SetVisible( true );
			m_pBonusMapTGAImage->SetVisible( false );
		}

		m_pLockIcon->SetVisible( map.bLocked );
		m_pCompleteIcon->SetVisible( map.bComplete );

		// set the title text
		m_pMapNameLabel->SetText( map.szMapName );
	}

	MESSAGE_FUNC_INT( OnPanelSelected, "PanelSelected", state )
	{
		if ( state )
		{
			// set the text color to be orange, and the pic border to be orange
			m_pBonusMapScreenshotBackground->SetFillColor( m_SelectedColor );
			m_pMapNameLabel->SetFgColor( Color( 0, 0, 0, 255 ) );
		}
		else
		{
			m_pBonusMapScreenshotBackground->SetFillColor( m_FillColor );
			m_pMapNameLabel->SetFgColor( m_TextColor );
		}

		PostMessage( m_pParent->GetVParent(), new KeyValues("PanelSelected") );
	}

	virtual void OnMousePressed( vgui::MouseCode code )
	{
		m_pParent->SetSelectedPanel( this );
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		m_TextColor = pScheme->GetColor( "NewGame.TextColor", Color(255, 255, 255, 255) );
		m_SelectedColor = pScheme->GetColor( "NewGame.SelectionColor", Color(255, 255, 255, 255) );

		BaseClass::ApplySchemeSettings( pScheme );
	}

	virtual void OnMouseDoublePressed( vgui::MouseCode code )
	{
		// call the panel
		OnMousePressed( code );
		PostMessage( m_pParent->GetParent(), new KeyValues("Command", "command", "loadbonusmap") );
	}

	int GetBonusMapListItemID()
	{
		return m_iBonusMapListItemID;
	}

private:
	vgui::PanelListPanel	*m_pParent;
	vgui::Label				*m_pMapNameLabel;
	ImagePanel				*m_pBonusMapImage;
	CTGAImagePanel			*m_pBonusMapTGAImage;

	ImagePanel	*m_pLockIcon;
	ImagePanel	*m_pCompleteIcon;

	// things to change color when the selection changes
	ImagePanel	*m_pBonusMapScreenshotBackground;
	Color		m_TextColor, m_FillColor, m_SelectedColor;

	int			m_iBonusMapListItemID;
};

CBonusMapsDialog *g_pBonusMapsDialog = NULL;


//-----------------------------------------------------------------------------
// Purpose:Constructor
//-----------------------------------------------------------------------------
CBonusMapsDialog::CBonusMapsDialog(vgui::Panel *parent) : BaseClass(parent, "BonusMapsDialog")
{
	g_pBonusMapsDialog = this;
	m_hImportBonusMapsDialog = NULL;

	BonusMapsDatabase()->RootPath();

	CreateBonusMapsList();

	BuildMapsList();

	new vgui::Button( this, "loadbonusmap", "" );
	SetControlEnabled( "loadbonusmap", false );

	SetDeleteSelfOnClose(true);
	//SetBounds(0, 0, 512, 384);
	//SetMinimumSize( 256, 300 );
	SetSizeable( false );

	SetTitle("#GameUI_BonusMaps", true);

	vgui::Button *cancel = new vgui::Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Close" );

	m_pPercentageBarBackground = SETUP_PANEL( new ImagePanel( this, "PercentageBarBackground" ) );
	m_pPercentageBar = SETUP_PANEL( new ImagePanel( this, "PercentageBar" ) );

	LoadControlSettings("resource/BonusMapsDialog.res");

	// Stop blinking the bonus maps menu item
	CBasePanel *pBasePanel = BasePanel();
	if ( pBasePanel )
		pBasePanel->SetMenuItemBlinkingState( "OpenBonusMapsDialog", false );

	BonusMapsDatabase()->SetBlink( false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBonusMapsDialog::~CBonusMapsDialog()
{
	BonusMapsDatabase()->WriteSaveData();	// Closing this dialog is a good time to save
	g_pBonusMapsDialog = NULL;
}


bool CBonusMapsDialog::ImportZippedBonusMaps( const char *pchZippedFileName )
{
	// Get the zip file's name with dir info
	char *pchShortFileName = Q_strrchr( pchZippedFileName, CORRECT_PATH_SEPARATOR );

	if ( !pchShortFileName )
		return false;

	// It's going to go in the maps folder
	char szOutFilename[ 512 ];

	Q_snprintf( szOutFilename, sizeof( szOutFilename ), "maps%s", pchShortFileName );
	Q_StripExtension( szOutFilename, szOutFilename, sizeof( szOutFilename ) );

	// If there's already a folder by the same name we're going to tack a number onto the end
	int iOutFilenameLength = Q_strlen( szOutFilename );
	int iSameFolderCount = 1;

	while ( g_pFullFileSystem->FileExists( szOutFilename, "MOD" ) )
	{
		++iSameFolderCount;

		if ( iSameFolderCount > 99 )
		{
			return false;
		}

		szOutFilename[ iOutFilenameLength ] = '\0';
		Q_snprintf( szOutFilename, sizeof( szOutFilename ), "%s%02i", szOutFilename, iSameFolderCount );
	}

	// Pull the files out of the zip
	if ( g_pFullFileSystem->UnzipFile( pchZippedFileName, "MOD", szOutFilename ) )
	{
		// New maps have been imported, so refresh the list
		BuildMapsList();
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: builds bonus map list from directory
//-----------------------------------------------------------------------------
void CBonusMapsDialog::BuildMapsList( void )
{
	// clear the current list
	m_pGameList->DeleteAllItems();
	
	BonusMapsDatabase()->ClearBonusMapsList();
	BonusMapsDatabase()->ScanBonusMaps();

	// Enable back button if we're in a sub folder
	bool bIsRoot = ( Q_strcmp( BonusMapsDatabase()->GetPath(), "." ) == 0 );
	SetControlEnabled( "Back", !bIsRoot );
	SetControlVisible( "Back", !bIsRoot );
	SetControlEnabled( "ImportBonusMaps", bIsRoot );
	SetControlVisible( "ImportBonusMaps", bIsRoot );

	char szDisplayPath[_MAX_PATH];
	Q_snprintf( szDisplayPath, _MAX_PATH, "%s/", BonusMapsDatabase()->GetPath() );

	SetControlString( "FileName", szDisplayPath );
	SetControlString( "CommentLabel", "" );

	int iMapCount = BonusMapsDatabase()->BonusCount();

	// add to the list
	for ( int iMapIndex = 0; iMapIndex < iMapCount && iMapIndex < MAX_LISTED_BONUS_MAPS; ++iMapIndex )
	{
		CBonusMapPanel *bonusMapPanel = new CBonusMapPanel( m_pGameList, "BonusMapPanel", iMapIndex );
		bonusMapPanel->SetBonusMapInfo( BonusMapsDatabase()->GetPath(), *(BonusMapsDatabase()->GetBonusData( iMapIndex )) );
		m_pGameList->AddItem( NULL, bonusMapPanel );
	}

	// display a message if there are no save games
	if ( iMapCount <= 0 )
	{
		vgui::Label *pNoSavesLabel = SETUP_PANEL(new Label(m_pGameList, "NoBonusMapsLabel", "#GameUI_NoBonusMapsToDisplay"));
		pNoSavesLabel->SetTextColorState(vgui::Label::CS_DULL);
		m_pGameList->AddItem( NULL, pNoSavesLabel );
		m_pGameList->SetNumColumns( 1 );
	}
	else
	{
		m_pGameList->SetNumColumns( 3 );
	}

	RefreshCompletionPercentage();

	// Disable load button
	SetControlEnabled( "loadbonusmap", false );

	// Make challenge selection invisible
	m_pChallengeSelection->SetEnabled( false );
	m_pChallengeSelection->SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: Creates the load game display list
//-----------------------------------------------------------------------------
void CBonusMapsDialog::CreateBonusMapsList()
{
	m_pGameList = new vgui::PanelListPanel( this, "listpanel_bonusmaps" );
	m_pGameList->SetFirstColumnWidth( 0 );

	new Label( this, "FileName", "./" );
	new Label( this, "CommentLabel", "" );

	m_pChallengeSelection = new vgui::ComboBox( this, "ChallengeSelection", 0, false );
}

//-----------------------------------------------------------------------------
// Purpose: returns the save file name of the selected item
//-----------------------------------------------------------------------------
int CBonusMapsDialog::GetSelectedItemBonusMapIndex()
{
	CBonusMapPanel *panel = dynamic_cast<CBonusMapPanel *>(m_pGameList->GetSelectedPanel());
	if ( panel && panel->GetBonusMapListItemID() < BonusMapsDatabase()->BonusCount() )
		return panel->GetBonusMapListItemID();

	return BonusMapsDatabase()->InvalidIndex();
}

void CBonusMapsDialog::SetSelectedBooleanStatus( const char *pchName, bool bValue )
{
	CBonusMapPanel *pSelectedBonusMapPanel = (CBonusMapPanel *)m_pGameList->GetSelectedPanel();
	if ( !pSelectedBonusMapPanel )
		return;

	BonusMapsDatabase()->SetBooleanStatus( pchName, pSelectedBonusMapPanel->GetBonusMapListItemID(), bValue );

	// Change the status in the dialog
	BonusMapDescription_t *pMap = BonusMapsDatabase()->GetBonusData( pSelectedBonusMapPanel->GetBonusMapListItemID() );
	pSelectedBonusMapPanel->SetBonusMapInfo( BonusMapsDatabase()->GetPath(), *pMap );

	RefreshCompletionPercentage();
	RefreshDialog( pMap );
}

void CBonusMapsDialog::RefreshData( void )
{
	for ( int iMap = 0; iMap < m_pGameList->GetItemCount(); ++iMap )
	{
		CBonusMapPanel *pBonusMapPanel = (CBonusMapPanel *)m_pGameList->GetItemPanel( iMap );

		if ( pBonusMapPanel )
			pBonusMapPanel->SetBonusMapInfo( BonusMapsDatabase()->GetPath(), *(BonusMapsDatabase()->GetBonusData( pBonusMapPanel->GetBonusMapListItemID() ) ) );
	}

	CBonusMapPanel *pSelectedBonusMapPanel = (CBonusMapPanel *)m_pGameList->GetSelectedPanel();
	if ( !pSelectedBonusMapPanel )
		return;

	BonusMapDescription_t *pMap = BonusMapsDatabase()->GetBonusData( pSelectedBonusMapPanel->GetBonusMapListItemID() );

	RefreshCompletionPercentage();
	RefreshDialog( pMap );
}

int CBonusMapsDialog::GetSelectedChallenge( void )
{
	if ( !m_pChallengeSelection->IsEnabled() )
		return -1;

	KeyValues *pUserDataKey = m_pChallengeSelection->GetActiveItemUserData();
	return pUserDataKey->GetInt( "challenge" );
}

void CBonusMapsDialog::RefreshDialog( BonusMapDescription_t *pMap )
{
	if ( !pMap || pMap->bLocked || ( m_pChallengeSelection->IsEnabled() && GetSelectedChallenge() == -1 ) )
	{
		// It's locked or no challenge is selected, so disable the load button
		SetControlEnabled( "loadbonusmap", false );
	}
	else
	{
		// Enable the load button
		SetControlEnabled( "loadbonusmap", true );
	}

	RefreshMedalDisplay( pMap );

	if ( pMap )
		SetControlString( "CommentLabel", pMap->szComment );
	else
		SetControlString( "CommentLabel", "" );
}

void CBonusMapsDialog::RefreshMedalDisplay( BonusMapDescription_t *pMap )
{
	{
		int iFirstChildIndex = FindChildIndexByName( "ChallengeMedalOverview00" );
		for ( int iMedal = 0; iMedal < 5; ++iMedal )
		{
			Panel *pMedalImage = GetChild( iFirstChildIndex + iMedal );
			pMedalImage->SetVisible( false );
		}
	}
	if ( !pMap || !pMap->m_pChallenges )
	{
		SetControlVisible( "ChallengeCommentLabel", false );
		SetControlVisible( "ChallengeEarnedMedal", false );
		SetControlVisible( "ChallengeBestLabel", false );
		SetControlVisible( "ChallengeNextMedal", false );
		SetControlVisible( "ChallengeNextLabel", false );

		return;
	}

	char szBuff[ 512 ];

	int iChallenge = GetSelectedChallenge();

	if ( iChallenge < 0 )
	{
		SetControlVisible( "ChallengeCommentLabel", false );
		SetControlVisible( "ChallengeEarnedMedal", false );
		SetControlVisible( "ChallengeBestLabel", false );
		SetControlVisible( "ChallengeNextMedal", false );
		SetControlVisible( "ChallengeNextLabel", false );

		int iFirstChildIndex = FindChildIndexByName( "ChallengeMedalOverview00" );
		for ( int iChallengeIndex = 0; iChallengeIndex < m_pChallengeSelection->GetItemCount(); ++iChallengeIndex )
		{
			KeyValues *pUserDataKey = m_pChallengeSelection->GetItemUserData( iChallengeIndex );
			int iChallengeNum = pUserDataKey->GetInt( "challenge" );

			if ( iChallengeNum >= 0 )
			{
				ChallengeDescription_t *pChallengeDescription = NULL;

				for ( int i = 0 ; i < pMap->m_pChallenges->Count(); ++i )
				{
					int iType = ((*pMap->m_pChallenges)[ i ]).iType;

					if ( iType == -1 )
					{
						iType = i;
					}

					if ( iType == iChallengeNum )
						pChallengeDescription = &((*pMap->m_pChallenges)[ i ]);
				}

				if ( pChallengeDescription )
				{
					int iBest, iEarnedMedal, iNext, iNextMedal;
					GetChallengeMedals( pChallengeDescription, iBest, iEarnedMedal, iNext, iNextMedal );

					if ( iChallengeNum < 10 )
						Q_snprintf( szBuff, 256, "medals/medal_0%i_%s", iChallengeNum, g_pszMedalNames[ iEarnedMedal ] );
					else
						Q_snprintf( szBuff, 256, "medals/medal_%i_%s", iChallengeNum, g_pszMedalNames[ iEarnedMedal ] );
				}

				CBitmapImagePanel *pBitmap = dynamic_cast<CBitmapImagePanel*>( GetChild( iFirstChildIndex + iChallengeNum ) );
				pBitmap->SetVisible( true );
				pBitmap->setTexture( szBuff );
			}
		}

		return;
	}

	ChallengeDescription_t *pChallengeDescription = NULL;

	for ( int i = 0 ; i < pMap->m_pChallenges->Count(); ++i )
	{
		int iType = ((*pMap->m_pChallenges)[ i ]).iType;

		if ( iType == -1 )
		{
			iType = i;
		}

		if ( iType == iChallenge )
			pChallengeDescription = &((*pMap->m_pChallenges)[ i ]);
	}

	if ( !pChallengeDescription )
		return;

	const char *pchChallengeComment = pChallengeDescription->szComment;

	int iBest, iEarnedMedal, iNext, iNextMedal;
	GetChallengeMedals( pChallengeDescription, iBest, iEarnedMedal, iNext, iNextMedal );

	// Set comment label
	SetControlString( "ChallengeCommentLabel", pchChallengeComment );
	SetControlVisible( "ChallengeCommentLabel", true );

	// Set earned medal
	if ( iEarnedMedal > -1 )
	{
		if ( iChallenge < 10 )
			Q_snprintf( szBuff, sizeof( szBuff ), "medals/medal_0%i_%s", iChallenge, g_pszMedalNames[ iEarnedMedal ] );
		else
			Q_snprintf( szBuff, sizeof( szBuff ), "medals/medal_%i_%s", iChallenge, g_pszMedalNames[ iEarnedMedal ] );

		CBitmapImagePanel *pBitmap = dynamic_cast<CBitmapImagePanel*>( FindChildByName( "ChallengeEarnedMedal" ) );
		pBitmap->SetVisible( true );
		pBitmap->setTexture( szBuff );
	}

	// Set next medal
	if ( iNextMedal > 0 )
	{
		if ( iChallenge < 10 )
			Q_snprintf( szBuff, sizeof( szBuff ), "medals/medal_0%i_%s", iChallenge, g_pszMedalNames[ iNextMedal ] );
		else
			Q_snprintf( szBuff, sizeof( szBuff ), "medals/medal_%i_%s", iChallenge, g_pszMedalNames[ iNextMedal ] );

		CBitmapImagePanel *pBitmap = dynamic_cast<CBitmapImagePanel*>( FindChildByName( "ChallengeNextMedal" ) );
		pBitmap->SetVisible( true );
		pBitmap->setTexture( szBuff );
	}
	else
	{
		SetControlVisible( "ChallengeNextMedal", false );
	}

	wchar_t szWideBuff[ 64 ];
	wchar_t szWideBuff2[ 64 ];

	// Best label
	if ( iBest != -1 )
	{
		Q_snprintf( szBuff, sizeof( szBuff ), "%i", iBest );
		g_pVGuiLocalize->ConvertANSIToUnicode( szBuff, szWideBuff2, sizeof( szWideBuff2 ) );
		g_pVGuiLocalize->ConstructString( szWideBuff, sizeof( szWideBuff ), g_pVGuiLocalize->Find( "#GameUI_BonusMapsBest" ), 1, szWideBuff2 );
		g_pVGuiLocalize->ConvertUnicodeToANSI( szWideBuff, szBuff, sizeof( szBuff ) );

		SetControlString( "ChallengeBestLabel", szBuff );
		SetControlVisible( "ChallengeBestLabel", true );
	}
	else
		SetControlVisible( "ChallengeBestLabel", false );

	// Next label
	if ( iNext != -1 )
	{
		Q_snprintf( szBuff, sizeof( szBuff ), "%i", iNext );
		g_pVGuiLocalize->ConvertANSIToUnicode( szBuff, szWideBuff2, sizeof( szWideBuff2 ) );
		g_pVGuiLocalize->ConstructString( szWideBuff, sizeof( szWideBuff ), g_pVGuiLocalize->Find( "#GameUI_BonusMapsGoal" ), 1, szWideBuff2 );
		g_pVGuiLocalize->ConvertUnicodeToANSI( szWideBuff, szBuff, sizeof( szBuff ) );

		SetControlString( "ChallengeNextLabel", szBuff );
		SetControlVisible( "ChallengeNextLabel", true );
	}
	else
		SetControlVisible( "ChallengeNextLabel", false );
}

void CBonusMapsDialog::RefreshCompletionPercentage( void )
{
	float fPercentage = BonusMapsDatabase()->GetCompletionPercentage();
	if ( fPercentage > 0.0f )
	{
		char szBuff[ 256 ];
		if ( fPercentage * 100.0f < 1.0f )
			Q_snprintf( szBuff, 256, "%.2f%%", fPercentage * 100.0f );	// Show decimal places if less than 1%
		else
			Q_snprintf( szBuff, 256, "%.0f%%", fPercentage * 100.0f );

		SetControlString( "PercentageText", szBuff );
		SetControlVisible( "PercentageText", true );
		SetControlVisible( "CompletionText", true );

		// Blend the color from backround color 0% to selected color 100%
		Color cProgressBar = Color( static_cast<float>( m_PercentageBarBackgroundColor.r() ) * ( 1.0f - fPercentage ) + static_cast<float>( m_PercentageBarColor.r() ) * fPercentage,
									static_cast<float>( m_PercentageBarBackgroundColor.g() ) * ( 1.0f - fPercentage ) + static_cast<float>( m_PercentageBarColor.g() ) * fPercentage,
									static_cast<float>( m_PercentageBarBackgroundColor.b() ) * ( 1.0f - fPercentage ) + static_cast<float>( m_PercentageBarColor.b() ) * fPercentage,
									static_cast<float>( m_PercentageBarBackgroundColor.a() ) * ( 1.0f - fPercentage ) + static_cast<float>( m_PercentageBarColor.a() ) * fPercentage );

		m_pPercentageBar->SetFillColor( cProgressBar );
		m_pPercentageBar->SetWide( m_pPercentageBarBackground->GetWide() * fPercentage );

		SetControlVisible( "PercentageBarBackground", true );
		SetControlVisible( "PercentageBar", true );
	}
	else
	{
		// 0% complete so don't display
		SetControlVisible( "PercentageText", false );
		SetControlVisible( "CompletionText", false );
		SetControlVisible( "PercentageBarBackground", false );
		SetControlVisible( "PercentageBar", false );
	}
}


void CBonusMapsDialog::ApplySchemeSettings( IScheme *pScheme )
{
	m_PercentageBarBackgroundColor = Color( 0, 0, 0, 64 );
	m_PercentageBarColor = pScheme->GetColor( "NewGame.SelectionColor", Color(255, 255, 255, 255) );

	BaseClass::ApplySchemeSettings( pScheme );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBonusMapsDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "loadbonusmap" ) )
	{
		int mapIndex = GetSelectedItemBonusMapIndex();
		if ( BonusMapsDatabase()->IsValidIndex( mapIndex ) )
		{
			BonusMapDescription_t *pBonusMap = BonusMapsDatabase()->GetBonusData( mapIndex );

			// Don't do anything with locked items
			if ( pBonusMap->bLocked || ( m_pChallengeSelection->IsEnabled() && GetSelectedChallenge() == -1 ) )
				return;			

			const char *shortName = pBonusMap->szShortName;
			if ( shortName && shortName[ 0 ] )
			{
				if ( pBonusMap->bIsFolder )
				{
					BonusMapsDatabase()->AppendPath( shortName );

					// repopulate list with current directory
					BuildMapsList();

					m_pGameList->MoveScrollBarToTop();
				}
				else
				{
					// Load the game, return to top and switch to engine
					char sz[ 256 ];

					// Set the challenge mode if one is selected
					int iChallenge = GetSelectedChallenge() + 1;

					if ( iChallenge > 0 )
					{
						Q_snprintf( sz, sizeof( sz ), "sv_bonus_challenge %i\n", iChallenge );
						engine->ClientCmd_Unrestricted( sz );

						ChallengeDescription_t *pChallengeDescription = &((*pBonusMap->m_pChallenges)[ iChallenge - 1 ]);

						// Set up medal goals
						BonusMapsDatabase()->SetCurrentChallengeObjectives( pChallengeDescription->iBronze, pChallengeDescription->iSilver, pChallengeDescription->iGold );
						BonusMapsDatabase()->SetCurrentChallengeNames( pBonusMap->szFileName, pBonusMap->szMapName, pChallengeDescription->szName );
					}

					if ( pBonusMap->szMapFileName[ 0 ] != '.' )
					{
						Q_snprintf( sz, sizeof( sz ), "map %s\n", pBonusMap->szMapFileName );
					}
					else
					{
						const char *pchSubDir = Q_strnchr( BonusMapsDatabase()->GetPath(), '/', Q_strlen( BonusMapsDatabase()->GetPath() ) );

						if ( pchSubDir )
						{
							pchSubDir = Q_strnchr( pchSubDir + 1, '/', Q_strlen( pchSubDir ) );

							if ( pchSubDir )
							{
								++pchSubDir;
								const char *pchMapFileName = pBonusMap->szMapFileName + 2;
								Q_snprintf( sz, sizeof( sz ), "map %s/%s\n", pchSubDir, pchMapFileName );
							}
						}
					}

					engine->ClientCmd_Unrestricted( sz );

					// Close this dialog
					//OnClose();
				}
			}
		}
	}
	else if ( !stricmp( command, "back" ) )
	{
		BonusMapsDatabase()->BackPath();

		// repopulate list with current directory
		BuildMapsList();

		m_pChallengeSelection->RemoveAll();
		m_pChallengeSelection->AddItem( "<Select A Challenge>", new KeyValues( "ChallengeSelection", "challenge", -1 ) );

		RefreshDialog( NULL );

		m_pGameList->MoveScrollBarToTop();
	}
	else if ( !stricmp( command, "ImportBonusMaps" ) )
	{
		if ( m_hImportBonusMapsDialog == NULL )
		{
			m_hImportBonusMapsDialog = new FileOpenDialog( NULL, "#GameUI_ImportBonusMaps", true );
			m_hImportBonusMapsDialog->AddFilter( "*.bmz", "#GameUI_BMZ_Files", true );
			m_hImportBonusMapsDialog->AddActionSignalTarget( this );
		}
		m_hImportBonusMapsDialog->DoModal(false);
		m_hImportBonusMapsDialog->Activate();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void CBonusMapsDialog::OnKeyCodeTyped( vgui::KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		OnCommand( "Close" );
		return;
	}

	BaseClass::OnKeyCodeTyped( code );
}

void CBonusMapsDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( code == KEY_XBUTTON_B )
	{
		OnCommand( "Close" );
		return;
	}
	else if ( code == KEY_XSTICK1_RIGHT || 
			  code == KEY_XSTICK2_RIGHT || 
			  code == KEY_XBUTTON_RIGHT || 
			  code == KEY_RIGHT )
	{
		if ( m_pGameList->GetItemCount() )
		{
			Panel *pSelectedPanel = m_pGameList->GetSelectedPanel();
			if ( !pSelectedPanel )
			{
				m_pGameList->SetSelectedPanel( m_pGameList->GetItemPanel( m_pGameList->FirstItem() ) );
				m_pGameList->ScrollToItem( m_pGameList->FirstItem() );
				return;
			}
			else
			{
				int nNextPanelID = m_pGameList->FirstItem();
				while ( nNextPanelID != m_pGameList->InvalidItemID() )
				{
					if ( m_pGameList->GetItemPanel( nNextPanelID ) == pSelectedPanel )
					{
						nNextPanelID = m_pGameList->NextItem( nNextPanelID );
						if ( nNextPanelID != m_pGameList->InvalidItemID() )
						{
							m_pGameList->SetSelectedPanel( m_pGameList->GetItemPanel( nNextPanelID ) );
							m_pGameList->ScrollToItem( nNextPanelID );
							return;
						}

						break;
					}

					nNextPanelID = m_pGameList->NextItem( nNextPanelID );
				}
			}
		}
	}
	else if ( code == KEY_XSTICK1_LEFT || 
			  code == KEY_XSTICK2_LEFT || 
			  code == KEY_XBUTTON_LEFT || 
			  code == KEY_LEFT )
	{
		if ( m_pGameList->GetItemCount() )
		{
			Panel *pSelectedPanel = m_pGameList->GetSelectedPanel();
			if ( !pSelectedPanel )
			{
				m_pGameList->SetSelectedPanel( m_pGameList->GetItemPanel( m_pGameList->FirstItem() ) );
				m_pGameList->ScrollToItem( m_pGameList->FirstItem() );
				return;
			}
			else
			{
				int nNextPanelID = m_pGameList->FirstItem();
				if ( m_pGameList->GetItemPanel( nNextPanelID ) != pSelectedPanel )
				{
					while ( nNextPanelID != m_pGameList->InvalidItemID() )
					{
						int nOldPanelID = nNextPanelID;
						nNextPanelID = m_pGameList->NextItem( nNextPanelID );

						if ( nNextPanelID != m_pGameList->InvalidItemID() )
						{
							if ( m_pGameList->GetItemPanel( nNextPanelID ) == pSelectedPanel )
							{
								m_pGameList->SetSelectedPanel( m_pGameList->GetItemPanel( nOldPanelID ) );
								m_pGameList->ScrollToItem( nOldPanelID  );
								return;
							}
						}
					}
				}
			}
		}
	}
	else if ( code == KEY_XSTICK1_DOWN || 
			  code == KEY_XSTICK2_DOWN || 
			  code == KEY_XBUTTON_DOWN || 
			  code == KEY_DOWN )
	{
		int nOldActiveItem = m_pChallengeSelection->GetActiveItem();
		int nActiveItem = min( nOldActiveItem + 1, m_pChallengeSelection->GetItemCount() - 1 );

		if ( nOldActiveItem != nActiveItem )
		{
			m_pChallengeSelection->ActivateItem( nActiveItem );
			return;
		}
	}
	else if ( code == KEY_XSTICK1_UP || 
			  code == KEY_XSTICK2_UP || 
			  code == KEY_XBUTTON_UP || 
			  code == KEY_UP )
	{
		int nOldActiveItem = m_pChallengeSelection->GetActiveItem();
		int nActiveItem = max( nOldActiveItem - 1, 0 );

		if ( nOldActiveItem != nActiveItem )
		{
			m_pChallengeSelection->ActivateItem( nActiveItem );
			return;
		}
	}
	else if ( code == KEY_ENTER || code == KEY_XBUTTON_A || code == STEAMCONTROLLER_A )
	{
		Panel *pSelectedPanel = m_pGameList->GetSelectedPanel();
		if ( pSelectedPanel )
		{
			OnCommand( "loadbonusmap" );
			return;
		}
	}

	BaseClass::OnKeyCodePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose: One item has been selected
//-----------------------------------------------------------------------------
void CBonusMapsDialog::OnPanelSelected()
{
	CBonusMapPanel *pSelectedBonusMapPanel = (CBonusMapPanel *)m_pGameList->GetSelectedPanel();
	if ( !pSelectedBonusMapPanel )
		return;
	
	BonusMapDescription_t *pMap = BonusMapsDatabase()->GetBonusData( pSelectedBonusMapPanel->GetBonusMapListItemID() );
	if ( !pMap )
		return;
	
	SetControlString( "CommentLabel", pMap->szComment );

	// Handle challenge selection box
	int iNumChallenges = 0;

	if ( pMap->m_pChallenges )
	{
		// Get the name of the previously selected challenge so that we can select a matching challenge (if one exists)
		int iSelectedChallenge = GetSelectedChallenge();

		m_pChallengeSelection->RemoveAll();

		m_pChallengeSelection->AddItem( "<Select A Challenge>", new KeyValues( "ChallengeSelection", "challenge", -1 ) );

		int iFoundSimilar = 0;

		for ( iNumChallenges; iNumChallenges < pMap->m_pChallenges->Count(); ++iNumChallenges )
		{
			ChallengeDescription_t *pChallenge = &(*pMap->m_pChallenges)[ iNumChallenges ];
			int iType = iNumChallenges;
			
			// If the challenge type was specified then use that instead of (legacy) the order the challenges were listed
			if ( pChallenge->iType != -1 )
				iType = pChallenge->iType;

			m_pChallengeSelection->AddItem( pChallenge->szName, new KeyValues( "ChallengeSelection", "challenge", iType ) );

			if ( iSelectedChallenge == iNumChallenges )
				iFoundSimilar = iNumChallenges + 1;
		}

		m_pChallengeSelection->ActivateItemByRow( iFoundSimilar );
	}

	if ( iNumChallenges > 0 )
	{
		m_pChallengeSelection->SetEnabled( true );
		m_pChallengeSelection->SetVisible( true );
		m_pChallengeSelection->SetNumberOfEditLines( iNumChallenges + 1 );
	}
	else
	{
		m_pChallengeSelection->SetEnabled( false );
		m_pChallengeSelection->SetVisible( false );
	}

	RefreshDialog( pMap );
}

void CBonusMapsDialog::OnControlModified()
{
	CBonusMapPanel *pSelectedBonusMapPanel = (CBonusMapPanel *)m_pGameList->GetSelectedPanel();
	if ( !pSelectedBonusMapPanel )
		return;
	BonusMapDescription_t *pMap = BonusMapsDatabase()->GetBonusData( pSelectedBonusMapPanel->GetBonusMapListItemID() );
	if ( !pMap )
		return;
	RefreshDialog( pMap );
}

// file selected.  This can only happen when someone selects an image to be imported as a spray logo.
void CBonusMapsDialog::OnFileSelected( const char *fullpath )
{
	if ( fullpath == NULL || fullpath[ 0 ] == '\0' )
		return;

	// this can take a while, put up a waiting cursor
	surface()->SetCursor(dc_hourglass);

	ImportZippedBonusMaps( fullpath );

	// change the cursor back to normal
	surface()->SetCursor(dc_user);
}
