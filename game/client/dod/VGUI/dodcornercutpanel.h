//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_CORNERCUTPANEL_H
#define DOD_CORNERCUTPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include <vgui_controls/EditablePanel.h>
#include <vgui/ISurface.h>

#include "dod_shareddefs.h"


//-----------------------------------------------------------------------------
// Purpose: Draws the corner-cut background panels
//-----------------------------------------------------------------------------
class CDoDCutEditablePanel : public vgui::EditablePanel
{
public:
	DECLARE_CLASS_SIMPLE( CDoDCutEditablePanel, vgui::EditablePanel );

	CDoDCutEditablePanel( vgui::Panel *parent, const char *name );

	virtual void PaintBackground();

	virtual void SetBorder( vgui::IBorder *border );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	virtual void SetVisible( bool state )
	{
		BaseClass::SetVisible( state );
	}

	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void GetSettings( KeyValues *outResourceData );

	virtual void SetCornerToCut( int nCorner ){ m_nCornerToCut = nCorner; }
	virtual void SetCornerCutSize( int nCutSize ){ m_nCornerCutSize = nCutSize; }
	virtual void SetBackGroundColor( const char *pszNewColor );
	virtual void SetBorderColor( const char *pszNewColor );

private:

	int m_iBackgroundTexture;

	int m_nCornerToCut;
	int m_nCornerCutSize;

	char m_szBackgroundTexture[128];
	char m_szBackgroudColor[128];
	char m_szBorderColor[128];

	Color m_clrBackground;
	Color m_clrBorder;
};

#endif // DOD_CORNERCUTPANEL_H

