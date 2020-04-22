//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "playeroverlayclass.h"
#include "playeroverlay.h"
#include <KeyValues.h>
#include "commanderoverlay.h"
#include "clientmode_tfnormal.h"
#include "tf_shareddefs.h"
#include "hud_commander_statuspanel.h"
#include "vgui_bitmapimage.h"
#include "igamesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Class info. Only load it once. 
//-----------------------------------------------------------------------------

CHudPlayerOverlayClass::CMapClassColors** CHudPlayerOverlayClass::s_ppClassInfo = 0;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCleanupPlayerOverlayClass : public CAutoGameSystem
{
public:
	virtual void Shutdown()
	{
		if ( !CHudPlayerOverlayClass::s_ppClassInfo )
			return;

		for (int i = 0; i <= MAX_TF_TEAMS; ++i)
		{
			for ( int j = 0; j < TFCLASS_CLASS_COUNT; j++ )
			{
				delete CHudPlayerOverlayClass::s_ppClassInfo[i][j].m_pClassImage;
			}
			delete[] CHudPlayerOverlayClass::s_ppClassInfo[i];
		}
		delete[] CHudPlayerOverlayClass::s_ppClassInfo;
	}
};

static CCleanupPlayerOverlayClass g_CleanupPlayerOverlayClass;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

CHudPlayerOverlayClass::CHudPlayerOverlayClass( CHudPlayerOverlay *baseOverlay )
: BaseClass( NULL, "CHudPlayerOverlayClass" )
{
	m_pBaseOverlay = baseOverlay;

	m_pImage = NULL;
	SetPaintBackgroundEnabled( false );

	// Send mouse inputs (but not cursorenter/exit for now) up to parent
	SetReflectMouse( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

CHudPlayerOverlayClass::~CHudPlayerOverlayClass( void )
{
	delete m_pImage;
}


//-----------------------------------------------------------------------------
// Parse class icons
//-----------------------------------------------------------------------------

bool CHudPlayerOverlayClass::ParseTeamClassInfo( KeyValues *pClassIcons, const char *classname, CMapClassColors *pClassColors )
{
	const char *classimage;
	KeyValues *pClass;
	pClass = pClassIcons->FindKey( classname );
	if ( !pClass )
		return false;

	classimage = pClass->GetString( "material" );
	if ( classimage && classimage[ 0 ] )
	{
		pClassColors->m_pClassImage = new BitmapImage( NULL, classimage );
	}
	else
	{
		return( false );
	}

	return ParseRGBA( pClass, "color", pClassColors->m_clrClass );
}

bool CHudPlayerOverlayClass::ParseTeamClassIcons( CMapClassColors *pT, KeyValues *pTeam )
{
	if ( !ParseTeamClassInfo( pTeam, "Recon", &pT[ TFCLASS_RECON ] ) )
		return false;
	if ( !ParseTeamClassInfo( pTeam, "Sniper", &pT[ TFCLASS_SNIPER ] ) )
		return false;
	if ( !ParseTeamClassInfo( pTeam, "Commando", &pT[ TFCLASS_COMMANDO ] ) )
		return false;
	if ( !ParseTeamClassInfo( pTeam, "Support", &pT[ TFCLASS_SUPPORT ] ) )
		return false;
	if ( !ParseTeamClassInfo( pTeam, "Medic", &pT[ TFCLASS_MEDIC ] ) )
		return false;
	if ( !ParseTeamClassInfo( pTeam, "Escort", &pT[ TFCLASS_ESCORT ] ) )
		return false;
	if ( !ParseTeamClassInfo( pTeam, "Defender", &pT[ TFCLASS_DEFENDER ] ) )
		return false;
	if ( !ParseTeamClassInfo( pTeam, "Sapper", &pT[ TFCLASS_SAPPER ] ) )
		return false;
	if ( !ParseTeamClassInfo( pTeam, "Infiltrator", &pT[ TFCLASS_INFILTRATOR ] ) )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

bool CHudPlayerOverlayClass::InitClassInfo( KeyValues* pKeyValues )
{
	if (s_ppClassInfo)
		return true;

	char teamkey[ 128 ];
	s_ppClassInfo = new CMapClassColors*[MAX_TF_TEAMS+1];
	for (int i = 0; i <= MAX_TF_TEAMS; ++i)
	{
		s_ppClassInfo[i] = new CMapClassColors[TFCLASS_CLASS_COUNT];
		Q_snprintf( teamkey, sizeof( teamkey ), "Team%i", i );
		KeyValues *pTeam = pKeyValues->FindKey( teamkey );
		if (pTeam)
		{
			if (!ParseTeamClassIcons( s_ppClassInfo[i], pTeam ))
				return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

bool CHudPlayerOverlayClass::Init( KeyValues* pKeyValues )
{
	if (!pKeyValues)
		return false;

	int x, y, w, h;
	if (!ParseRect(pKeyValues, "friendlyposition", x, y, w, h ))
		return false;

	if (!InitClassInfo( pKeyValues ))
		return false;

	SetPos( x, y );
	SetSize( w, h );

	SetImage( 0 );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pImage - Class specific image
//-----------------------------------------------------------------------------
void CHudPlayerOverlayClass::SetImage( BitmapImage *pImage )
{
	m_pImage = pImage;
	if (m_pImage)
	{
		// Make sure the image size is correct
		int w,h;
		GetSize(w,h);
	}
}

void CHudPlayerOverlayClass::SetTeamAndClass( int team, int playerclass )
{
	if (!s_ppClassInfo)
		return;

	CMapClassColors *pCC = &s_ppClassInfo[ team ][ playerclass ];
	if ( pCC )
	{
		int r, g, b, a;
		pCC->m_clrClass.GetColor( r, g, b, a );
		SetColor( r, g, b, a );
		SetImage( pCC->m_pClassImage );
	}
}

//-----------------------------------------------------------------------------
// If this guy changes size, so must the associated image
//-----------------------------------------------------------------------------

void CHudPlayerOverlayClass::SetSize( int w, int h ) 
{
	// chain...
	Panel::SetSize( w, h );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : r - 
//			g - 
//			b - 
//			a - 
//-----------------------------------------------------------------------------
void CHudPlayerOverlayClass::SetColor( int r, int g, int b, int a )
{
	m_r = r;
	m_g = g;
	m_b = b;
	m_a = a;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudPlayerOverlayClass::Paint( void )
{
	int w, h;

	m_pBaseOverlay->SetColorLevel( this, m_fgColor, m_bgColor );

	GetSize( w, h );
	vgui::surface()->DrawSetColor( m_r, m_g, m_b, m_a * m_pBaseOverlay->GetAlphaFrac() );

	if ( !m_pImage )
		return;

	Color color;
	color.SetColor( m_r, m_g, m_b, m_a * m_pBaseOverlay->GetAlphaFrac() );
	m_pImage->SetColor( color );
	m_pImage->DoPaint( GetVPanel() );
}

void CHudPlayerOverlayClass::OnCursorEntered()
{
	if ( m_pBaseOverlay->GetMouseOverText() )
	{
		StatusPrint( TYPE_HINT, "%s", m_pBaseOverlay->GetMouseOverText() );
	}
}

void CHudPlayerOverlayClass::OnCursorExited()
{
	if ( m_pBaseOverlay->GetMouseOverText() )
	{
		StatusClear();
	}
}