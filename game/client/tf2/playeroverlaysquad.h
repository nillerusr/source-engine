//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( PLAYEROVERLAYSQUAD_H )
#define PLAYEROVERLAYSQUAD_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Label.h>

class KeyValues;
class CHudPlayerOverlay;
class CHudPlayerOverlaySquad : public vgui::Label
{
public:
	typedef vgui::Label BaseClass;

	CHudPlayerOverlaySquad( CHudPlayerOverlay *baseOverlay, const char *squadname );
	virtual ~CHudPlayerOverlaySquad( void );

	bool Init( KeyValues* pInitData );

	void SetSquad( const char *squadname );
	
	virtual void	Paint();
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
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

	char	m_szSquad[ 64 ];
	Color	m_fgColor;
	Color	m_bgColor;
	CHudPlayerOverlay *m_pBaseOverlay;

	bool			m_bReflectMouse;
};

#endif // PLAYEROVERLAYSQUAD_H