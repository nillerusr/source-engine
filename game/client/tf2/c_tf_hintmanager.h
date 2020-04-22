//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_HINTMANAGER_H
#define C_TF_HINTMANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "shareddefs.h"

class C_TFBaseHint;
class KeyValues;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_TFHintManager : public C_BaseEntity
{
	DECLARE_CLASS( C_TFHintManager, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

						C_TFHintManager( void );
						~C_TFHintManager( void );

	virtual void		OnDataChanged( DataUpdateType_t updateType );

	// Override think method
	virtual void		ClientThink( void );

	// Add hint to list
	C_TFBaseHint		*AddHint( int hintID, const char *subsection, int entityIndex, int maxduplicates );

	// Clear hints
	void				ClearHints( void );
	// Complete specified hint
	void				CompleteHint( int hintID, bool visibleOnly );
	// Determine ID of hint currently being shown to player
	int					GetCurrentHintID( void );

	KeyValues			*GetHintKeyValues( void );
	KeyValues			*GetHintDisplayStats( void );

	// Zero out all counters
	void				ResetDisplayStats( void );

private:
	// See how many of the type of hint are already being shown
	int					CountInstancesOfHintID( int hintID );

	// Hint list
	CUtlVector< C_TFBaseHint * >	m_aHints;

	KeyValues			*m_pkvHintSystem;
	KeyValues			*m_pkvHintDisplayStats;
};


#include "tf_hints.h"

class C_TFBaseHint;
namespace vgui
{
	class Panel;
}
class KeyValues;

// Use this when you want to allow an unlimited number of a certain type of hint
// Just a huge number of simultaneous duplicates allowed
#define HINTTYPE_NOLIMIT	5000

C_TFBaseHint *CreateGlobalHint( int hintid, const char *subsection = NULL, int entity = -1, int maxduplicates = 0 );
C_TFBaseHint *CreateGlobalHint_Panel( vgui::Panel *targetPanel, int hintid, const char *subsection = NULL, int entity = -1, int maxduplicates = 0 );
void DestroyGlobalHint( int hintid );
KeyValues *GetHintKeyValues( void );
KeyValues *GetHintDisplayStats( void );

// Returns true if hint system swallowed escape key
bool HintSystemEscapeKey( void );


#endif // C_TF_HINTMANAGER_H
