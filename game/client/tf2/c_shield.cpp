//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's sheild entity
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "C_Shield.h"
#include "clienteffectprecachesystem.h"
#include "clientmode.h"
#include "materialsystem/imesh.h"
#include "mapdata.h"
#include "ivrenderview.h"
#include "tf_shareddefs.h"
#include "collisionutils.h"
#include "functionproxy.h"

// Precache the effects
CLIENTEFFECT_REGISTER_BEGIN( Shield )
CLIENTEFFECT_MATERIAL( "shadertest/wireframevertexcolor" )
CLIENTEFFECT_MATERIAL( "effects/shield/shield" )
CLIENTEFFECT_MATERIAL( "effects/shieldhit" )
CLIENTEFFECT_MATERIAL( "effects/shieldpass" )
CLIENTEFFECT_MATERIAL( "effects/shieldpass2" )
CLIENTEFFECT_REGISTER_END()

//-----------------------------------------------------------------------------
// Stores a list of all active shields
//-----------------------------------------------------------------------------
CUtlVector< C_Shield* >	C_Shield::s_Shields;


//-----------------------------------------------------------------------------
// Various important constants: 
//-----------------------------------------------------------------------------

#define SHIELD_DAMAGE_CHANGE_FIRST_PASS_TIME		0.3f
#define SHIELD_DAMAGE_CHANGE_TRANSITION_TIME		0.5f
#define SHIELD_DAMAGE_CHANGE_TRANSITION_START_TIME	(SHIELD_DAMAGE_CHANGE_TIME -  SHIELD_DAMAGE_CHANGE_TRANSITION_TIME)
#define SHIELD_DAMAGE_CHANGE_TOTAL_TIME	(SHIELD_DAMAGE_CHANGE_TRANSITION_START_TIME + SHIELD_DAMAGE_CHANGE_TRANSITION_TIME)
#define SHIELD_TRANSITION_MAX_BLEND_AMT				0.2f


//-----------------------------------------------------------------------------
// Data table
//-----------------------------------------------------------------------------
//EXTERN_RECV_TABLE(DT_BaseEntity);

IMPLEMENT_CLIENTCLASS_DT(C_Shield, DT_Shield, CShield)
	RecvPropInt( RECVINFO(m_nOwningPlayerIndex) ),
	RecvPropFloat( RECVINFO(m_flPowerLevel) ),
	RecvPropInt( RECVINFO(m_bIsEMPed) ),
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Shield color for the various protection types
//-----------------------------------------------------------------------------
static unsigned char s_ImpactDecalColor[3] = {   0,   0, 255 };


// ----------------------------------------------------------------------------
// Functions.
// ----------------------------------------------------------------------------
C_Shield::C_Shield()
{
	m_pWireframe.Init( "shadertest/wireframevertexcolor", TEXTURE_GROUP_OTHER );
	m_pShield.Init( "effects/shield/shield", TEXTURE_GROUP_CLIENT_EFFECTS );
	m_pHitDecal.Init( "effects/shieldhit", TEXTURE_GROUP_CLIENT_EFFECTS );
	m_pPassDecal.Init( "effects/shieldpass", TEXTURE_GROUP_CLIENT_EFFECTS );
	m_pPassDecal2.Init( "effects/shieldpass2", TEXTURE_GROUP_CLIENT_EFFECTS );
	m_FadeValue = 1.0f;
	m_CurveValue = 1.0f;
	m_bCollisionsActive = true;

	s_Shields.AddToTail(this);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_Shield::~C_Shield()
{
	int i = s_Shields.Find(this);
	if ( i >= 0 )
	{
		s_Shields.FastRemove(i);
	}
}

//-----------------------------------------------------------------------------
// Inherited classes should call this in their constructor to indicate size...
//-----------------------------------------------------------------------------
void C_Shield::InitShield( int w, int h, int subdivisions )
{
	m_SplinePatch.Init( w, h, 2 );

	m_SubdivisionCount = subdivisions;
	Assert( m_SubdivisionCount > 1 );
	m_InvSubdivisionCount = 1.0f / (m_SubdivisionCount - 1);
}

//-----------------------------------------------------------------------------
// This is called after a network update 
//-----------------------------------------------------------------------------
void C_Shield::OnDataChanged( DataUpdateType_t updateType )
{
	if (updateType == DATA_UPDATE_CREATED)
	{
		m_StartTime = engine->GetLastTimeStamp();
	}
	
	BaseClass::OnDataChanged( updateType );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : collisionGroup - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_Shield::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	return m_bCollisionsActive && ((collisionGroup == TFCOLLISION_GROUP_WEAPON) || (collisionGroup == TFCOLLISION_GROUP_GRENADE));
}

//-----------------------------------------------------------------------------
// Should I draw?
//-----------------------------------------------------------------------------
bool C_Shield::ShouldDraw()
{
	// Let the client mode (like commander mode) reject drawing entities.
	if (g_pClientMode && !g_pClientMode->ShouldDrawEntity(this) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Activates/deactivates a shield for collision purposes
//-----------------------------------------------------------------------------
void C_Shield::ActivateCollisions( bool activate )
{
	m_bCollisionsActive = activate;
}

//-----------------------------------------------------------------------------
// Activates all shields 
//-----------------------------------------------------------------------------
void C_Shield::ActivateShields( bool activate, int team )
{
	for (int i = s_Shields.Count(); --i >= 0; )
	{
		// Activate all shields on the same team
		if ( (team == -1) || (team == s_Shields[i]->GetTeamNumber()) )
		{
			s_Shields[i]->ActivateCollisions( activate );
		}
	}
}

//-----------------------------------------------------------------------------
// Helper method for collision testing
//-----------------------------------------------------------------------------
#pragma warning ( disable : 4701 )

bool C_Shield::TestCollision( const Ray_t& ray, unsigned int mask, trace_t& trace )
{
	// Can't block anything if we're EMPed, or we've got no power left to block
	if ( m_bIsEMPed )
		return false;
	if ( m_flPowerLevel <= 0 )
		return false;

	// Here, we're gonna test for collision.
	// If we don't stop this kind of bullet, we'll generate an effect here
	// but we won't change the trace to indicate a collision.

	// It's just polygon soup...
	int hitgroup;
	bool firstTri;
	int v1[2], v2[2], v3[2];
	float ihit, jhit;
	float mint = FLT_MAX;
	float t;

	int h = Height();
	int w = Width();

	for (int i = 0; i < h - 1; ++i)
	{
		for (int j = 0; j < w - 1; ++j)
		{
			// Don't test if this panel ain't active...
			if (!IsPanelActive( j, i ))
				continue;

			// NOTE: Structure order of points so that our barycentric
			// axes for each triangle are along the (u,v) directions of the mesh
			// The barycentric coords we'll need below 

			// Two triangles per quad...
			t = IntersectRayWithTriangle( ray,
				GetPoint( j, i + 1 ),
				GetPoint( j + 1, i + 1 ),
				GetPoint( j, i ), true );
			if ((t >= 0.0f) && (t < mint))
			{
				mint = t;
				v1[0] = j;		v1[1] = i + 1;
				v2[0] = j + 1;	v2[1] = i + 1;
				v3[0] = j;		v3[1] = i;
				ihit = i; jhit = j;
				firstTri = true;
			}
			
			t = IntersectRayWithTriangle( ray,
				GetPoint( j + 1, i ),
				GetPoint( j, i ),
				GetPoint( j + 1, i + 1 ), true );
			if ((t >= 0.0f) && (t < mint))
			{
				mint = t;
				v1[0] = j + 1;	v1[1] = i;
				v2[0] = j;		v2[1] = i;
				v3[0] = j + 1;	v3[1] = i + 1;
				ihit = i; jhit = j;
				firstTri = false;
			}
		}
	}

	if (mint == FLT_MAX)
		return false;

	// Stuff the barycentric coordinates of the triangle hit into the hit group 
	// For the first triangle, the first edge goes along u, the second edge goes
	// along -v. For the second triangle, the first edge goes along -u,
	// the second edge goes along v.
	const Vector& v1vec = GetPoint(v1[0], v1[1]);
	const Vector& v2vec = GetPoint(v2[0], v2[1]);
	const Vector& v3vec = GetPoint(v3[0], v3[1]);
	float u, v;
	bool ok = ComputeIntersectionBarycentricCoordinates( ray, 
		v1vec, v2vec, v3vec, u, v );
	Assert( ok );
	if ( !ok )
	{
		return false;
	}

	if (firstTri)
		v = 1.0 - v;
	else
		u = 1.0 - u;
	v += ihit; u += jhit;
	v /= (h - 1);
	u /= (w - 1);

	// Compress (u,v) into 1 dot 15, v in top bits
	hitgroup = (((int)(v * (1 << 15))) << 16) + (int)(u * (1 << 15));

	Vector normal;
	float intercept;
	ComputeTrianglePlane( v1vec, v2vec, v3vec, normal, intercept ); 

	UTIL_SetTrace( trace, ray, this, mint, hitgroup, CONTENTS_SOLID, normal, intercept );
	return true;
}

#pragma warning ( default : 4701 )

//-----------------------------------------------------------------------------
// Called when we hit something that we deflect...
//-----------------------------------------------------------------------------
void C_Shield::RegisterDeflection(const Vector& vecDir, int bitsDamageType, trace_t *ptr)
{
	Vector normalDir;
	VectorCopy( vecDir, normalDir );
	VectorNormalize( normalDir );

	CreateShieldDeflection( ptr->hitgroup, normalDir, false );
}

//-----------------------------------------------------------------------------
// This is required to get all the decals to animate correctly
//-----------------------------------------------------------------------------
void C_Shield::SetCurrentDecal( int idx )
{
	m_CurrentDecal = idx;
}

//-----------------------------------------------------------------------------
// returns the address of a variable that stores the material animation frame
//-----------------------------------------------------------------------------
float C_Shield::GetTextureAnimationStartTime()
{
	if( m_CurrentDecal == -1 )
		return m_StartTime;
	return m_Decals[m_CurrentDecal].m_StartTime;
}

//-----------------------------------------------------------------------------
// Indicates that a texture animation has wrapped
//-----------------------------------------------------------------------------
void C_Shield::TextureAnimationWrapped()
{
	if( m_CurrentDecal != -1 )
	{
		m_Decals[m_CurrentDecal].m_StartTime = -1.0f;
	}
}


//-----------------------------------------------------------------------------
// Indicates a collision occurred: 
//-----------------------------------------------------------------------------
void C_Shield::ReceiveMessage( int classID,  bf_read &msg )
{
	if ( classID != GetClientClass()->m_ClassID )
	{
		// message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}

	int hitgroup;
	Vector dir;
	unsigned char partialBlock;

	hitgroup = msg.ReadLong( );
	msg.ReadBitVec3Normal( dir );
	partialBlock = msg.ReadByte( );

	CreateShieldDeflection( hitgroup, dir, partialBlock );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_Shield::CreateShieldDeflection( int hitgroup, const Vector &dir, bool partialBlock )
{
	float hitU = (float)(hitgroup & 0xFFFF) / (float)(1 << 15);
	float hitV = (float)(hitgroup >> 16) / (float)(1 << 15);

	Ripple_t ripple;
	ripple.m_RippleU = hitU;
	ripple.m_RippleV = hitV;
	ripple.m_Amplitude = partialBlock ? 4 : 30;
	ripple.m_Radius = 0.08f;
	ripple.m_StartTime = engine->GetLastTimeStamp();
	ripple.m_Direction = dir;
	m_Ripples.AddToTail(ripple);

	Decal_t decal;
	decal.m_RippleU = hitU;
	decal.m_RippleV = hitV;
	decal.m_Radius = partialBlock ? 0.03f : 0.08f;
	decal.m_StartTime = engine->GetLastTimeStamp();
	m_Decals.AddToTail(decal);
}


//-----------------------------------------------------------------------------
// Draws the control points in wireframe
//-----------------------------------------------------------------------------
void C_Shield::DrawWireframeModel( Vector const** ppPositions )
{
	IMesh* pMesh = materials->GetDynamicMesh( true, NULL, NULL, m_pWireframe );

	int numLines = (Height() - 1) * Width() + Height() * (Width() - 1);

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, numLines );

	Vector const* tmp;
	for (int i = 0; i < Height(); ++i)
	{
		for (int j = 0; j < Width(); ++j)
		{
			if ( i > 0 )
			{
				tmp = ppPositions[j + Width() * i];
				meshBuilder.Position3fv( tmp->Base() );
				meshBuilder.Color4ub( 255, 255, 255, 128 );
				meshBuilder.AdvanceVertex();

				tmp = ppPositions[j + Width() * (i-1)];
				meshBuilder.Position3fv( tmp->Base() );
				meshBuilder.Color4ub( 255, 255, 255, 128 );
				meshBuilder.AdvanceVertex();
			}

			if (j > 0)
			{
				tmp = ppPositions[j + Width() * i];
				meshBuilder.Position3fv( tmp->Base() );
				meshBuilder.Color4ub( 255, 255, 255, 128 );
				meshBuilder.AdvanceVertex();

				tmp = ppPositions[j - 1 + Width() * i];
				meshBuilder.Position3fv( tmp->Base() );
				meshBuilder.Color4ub( 255, 255, 255, 128 );
				meshBuilder.AdvanceVertex();
			}
		}
	}

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Draws the base shield
//-----------------------------------------------------------------------------
#define TRANSITION_REGION_WIDTH 0.5f

extern ConVar mat_wireframe;

void C_Shield::DrawShieldPoints(Vector* pt, Vector* normal, float* opacity)
{
	SetCurrentDecal( -1 );

	if (mat_wireframe.GetInt() == 0)
		materials->Bind( m_pShield, (IClientRenderable*)this );
	else
		materials->Bind( m_pWireframe, (IClientRenderable*)this );
	IMesh* pMesh = materials->GetDynamicMesh( true, NULL, NULL );

	int numTriangles = (m_SubdivisionCount - 1) * (m_SubdivisionCount - 1) * 2;

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, numTriangles );

	float du = 1.0f * m_InvSubdivisionCount;
	float dv = du;

	unsigned char color[3];
	color[0] = 255; 
	color[1] = 255; 
	color[2] = 255; 

	for ( int i = 0; i < m_SubdivisionCount - 1; ++i)
	{
		float v = i * dv;

		for (int j = 0; j < m_SubdivisionCount - 1; ++j)
		{
			int idx = i * m_SubdivisionCount + j;
			float u = j * du;

			meshBuilder.Position3fv( pt[idx].Base() );
			meshBuilder.Color4ub( color[0], color[1], color[2], opacity[idx] );
			meshBuilder.Normal3fv( normal[idx].Base() );
			meshBuilder.TexCoord2f( 0, u, v );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pt[idx + m_SubdivisionCount].Base() );
			meshBuilder.Color4ub( color[0], color[1], color[2], opacity[idx+m_SubdivisionCount] );
			meshBuilder.Normal3fv( normal[idx + m_SubdivisionCount].Base() );
			meshBuilder.TexCoord2f( 0, u, v + dv );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pt[idx + 1].Base() );
			meshBuilder.Color4ub( color[0], color[1], color[2], opacity[idx+1] );
			meshBuilder.Normal3fv( normal[idx+1].Base() );
			meshBuilder.TexCoord2f( 0, u + du, v );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pt[idx + 1].Base() );
			meshBuilder.Color4ub( color[0], color[1], color[2], opacity[idx+1] );
			meshBuilder.Normal3fv( normal[idx+1].Base() );
			meshBuilder.TexCoord2f( 0, u + du, v );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pt[idx + m_SubdivisionCount].Base() );
			meshBuilder.Color4ub( color[0], color[1], color[2], opacity[idx+m_SubdivisionCount] );
			meshBuilder.Normal3fv( normal[idx + m_SubdivisionCount].Base() );
			meshBuilder.TexCoord2f( 0, u, v + dv );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pt[idx + m_SubdivisionCount + 1].Base() );
			meshBuilder.Color4ub( color[0], color[1], color[2], opacity[idx+m_SubdivisionCount+1] );
			meshBuilder.Normal3fv( normal[idx + m_SubdivisionCount + 1].Base() );
			meshBuilder.TexCoord2f( 0, u + du, v + dv );
			meshBuilder.AdvanceVertex();
		}
	}

	meshBuilder.End();
	pMesh->Draw();
}



//-----------------------------------------------------------------------------
// Draws shield decals
//-----------------------------------------------------------------------------
void C_Shield::DrawShieldDecals( Vector* pt, bool hitDecals )
{
	if (m_Decals.Size() == 0)
		return;

	// Compute ripples:
	for ( int r = m_Decals.Size(); --r >= 0; )
	{
		// At the moment, nothing passes!
		bool passDecal = false;
		if ((!hitDecals) && (passDecal == hitDecals))
			continue;

		SetCurrentDecal( r );

		// We have to force a flush here because we're changing the proxy state
		if (!hitDecals)
			materials->Bind( m_pPassDecal, (IClientRenderable*)this );
		else
			materials->Bind( passDecal ? m_pPassDecal2 : m_pHitDecal, (IClientRenderable*)this );

		float dtime = gpGlobals->curtime - m_Decals[r].m_StartTime;
		float decay = exp( -( 2 * dtime) );

		// Retire the animation if it wraps
		// This gets set by TextureAnimatedWrapped above
		if ((m_Decals[r].m_StartTime < 0.0f) || (decay < 1e-3))
		{
			m_Decals.Remove(r);
			continue;
		}

		IMesh* pMesh = materials->GetDynamicMesh();

		// Figure out the quads we must mod2x....
		float u0 = m_Decals[r].m_RippleU - m_Decals[r].m_Radius;
		float u1 = m_Decals[r].m_RippleU + m_Decals[r].m_Radius;
		float v0 = m_Decals[r].m_RippleV - m_Decals[r].m_Radius;
		float v1 = m_Decals[r].m_RippleV + m_Decals[r].m_Radius;
		float du = u1 - u0;
		float dv = v1 - v0;

		int i0 = Floor2Int( v0 * (m_SubdivisionCount - 1) );
		int i1 = Ceil2Int( v1 * (m_SubdivisionCount - 1) );
		int j0 = Floor2Int( u0 * (m_SubdivisionCount - 1) );
		int j1 = Ceil2Int( u1 * (m_SubdivisionCount - 1) );
		if (i0 < 0)
			i0 = 0;
		if (i1 >= m_SubdivisionCount)
			i1 = m_SubdivisionCount - 1;
		if (j0 < 0)
			j0 = 0;
		if (j1 >= m_SubdivisionCount)
			j1 = m_SubdivisionCount - 1;

		int numTriangles = (i1 - i0) * (j1 - j0) * 2;

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, numTriangles );

		float decalDu = m_InvSubdivisionCount / du;
		float decalDv = m_InvSubdivisionCount / dv;

		unsigned char color[3];
		color[0] = s_ImpactDecalColor[0] * decay;
		color[1] = s_ImpactDecalColor[1] * decay;
		color[2] = s_ImpactDecalColor[2] * decay;

		for ( int i = i0; i < i1; ++i)
		{
			float t = (float)i * m_InvSubdivisionCount;
			for (int j = j0; j < j1; ++j)
			{
				float s = (float)j * m_InvSubdivisionCount;
				int idx = i * m_SubdivisionCount + j;

				// Compute (u,v) into the decal
				float decalU = (s - u0) / du;
				float decalV = (t - v0) / dv;

				meshBuilder.Position3fv( pt[idx].Base() );
				meshBuilder.Color3ubv( color );
				meshBuilder.TexCoord2f( 0, decalU, decalV );
				meshBuilder.AdvanceVertex();

				meshBuilder.Position3fv( pt[idx + m_SubdivisionCount].Base() );
				meshBuilder.Color3ubv( color );
				meshBuilder.TexCoord2f( 0, decalU, decalV + decalDv );
				meshBuilder.AdvanceVertex();

				meshBuilder.Position3fv( pt[idx + 1].Base() );
				meshBuilder.Color3ubv( color );
				meshBuilder.TexCoord2f( 0, decalU + decalDu, decalV );
				meshBuilder.AdvanceVertex();

				meshBuilder.Position3fv( pt[idx + 1].Base() );
				meshBuilder.Color3ubv( color );
				meshBuilder.TexCoord2f( 0, decalU + decalDu, decalV );
				meshBuilder.AdvanceVertex();

				meshBuilder.Position3fv( pt[idx + m_SubdivisionCount].Base() );
				meshBuilder.Color3ubv( color );
				meshBuilder.TexCoord2f( 0, decalU, decalV + decalDv );
				meshBuilder.AdvanceVertex();

				meshBuilder.Position3fv( pt[idx + m_SubdivisionCount + 1].Base() );
				meshBuilder.Color3ubv( color );
				meshBuilder.TexCoord2f( 0, decalU + decalDu, decalV + decalDv );
				meshBuilder.AdvanceVertex();
			}
		}

		meshBuilder.End();
		pMesh->Draw();
	}
}


//-----------------------------------------------------------------------------
// Computes a single point
//-----------------------------------------------------------------------------
void C_Shield::ComputePoint( float s, float t, Vector& pt, Vector& normal, float& opacity )
{
	// Precache some computations for the point on the spline at (s, t).
	m_SplinePatch.SetupPatchQuery( s, t );

	// Get the position + normal
	m_SplinePatch.GetPointAndNormal( pt, normal );

	// From here on down is all futzing with opacity

	// Check neighbors for activity...
	bool active = IsPanelActive(m_SplinePatch.m_is, m_SplinePatch.m_it);
	if (m_SplinePatch.m_fs == 0.0f)
		active = active || IsPanelActive(m_SplinePatch.m_is - 1, m_SplinePatch.m_it);
	if (m_SplinePatch.m_ft == 0.0f)
		active = active || IsPanelActive(m_SplinePatch.m_is, m_SplinePatch.m_it - 1);

	if (!active)
	{
		// If the panel's not active, it's transparent.
		opacity = 0.0f;
	}
	else
	{
		if ((s == 0.0f) || (t == 0.0f) ||
			(s == (Width() - 1.0f)) || (t == (Height() - 1.0f)) )
		{
			// If it's on the edge, it's max opacity
			opacity = 192.0f;
		}
		else
		{
			// Channel zero is the opacity data
			opacity = m_SplinePatch.GetChannel( 0 );

			// Make the shield translucent if the owner is the local player...
			// Also don't mess with the edges..
			if (m_ShieldOwnedByLocalPlayer)
			{
				// Channel 1 is the opacity blend
				float blendFactor = m_SplinePatch.GetChannel( 1 );
				blendFactor = clamp( blendFactor, 0.0f, 1.0f );

				float blendValue = 1.0f; 
				Vector delta;
				VectorSubtract( pt, GetAbsOrigin(), delta );
				float dist = VectorLength( delta );
				if (dist != 0.0f)
				{
					delta *= 1.0f / dist;
					float dot = DotProduct( m_ViewDir, delta );
					float angle = acos(	dot );
					float fov = M_PI * render->GetFieldOfView() / 180.0f;
					if (angle < fov * .2f)
						blendValue = 0.1f;
					else if (angle < fov * 0.4f)
					{
						// Want a cos falloff between .2 and .4
						// 0.1 at .2 and 1.0 at .4
						angle -= fov * 0.2f;
						blendValue = 1.0f - 0.9f * 0.5f * (cos ( M_PI * angle / (fov * 0.2f) ) + 1.0f);
					}
				}

				// Interpolate between 1 and the blend value based on the blend factor...
				opacity *= (1.0f - blendFactor) + blendFactor * blendValue;
			}

			opacity = clamp( opacity, 0.0f, 192.0f );
		}
	}
	opacity *= m_FadeValue;
}


//-----------------------------------------------------------------------------
// Compute the shield points using catmull-rom
//-----------------------------------------------------------------------------
void C_Shield::ComputeShieldPoints( Vector* pt, Vector* normal, float* opacity )
{
	int i;
	for ( i = 0; i < m_SubdivisionCount; ++i)
	{
		float t = (Height() - 1) * (float)i * m_InvSubdivisionCount;
		for (int j = 0; j < m_SubdivisionCount; ++j)
		{
			float s = (Width() - 1) * (float)j * m_InvSubdivisionCount;
			int idx = i * m_SubdivisionCount + j;

			ComputePoint( s, t, pt[idx], normal[idx], opacity[idx] );
		}
	}
}

//-----------------------------------------------------------------------------
// Compute the shield ripples from being hit
//-----------------------------------------------------------------------------
void C_Shield::RippleShieldPoints( Vector* pt, float* opacity )
{
	// Compute ripples:
	for ( int r = m_Ripples.Size(); --r >= 0; )
	{
		float dtime = gpGlobals->curtime - m_Ripples[r].m_StartTime;
		float decay = exp( -( 2 * dtime) );
		float amplitude = m_Ripples[r].m_Amplitude * decay;

		for ( int i = 0; i < m_SubdivisionCount; ++i)
		{
			float t = i * m_InvSubdivisionCount;
			for (int j = 0; j < m_SubdivisionCount; ++j)
			{
				float s = j * m_InvSubdivisionCount;
				int idx = i * m_SubdivisionCount + j;

				float ds = s - m_Ripples[r].m_RippleU;
				float dt = t - m_Ripples[r].m_RippleV;
				float dr = sqrt( ds * ds + dt * dt );
				if (dr < m_Ripples[r].m_Radius)
				{
					// need to apply ripple
					float diff = amplitude * cos( 0.5f * M_PI * dr / m_Ripples[r].m_Radius );
					VectorMA( pt[idx], diff, m_Ripples[r].m_Direction, pt[idx] );

					// Compute opacity at this point...
					float impactopacity = 192.0f * decay * dr / m_Ripples[r].m_Radius;
					if (impactopacity > opacity[idx])
						opacity[idx] = impactopacity;
				}
			}
		}

		if (amplitude < 0.1)
			m_Ripples.Remove(r);
	}
}

//-----------------------------------------------------------------------------
// Main draw entry point
//-----------------------------------------------------------------------------
int	C_Shield::DrawModel( int flags )
{
	if ( !m_bReadyToDraw )
		return 0;

	if (m_FadeValue == 0.0f)
		return 1;

	// If I have no power, don't draw
	if ( m_flPowerLevel <= 0 )
		return 1;

	// Make it curvy or not!!
	m_SplinePatch.SetLinearBlend( m_CurveValue );

	// Set up the patch with all the data it's going to need
	int count = Width() * Height();
 	Vector const** pControlPoints = (Vector const**)stackalloc(count * sizeof(Vector*));
	float* pControlOpacity = (float*)stackalloc(count * sizeof(float));
	float* pControlBlend = (float*)stackalloc(count * sizeof(float));

	GetShieldData( pControlPoints, pControlOpacity, pControlBlend );
	m_SplinePatch.SetControlPositions( pControlPoints );
	m_SplinePatch.SetChannelData( 0, pControlOpacity );
	m_SplinePatch.SetChannelData( 1, pControlBlend );

//	DrawWireframeModel( pControlPoints );

	// Allocate space for temporary data
	int numSubdivisions = m_SubdivisionCount * m_SubdivisionCount;
	Vector* pt = (Vector*)stackalloc(numSubdivisions * sizeof(Vector));
	Vector* normal = (Vector*)stackalloc(numSubdivisions * sizeof(Vector));
	float* opacity = (float*)stackalloc(numSubdivisions * sizeof(float));

	// Do something a little special if this shield is owned by the local player
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	m_ShieldOwnedByLocalPlayer = (player->entindex() == m_nOwningPlayerIndex);
	if (m_ShieldOwnedByLocalPlayer)
	{
		QAngle viewAngles;
		engine->GetViewAngles(viewAngles);
		AngleVectors( viewAngles, &m_ViewDir );
	}

	ComputeShieldPoints( pt, normal, opacity );
	RippleShieldPoints( pt, opacity );

	// Commented out because it causes things to not be drawn behind it
//	DrawShieldDecals( pt, false );

    DrawShieldPoints( pt, normal, opacity );
	DrawShieldDecals( pt, true );

	return 1;
}



//============================================================================================================
// SHIELD POWERLEVEL PROXY
//============================================================================================================
class CShieldPowerLevelProxy : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity );
};

void CShieldPowerLevelProxy::OnBind( void *pRenderable )
{
	IClientRenderable *pRend = (IClientRenderable *)pRenderable;
	C_BaseEntity *pEntity = pRend->GetIClientUnknown()->GetBaseEntity();
	C_Shield *pShield = dynamic_cast<C_Shield*>(pEntity);
	if (!pShield)
		return;

	SetFloatResult( pShield->GetPowerLevel() );
}

EXPOSE_INTERFACE( CShieldPowerLevelProxy, IMaterialProxy, "ShieldPowerLevel" IMATERIAL_PROXY_INTERFACE_VERSION );
