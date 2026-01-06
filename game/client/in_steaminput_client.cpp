//=============================================================================//
//
// Purpose: Community integration of Steam Input on Source SDK 2013.
//
// Author: Blixibon
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "expanded_steam/isteaminput.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-------------------------------------------

CON_COMMAND( si_print_state, "" )
{
	bool bEnabled = g_pSteamInput->IsEnabled();

	char szState[512];
	Q_snprintf( szState, sizeof( szState ), "STEAM INPUT: %s\n", bEnabled ? "Enabled" : "Disabled" );

	if (bEnabled)
	{
		Q_strncat( szState, "\nLIST OF CONTROLLERS:\n", sizeof( szState ) );

		InputHandle_t inputHandles[STEAM_INPUT_MAX_COUNT];
		int iNumHandles = g_pSteamInput->GetConnectedControllers( inputHandles );
		for (int i = 0; i < iNumHandles; i++)
		{
			Q_strncat( szState, VarArgs("\t%s/%i %s\n",
				g_pSteamInput->GetControllerName( inputHandles[i] ),
				g_pSteamInput->GetControllerType( inputHandles[i] ),
				g_pSteamInput->GetActiveController() == inputHandles[i] ? "[ACTIVE]" : ""), sizeof(szState));
		}
	}

	Msg( "%s\n", szState );
}

CON_COMMAND( si_restart, "" )
{
	g_pSteamInput->Shutdown();
	g_pSteamInput->InitSteamInput();
}
