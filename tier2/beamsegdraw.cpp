//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "tier2/beamsegdraw.h"
#include "materialsystem/imaterialvar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
//
// CBeamSegDraw implementation.
//
//-----------------------------------------------------------------------------
void CBeamSegDraw::Start( IMatRenderContext *pRenderContext, int nSegs, IMaterial *pMaterial, CMeshBuilder *pMeshBuilder, int nMeshVertCount )
{
	m_pRenderContext = pRenderContext;
	Assert( nSegs >= 2 );

	m_nSegsDrawn = 0;
	m_nTotalSegs = nSegs;

	if ( pMeshBuilder )
	{
		m_pMeshBuilder = pMeshBuilder;
		m_nMeshVertCount = nMeshVertCount;
	}
	else
	{
		m_pMeshBuilder = NULL;
		m_nMeshVertCount = 0;

		IMesh *pMesh = m_pRenderContext->GetDynamicMesh( true, NULL, NULL, pMaterial );
		m_Mesh.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, (nSegs-1) * 2 );
	}
}

inline void CBeamSegDraw::ComputeNormal( const Vector &vecCameraPos, const Vector &vStartPos, const Vector &vNextPos, Vector *pNormal )
{
	// vTangentY = line vector for beam
	Vector vTangentY;
	VectorSubtract( vStartPos, vNextPos, vTangentY );
	
	// vDirToBeam = vector from viewer origin to beam
	Vector vDirToBeam;
	VectorSubtract( vStartPos, vecCameraPos, vDirToBeam );

	// Get a vector that is perpendicular to us and perpendicular to the beam.
	// This is used to fatten the beam.
	CrossProduct( vTangentY, vDirToBeam, *pNormal );
	VectorNormalizeFast( *pNormal );
}

inline void CBeamSegDraw::SpecifySeg( const Vector &vecCameraPos, const Vector &vNormal )
{
	// SUCKY: Need to do a fair amount more work to get the tangent owing to the averaged normal
	Vector vDirToBeam, vTangentY;
	VectorSubtract( m_Seg.m_vPos, vecCameraPos, vDirToBeam );
	CrossProduct( vDirToBeam, vNormal, vTangentY );
	VectorNormalizeFast( vTangentY );

	// Build the endpoints.
	Vector vPoint1, vPoint2;
	VectorMA( m_Seg.m_vPos,  m_Seg.m_flWidth*0.5f, vNormal, vPoint1 );
	VectorMA( m_Seg.m_vPos, -m_Seg.m_flWidth*0.5f, vNormal, vPoint2 );

	if ( m_pMeshBuilder )
	{
		// Specify the points.
		m_pMeshBuilder->Position3fv( vPoint1.Base() );
		m_pMeshBuilder->Color4f( VectorExpand( m_Seg.m_vColor ), m_Seg.m_flAlpha );
		m_pMeshBuilder->TexCoord2f( 0, 0, m_Seg.m_flTexCoord );
		m_pMeshBuilder->TexCoord2f( 1, 0, m_Seg.m_flTexCoord );
		m_pMeshBuilder->TangentS3fv( vNormal.Base() );
		m_pMeshBuilder->TangentT3fv( vTangentY.Base() );
		m_pMeshBuilder->AdvanceVertex();
		
		m_pMeshBuilder->Position3fv( vPoint2.Base() );
		m_pMeshBuilder->Color4f( VectorExpand( m_Seg.m_vColor ), m_Seg.m_flAlpha );
		m_pMeshBuilder->TexCoord2f( 0, 1, m_Seg.m_flTexCoord );
		m_pMeshBuilder->TexCoord2f( 1, 1, m_Seg.m_flTexCoord );
		m_pMeshBuilder->TangentS3fv( vNormal.Base() );
		m_pMeshBuilder->TangentT3fv( vTangentY.Base() );
		m_pMeshBuilder->AdvanceVertex();

		if ( m_nSegsDrawn > 1 )
		{
			int nBase = ( ( m_nSegsDrawn - 2 ) * 2 ) + m_nMeshVertCount;

			m_pMeshBuilder->FastIndex( nBase );
			m_pMeshBuilder->FastIndex( nBase + 1 );
			m_pMeshBuilder->FastIndex( nBase + 2 );
			m_pMeshBuilder->FastIndex( nBase + 1 );
			m_pMeshBuilder->FastIndex( nBase + 3 );
			m_pMeshBuilder->FastIndex( nBase + 2 );
		}
	}
	else
	{
		// Specify the points.
		m_Mesh.Position3fv( vPoint1.Base() );
		m_Mesh.Color4f( VectorExpand( m_Seg.m_vColor ), m_Seg.m_flAlpha );
		m_Mesh.TexCoord2f( 0, 0, m_Seg.m_flTexCoord );
		m_Mesh.TexCoord2f( 1, 0, m_Seg.m_flTexCoord );
		m_Mesh.TangentS3fv( vNormal.Base() );
		m_Mesh.TangentT3fv( vTangentY.Base() );
		m_Mesh.AdvanceVertex();
		
		m_Mesh.Position3fv( vPoint2.Base() );
		m_Mesh.Color4f( VectorExpand( m_Seg.m_vColor ), m_Seg.m_flAlpha );
		m_Mesh.TexCoord2f( 0, 1, m_Seg.m_flTexCoord );
		m_Mesh.TexCoord2f( 1, 1, m_Seg.m_flTexCoord );
		m_Mesh.TangentS3fv( vNormal.Base() );
		m_Mesh.TangentT3fv( vTangentY.Base() );
		m_Mesh.AdvanceVertex();
	}
}

void CBeamSegDraw::NextSeg( BeamSeg_t *pSeg )
{
	Vector vecCameraPos;
	m_pRenderContext->GetWorldSpaceCameraPosition( &vecCameraPos );

 	if ( m_nSegsDrawn > 0 )
	{
		// Get a vector that is perpendicular to us and perpendicular to the beam.
		// This is used to fatten the beam.
		Vector vNormal, vAveNormal;
		ComputeNormal( vecCameraPos, m_Seg.m_vPos, pSeg->m_vPos, &vNormal );

		if ( m_nSegsDrawn > 1 )
		{
			// Average this with the previous normal
			VectorAdd( vNormal, m_vNormalLast, vAveNormal );
			vAveNormal *= 0.5f;
			VectorNormalizeFast( vAveNormal );
		}
		else
		{
			vAveNormal = vNormal;
		}

		m_vNormalLast = vNormal;
		SpecifySeg( vecCameraPos, vAveNormal );
	}

	m_Seg = *pSeg;
	++m_nSegsDrawn;

 	if( m_nSegsDrawn == m_nTotalSegs )
	{
		SpecifySeg( vecCameraPos, m_vNormalLast );
	}
}

void CBeamSegDraw::End()
{
	if ( m_pMeshBuilder )
	{
		m_pMeshBuilder = NULL;
		return;
	}

	m_Mesh.End( false, true );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBeamSegDrawArbitrary::SetNormal( const Vector &normal )
{
	m_vNormalLast = normal;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBeamSegDrawArbitrary::NextSeg( BeamSeg_t *pSeg )
{
	if ( m_nSegsDrawn > 0 )
	{
		Vector	segDir = ( m_PrevSeg.m_vPos - pSeg->m_vPos );
		VectorNormalize( segDir );

		Vector	normal = CrossProduct( segDir, m_vNormalLast );
		SpecifySeg( normal );
	}

	m_PrevSeg = m_Seg;
	m_Seg = *pSeg;
	++m_nSegsDrawn;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vNextPos - 
//-----------------------------------------------------------------------------
void CBeamSegDrawArbitrary::SpecifySeg( const Vector &vNormal )
{
	// Build the endpoints.
	Vector vPoint1, vPoint2;
	Vector vDelta;
	VectorMultiply( vNormal, m_Seg.m_flWidth*0.5f, vDelta );
	VectorAdd( m_Seg.m_vPos, vDelta, vPoint1 );
	VectorSubtract( m_Seg.m_vPos, vDelta, vPoint2 );

	// Specify the points.
	Assert( IsFinite(m_Seg.m_vColor.x) && IsFinite(m_Seg.m_vColor.y) && IsFinite(m_Seg.m_vColor.z) && IsFinite(m_Seg.m_flAlpha) );
	Assert( (m_Seg.m_vColor.x >= 0.0) && (m_Seg.m_vColor.y >= 0.0) && (m_Seg.m_vColor.z >= 0.0) && (m_Seg.m_flAlpha >= 0.0) );
	Assert( (m_Seg.m_vColor.x <= 1.0) && (m_Seg.m_vColor.y <= 1.0) && (m_Seg.m_vColor.z <= 1.0) && (m_Seg.m_flAlpha <= 1.0) );

	unsigned char r = FastFToC( m_Seg.m_vColor.x );
	unsigned char g = FastFToC( m_Seg.m_vColor.y );
	unsigned char b = FastFToC( m_Seg.m_vColor.z );
	unsigned char a = FastFToC( m_Seg.m_flAlpha );
	m_Mesh.Position3fv( vPoint1.Base() );
	m_Mesh.Color4ub( r, g, b, a );
	m_Mesh.TexCoord2f( 0, 0, m_Seg.m_flTexCoord );
	m_Mesh.TexCoord2f( 1, 0, m_Seg.m_flTexCoord );
	m_Mesh.AdvanceVertex();
	
	m_Mesh.Position3fv( vPoint2.Base() );
	m_Mesh.Color4ub( r, g, b, a );
	m_Mesh.TexCoord2f( 0, 1, m_Seg.m_flTexCoord );
	m_Mesh.TexCoord2f( 1, 1, m_Seg.m_flTexCoord );
	m_Mesh.AdvanceVertex();
}
