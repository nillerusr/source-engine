//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HINTITEMORDERBASE_H
#define HINTITEMORDERBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "hintitembase.h"
#include "c_tf2rootpanel.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHintItemOrderBase : public CHintItemBase
{
	DECLARE_CLASS( CHintItemOrderBase, CHintItemBase );
public:
	CHintItemOrderBase( vgui::Panel *parent, const char *panelName );

	virtual void		DrawAxialLineToOrder( void );

	virtual void		SetHintTarget( vgui::Panel *panel );

protected:
	bool				m_bEffects;

	EFFECT_HANDLE		m_LineEffect;
	EFFECT_HANDLE		m_FlashEffect;
};

#endif // HINTITEMORDERBASE_H
