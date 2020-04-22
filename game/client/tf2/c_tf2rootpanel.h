//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF2ROOTPANEL_H
#define C_TF2ROOTPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/EditablePanel.h>
#include "utlvector.h"

class CPanelEffect;
class ITFHintItem;

// Serial under of effect, for safe lookup
typedef unsigned int EFFECT_HANDLE;

//-----------------------------------------------------------------------------
// Purpose: Sits between engine and client .dll panels
//  Responsible for drawing screen overlays
//-----------------------------------------------------------------------------
class C_TF2RootPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
public:
						C_TF2RootPanel( vgui::VPANEL parent );
	virtual				~C_TF2RootPanel( void );

	// Draw Panel effects here
	virtual void		PostChildPaint();

	// Clear list of Panel Effects
	virtual void		LevelInit( void );
	virtual void		LevelShutdown( void );

	// Panel Effect handlers
	EFFECT_HANDLE		AddEffect( CPanelEffect *effect );
	void				RemoveEffect( EFFECT_HANDLE handle );
	CPanelEffect		*FindEffect( EFFECT_HANDLE handle );
	void				DestroyPanelEffects( ITFHintItem *owner );
	void				ClearAllEffects( void );

	// Run effects and let them decide whether to remove themselves
	void				OnTick( void );

private:

	// Render all panel effects
	void		RenderPanelEffects( void );

	// List of current panel effects
	CUtlVector< CPanelEffect *> m_Effects;
};

extern C_TF2RootPanel *g_pTF2RootPanel;

#endif // C_TF2ROOTPANEL_H
