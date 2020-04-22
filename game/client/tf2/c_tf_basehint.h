//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_BASEHINT_H
#define C_TF_BASEHINT_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/PHandle.h>
#include "utlvector.h"

class C_BaseTFPlayer;
class C_BaseEntity;
class ITFHintItem;
class KeyValues;
namespace vgui
{
	class Label;
}

class C_TFBaseHint;

typedef bool (*HINTCOMPLETIONFUNCTION)( C_TFBaseHint *hint );

#define TFBASEHINT_DEFAULT_WIDTH 400
#define TFBASEHINT_DEFAULT_HEIGHT 300

//-----------------------------------------------------------------------------
// Purpose: Base TF Hint UI Element
//  hints must position themselves and are responsible for any "sub objects"
//-----------------------------------------------------------------------------
class C_TFBaseHint : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
public:
	enum
	{
		BORDER = 2,
		CAPTION = 17
	};

	// Factory for hints
	static C_TFBaseHint				*CreateHint( int id, const char *subsection, int entity );

	// Construction
									C_TFBaseHint( int id, int priority, int entity, HINTCOMPLETIONFUNCTION pfn = NULL );
	virtual							~C_TFBaseHint( void );

	// Initialization
	virtual void					ParseFromData( KeyValues *pkv );

	// Redraw self
	virtual void					PaintBackground();
	virtual void					PerformLayout();
	virtual void					GetClientArea( int&x, int& y, int& w, int &h );
	virtual void					SetTitle( const char *title );

	// Panel will move itself to spot over movementtime
	virtual void					SetDesiredPosition( int x, int y, float movementtime = 0.3f );

	// Target element for hint
	virtual void					SetHintTarget( vgui::Panel *panel );

	// Scheme
	virtual void					ApplySchemeSettings(vgui::IScheme *pScheme);

	// Think
	virtual void					OnTick();
	// Derived classes should set m_bCompleted here for entire hint
	virtual void					CheckForCompletion( void );

	// Data access
	virtual int						GetID( void );
	virtual void					SetID( int id );

	virtual int						GetPriority( void );
	virtual void					SetPriority( int priority );

	virtual int						GetEntity( void );
	virtual C_BaseEntity			*GetBaseEntity( void );
	virtual void					SetEntity( int entity );
	virtual void					SetBaseEntity( C_BaseEntity *entity );

	// Install a function used to determine if the global hint requirements have
	//  been met
	virtual bool					GetCompleted( void );
	virtual void					SetCompleted( bool completed );
	virtual void					SetCompletionFunction( HINTCOMPLETIONFUNCTION pfn );

	// Accessors for hint items themselves
	virtual void					AddHintItem( ITFHintItem *item );
	virtual void					RemoveHintItem( int index );
	virtual int						GetNumHintItems( void );
	virtual void					RemoveAllHintItems( bool deleteitems );
	virtual ITFHintItem				*GetHintItem( int index );

protected:
	// Move current position toward desirec position
	virtual void					AnimatePosition( void );
	virtual float					GetMovementFraction( void );

	// In the process of moving
	bool							m_bMoving;

	// Amount of move time remaining
	float							m_flMoveRemaining;
	// Total move time
	float							m_flMoveTotal;
	// Start/end positions for panel movement
	int								m_nMoveStart[2];
	int								m_nMoveEnd[2];

private:
	// ID of the hint
	int								m_nID;
	// Priority of the hint
	int								m_nPriority;
	// Associated entity
	int								m_nEntity;
	// Has the hint been completed
	bool							m_bCompleted;
	// Object hint is associated with
	void							*m_pObject;
	// No mouse
	vgui::HCursor					m_CursorNone;
	// "Press [Esc]..."
	vgui::Label						*m_pClearLabel;
	// Window caption
	vgui::Label						*m_pCaption;

	// Safe handle to target object
	vgui::PHandle					m_hTarget;

	// Completion function
	HINTCOMPLETIONFUNCTION			m_pfnCompletion;

	// List of items
	CUtlVector< ITFHintItem * >		m_Hints;
};

#endif // C_TF_BASEHINT_H
