//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's sheild entity
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef C_SHIELD_H
#define C_SHIELD_H

#ifdef _WIN32
#pragma once
#endif

#include "SplinePatch.h"

//-----------------------------------------------------------------------------
// Shield: 
//-----------------------------------------------------------------------------
class C_Shield : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_Shield, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	// constructor, destructor
	C_Shield();
	~C_Shield();

	// Inherited classes should call this in their constructor to indicate size...
	void InitShield( int w, int h, int subdivisions );

	void OnDataChanged( DataUpdateType_t updateType );
	int	DrawModel( int flags );
	void ReceiveMessage( int classID,  bf_read &msg );

	void CreateShieldDeflection( int hitgroup, const Vector &dir, bool partialBlock );

	virtual bool ShouldDraw();
	virtual bool IsTransparent() { return true; }
	virtual void SetAlwaysOrient( bool bOrient ) {}
	virtual bool IsAlwaysOrienting( ) { return false; }
	virtual void SetCenterAngles( const QAngle & ) {}
	virtual void SetAttachmentIndex( int nAttachmentIndex ) {}

	// returns the address of a variable that stores the material animation frame
	float	GetTextureAnimationStartTime();

	// Indicates that a texture animation has wrapped
	void	TextureAnimationWrapped();

	virtual bool	ShouldCollide( int collisionGroup, int contentsMask ) const;

	// Collision detection
	// Activates/deactivates a shield for collision purposes
	void ActivateCollisions( bool activate );

	// Deactivates all shields of players on a particular team
	// If you don't specify a team, it'll affect all shields
	static void ActivateShields( bool activate, int team = -1 );
	virtual const Vector& GetPoint( int x, int y ) { return vec3_origin; }

	// For collision testing
	bool TestCollision( const Ray_t& ray, unsigned int mask, trace_t& trace );

	// Called when we hit something that we deflect...
	void RegisterDeflection(const Vector& vecDir, int bitsDamageType, trace_t *ptr);

	float	GetPowerLevel( void ) { return m_flPowerLevel; }

	bool	IsEMPed() const;
	void	SetEMPed( bool bIsEmped ); 

protected:
	//
	// Inheriting classes must implement these methods!!!
	//

	// Return true if the panel is active 
	virtual bool IsPanelActive( int x, int y )		{ assert(0); return false; }

	// Gets at the control point data; who knows how it was made?
	virtual void GetShieldData( Vector const** ppVerts, float* pOpacity, float* pBlend )	{ assert(0); }

private:
	void	DrawWireframeModel( Vector const** pPositions );
	void	ComputePoint( float s, float t, Vector& pt, Vector& normal, float& opacity );
	void	ComputeShieldPoints( Vector* pt, Vector* normal, float* opacity );
	void	RippleShieldPoints( Vector* pt, float* opacity );
	void	DrawShieldPoints(Vector* pt, Vector* normal, float* opacity);
	void	DrawShieldDecals(Vector* pt, bool hitDecals );
	void	SetCurrentDecal( int idx );

	int		Width() const;
	int		Height() const;

protected:
	// Used to fade out the shield
	float m_FadeValue;
	
	// Used to make the shield more or less curvy
	float m_CurveValue;

private:
	// no copy constructor
	C_Shield( const C_Shield& );

	// Data needs to ripple the shield control points
	struct Ripple_t
	{
		float m_RippleU;
		float m_RippleV;
		float m_Amplitude;
		float m_Radius;
		float m_StartTime;
		Vector m_Direction;
	};

	struct Decal_t
	{
		float	m_RippleU;
		float	m_RippleV;
		float	m_Radius;
		float	m_StartTime;
	};

	// Owner entity
	int		m_nOwningPlayerIndex;

	// number of subdivisions
	int		m_SubdivisionCount;
	float	m_InvSubdivisionCount;

	// Shield powerlevel 
	float	m_flPowerLevel;

	// Texture animation
	float	m_StartTime;

	bool	m_bCollisionsActive;
	bool	m_bIsEMPed;

	// Used to do spline queries
	CSplinePatch	m_SplinePatch;

	// List of all ripples + decals
	int	m_CurrentDecal;
	CUtlVector<	Ripple_t > m_Ripples;
	CUtlVector<	Decal_t > m_Decals;

	// All the various materials we use
	CMaterialReference m_pWireframe;
	CMaterialReference m_pShield;
	CMaterialReference m_pHitDecal;
	CMaterialReference m_pPassDecal;
	CMaterialReference m_pPassDecal2;

	// A little state used only during rendering, but I didn't want
	// to pass these as arguments to a bunch of functions
	bool m_ShieldOwnedByLocalPlayer;
	Vector m_ViewDir;

	// List of all active shields
	static CUtlVector< C_Shield* >	s_Shields;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline int C_Shield::Width() const	
{ 
	return m_SplinePatch.Width(); 
}

inline int C_Shield::Height() const	
{ 
	return m_SplinePatch.Height(); 
}

inline bool C_Shield::IsEMPed() const 
{ 
	return m_bIsEMPed; 
}

inline void C_Shield::SetEMPed( bool bIsEmped ) 
{ 
	m_bIsEMPed = bIsEmped; 
}


//-----------------------------------------------------------------------------
// Class factory methods to create the various versions of the shield
//-----------------------------------------------------------------------------
C_Shield* CreateMobileShield( C_BaseEntity *owner, float flFrontDistance = 0 );

#endif // C_SHIELD_H