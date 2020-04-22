//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_baseobject.h"
#include "ObjectControlPanel.h"
#include "tf_shareddefs.h"
#include "tempent.h"
#include "c_te_legacytempents.h"
#include "iviewrender_beams.h"
#include "beamdraw.h"
#include "view.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define NUM_POWERPACK_GLOWS		6

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ObjectPowerPack : public C_BaseObject
{
	DECLARE_CLASS( C_ObjectPowerPack, C_BaseObject );
public:
	DECLARE_CLIENTCLASS();

	C_ObjectPowerPack();
	~C_ObjectPowerPack();
	int SocketsLeft() const { return (MAX_OBJECTS_PER_PACK - m_iObjectsAttached); }

	// Since we have material proxies to show building amount, don't offset origin
	virtual bool	OffsetObjectOrigin( Vector& origin )
	{
		return false;
	}

	virtual void	OnGoActive( void );
	virtual void	OnGoInactive( void );
	void			RemoveGlows( void );
	virtual void	ClientThink( void );
	virtual int		DrawModel( int flags );

private:
	int					m_iObjectsAttached;
	int					m_iGlowModelIndex;
	C_LocalTempEntity	*m_pGlowSprites[ NUM_POWERPACK_GLOWS ];

	// Jacob's laddder
	Beam_t				*m_pJacobsLadderBeam;
	float				m_flJacobsLeftPoint;
	float				m_flJacobsRightPoint;
	Vector				m_vecJacobsStart;
	Vector				m_vecJacobsEnd;
	CMaterialReference	m_hJacobsPointMaterial;

private:
	C_ObjectPowerPack( const C_ObjectPowerPack & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_ObjectPowerPack, DT_ObjectPowerPack, CObjectPowerPack)
	RecvPropInt( RECVINFO(m_iObjectsAttached) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectPowerPack::C_ObjectPowerPack()
{
	for ( int i = 0; i < NUM_POWERPACK_GLOWS; i++ )
	{
		m_pGlowSprites[i] = NULL;
	}

	m_iGlowModelIndex = PrecacheModel( "effects/human_object_glow.vmt" ); 
	m_pJacobsLadderBeam = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectPowerPack::~C_ObjectPowerPack( void )
{
	RemoveGlows();
}

//-----------------------------------------------------------------------------
// Purpose: We've just gone active
//-----------------------------------------------------------------------------
void C_ObjectPowerPack::OnGoActive( void )
{
	// Turn on our glows
	for ( int i = 0; i < NUM_POWERPACK_GLOWS; i++ )
	{
		// Find the attachment point
		int iAttachment = LookupAttachment( VarArgs("glow_%d",(i+1)) );
		Vector vecOrigin;
		QAngle vecAngles;
		if ( GetAttachment( iAttachment, vecOrigin, vecAngles ) )
		{
			Vector vecForward;
			AngleVectors( vecAngles, &vecForward );
			m_pGlowSprites[i] = tempents->TempSprite( vecOrigin, vec3_origin, 0.35, m_iGlowModelIndex, kRenderTransAdd, 0, 0.5, 1, FTENT_PERSIST | FTENT_NEVERDIE | FTENT_BEOCCLUDED, vecForward );
		}
	}

	m_flJacobsLeftPoint = 0;
	m_flJacobsRightPoint = 0;
	m_hJacobsPointMaterial.Init( "sprites/blueflare2", TEXTURE_GROUP_CLIENT_EFFECTS );
}

//-----------------------------------------------------------------------------
// Purpose: We've just gone inactive
//-----------------------------------------------------------------------------
void C_ObjectPowerPack::OnGoInactive( void )
{
	// Turn off our glows
	RemoveGlows();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectPowerPack::RemoveGlows( void )
{
	for ( int i = 0; i < NUM_POWERPACK_GLOWS; i++ )
	{
		if ( m_pGlowSprites[i] )
		{
			m_pGlowSprites[i]->die = 0;
			m_pGlowSprites[i] = NULL;
		}
	}

	// Stop the jacob's ladder
	if ( m_pJacobsLadderBeam )
	{
		m_pJacobsLadderBeam->flags &= ~FBEAM_FOREVER;
		m_pJacobsLadderBeam->die = gpGlobals->curtime;
		m_pJacobsLadderBeam = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectPowerPack::ClientThink( void )
{
	// Create the jacob's ladder
	if ( !m_pJacobsLadderBeam )
	{
		BeamInfo_t beamInfo;
		beamInfo.m_vecStart.Init();
		beamInfo.m_vecEnd.Init();
		beamInfo.m_pszModelName = "sprites/physbeam.vmt";
		beamInfo.m_flHaloScale = 0.0f;
		beamInfo.m_flLife = 0.0f;
		beamInfo.m_flWidth = 8.0f;
		beamInfo.m_flEndWidth = 4.0f;
		beamInfo.m_flFadeLength = 0.0f;
		beamInfo.m_flAmplitude = 20.0f;
		beamInfo.m_flBrightness = 255.0f;
		beamInfo.m_flSpeed = 0.0f;
		beamInfo.m_nStartFrame = 0;
		beamInfo.m_flFrameRate = 0.0f;
		beamInfo.m_flRed = 206.0f;
		beamInfo.m_flGreen = 181.0f;
		beamInfo.m_flBlue = 127.0f;
		beamInfo.m_nSegments = 5;
		beamInfo.m_bRenderable = true;
		m_pJacobsLadderBeam = beams->CreateBeamPoints( beamInfo );
	}

	// Update the position of the jacob's ladder
	BeamInfo_t beamInfo;
	QAngle vecAngle;
	int iAttachment;

	// Setup a color reflecting the amount of power being used
	color32 color;
	color.r = 206;
	color.g = 182;
	color.b = 127;
	color.a = 255;

	// Tesla Effect
	Vector vecRightTop, vecRightBottom;
	Vector vecLeftTop, vecLeftBottom;
	iAttachment = LookupAttachment( "Tesla_ll" );
	GetAttachment( iAttachment, vecLeftBottom, vecAngle );
	iAttachment = LookupAttachment( "Tesla_ul" );
	GetAttachment( iAttachment, vecLeftTop, vecAngle );
	iAttachment = LookupAttachment( "Tesla_lr" );
	GetAttachment( iAttachment, vecRightBottom, vecAngle );
	iAttachment = LookupAttachment( "Tesla_ur" );
	GetAttachment( iAttachment, vecRightTop, vecAngle );

	float flSpeed = 0.02;

	m_flJacobsLeftPoint += random->RandomFloat( flSpeed * 0.25, flSpeed * 2);
	m_flJacobsRightPoint += random->RandomFloat( flSpeed * 0.25, flSpeed * 2);

	// If they've both hit the end, break the ladder
	if ( m_flJacobsLeftPoint >= 1.0f && m_flJacobsRightPoint >= 1.0f )
	{
		// Snap!
		m_flJacobsLeftPoint = 0.0f; 
		m_flJacobsRightPoint = 0.0f; 
	}
	else if ( m_flJacobsLeftPoint > 1.0f ) 
	{ 
		// Only the left point's made it
		m_flJacobsLeftPoint = 1.0f; 
	}
	else if ( m_flJacobsRightPoint > 1.0f ) 
	{ 
		// Only the right point's made it
		m_flJacobsRightPoint = 1.0f; 
	}

	Vector vecLeft = vecLeftTop - vecLeftBottom;
	Vector vecRight = vecRightTop - vecRightBottom;
	m_vecJacobsStart = vecLeftBottom + ( m_flJacobsLeftPoint * vecLeft );
	m_vecJacobsEnd = vecRightBottom + ( m_flJacobsRightPoint * vecRight );

	beamInfo.m_vecStart = m_vecJacobsStart;
	beamInfo.m_vecEnd = m_vecJacobsEnd;
	beamInfo.m_flRed = color.r;
	beamInfo.m_flGreen = color.g;
	beamInfo.m_flBlue = color.b;
	beams->UpdateBeamInfo( m_pJacobsLadderBeam, beamInfo );	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_ObjectPowerPack::DrawModel( int flags )
{
	if ( BaseClass::DrawModel( flags ) )
	{
		if ( ShouldBeActive() )
		{
			// Get the distance to the view
			float flDistance = (GetAbsOrigin() - MainViewOrigin()).LengthSqr();
			if ( flDistance < (1024 * 1024) )
			{
				// Draw a sprite at the tips.
				color32 color;
				color.r = 255;
				color.g = 255;
				color.b = 255;
				color.a = 255;
				float flSize = 25.0f;
				materials->Bind( m_hJacobsPointMaterial, this );
				DrawSprite( m_vecJacobsStart, flSize, flSize, color );
				DrawSprite( m_vecJacobsEnd, flSize, flSize, color );
			}
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CPowerPackControlPanel : public CObjectControlPanel
{
	DECLARE_CLASS( CPowerPackControlPanel, CObjectControlPanel );

public:
	CPowerPackControlPanel( vgui::Panel *parent, const char *panelName );
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();

private:
	vgui::Label *m_pSocketsLabel;
};


DECLARE_VGUI_SCREEN_FACTORY( CPowerPackControlPanel, "powerpack_control_panel" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CPowerPackControlPanel::CPowerPackControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CPowerPackControlPanel" ) 
{
}


//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CPowerPackControlPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	m_pSocketsLabel = new vgui::Label( GetActivePanel(), "SocketReadout", "" );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CPowerPackControlPanel::OnTick()
{
	BaseClass::OnTick();

	C_BaseObject *pObj = GetOwningObject();
	if (!pObj)
		return;

	Assert( dynamic_cast<C_ObjectPowerPack*>(pObj) );
	C_ObjectPowerPack *pPowerPack = static_cast<C_ObjectPowerPack*>(pObj);

	char buf[256];
	int nSocketsLeft = pPowerPack->SocketsLeft();
	if (nSocketsLeft > 0)
	{
		Q_snprintf( buf, sizeof( buf ), "%d sockets left", pPowerPack->SocketsLeft() );
	}
	else
	{
		Q_strncpy( buf, "No sockets left", sizeof( buf ) );
	}

	m_pSocketsLabel->SetText( buf );
}


