//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's sheild entity
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "C_Shield.h"
#include "tf_shieldshared.h"

enum
{
	NUM_SUBDIVISIONS = 21,
};

#define EMP_WAVE_AMPLITUDE 8.0f

//-----------------------------------------------------------------------------
// Mobile version of the shield 
//-----------------------------------------------------------------------------

class C_ShieldMobile;
class C_ShieldMobileActiveVertList : public IActiveVertList
{
public:
	void			Init( C_ShieldMobile *pShield, unsigned char *pVertList );

// IActiveVertList overrides.
public:

	virtual int		GetActiveVertState( int iVert );
	virtual void	SetActiveVertState( int iVert, int bOn );

private:
	C_ShieldMobile	*m_pShield;
	unsigned char	*m_pVertsActive;
};


class C_ShieldMobile : public C_Shield
{
	DECLARE_CLASS( C_ShieldMobile, C_Shield );
public:
	DECLARE_CLIENTCLASS();

	C_ShieldMobile();
	~C_ShieldMobile();

	void OnDataChanged( DataUpdateType_t updateType );
	virtual void GetBounds( Vector& mins, Vector& maxs );

	virtual void AddEntity( );

	// Return true if the panel is active 
	virtual bool IsPanelActive( int x, int y );

	// Gets at the control point data; who knows how it was made?
	virtual void GetShieldData( Vector const** ppVerts, float* pOpacity, float* pBlend );
	virtual const Vector& GetPoint( int x, int y ) { return m_ShieldEffect.GetPoint( x, y ); }

	virtual void SetThetaPhi( float flTheta, float flPhi ) { m_ShieldEffect.SetThetaPhi(flTheta,flPhi); }

public:
	// networked data
	unsigned char	m_pVertsActive[SHIELD_VERTEX_BYTES];
	unsigned char	m_ShieldState;

private:
	C_ShieldMobile( const C_ShieldMobile& );

	// Is a particular panel an edge?
	bool IsVertexValid( float s, float t ) const;
	void PreRender( );

private:
	CShieldEffect					m_ShieldEffect;
	C_ShieldMobileActiveVertList	m_VertList;
	float m_flTheta;
	float m_flPhi;
};


//-----------------------------------------------------------------------------
// C_ShieldMobileActiveVertList functions
//-----------------------------------------------------------------------------

void C_ShieldMobileActiveVertList::Init( C_ShieldMobile *pShield, unsigned char *pVertList )
{
	m_pShield = pShield;
	m_pVertsActive = pVertList;
}


int C_ShieldMobileActiveVertList::GetActiveVertState( int iVert )
{
	return m_pVertsActive[iVert>>3] & (1 << (iVert & 7));
}


void C_ShieldMobileActiveVertList::SetActiveVertState( int iVert, int bOn )
{
	if ( bOn )
		m_pVertsActive[iVert>>3] |= (1 << (iVert & 7));
	else
		m_pVertsActive[iVert>>3] &= ~(1 << (iVert & 7));
}


//-----------------------------------------------------------------------------
// Data table
//-----------------------------------------------------------------------------

IMPLEMENT_CLIENTCLASS_DT(C_ShieldMobile, DT_Shield_Mobile, CShieldMobile)

	RecvPropInt( RECVINFO(m_ShieldState) ),
	RecvPropArray( 
		RecvPropInt( RECVINFO(m_pVertsActive[0])), 
		m_pVertsActive 
	),
	RecvPropFloat( RECVINFO(m_flTheta) ),
	RecvPropFloat( RECVINFO(m_flPhi) ),

END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Various raycasting routines
//-----------------------------------------------------------------------------


void ShieldTraceLine(const Vector &vecStart, const Vector &vecEnd, 
					 unsigned int mask, int collisionGroup, trace_t *ptr)
{
	UTIL_TraceLine(vecStart, vecEnd, mask, NULL, collisionGroup, ptr );
}

void ShieldTraceHull(const Vector &vecStart, const Vector &vecEnd, 
					 const Vector &hullMin, const Vector &hullMax, 
					 unsigned int mask, int collisionGroup, trace_t *ptr)
{
	CTraceFilterWorldOnly traceFilter;
	enginetrace->TraceHull( vecStart, vecEnd, hullMin, hullMax, mask, &traceFilter, ptr );
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------

C_ShieldMobile::C_ShieldMobile() : m_ShieldEffect(ShieldTraceLine, ShieldTraceHull)
{
	m_VertList.Init( this, m_pVertsActive );
	m_ShieldEffect.SetActiveVertexList( &m_VertList );
	m_ShieldEffect.Spawn(vec3_origin, vec3_angle);
	InitShield( SHIELD_NUM_HORIZONTAL_POINTS, SHIELD_NUM_VERTICAL_POINTS, NUM_SUBDIVISIONS );
}

C_ShieldMobile::~C_ShieldMobile()
{
}

//-----------------------------------------------------------------------------
// Get this after the data changes
//-----------------------------------------------------------------------------

void C_ShieldMobile::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	m_ShieldEffect.SetCurrentPosition( GetAbsOrigin() );
	m_ShieldEffect.SetCurrentAngles( GetAbsAngles() );
	m_ShieldEffect.SetThetaPhi( m_flTheta, m_flPhi );

	// No need to simulate, just compute active panels from network data
	m_ShieldEffect.ComputeControlPoints();
	m_ShieldEffect.ComputePanelActivity();
}

//-----------------------------------------------------------------------------
// A little pre-render processing
//-----------------------------------------------------------------------------

void C_ShieldMobile::PreRender( )
{
	if (m_ShieldState & SHIELD_MOBILE_EMP)
	{
		// Decay fade if we've been EMPed or if we're inactive
		if (m_FadeValue > 0.0f)
		{
			m_FadeValue -= gpGlobals->frametime / SHIELD_EMP_FADE_TIME;
			if (m_FadeValue < 0.0f)
			{
				m_FadeValue = 0.0f;

				// Reset the shield to un-wobbled state
				m_ShieldEffect.ComputeControlPoints();
			}
			else
			{
				Vector dir;
				AngleVectors( m_ShieldEffect.GetCurrentAngles(), & dir );

				// Futz with the control points if we've been EMPed
				for (int i = 0; i < SHIELD_NUM_CONTROL_POINTS; ++i)
				{
					// Get the direction for the point
					float factor = -EMP_WAVE_AMPLITUDE * sin( i * M_PI * 0.5f + gpGlobals->curtime * M_PI / SHIELD_EMP_WOBBLE_TIME );
					m_ShieldEffect.GetPoint(i) += dir * factor;
				}
			}
		}
	}
	else
	{
		// Fade back in, no longer EMPed
		if (m_FadeValue < 1.0f)
		{
			m_FadeValue += gpGlobals->frametime / SHIELD_EMP_FADE_TIME;
			if (m_FadeValue >= 1.0f)
			{
				m_FadeValue = 1.0f;
			}
		}
	}
}

void C_ShieldMobile::AddEntity( )
{
	BaseClass::AddEntity( );
	PreRender();
}

//-----------------------------------------------------------------------------
// Bounds computation
//-----------------------------------------------------------------------------

void C_ShieldMobile::GetBounds( Vector& mins, Vector& maxs )
{
	m_ShieldEffect.ComputeBounds( mins, maxs );
}

//-----------------------------------------------------------------------------
// Return true if the panel is active 
//-----------------------------------------------------------------------------

bool C_ShieldMobile::IsPanelActive( int x, int y )
{
	return m_ShieldEffect.IsPanelActive(x, y);
}

//-----------------------------------------------------------------------------
// Gets at the control point data; who knows how it was made?
//-----------------------------------------------------------------------------

void C_ShieldMobile::GetShieldData( Vector const** ppVerts, float* pOpacity, float* pBlend )
{
	for ( int i = 0; i < SHIELD_NUM_CONTROL_POINTS; ++i )
	{
		ppVerts[i] = &m_ShieldEffect.GetControlPoint(i);

		if ( m_pVertsActive[i >> 3] & (1 << (i & 0x7)) )
		{
			pOpacity[i] = m_ShieldEffect.ComputeOpacity( *ppVerts[i], GetAbsOrigin() );
			pBlend[i] = 1.0f;
		}
		else
		{
			pOpacity[i] = 192.0f;
			pBlend[i] = 0.0f;
		}
	}
}

