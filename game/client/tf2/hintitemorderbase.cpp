//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hintitemorderbase.h"
#include "paneleffect.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			*panelName - 
//-----------------------------------------------------------------------------
CHintItemOrderBase::CHintItemOrderBase( vgui::Panel *parent, const char *panelName )
: BaseClass( parent, panelName )
{
	m_bEffects = false;
	m_LineEffect = EFFECT_INVALID_HANDLE;
	m_FlashEffect = EFFECT_INVALID_HANDLE;

	DrawAxialLineToOrder();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintItemOrderBase::DrawAxialLineToOrder( void )
{
	// Derived class already set up effects
	if ( m_bEffects )
		return;

	m_bEffects = true;

	// Flash for vote order
	m_FlashEffect = CreateFlashEffect( this, NULL );
	// Flash the hint panel itself
	CreateFlashEffect( this, GetParent() );
	// Point from hint to vote order panel
	m_LineEffect = CreateAxialLineEffect( this, GetParent(), NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *panel - 
//-----------------------------------------------------------------------------
void CHintItemOrderBase::SetHintTarget( vgui::Panel *panel )
{
	BaseClass::SetHintTarget( panel );

	if ( !m_bEffects )
		return;

	// Update effect target
	if ( !panel || !g_pTF2RootPanel )
		return;
	
	CPanelEffect *e = g_pTF2RootPanel->FindEffect( m_LineEffect );
	if ( e )
	{
		e->SetPanelOther( panel );
	}
	
	e = g_pTF2RootPanel->FindEffect( m_FlashEffect );
	if ( e )
	{
		e->SetPanel( panel );
	}
}