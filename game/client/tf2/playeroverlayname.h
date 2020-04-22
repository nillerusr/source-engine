//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#if !defined( PLAYEROVERLAYNAME_H )
#define PLAYEROVERLAYNAME_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Label.h>

class KeyValues;
class CHudPlayerOverlay;
class CHudPlayerOverlayName : public vgui::Label
{
public:
	typedef vgui::Label BaseClass;

	CHudPlayerOverlayName( CHudPlayerOverlay *baseOverlay, const char *name );
	virtual ~CHudPlayerOverlayName( void );

	bool Init( KeyValues* pInitData );

	void SetName( const char *name );
	
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	Paint();

	virtual void	SetReflectMouse( bool reflect );
	// If reflect mouse is true, then pass these up to parent
	virtual void	OnCursorMoved(int x,int y);
	virtual void	OnMousePressed(vgui::MouseCode code);
	virtual void	OnMouseDoublePressed(vgui::MouseCode code);
	virtual void	OnMouseReleased(vgui::MouseCode code);
	virtual void	OnMouseWheeled(int delta);

	virtual void OnCursorEntered();
	virtual void OnCursorExited();
private:

	char			m_szName[ 64 ];
	Color			m_fgColor;
	Color			m_bgColor;

	CHudPlayerOverlay *m_pBaseOverlay;

	bool			m_bReflectMouse;
};

#endif // PLAYEROVERLAYNAME_H