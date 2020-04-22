//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include "c_basetfplayer.h"
#include "tf_shareddefs.h"
#include "iclientmode.h"
#include "clientmode_tfnormal.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialvar.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include "clienteffectprecachesystem.h"

// EMP level positions
#define HUDEMP_LEFT					((ScreenWidth() / 2)- XRES(40))
#define HUDEMP_WIDTH				XRES(16)
#define HUDEMP_TOP					YRES(210)
#define HUDEMP_HEIGHT				YRES(16)

// Changes per second
#define HUDEMP_FRAMERATE			30.0f

CLIENTEFFECT_REGISTER_BEGIN( PrecacheHudDamage )
CLIENTEFFECT_MATERIAL( "Hud/emp/emp_damage" )
CLIENTEFFECT_REGISTER_END()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool GetEMPDamage()
{
	C_BaseTFPlayer *pPlayer;
	pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	return pPlayer->HasPowerup(POWERUP_EMP) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: Icon showing EMP damage is occuring
//-----------------------------------------------------------------------------
class CHudEMP : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudEMP, vgui::Panel );

public:
	DECLARE_MULTIPLY_INHERITED();

	CHudEMP( const char *pElementName );
	~CHudEMP();

	// vgui::Panel overrides.
	virtual void Paint( void );

	// CHudElement overrides.
	virtual bool ShouldDraw( void );
	virtual void Init( void );

protected:
	IMaterial		*m_pEMPIcon;
	IMaterialVar	*m_pFrameVar;

	float			m_flNextFrameChange;
	int				m_nNumFrames;
};

//DECLARE_HUDELEMENT( CHudEMP, CHudEMP );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudEMP::CHudEMP( const char *pElementName ) :
	CHudElement( pElementName ), vgui::Panel( NULL, pElementName )
{
	m_pEMPIcon = NULL;
	m_pFrameVar = NULL;
	m_flNextFrameChange = 0.0f;
	m_nNumFrames = 1;
	//SetPaintBackgroundEnabled( false );
	SetAutoDelete( false );
	SetName( "emp" );
	
	SetHiddenBits( HIDEHUD_HEALTH );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudEMP::~CHudEMP( void )
{
	if ( m_pEMPIcon )
	{
		m_pEMPIcon->DecrementReferenceCount();
		m_pEMPIcon = NULL;
	}
	SetParent( (vgui::Panel *)NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Attach the jetpack bar to the main panel
//-----------------------------------------------------------------------------
void CHudEMP::Init( void )
{
	if ( !m_pEMPIcon )
	{
		m_pEMPIcon = materials->FindMaterial( "Hud/emp/emp_damage", TEXTURE_GROUP_VGUI );
		assert( m_pEMPIcon );
		m_pEMPIcon->IncrementReferenceCount();

		m_pFrameVar = m_pEMPIcon->FindVar( "$frame", NULL, false );
		m_nNumFrames = m_pEMPIcon->GetNumAnimationFrames();
	}

	SetPos( HUDEMP_LEFT, HUDEMP_TOP );
	SetSize( HUDEMP_WIDTH, HUDEMP_HEIGHT );

	vgui::Panel *pParent = GetClientModeNormal()->GetViewport();
	SetParent(pParent);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudEMP::ShouldDraw( void )
{
	return ( CHudElement::ShouldDraw() && GetEMPDamage() );
}

//-----------------------------------------------------------------------------
// Purpose: Draw the jetpack level
//-----------------------------------------------------------------------------
void CHudEMP::Paint()
{
	// Rush label
	int iX, iY;
	GetPos( iX, iY );
	int iWidth = XRES(16);
	int iHeight = YRES(16);

	if ( m_pFrameVar )
	{
		float curtime = gpGlobals->curtime;

		if ( curtime >= m_flNextFrameChange )
		{
			m_flNextFrameChange = curtime + ( 1.0f / HUDEMP_FRAMERATE );

			int frame = m_pFrameVar->GetIntValue();
			frame++;
			if ( frame >= m_nNumFrames )
			{
				frame = 0;
			}
			m_pFrameVar->SetIntValue(frame);
		}
	}

	IMesh* pMesh = materials->GetDynamicMesh( true, NULL, NULL, m_pEMPIcon );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Color3f( 1.0, 1.0, 1.0 );
	meshBuilder.TexCoord2f( 0,0,0 );
	meshBuilder.Position3f( iX,iY,0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3f( 1.0, 1.0, 1.0 );
	meshBuilder.TexCoord2f( 0,1,0 );
	meshBuilder.Position3f( iX+iWidth, iY, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3f( 1.0, 1.0, 1.0 );
	meshBuilder.TexCoord2f( 0,1,1 );
	meshBuilder.Position3f( iX+iWidth, iY+iHeight, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3f( 1.0, 1.0, 1.0 );
	meshBuilder.TexCoord2f( 0,0,1 );
	meshBuilder.Position3f( iX, iY+iHeight, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

