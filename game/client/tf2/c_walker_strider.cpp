//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_walker_strider.h"
#include "beamdraw.h"
#include "clienteffectprecachesystem.h"

extern ConVar vehicle_free_pitch, vehicle_free_roll;


CLIENTEFFECT_REGISTER_BEGIN( PrecacheWalkerStrider )
CLIENTEFFECT_MATERIAL( STRIDER_BEAM_MATERIAL )
CLIENTEFFECT_REGISTER_END()


IMPLEMENT_CLIENTCLASS_DT( C_WalkerStrider, DT_WalkerStrider, CWalkerStrider )
	RecvPropInt( RECVINFO( m_bCrouched ) )
END_RECV_TABLE()
LINK_ENTITY_TO_CLASS( walker_strider, C_WalkerStrider );


ConVar cl_brush_ropes( "cl_brush_ropes", "0" );


C_WalkerStrider::C_WalkerStrider()
{
	m_bCrouched = false;
}


C_WalkerStrider::~C_WalkerStrider()
{
	for ( int i=0; i < STRIDER_NUM_ROPES; i++ )
	{
		if ( m_hRopes[i].Get() )
			m_hRopes[i]->Release();
	}
}


float sideDist = 90;
float downDist = -400;
#include "studio.h"
bool C_WalkerStrider::GetAttachment( int iAttachment, matrix3x4_t &attachmentToWorld )
{
	//
	//
	// This is a TOTAL hack, but we don't have any nodes that work well at all for mounted guns.
	//
	//
	CStudioHdr *pStudioHdr = GetModelPtr( );
	if ( !pStudioHdr || iAttachment < 1 || iAttachment > pStudioHdr->GetNumAttachments() )
	{
		return false;
	}

	Vector vLocalPos( 0, 0, 0 );
	const mstudioattachment_t &pAttachment = pStudioHdr->pAttachment( iAttachment-1 );
	if ( stricmp( pAttachment.pszName(), "build_point_left_gun" ) == 0 )
	{
		vLocalPos.y = sideDist;
	}
	else if ( stricmp( pAttachment.pszName(), "build_point_right_gun" ) == 0 )
	{
		vLocalPos.y = -sideDist;
	}
	else if ( stricmp( pAttachment.pszName(), "ThirdPersonCameraOrigin" ) == 0 )
	{
	}
	else
	{
		// Ok, it's not one of our magical attachments. Use the regular attachment setup stuff.
		return BaseClass::GetAttachment( iAttachment, attachmentToWorld );
	}

	if ( m_bCrouched )
	{
		vLocalPos.z += downDist;
	}

	// Now build the output matrix.
	matrix3x4_t localMatrix;
	SetIdentityMatrix( localMatrix );
	PositionMatrix( vLocalPos, localMatrix );

	ConcatTransforms( EntityToWorldTransform(), localMatrix, attachmentToWorld );
	return true;
}


void C_WalkerStrider::ReceiveMessage( int classID, bf_read &msg )
{
	if ( classID != GetClientClass()->m_ClassID )
	{
		// message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}

	Vector vHitPos;
	msg.ReadBitVec3Coord( vHitPos );

	CStriderBeamEffect eff;
	eff.m_vHitPos = vHitPos;
	eff.m_flStartTime = gpGlobals->curtime;
	m_BeamEffects.AddToTail( eff );
}


float flTestSlack = 280;
float flTestSlack2 = 380;

void C_WalkerStrider::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		if ( cl_brush_ropes.GetInt() )
		{
			// Create some ropes and hang them off certain attachments.
			int indices[7] = 
			{
				LookupAttachment( "kneeL" ),
				LookupAttachment( "kneeR" ),
				LookupAttachment( "kneeB" ),
				
				LookupAttachment( "MiniGun" ),
				LookupAttachment( "left foot" ),
				LookupAttachment( "right foot" ),
				LookupAttachment( "back foot" )
			};
			
			m_hRopes[0] = C_RopeKeyframe::Create( this, this, indices[0], indices[1] );
			m_hRopes[1] = C_RopeKeyframe::Create( this, this, indices[0], indices[2] );
			m_hRopes[2] = C_RopeKeyframe::Create( this, this, indices[1], indices[2] );

			for ( int i=0; i < 3; i++ )
			{
				if ( m_hRopes[i].Get() )
					m_hRopes[i]->SetSlack( flTestSlack );
			}


			m_hRopes[3] = C_RopeKeyframe::Create( this, this, indices[3], indices[4] );
			m_hRopes[4] = C_RopeKeyframe::Create( this, this, indices[3], indices[5] );
			m_hRopes[5] = C_RopeKeyframe::Create( this, this, indices[3], indices[6] );

			for ( i=3; i < 6; i++ )
			{
				if ( m_hRopes[i].Get() )
					m_hRopes[i]->SetSlack( flTestSlack2 );
			}
		}
	}
}


void C_WalkerStrider::ClientThink()
{
	// Retire beam effects.
	int iNext;
	for ( int i=m_BeamEffects.Head(); i != m_BeamEffects.InvalidIndex(); i=iNext )
	{
		iNext = m_BeamEffects.Next( i );
	
		if ( gpGlobals->curtime >= (m_BeamEffects[i].m_flStartTime + STRIDER_BEAM_LIFETIME) )
		{
			m_BeamEffects.Remove( i );
		}
	}
}


int C_WalkerStrider::DrawModel( int flags )
{
	BaseClass::DrawModel( flags );

	IMaterial *pMaterial = materials->FindMaterial( STRIDER_BEAM_MATERIAL, TEXTURE_GROUP_CLIENT_EFFECTS );

	Vector vGunPos;
	QAngle vAngles;
	BaseClass::GetAttachment( LookupAttachment( "BigGun" ), vGunPos, vAngles );

	// Draw our beam effects.
	FOR_EACH_LL( m_BeamEffects, i )
	{
		CStriderBeamEffect *pEff = &m_BeamEffects[i];

		float flAlpha = (gpGlobals->curtime - pEff->m_flStartTime) / STRIDER_BEAM_LIFETIME;
		flAlpha = 1.0 - clamp( flAlpha, 0, 1 );

		CBeamSegDraw segDraw;
		segDraw.Start( 2, pMaterial );

		CBeamSeg seg;
		seg.m_vColor.Init( 1, 0, 0 );
		seg.m_flWidth = STRIDER_BEAM_WIDTH;
		seg.m_flAlpha = flAlpha;

		seg.m_flTexCoord = 0;
		seg.m_vPos = vGunPos;
		segDraw.NextSeg( &seg );

		seg.m_flTexCoord = 1;
		seg.m_vPos = pEff->m_vHitPos;
		segDraw.NextSeg( &seg );
		
		segDraw.End();
	}

	return 1;
}

