//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's sheild entity
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "C_Shield.h"
#include "tf_shieldshared.h"
#include "c_basetfplayer.h"

enum
{
	NUM_SUBDIVISIONS = 21,
};

#define EMP_WAVE_AMPLITUDE 6.0f
#define EMP_GROW_WIDTH_DELAY 0.2f
#define EMP_GROW_TIME 0.6f
#define EMP_GROW_ATTEN 0.5f
#define EMP_MIN_WIDTH 5.0f

//-----------------------------------------------------------------------------
// Flat version of the shield 
//-----------------------------------------------------------------------------

class C_ShieldFlat : public C_Shield
{
public:
	DECLARE_CLASS( C_ShieldFlat, C_Shield );
	DECLARE_CLIENTCLASS();

	C_ShieldFlat();
	~C_ShieldFlat();

	virtual void GetBounds( Vector& mins, Vector& maxs );

	virtual void AddEntity( );

	virtual void SetDormant( bool bDormant );

	// Return true if the panel is active 
	virtual bool IsPanelActive( int x, int y );

	// Gets at the control point data; who knows how it was made?
	virtual void GetShieldData( Vector const** ppVerts, float* pOpacity, float* pBlend );
	virtual const Vector& GetPoint( int x, int y );

	// Draws the model
	virtual int	DrawModel( int flags );

public:
	// networked data
	unsigned char	m_ShieldState;
	float			m_Width;
	float			m_Height;

	float			m_DeathFade;
	float			m_EMPFade;

private:
	void	ShieldMoved( void );

private:
	C_ShieldFlat( const C_ShieldFlat& );
	void ComputeEMPFade();
	void ComputeDeathFade();
	void ComputeSize( float& w, float& h );
	void PreRender( );

	Vector			m_pPositions[4];
	Vector			m_Forward;
	float			m_EnterPVSTime;

	QAngle	m_LastAngles;
	Vector	m_LastPosition;
	Vector	m_Pos[4];
};


//-----------------------------------------------------------------------------
// Data table
//-----------------------------------------------------------------------------

IMPLEMENT_CLIENTCLASS_DT(C_ShieldFlat, DT_Shield_Flat, CShieldFlat)

	RecvPropInt( RECVINFO(m_ShieldState) ),
	RecvPropFloat( RECVINFO(m_Width) ),
	RecvPropFloat( RECVINFO(m_Height) ),

END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------

C_ShieldFlat::C_ShieldFlat()
{
	m_DeathFade = 1.0f;
	m_EMPFade = 1.0f;

	InitShield( 2, 2, 6 );
}

C_ShieldFlat::~C_ShieldFlat()
{
}

//-----------------------------------------------------------------------------
// Leaving/entering the PVS on the server.
//-----------------------------------------------------------------------------

void C_ShieldFlat::SetDormant( bool bDormant )
{
	if (!bDormant)
	{
		if (m_ShieldState & SHIELD_FLAT_EMP)
		{
			m_EMPFade = 0.0f;
		}
		else
		{
			m_EMPFade = 1.0f;
		}

		m_EnterPVSTime = 0.0f;
		if (m_ShieldState & SHIELD_FLAT_INACTIVE)
		{
			m_DeathFade = 0.0f;
		}
		else
		{
			m_EnterPVSTime = gpGlobals->curtime;
			m_DeathFade = 1.0f;
		}
	}

	BaseClass::SetDormant(bDormant);
}

//-----------------------------------------------------------------------------
// Figures the EMP fade factor
//-----------------------------------------------------------------------------

void C_ShieldFlat::ComputeEMPFade()
{
	if (m_ShieldState & SHIELD_FLAT_EMP)
	{
		// Decay fade if we've been EMPed or if we're inactive
		if (m_EMPFade > 0.0f)
		{
			m_EMPFade -= gpGlobals->frametime / SHIELD_EMP_FADE_TIME;
			if (m_EMPFade < 0.0f)
			{
				m_EMPFade = 0.0f;
			}
			else
			{
				Vector dir;

				// Futz with the control points if we've been EMPed
				for (int i = 0; i < 4; ++i)
				{
					float dist = -EMP_WAVE_AMPLITUDE * sin( i * M_PI * 0.5f + gpGlobals->curtime * M_PI / SHIELD_EMP_WOBBLE_TIME );
					VectorMA( m_pPositions[i], dist, m_Forward, m_pPositions[i] );
				}
			}
		}
	}
	else
	{
		// Fade back in, no longer EMPed
		if (m_EMPFade < 1.0f)
		{
			m_EMPFade += gpGlobals->frametime / SHIELD_EMP_FADE_TIME;
			if (m_EMPFade >= 1.0f)
			{
				m_EMPFade = 1.0f;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Figures the networked fade factor
//-----------------------------------------------------------------------------

void C_ShieldFlat::ComputeDeathFade()
{
	if (m_ShieldState & SHIELD_FLAT_INACTIVE)
	{
		// Fade out when we become inactive
		if (m_DeathFade > 0.0f)
		{
			m_DeathFade -= gpGlobals->frametime / SHIELD_FLAT_SHUTDOWN_TIME;
			if (m_DeathFade < 0.0f)
			{
				m_DeathFade = 0.0f;
			}
		}
	}
	else
	{
		// Active? We should be visible
		m_DeathFade = 1.0f;
	}
}

//-----------------------------------------------------------------------------
// A little pre-render processing
//-----------------------------------------------------------------------------

void C_ShieldFlat::ComputeSize( float& w, float& h )
{
	w = m_Width;
	h = m_Height;

	float dt = gpGlobals->curtime - m_EnterPVSTime;
	if (dt > EMP_GROW_TIME)
	{
		return;
	}

	if (dt < 0)
		dt = 0.0f;

	// Attenuate it up
	w *= 1.0f - pow ( EMP_GROW_ATTEN, 10 * (dt / EMP_GROW_TIME ));

	if (w < EMP_MIN_WIDTH)
		w = EMP_MIN_WIDTH;
}

//-----------------------------------------------------------------------------
// A little pre-render processing
//-----------------------------------------------------------------------------

void C_ShieldFlat::PreRender( )
{
	// Compute the shield positions...
	Vector right, up;
	AngleVectors( GetRenderAngles(), &m_Forward, &right, &up );

	float w, h;
	ComputeSize( w, h );

	VectorMA( GetRenderOrigin(), -w * 0.5, right, m_pPositions[0] );
	VectorMA( m_pPositions[0], -h * 0.5, up, m_pPositions[0] );
	VectorMA( m_pPositions[0], w, right, m_pPositions[1] );
	VectorMA( m_pPositions[0], h, up, m_pPositions[2] );
	VectorMA( m_pPositions[2], w, right, m_pPositions[3] );

	ComputeEMPFade();
	ComputeDeathFade();

	m_FadeValue = m_DeathFade * m_EMPFade;
}

void C_ShieldFlat::AddEntity( )
{
	BaseClass::AddEntity( );
	PreRender();
}

//-----------------------------------------------------------------------------
// Bounds computation
//-----------------------------------------------------------------------------

void C_ShieldFlat::GetBounds( Vector& mins, Vector& maxs )
{
	mins.Init( -1.0/16.0f, -m_Width * 0.5f, -m_Height * 0.5f );
	maxs.Init(  1.0/16.0f,  m_Width * 0.5f,  m_Height * 0.5f );
}

//-----------------------------------------------------------------------------
// Return true if the panel is active 
//-----------------------------------------------------------------------------

bool C_ShieldFlat::IsPanelActive( int x, int y )
{
	return true;
}

//-----------------------------------------------------------------------------
// Gets at the control point data; who knows how it was made?
//-----------------------------------------------------------------------------

void C_ShieldFlat::GetShieldData( Vector const** ppVerts, float* pOpacity, float* pBlend )
{
	for ( int i = 0; i < 4; ++i )
	{
		ppVerts[i] = &m_pPositions[i];
		pOpacity[i] = 32.0f;
		pBlend[i] = 0.0f;
	}
}

//-----------------------------------------------------------------------------
// Shield points
//-----------------------------------------------------------------------------
const Vector& C_ShieldFlat::GetPoint( int x, int y ) 
{
	if ((m_LastAngles != GetAbsAngles()) || (m_LastPosition != GetAbsOrigin() ))
	{
		ShieldMoved();
	}

	int i = (x >= 1);
	i += (y >= 1) * 2;
	return m_Pos[i];
}

//-----------------------------------------------------------------------------
// Purpose: Computes the shield bounding box
//-----------------------------------------------------------------------------
void C_ShieldFlat::ShieldMoved( void )
{
	Vector forward, right, up;
	AngleVectors( GetAbsAngles(), &forward, &right, &up );

	VectorMA( GetAbsOrigin(), -m_Width * 0.5, right, m_Pos[0] );
	VectorMA( m_Pos[0], -m_Height * 0.5, up, m_Pos[0] );
	VectorMA( m_Pos[0], m_Width, right, m_Pos[1] );
	VectorMA( m_Pos[0], m_Height, up, m_Pos[2] );
	VectorMA( m_Pos[2], m_Width, right, m_Pos[3] );

	m_LastAngles = GetAbsAngles();
	m_LastPosition = GetAbsOrigin();
}

//-----------------------------------------------------------------------------
// Suppress rendering if the player owns it
//-----------------------------------------------------------------------------

int	C_ShieldFlat::DrawModel( int flags )
{
	if ( !m_bReadyToDraw )
		return 0;

	// Don't draw it if the owner is the local player
//	if ( m_OwnerEntity == C_BasePlayer::GetLocalPlayer()->index )
//		return 0;

	return BaseClass::DrawModel( flags );
}

