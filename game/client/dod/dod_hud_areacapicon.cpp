//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include "c_dod_player.h"
#include "iclientmode.h"

class CHudAreaCapIcon : public CHudElement, public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CHudAreaCapIcon, vgui::Panel );

	CHudAreaCapIcon( const char *name );
	
	virtual void Paint();
	virtual void Init();
	virtual bool ShouldDraw();

private:
	int	m_iAreaTexture;
	int m_iPrevMaterialIndex;
	Color m_clrIcon;
};


// DECLARE_HUDELEMENT( CHudAreaCapIcon );


CHudAreaCapIcon::CHudAreaCapIcon( const char *pName ) :
	vgui::Panel( NULL, "HudAreaCapIcon" ), CHudElement( pName )
{
	SetParent( g_pClientMode->GetViewport() );

	m_clrIcon = Color(255,255,255,255);

	m_iPrevMaterialIndex = 0;
	
	SetHiddenBits( HIDEHUD_PLAYERDEAD );
}

void CHudAreaCapIcon::Init()
{
	m_iAreaTexture = vgui::surface()->CreateNewTextureID();
}

bool CHudAreaCapIcon::ShouldDraw()
{
	return false;
}

void CHudAreaCapIcon::Paint()
{
	/*
	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();
	
		if( !pPlayer )
			return;
	
		int x,y,w,h;
		GetBounds( x,y,w,h );	
	
		// The player knows what material to show as our area icon
		int iMaterialIndex = pPlayer->m_Shared.GetAreaIconMaterial();
	
		if( iMaterialIndex <= 0 )
			return;
	
		// if the icon is changed from last draw, force the texture to reload
		bool bForceReload = ( m_iPrevMaterialIndex != iMaterialIndex );
	
		// get the material name from the material string table
		const char *szMatName = GetMaterialNameFromIndex( iMaterialIndex );
		
		// draw the icon
		vgui::surface()->DrawSetTextureFile( m_iAreaTexture, szMatName , true, bForceReload);
		vgui::surface()->DrawSetColor( m_clrIcon );
		vgui::surface()->DrawTexturedRect( 0, 0, w, h );
	
		// store the previous material index to compare next frame
		m_iPrevMaterialIndex = iMaterialIndex;*/
	
}

