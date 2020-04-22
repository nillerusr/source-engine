//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ITFHINTITEM_H
#define ITFHINTITEM_H
#ifdef _WIN32
#pragma once
#endif

namespace vgui
{
	class Panel;
}

class KeyValues;

//-----------------------------------------------------------------------------
// Purpose: All TF Hint Item Panels multiply inheret from this interface
//-----------------------------------------------------------------------------
class ITFHintItem
{
public:
	virtual void	ParseItem( KeyValues *pKeyValues ) = 0;

	// Delete the hint
	virtual void	DeleteThis( void ) = 0;

	// Returns true if the hint criteria have been met
	virtual bool	GetCompleted( void ) = 0;
	// Mark the hint as active
	virtual void	SetActive( bool active ) = 0;
	// Determine if the hint is the current, active hint
	virtual bool	GetActive( void ) = 0;
	// Allow hint to run code
	virtual void	Think( void ) = 0;
	// Determine height of hint
	virtual int		GetHeight( void ) = 0;
	// Set screen position of hint
	virtual void	SetPosition( int x, int y ) = 0;
	// Tell hint where it sits in current list of hints
	virtual void	SetItemNumber( int index ) = 0;
	// Set the hint visible/invisible
	virtual void	SetVisible( bool visible ) = 0;
	// Change the target (Panel) of the hint
	virtual void	SetHintTarget( vgui::Panel *panel ) = 0;
	// Return true if hint should render this frame (default returns true if GetActive() is true)
	virtual bool	ShouldRenderPanelEffects( void ) = 0;
	// Allow item to change title text
	virtual void	ComputeTitle( void ) = 0;
	// Allow outside influence over keyvalues
	virtual void	SetKeyValue( const char *key, const char *value ) = 0;
};

#endif // ITFHINTITEM_H
