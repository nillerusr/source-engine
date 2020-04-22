//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Revision:     $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include <VGUI_EntityPanel.h>
#include <KeyValues.h>
#include "commanderoverlay.h"
#include "clientmode_tfnormal.h"
#include "tf_shareddefs.h"
#include "shareddefs.h"
#include "c_func_resource.h"
#include "techtree.h"
#include "c_basetfplayer.h"
#include "vgui_HealthBar.h"
#include "vgui_bitmapimage.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CResourceZoneOverlay : public CEntityPanel
{
	DECLARE_CLASS( CResourceZoneOverlay, CEntityPanel );

public:
	CResourceZoneOverlay( vgui::Panel *parent, const char *panelName );
	virtual ~CResourceZoneOverlay( void );

	bool Init( KeyValues* pKeyValues, C_BaseEntity* pEntity );
	bool InitResourceBitmaps( KeyValues* pKeyValues );

	void SetColor( int r, int g, int b, int a );
	void SetImage( BitmapImage *pImage );

	virtual void OnTick();

	virtual void Paint( void );
	virtual void PaintBackground( void ) {}

private:
	class CResourceBitmaps
	{
	public:
		CResourceBitmaps() : m_pImage(0) {}

		BitmapImage		*m_pImage;
		Color		m_Color;
	};

	struct Rect_t
	{
		int x, y, w, h;
	};

	bool ParseSingleResourceBitmap( KeyValues *pKeyValues, 
			const char *pResourceName, CResourceBitmaps *pResourceBitmap );
	bool ParseTeamResourceBitmaps( CResourceBitmaps *pT, KeyValues *pTeam );

	int					m_r, m_g, m_b, m_a;
	CHealthBarPanel		m_UsageBar;
	Rect_t				m_Icon;
	C_ResourceZone		*m_pResourceZone;
	CResourceBitmaps	m_pResourceBitmap;
	BitmapImage			*m_pImage;

};


//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------

DECLARE_OVERLAY_FACTORY( CResourceZoneOverlay, "resourcezone" );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

CResourceZoneOverlay::CResourceZoneOverlay( vgui::Panel *parent, const char *panelName )
: BaseClass( parent, "CResourceZoneOverlay" )
{
	m_pImage = 0;
	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

CResourceZoneOverlay::~CResourceZoneOverlay( void )
{
	if ( m_pResourceBitmap.m_pImage )
	{
		delete m_pResourceBitmap.m_pImage;
	}
}


//-----------------------------------------------------------------------------
// Parse class icons
//-----------------------------------------------------------------------------

bool CResourceZoneOverlay::ParseSingleResourceBitmap( KeyValues *pKeyValues, 
				const char *pResourceName, CResourceBitmaps *pResourceBitmap )
{
	const char *image;
	KeyValues *pResource;
	pResource = pKeyValues->FindKey( pResourceName );
	if ( !pResource )
		return false;

	image = pResource->GetString( "material" );
	if ( image && image[ 0 ] )
	{
		pResourceBitmap->m_pImage = new BitmapImage( GetVPanel(), image );
	}
	else
	{
		return( false );
	}

	return ParseRGBA( pResource, "color", pResourceBitmap->m_Color );
}

bool CResourceZoneOverlay::ParseTeamResourceBitmaps( CResourceBitmaps *pT, KeyValues *pTeam )
{
	if ( !ParseSingleResourceBitmap( pTeam, "Alpha", pT ) )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

bool CResourceZoneOverlay::InitResourceBitmaps( KeyValues* pKeyValues )
{
//	char teamkey[ 128 ];
//	for (int i = 0; i < 3; ++i)
//	{
//		Q_snprintf( teamkey, sizeof( teamkey ), "Team%i", i );
//		KeyValues *pTeam = pKeyValues->getSection( teamkey );
//		if (pTeam)
		{
			if (!ParseTeamResourceBitmaps( &m_pResourceBitmap, pKeyValues ))
				return false;
		}
//	}

	return true;
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

bool CResourceZoneOverlay::Init( KeyValues* pKeyValues, C_BaseEntity* pEntity )
{
	if (!BaseClass::Init( pKeyValues, pEntity))
		return false;

	if (!pKeyValues)
		return false;

	// We gotta be attached to a resource zone
	m_pResourceZone = dynamic_cast<C_ResourceZone*>(GetEntity());
	if (!m_pResourceZone)
		return false;

	KeyValues* pUsage = pKeyValues->FindKey("Usage");
	if (!m_UsageBar.Init( pUsage ))
		return false;
	m_UsageBar.SetParent( this );

	// get the icon info...
	if (!ParseRect( pKeyValues, "iconposition", m_Icon.x, m_Icon.y, m_Icon.w, m_Icon.h ))
		return false;

	if (!InitResourceBitmaps( pKeyValues ))
		return false;

	SetImage( 0 );
	SetColor( m_pResourceBitmap.m_Color[0], m_pResourceBitmap.m_Color[1], m_pResourceBitmap.m_Color[2], m_pResourceBitmap.m_Color[3] );

	// we need updating
	return true;
}


//-----------------------------------------------------------------------------
// called when we're ticked...
//-----------------------------------------------------------------------------

void CResourceZoneOverlay::OnTick()
{
	// Update position
	CEntityPanel::OnTick();

	SetImage( m_pResourceBitmap.m_pImage );
	int r, g, b, a;
	m_pResourceBitmap.m_Color.GetColor( r, g, b, a );
	SetColor( r, g, b, a );

	m_UsageBar.SetHealth( m_pResourceZone->m_flClientResources );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pImage - Class specific image
//-----------------------------------------------------------------------------

void CResourceZoneOverlay::SetImage( BitmapImage *pImage )
{
	m_pImage = pImage;
}

//-----------------------------------------------------------------------------
// Sets the draw color 
//-----------------------------------------------------------------------------

void CResourceZoneOverlay::SetColor( int r, int g, int b, int a )
{
	m_r = r;
	m_g = g;
	m_b = b;
	m_a = a;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void CResourceZoneOverlay::Paint( void )
{
	if ( !m_pImage )
		return;

	ComputeAndSetSize();

	Color color;
	color.SetColor( m_r, m_g, m_b, m_a );
	m_pImage->SetPos( m_Icon.x, m_Icon.y );
	m_pImage->SetColor( color );
	m_pImage->DoPaint( GetVPanel() );
}