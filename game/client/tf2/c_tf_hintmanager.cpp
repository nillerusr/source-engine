//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tf_hintmanager.h"
#include "c_tf_basehint.h"
#include <KeyValues.h>
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// Global off switch for hint system
static ConVar tf2_hintsystem( "tf2_hintsystem", "0", 0, "Enable interface hints in TF2." );
static C_TFHintManager *g_pHintManager = NULL;

#define HINT_DISPLAY_STATS_FILE "scripts/hintdisplaystats.txt"

static float g_flLastEscapeKeyTime = -1.0f;
//-----------------------------------------------------------------------------
// Purpose: Helper to create panel in center and then shift toward right edge of screen
// Input  : *panel - 
// Output : static void
//-----------------------------------------------------------------------------
static void PositionPanel( C_TFBaseHint *panel )
{
	int w, h;
	panel->GetSize( w, h );

	int x = ( ScreenWidth() - w ) / 2;
	int y = ( ScreenHeight() - h ) / 2;

	panel->SetPos( x, y );

	panel->SetDesiredPosition( ScreenWidth() - w - 10, y - 75 );
}

IMPLEMENT_CLIENTCLASS_DT_NOBASE(C_TFHintManager, DT_TFHintManager, CTFHintManager)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TFHintManager::C_TFHintManager( void )
{
	g_pHintManager = this;

	m_pkvHintSystem = new KeyValues( "HintSystem" );
	if ( m_pkvHintSystem )
	{
		bool valid = m_pkvHintSystem->LoadFromFile( filesystem, "scripts//hintsystem.txt" );
		if ( !valid )
		{
			m_pkvHintSystem->deleteThis();
			m_pkvHintSystem = NULL;
		}
	}

	m_pkvHintDisplayStats = new KeyValues( "HintDisplayStats" );
	if ( m_pkvHintDisplayStats )
	{
		m_pkvHintDisplayStats->LoadFromFile( filesystem, HINT_DISPLAY_STATS_FILE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : KeyValues
//-----------------------------------------------------------------------------
KeyValues *C_TFHintManager::GetHintKeyValues( void )
{
	return m_pkvHintSystem;
}

KeyValues *C_TFHintManager::GetHintDisplayStats( void )
{
	return m_pkvHintDisplayStats;
}

void C_TFHintManager::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// Think right away
	if ( updateType ==  DATA_UPDATE_CREATED )
		SetNextClientThink( 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Remove complete hints, and activate next highest priority hint
//-----------------------------------------------------------------------------
void C_TFHintManager::ClientThink( void )
{
	SetNextClientThink( gpGlobals->curtime + 1.0 );

	int i;
	int highestPriority = -1;
	C_TFBaseHint *best = NULL;
	bool anyVisible = false;

	// See if any of the hints are completed, otherwise, store off the highest priority one
	for ( i = m_aHints.Size() - 1; i >= 0; i-- )
	{
		C_TFBaseHint *hint = m_aHints[ i ];

		// See if it should just be deleted
		if ( hint && hint->GetCompleted() )
		{
			m_aHints.Remove( i );
			delete hint;
			continue;
		}

		if ( hint->IsVisible() )
		{
			anyVisible = true;
		}

		if ( !best || ( hint->GetPriority() > highestPriority ) )
		{
			highestPriority = hint->GetPriority();
			best = hint;
		}
	}

	// Last hint finished, show next best one
	if ( !anyVisible )
	{
		// Now hide all but best one
		for ( i = 0; i < m_aHints.Size(); i++ )
		{
			C_TFBaseHint *hint = m_aHints[ i ];
			hint->SetVisible( hint == best );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : playerIndex - 
//			hintID - 
//			priority - 
//			entityIndex - 
//-----------------------------------------------------------------------------
C_TFBaseHint *C_TFHintManager::AddHint( int hintID, const char *subsection, int entityIndex, int maxduplicates )
{
	if ( !tf2_hintsystem.GetBool() )
		return NULL;

	// Don't add the same hint more than once unless maxduplicates >= 1 has been specifically requested
	int count = CountInstancesOfHintID( hintID );
	if ( count > maxduplicates )
	{
		return NULL;
	}

	C_TFBaseHint *hint = C_TFBaseHint::CreateHint( hintID, subsection, entityIndex );
	if ( hint )
	{
		// Force it to compute it's exact size
		hint->PerformLayout();

		PositionPanel( hint );

		m_aHints.AddToTail( hint );
	}

	return hint;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFHintManager::ClearHints( void )
{
	for ( int i = 0; i < m_aHints.Size(); i++ )
	{
		C_TFBaseHint *hint = m_aHints[ i ];
		delete hint;
	}

	m_aHints.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : playerIndex - 
//			hintID - 
//-----------------------------------------------------------------------------
void C_TFHintManager::CompleteHint( int hintID, bool visibleOnly )
{
	for ( int i = m_aHints.Size() - 1; i >= 0; i-- )
	{
		C_TFBaseHint *hint = m_aHints[ i ];
		if ( !hint )
			continue;

		if ( hint->GetID() != hintID )
			continue;

		if ( visibleOnly && !hint->IsVisible() )
			continue;

		delete hint;
		m_aHints.Remove( i );
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : playerIndex - 
//			hintID - 
// Output :	Number of instances in list
//-----------------------------------------------------------------------------
int C_TFHintManager::CountInstancesOfHintID( int hintID )
{
	int c = 0;
	for ( int i = m_aHints.Size() - 1; i >= 0; i-- )
	{
		C_TFBaseHint *hint = m_aHints[ i ];
		if ( hint && hint->GetID() == hintID )
		{
			c++;
		}
	}

	return c;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : playerIndex - 
// Output : int
//-----------------------------------------------------------------------------
int C_TFHintManager::GetCurrentHintID( void )
{
	for ( int i = m_aHints.Size() - 1; i >= 0; i-- )
	{
		C_TFBaseHint *hint = m_aHints[ i ];
		if ( hint && hint->IsVisible() )
		{
			return hint->GetID();
		}
	}

	return TF_HINT_UNDEFINED;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFHintManager::ResetDisplayStats( void )
{
	if ( !m_pkvHintDisplayStats )
	{
		Assert( 0 );
		return;
	}

	KeyValues *kv = m_pkvHintDisplayStats->GetFirstSubKey();
	while ( kv )
	{
		KeyValues *subKey = kv->GetFirstSubKey();
		if ( subKey && stricmp( subKey->GetName(), "times_shown") )
		{
			while ( subKey )
			{
				subKey->SetString( "times_shown", "0" );
				subKey = subKey->GetNextKey();
			}
		}
		else
		{
			kv->SetString( "times_shown", "0" );
		}

		kv = kv->GetNextKey();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hintid - 
//			100 - 
//			1 - 
//			-1 - 
//-----------------------------------------------------------------------------
C_TFBaseHint *CreateGlobalHint( int hintid, const char *subsection /*=NULL*/, int entity /*= -1*/, int maxduplicates /*=0*/ )
{
	if ( !g_pHintManager )
	{
		return NULL;
	}

	return g_pHintManager->AddHint( hintid, subsection, entity, maxduplicates );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hintid - 
//			100 - 
//			1 - 
//			-1 - 
//-----------------------------------------------------------------------------
C_TFBaseHint *CreateGlobalHint_Panel( vgui::Panel *targetPanel, int hintid, const char *subsection /*=NULL*/, int entity /*= -1*/, int maxduplicates /*=0*/ )
{
	C_TFBaseHint *hint = CreateGlobalHint( hintid, subsection, entity, maxduplicates );
	if ( hint )
	{
		// Find an appropriate position near the panel
		hint->SetHintTarget( targetPanel );
	}
	return hint;
}


//-----------------------------------------------------------------------------
// Purpose: Trap the escape key when a hint is showing
// Output : Returns true if the escape key was swallowed by the hint system
//-----------------------------------------------------------------------------

// Hitting escape twice withing this amount of seconds will clear all hints pending
#define ENTER_DOUBLETAP_TIME 1.0f

bool HintSystemEscapeKey( void )
{
	if ( !g_pHintManager )
		return false;

	int hintID = g_pHintManager->GetCurrentHintID();
	if ( hintID != TF_HINT_UNDEFINED )
	{
		bool killall = false;

		float curtime = gpGlobals->curtime;
		float dt = curtime - g_flLastEscapeKeyTime;

		if ( dt < ENTER_DOUBLETAP_TIME )
		{
			killall = true;
		}

		g_flLastEscapeKeyTime = curtime;

		if ( killall )
		{
			g_pHintManager->ClearHints();
		}
		else
		{
			g_pHintManager->CompleteHint( hintID, true );
		}
		// Swallow the escape key
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hindid - 
//-----------------------------------------------------------------------------
void DestroyGlobalHint( int hintid )
{
	if ( !g_pHintManager )
		return;

	g_pHintManager->CompleteHint( hintid, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : KeyValues
//-----------------------------------------------------------------------------
KeyValues *GetHintKeyValues( void )
{
	if ( !g_pHintManager )
		return NULL;

	return g_pHintManager->GetHintKeyValues();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : KeyValues
//-----------------------------------------------------------------------------
KeyValues *GetHintDisplayStats( void )
{
	if ( !g_pHintManager )
		return NULL;

	return g_pHintManager->GetHintDisplayStats();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ResetDisplayStats( void )
{
	if ( g_pHintManager )
	{
		g_pHintManager->ResetDisplayStats();
	}
}

static ConCommand tf2_hintreset( "tf2_hintreset", ResetDisplayStats, 0, FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TFHintManager::~C_TFHintManager( void )
{
	ClearHints();
	g_pHintManager = NULL;
	m_pkvHintSystem->deleteThis();
	if ( m_pkvHintDisplayStats )
	{
		m_pkvHintDisplayStats->SaveToFile( filesystem, HINT_DISPLAY_STATS_FILE );
	}
	m_pkvHintDisplayStats->deleteThis();
}