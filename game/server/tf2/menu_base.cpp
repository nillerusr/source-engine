//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Ugly menus for prototyping
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "player.h"
#include "tf_player.h"
#include "menu_base.h"
#include "tf_team.h"
#include "baseviewmodel.h"
#include "tf_gamerules.h"
#include "tf_class_infiltrator.h"
#include "tier1/strtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
		    
// Global list of menus
CMenu	*gMenus[MENU_LAST];

//-----------------------------------------------------------------------------
// Purpose: Initialize the Global Menu structure
//-----------------------------------------------------------------------------
void InitializeMenus( void )
{
	gMenus[MENU_DEFAULT] = NULL;
	gMenus[MENU_TEAM] = new CMenuTeam();
	gMenus[MENU_CLASS] = new CMenuClass();
}

void DestroyMenus( void )
{
	delete gMenus[MENU_DEFAULT];
	delete gMenus[MENU_TEAM];
	delete gMenus[MENU_CLASS];
}

//-----------------------------------------------------------------------------
// Purpose: Base Menu Handling
//-----------------------------------------------------------------------------
CMenu::CMenu()
{
	memset( m_szMenuString, 0, sizeof(m_szMenuString) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMenu::Display( CBaseTFPlayer *pViewer, int allowed, int display_time )
{
	RecalculateMenu( pViewer );

	// Only display if the menu's not identical to the one the player's already seeing
	if ( pViewer->m_MenuUpdateTime > gpGlobals->curtime )
	{
		if ( (allowed == pViewer->m_MenuSelectionBuffer) && FStrEq( m_szMenuString, pViewer->m_MenuStringBuffer) )
			return;
	}
	pViewer->m_MenuUpdateTime = gpGlobals->curtime + 10;
	pViewer->m_MenuSelectionBuffer = allowed;

	const char *msg_portion = m_szMenuString;
	Q_strncpy( pViewer->m_MenuStringBuffer, m_szMenuString, MENU_STRING_BUFFER_SIZE );

	CSingleUserRecipientFilter user( pViewer );
	user.MakeReliable();

	while ( strlen(msg_portion) >= MENU_MSG_TEXTCHUNK_SIZE )
	{
		// split the string
		char sbuf[MENU_MSG_TEXTCHUNK_SIZE+1];
		Q_strncpy( sbuf, msg_portion, MENU_MSG_TEXTCHUNK_SIZE+1 );
		msg_portion += MENU_MSG_TEXTCHUNK_SIZE;


		// send the string portion
		UserMessageBegin( user, "ShowMenu" );
			WRITE_WORD( allowed );
			WRITE_CHAR( display_time );		// display time (-1 means unlimited)
			WRITE_BYTE( TRUE );				// there is more message to come
			WRITE_STRING( sbuf );
		MessageEnd();
	}

	// send the remaining string
	UserMessageBegin( user, "ShowMenu" );
		WRITE_WORD( allowed );
		WRITE_CHAR( display_time );			// display time (-1 means unlimited)
		WRITE_BYTE( FALSE );				// there is no more message to come
		WRITE_STRING( (char*)msg_portion );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMenu::RecalculateMenu( CBaseTFPlayer *pViewer )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMenu::Input( CBaseTFPlayer *pViewer, int iInput )
{
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Team Menu
//-----------------------------------------------------------------------------
CMenuTeam::CMenuTeam()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMenuTeam::RecalculateMenu( CBaseTFPlayer *pViewer )
{
	Q_strncpy( m_szMenuString, "Pick a Team: \n\n\n->1. Humans\n\n->2. Aliens\n",  sizeof(m_szMenuString) );

	// Allow aborting if they have a class
	if ( pViewer->GetTeam() )
	{
		Q_strncat( m_szMenuString, "\n\n->9. Don't change team.\n", sizeof(m_szMenuString), COPY_ALL_CHARACTERS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle input for the Team menu
//-----------------------------------------------------------------------------
bool CMenuTeam::Input( CBaseTFPlayer *pViewer, int iInput )
{
	// Allow aborting if they have a team
	if ( pViewer->GetTeam() && iInput == 9 )
	{
		pViewer->MenuReset();
		return true;
	}

	if (iInput < 0 || iInput >= GetNumberOfTeams())
		return false;

	// Ignore changeteam requests to their current team
	if ( pViewer->GetTeam() )
	{
		if ( iInput == pViewer->GetTeam()->GetTeamNumber() )
		{
			pViewer->MenuReset();
			return true;
		}
	}

	// Add the player to the team and then bring up the Class Menu
	pViewer->ChangeTeam( iInput );

	// Clear out the class
	if ( pViewer->GetPlayerClass() )
	{
		// Remove all the player's items 
		pViewer->RemoveAllItems( false );
		pViewer->HideViewModels();
		pViewer->ClearPlayerClass();
	}

	pViewer->ForceRespawn();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Class Menu
//-----------------------------------------------------------------------------
CMenuClass::CMenuClass()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMenuClass::RecalculateMenu( CBaseTFPlayer *pViewer )
{
	if ( !pViewer->GetTeam() )
		return;

	Q_snprintf( m_szMenuString,sizeof(m_szMenuString), "You are on Team %s\n\n\n\n\n\n\nPick your Class: \n\n\n", pViewer->GetTeam()->GetName() );

	int iClassNum = 1;

	// Check technology for each class
	for ( int i = 0; i < TFCLASS_CLASS_COUNT; i++ )
	{
		char sClass[256];

		if ( !( pViewer->IsClassAvailable( (TFClass)i ) ) )
			continue;

		int iNumber = pViewer->GetTFTeam()->GetNumOfClass( (TFClass)i );
		if ( !iNumber )
		{
			Q_snprintf( sClass, sizeof(sClass), "->%d. %s\n\n", iClassNum, GetTFClassInfo( i )->m_pClassName );
		}
		else
		{
			Q_snprintf( sClass, sizeof(sClass), "->%d. %s    (%d in your team)\n\n", iClassNum, GetTFClassInfo( i )->m_pClassName, iNumber );
		}

		Q_strncat( m_szMenuString, sClass,sizeof(m_szMenuString), COPY_ALL_CHARACTERS );
		iClassNum++;
	}

	// Allow aborting if they have a class
	if ( pViewer->GetPlayerClass() )
	{
		Q_strncat( m_szMenuString, "\n\n->9. Don't change class.\n", sizeof(m_szMenuString), COPY_ALL_CHARACTERS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle input for the Class menu
//-----------------------------------------------------------------------------
bool CMenuClass::Input( CBaseTFPlayer *pViewer, int iInput )
{
	int iClassNum = 0;

	// Allow aborting if they have a class
	if ( pViewer->GetPlayerClass() && iInput == 9 )
	{
		pViewer->MenuReset();
		return true;
	}

	// Get the class number
	for ( int i = 1; iInput && i < TFCLASS_CLASS_COUNT; i++ )
	{
		if ( !( pViewer->IsClassAvailable( (TFClass)i ) ) )
			continue;
		iInput--;
		iClassNum = i;
	}

	// Ignore changeclass requests to their current class
	if ( pViewer->GetPlayerClass() )
	{
		if ( (TFClass)iClassNum == pViewer->PlayerClass() )
		{
			pViewer->MenuReset();
			return true;
		}
	}

	if ( !pViewer->IsClassAvailable( (TFClass)iClassNum ) )
		return false;

	pViewer->ChangeClass( (TFClass)iClassNum );
	pViewer->m_pCurrentMenu = NULL;
	return true;
}