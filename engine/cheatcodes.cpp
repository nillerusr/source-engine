//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "cheatcodes.h"
#include "cmd.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "tier2/tier2.h"
#include "inputsystem/iinputsystem.h"
#include "host.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//=========================================================
// Cheat Codes
//=========================================================
#define CHEAT_NAME_MAX_LEN		32
#define CHEAT_CODE_MAX_LEN		10
#define CHEAT_COMMAND_MAX_LEN	128
static ButtonCode_t s_pKeyLog[CHEAT_CODE_MAX_LEN];
static int s_nKeyLogIndex = 0;


struct CheatCodeData_t
{
	char			szName[ CHEAT_NAME_MAX_LEN ];

	bool			bDevOnly;

	int				iCodeLength;
	ButtonCode_t	pButtonCodes[ CHEAT_CODE_MAX_LEN ];

	char			szCommand[ CHEAT_COMMAND_MAX_LEN ];
};


static CUtlVector<CheatCodeData_t> s_CheatCodeCommands;


void ClearCheatCommands( void )
{
	s_CheatCodeCommands.RemoveAll();
}

void ReadCheatCommandsFromFile( char *pchFileName )
{
	KeyValues *pCheatCodeKeys = new KeyValues( "cheat_codes" );
	pCheatCodeKeys->LoadFromFile( g_pFullFileSystem, pchFileName, NULL );

	KeyValues *pKey = NULL;
	for ( pKey = pCheatCodeKeys->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey() )
	{
		int iCheat = s_CheatCodeCommands.AddToTail();
		CheatCodeData_t *pNewCheatCode = &(s_CheatCodeCommands[ iCheat ]);

		Q_strncpy( pNewCheatCode->szName, pKey->GetName(), CHEAT_NAME_MAX_LEN );	// Get the name
		pNewCheatCode->bDevOnly = ( pKey->GetInt( "dev", 0 ) != 0 );				// Get developer only flag
		pNewCheatCode->iCodeLength = 0;												// Start at zero code elements
		Q_strncpy( pNewCheatCode->szCommand, pKey->GetString( "command", "echo \"Cheat code has no command!\"" ), CHEAT_COMMAND_MAX_LEN );

		KeyValues *pSubKey = NULL;
		for ( pSubKey = pKey->GetFirstSubKey(); pSubKey; pSubKey = pSubKey->GetNextKey() )
		{
			const char *pchType = pSubKey->GetName();
			if ( Q_strcmp( pchType, "code" ) == 0 )
			{
				AssertMsg( ( pNewCheatCode->iCodeLength < CHEAT_NAME_MAX_LEN ), "Cheat code elements exceeded max!" );

				pNewCheatCode->pButtonCodes[ pNewCheatCode->iCodeLength ] = g_pInputSystem->StringToButtonCode( pSubKey->GetString() );
				++pNewCheatCode->iCodeLength;
			}
		}

		if ( pNewCheatCode->iCodeLength < CHEAT_NAME_MAX_LEN )
		{
			// If it's activation is a subsequence of another cheat, the longer cheat can't be activated!
			DevWarning( "Cheat code \"%s\" has less than %i code elements!", pKey->GetName(), CHEAT_NAME_MAX_LEN );
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void ResetKeyLogging()
{
	s_nKeyLogIndex = 0;
}

//---------------------------------------------------------
//---------------------------------------------------------
void LogKeyPress( ButtonCode_t code )
{
	if ( s_nKeyLogIndex < CHEAT_CODE_MAX_LEN )
	{
		// Log isn't full, so add it in the next spot
		s_pKeyLog[ s_nKeyLogIndex ] = code;
		++s_nKeyLogIndex;
		return;
	}

	// Log is full so shift all data to the previous bucket
	int i;
	for ( i = 0; i < CHEAT_CODE_MAX_LEN - 1; ++i )
	{
		s_pKeyLog[ i ] = s_pKeyLog[ i + 1 ];
	}

	// Log into the last bucket
	s_pKeyLog[ i ] = code;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CheckCheatCodes()
{
	// Loop through all cheat codes
	int iNumCheatCodes = s_CheatCodeCommands.Count();
	for ( int iCheatCode = 0; iCheatCode < iNumCheatCodes; ++iCheatCode )
	{
		CheatCodeData_t *pCheatCode = &(s_CheatCodeCommands[ iCheatCode ]);

		if ( pCheatCode->bDevOnly && !developer.GetBool() )
			continue;	// This cheat is only allowed in developer mode

		int iLogIndex = s_nKeyLogIndex - pCheatCode->iCodeLength;	// Check code against the back chunk of the log

		if ( iLogIndex < 0 )
			continue;	// There's less codes in the log than we need

		int iCode = 0;

		while ( iCode < pCheatCode->iCodeLength && pCheatCode->pButtonCodes[ iCode ] == s_pKeyLog[ iLogIndex ] )
		{
			++iCode;
			++iLogIndex;
		}
		
		if ( iCode == pCheatCode->iCodeLength )
		{
			// Every part of the code was correct
			DevMsg( "Cheat code \"%s\" activated!", pCheatCode->szName );

			Cbuf_AddText( "sv_cheats 1\n" );
			Cbuf_AddText( pCheatCode->szCommand );
			Cbuf_AddText( "\n" );

			ResetKeyLogging();
			return;
		}
	}
}
