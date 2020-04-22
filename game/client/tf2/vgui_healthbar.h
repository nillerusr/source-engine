//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#ifndef VGUI_HEALTHBAR_H
#define VGUI_HEALTHBAR_H

#ifdef _WIN32
#pragma once
#endif


#include <vgui_controls/Panel.h>

class KeyValues;

class CHealthBarPanel : public vgui::Panel
{
public:
	CHealthBarPanel( vgui::Panel *pParent = NULL );
	virtual ~CHealthBarPanel( void );

	// Setup
	bool Init( KeyValues* pInitData );
	void SetGoodColor( int r, int g, int b, int a );
	void SetBadColor( int r, int g, int b, int a );
	void SetVertical( bool bVertical );

	// Health is expected to go from 0 to 1
	void SetHealth( float health );

	virtual void Paint( void );
	virtual void PaintBackground( void ) {}

private:
	float	m_Health;
	Color m_Ok;
	Color m_Bad;
	bool	m_bVertical;	// True if this bar should be vertical
};

#endif // VGUI_HEALTHBAR_H