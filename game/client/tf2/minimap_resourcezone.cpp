//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "c_func_resource.h"
#include "vgui_bitmapimage.h"
#include <KeyValues.h>
#include "minimap_trace.h"
#include "techtree.h"
#include "shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CMinimapResourceZonePanel : public CMinimapTracePanel
{
	DECLARE_CLASS( CMinimapResourceZonePanel, CMinimapTracePanel );

public:
	CMinimapResourceZonePanel( vgui::Panel *parent, const char *panelName );
	virtual ~CMinimapResourceZonePanel();

	virtual bool Init( KeyValues* pKeyValues, MinimapInitData_t* pInitData );
	virtual void Paint( );

private:
	BitmapImage *m_ppImage;
};

DECLARE_MINIMAP_FACTORY( CMinimapResourceZonePanel, "minimap_resource_zone_panel" );


CMinimapResourceZonePanel::CMinimapResourceZonePanel( vgui::Panel *parent, const char *panelName )
: BaseClass( parent, "CMinimapResourceZonePanel" )
{
	m_ppImage = NULL;
}

CMinimapResourceZonePanel::~CMinimapResourceZonePanel()
{
	if ( m_ppImage )
	{
		delete m_ppImage;
		m_ppImage = NULL;
	}
}

bool CMinimapResourceZonePanel::Init( KeyValues* pKeyValues, MinimapInitData_t* pInitData )
{
	// Can only be applied to resource zones...
	C_ResourceZone* pResource = dynamic_cast<C_ResourceZone*>( pInitData->m_pEntity );
	if (!pResource)
		return false;

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	// Read in the images for the resource zone...
	// Default to null
	m_ppImage = NULL;

	char const* pMaterialName = pKeyValues->GetString( "material" );
	if ( !pMaterialName || !pMaterialName[ 0 ] )
		return false;

	// modulation color
	Color color;
	if (!ParseRGBA( pKeyValues, "color", color ))
		color.SetColor( 255, 255, 255, 255 );

	// hook in the bitmap
	m_ppImage = new BitmapImage( GetVPanel(), pMaterialName );
	m_ppImage->SetColor( color );

	return true;
}

void CMinimapResourceZonePanel::Paint( )
{
	if ( gHUD.IsHidden( HIDEHUD_MISCSTATUS ) )
		return;

	// Paint the image for the zone type
	if ( m_ppImage )
	{
		m_ppImage->Paint();
	}
}



