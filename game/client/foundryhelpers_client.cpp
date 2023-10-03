//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "foundryhelpers_client.h"
#include "c_basetempentity.h"
#include "tier2/beamsegdraw.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static CUtlVector<EHANDLE> g_EntityHighlightEffects;
static ConVar cl_foundry_ShowEntityHighlights( "cl_foundry_ShowEntityHighlights", "1" );


void AddCoolLine( const Vector &v1, const Vector &v2, unsigned long iExtraFadeOffset, bool bNegateMovementDir )
{
	float flLineSectionLength = 3;		// How many inches each line travels. Each line is a solid color and alpha.
	int nLineSectionsToFade = 2;	// How many lines to fade from translucent to opaque.
	int baseColor[3] = { 216, 183, 67 }; // gold
	float flTimeBetweenUpdates = 0.2f;
	unsigned long iLineFadeOffset = iExtraFadeOffset + (int)(gpGlobals->curtime / flTimeBetweenUpdates);
	if ( bNegateMovementDir )
		iLineFadeOffset = 0xFFFFFFFF - iLineFadeOffset;

	
	Vector vDelta = v2 - v1;
	float flLineLen = vDelta.Length();
	vDelta /= flLineLen;

	int nMaxLines = (int)(flLineLen / flLineSectionLength) + 1;


	static IMaterial *pWireframeMaterial = NULL;
	if ( !pWireframeMaterial )
		pWireframeMaterial = materials->FindMaterial( "debug/debugwireframevertexcolor", TEXTURE_GROUP_OTHER );

	static IMaterial *pBeamMaterial = NULL;
	if ( !pBeamMaterial )
		pBeamMaterial = materials->FindMaterial( "effects/laser1", TEXTURE_GROUP_OTHER );

	CMatRenderContextPtr pRenderContext( materials );


	// Draw the solid underlying lines.
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, pWireframeMaterial );
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, 1 );

	meshBuilder.Position3fv( v1.Base() );
	meshBuilder.Color4ub( baseColor[0], baseColor[1], baseColor[2], 255 );
	meshBuilder.AdvanceVertex();
	
	meshBuilder.Position3fv( v2.Base() );
	meshBuilder.Color4ub( baseColor[0], baseColor[1], baseColor[2], 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End( false, true );


	// Draw the additive beams.
	float flCurDist = 0;
	Vector vStartPos = v1;
	for ( int i=0; i < nMaxLines; i++ )
	{
		float flEndDist = MIN( flCurDist + flLineSectionLength, flLineLen );
		Vector vEndPos = v1 + vDelta * flEndDist;
		
		int alpha;
		int iFadeAmt = (iLineFadeOffset+i) % (nLineSectionsToFade * 2);
		if ( iFadeAmt < nLineSectionsToFade )
			alpha = (iFadeAmt * 255) / nLineSectionsToFade;
		else
			alpha = (255 * (nLineSectionsToFade - (iFadeAmt - nLineSectionsToFade))) / nLineSectionsToFade;

		float flAlpha = Bias( alpha / 255.0f, 0.6 );

		
		CBeamSegDraw beamDraw;
		beamDraw.Start( pRenderContext, 2, pBeamMaterial );
		BeamSeg_t beamSeg;
		beamSeg.SetColor( baseColor[0] * flAlpha / 255.0f, baseColor[1] * flAlpha / 255.0f, baseColor[2] * flAlpha / 255.0f, 1.0f );
		beamSeg.m_flTexCoord = 0;
		beamSeg.m_flWidth = 6;

		beamSeg.m_vPos = vStartPos;
		beamDraw.NextSeg( &beamSeg );

		beamSeg.m_vPos = vEndPos;
		beamDraw.NextSeg( &beamSeg );

		beamDraw.End();

		flCurDist = flEndDist;
		vStartPos = vEndPos;
	}
}


void FoundryHelpers_DrawEntityHighlightEffect( C_BaseEntity *pEnt )
{
	CCollisionProperty *pCollision = pEnt->CollisionProp();

	// Transform the OBB corners into world space.
	const Vector &vMins = pCollision->OBBMins();
	const Vector &vMaxs = pCollision->OBBMaxs();
	Vector vPoints[8] = 
	{
		Vector( vMins.x, vMins.y, vMins.z ),
		Vector( vMaxs.x, vMins.y, vMins.z ),
		Vector( vMaxs.x, vMaxs.y, vMins.z ),
		Vector( vMins.x, vMaxs.y, vMins.z ),
		
		Vector( vMins.x, vMins.y, vMaxs.z ),
		Vector( vMaxs.x, vMins.y, vMaxs.z ),
		Vector( vMaxs.x, vMaxs.y, vMaxs.z ),
		Vector( vMins.x, vMaxs.y, vMaxs.z )
	};

	for ( int i=0; i < 8; i++ )
	{
		Vector vTmp;
		vPoints[i] = pCollision->CollisionToWorldSpace( vPoints[i], &vTmp );
	}

	// Draw lines connecting them up...
	for ( int i=0; i < 4; i++ )
	{
		AddCoolLine( vPoints[i], vPoints[(i+1)%4], 0, false );
		AddCoolLine( vPoints[i+4], vPoints[(i+1)%4 + 4], 4, true );
		AddCoolLine( vPoints[i], vPoints[i+4], i*2+4, (i%1) == 0 );
	}
}


void FoundryHelpers_DrawAll()
{
	if ( cl_foundry_ShowEntityHighlights.GetBool() )
	{
		for ( int i=0; i < g_EntityHighlightEffects.Count(); i++ )
		{
			C_BaseEntity *pEnt = g_EntityHighlightEffects[i];

			if ( !pEnt )
				continue;

			FoundryHelpers_DrawEntityHighlightEffect( pEnt );
		}
	}
}


void FoundryHelpers_ClearEntityHighlightEffects()
{
	g_EntityHighlightEffects.Purge();
}

void FoundryHelpers_AddEntityHighlightEffect( int iEntity )
{
	EHANDLE hEnt = cl_entitylist->GetBaseEntity( iEntity );
	if ( hEnt.IsValid() )
		g_EntityHighlightEffects.AddToTail( hEnt );
}


//-----------------------------------------------------------------------------
// Purpose: This marshalls calls from the server to the client.
//-----------------------------------------------------------------------------
class C_TEFoundryHelpers : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEFoundryHelpers, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void PostDataUpdate( DataUpdateType_t updateType );

	int m_iEntity;
};

void C_TEFoundryHelpers::PostDataUpdate( DataUpdateType_t updateType )
{
	if ( m_iEntity == -1 )
		FoundryHelpers_ClearEntityHighlightEffects();
	else
		FoundryHelpers_AddEntityHighlightEffect( m_iEntity );
}

IMPLEMENT_CLIENTCLASS_EVENT_DT( C_TEFoundryHelpers, DT_TEFoundryHelpers, CTEFoundryHelpers )
	RecvPropInt( RECVINFO(m_iEntity) )
END_RECV_TABLE()

