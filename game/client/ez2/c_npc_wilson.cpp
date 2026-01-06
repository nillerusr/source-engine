//================= Copyright LOLOLOLOL, All rights reserved. ==================//
//
// Purpose: Bad Cop's friend, a pacifist turret who could open doors for some reason.
// 
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "c_npc_wilson.h"
#include "functionproxy.h"
#include "materialsystem/imaterialvar.h"

C_AI_TurretBase::C_AI_TurretBase()
{
	m_iRopeStartAttachments[0] = -1;

	m_EyeLightColor[0] = 1.0f;
	m_EyeLightColor[1] = 0.0f;
	m_EyeLightColor[2] = 0.0f;
}

C_AI_TurretBase::~C_AI_TurretBase()
{
	if ( m_EyeLight )
	{
		// Turned off the flashlight; delete it.
		delete m_EyeLight;
		m_EyeLight = NULL;
	}

	if (m_pRopes[0])
	{
		for (int i = 0; i < 4; i++)
		{
			if (m_pRopes[i])
				m_pRopes[i]->Remove();
			m_pRopes[i] = NULL;
		}
	}
}

void C_AI_TurretBase::ClientThink( void )
{
	BaseClass::ClientThink();

	if (!IsDormant() && !m_bClosedIdle)
	{
		if (!m_pRopes[0])
		{
			if (m_iRopeStartAttachments[0] == -1)
			{
				for (int i = 0; i < 4; i++)
				{
					m_iRopeStartAttachments[i] = LookupAttachment( VarArgs( "Wire%i_start", i+1 ) );
					m_iRopeEndAttachments[i] = LookupAttachment( VarArgs( "Wire%i_end", i+1 ) );
				}
			}

			for (int i = 0; i < 4; i++)
			{
				m_pRopes[i] = CreateRope( m_iRopeStartAttachments[i], m_iRopeEndAttachments[i] );
			}
		}
	}
	else if (m_pRopes[0])
	{
		for (int i = 0; i < 4; i++)
		{
			if (m_pRopes[i])
				m_pRopes[i]->Remove();
			m_pRopes[i] = NULL;
		}
	}
}

static const float g_flTurretLightMaxFar = 500.0f;

void C_AI_TurretBase::Simulate( void )
{
	// The dim light is the flashlight.
	if ( m_bEyeLightEnabled && (m_EyeLight == NULL || m_EyeLight->m_flBrightnessScale > 0.0f) )
	{
		if ( m_EyeLight == NULL )
		{
			// Turned on the headlight; create it.
			m_EyeLight = new CTurretLightEffect;

			if ( m_EyeLight == NULL )
				return;

			m_EyeLight->m_Color[0] = m_EyeLightColor[0];
			m_EyeLight->m_Color[1] = m_EyeLightColor[1];
			m_EyeLight->m_Color[2] = m_EyeLightColor[2];

			m_EyeLight->m_flBrightnessScale = m_flEyeLightBrightnessScale;
			if (m_flRange > 0)
				m_EyeLight->m_flFarZ = MIN( m_flRange, g_flTurretLightMaxFar );
			if (m_flFOV > 0)
				m_EyeLight->m_flFOV = AngleNormalize( m_flFOV + 25 );
			m_EyeLight->TurnOn();
		}
		else if (m_EyeLight->m_flBrightnessScale != m_flEyeLightBrightnessScale)
		{
			m_EyeLight->m_flBrightnessScale = FLerp( m_EyeLight->m_flBrightnessScale, m_flEyeLightBrightnessScale, 0.1f );
		}

		QAngle vAngle;
		Vector vVector;
		Vector vecForward, vecRight, vecUp;

		if ( m_iLightAttachment == -1 )
			m_iLightAttachment = LookupAttachment( "light" );

		if ( m_iLightAttachment != -1 )
		{
			GetAttachment( m_iLightAttachment, vVector, vAngle );
			AngleVectors( vAngle, &vecForward, &vecRight, &vecUp );

			// Update the flashlight
			m_EyeLight->UpdateLight( vVector, vecForward, vecRight, vecUp, 40 );

			// Update the glow
			// The glow needs to be a few units in front of the eye
			UpdateGlow( vVector, vecForward );
		}
	}
	else
	{
		if ( m_EyeLight )
		{
			// Turned off the flashlight; delete it.
			delete m_EyeLight;
			m_EyeLight = NULL;
		}
	}

	BaseClass::Simulate();
}

void C_AI_TurretBase::OnDataChanged( DataUpdateType_t type )
{
	if (type == DATA_UPDATE_CREATED)
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	if (m_iEyeLightBrightness != m_iLastEyeLightBrightness)
	{
		// 255 = Equivalent to sprite color byte
		m_flEyeLightBrightnessScale = (float)(m_iEyeLightBrightness) / 255.0f;
		m_iLastEyeLightBrightness = m_iEyeLightBrightness;
	}

	BaseClass::OnDataChanged( type );
}


ConVar npc_wilson_talk_glow_fade( "npc_wilson_talk_glow_fade", "0.1" );
ConVar npc_wilson_talk_scale( "npc_wilson_talk_scale", "0.25" );
ConVar npc_wilson_talk_min( "npc_wilson_talk_min", "0.75" );
ConVar npc_wilson_talk_max( "npc_wilson_talk_max", "5.0" );
ConVar npc_wilson_talk_start( "npc_wilson_talk_start", "1.0" );

IMPLEMENT_CLIENTCLASS_DT( C_NPC_Wilson, DT_NPC_Wilson, CNPC_Wilson )
	RecvPropBool( RECVINFO( m_bEyeLightEnabled ) ),
	RecvPropInt( RECVINFO( m_iEyeLightBrightness ) ),
	RecvPropBool( RECVINFO( m_bClosedIdle ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_NPC_Wilson )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( npc_wilson, C_NPC_Wilson )

C_NPC_Wilson::C_NPC_Wilson()
{
	m_Glow.m_bDirectional = false;
	m_Glow.m_bInSky = false;
	m_flTalkGlow = 1.0f;
}

C_NPC_Wilson::~C_NPC_Wilson()
{
}

void C_NPC_Wilson::Simulate( void )
{
	if (m_flTalkGlow > 1.01f)
	{
		m_flTalkGlow -= (npc_wilson_talk_glow_fade.GetFloat() * (m_flTalkGlow - 1.0f));
	}
	else if (m_flTalkGlow < 0.99f)
	{
		m_flTalkGlow += (npc_wilson_talk_glow_fade.GetFloat() * (abs(m_flTalkGlow) + 1.0f));
	}
	else if (m_flTalkGlow != 1.0f)
	{
		m_flTalkGlow = 1.0f;
	}

	BaseClass::Simulate();
}

void C_NPC_Wilson::OnDataChanged( DataUpdateType_t type )
{
	if ( type == DATA_UPDATE_CREATED )
	{
		// Setup our light glow.
		m_Glow.m_nSprites = 1;

		m_Glow.m_Sprites[0].m_flVertSize = 4.0f;
		m_Glow.m_Sprites[0].m_flHorzSize = 4.0f;
		m_Glow.m_Sprites[0].m_vColor = Vector( 255, 0, 0 );
		
		m_Glow.SetOrigin( GetAbsOrigin() );
		m_Glow.SetFadeDistances( 512, 2048, 3072 );
		m_Glow.m_flProxyRadius = 2.0f;

		SetNextClientThink( gpGlobals->curtime + RandomFloat(0,3.0) );
	}

	BaseClass::OnDataChanged( type );
}

void C_NPC_Wilson::ClientThink( void )
{
	Vector mins = GetAbsOrigin();
	if ( engine->IsBoxVisible( mins, mins ) )
	{
		m_Glow.Activate();
	}
	else
	{
		m_Glow.Deactivate();
	}

	SetNextClientThink( gpGlobals->curtime + RandomFloat(1.0,3.0) );
}

void C_NPC_Wilson::UpdateGlow( Vector &vVector, Vector &vecForward )
{
	Vector vGlowPos = vVector + (vecForward.Normalized() * 2.0f);
	m_Glow.SetOrigin( vGlowPos );
	m_Glow.m_vPos = vGlowPos;
	m_Glow.m_flHDRColorScale = FLerp( m_Glow.m_flHDRColorScale, m_flEyeLightBrightnessScale, 0.1f );
}

C_RopeKeyframe *C_NPC_Wilson::CreateRope( int iStartAttach, int iEndAttach )
{
	C_RopeKeyframe *pRope = C_RopeKeyframe::Create( this, this, iStartAttach, iEndAttach, 0.5f, "cable/cable.vmt", 5, ROPE_SIMULATE | ROPE_USE_WIND );
	pRope->SetSlack( 110 );
	return pRope;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *classes - 
//			phoneme - 
//			scale - 
//			newexpression - 
//-----------------------------------------------------------------------------
void C_NPC_Wilson::AddViseme( Emphasized_Phoneme *classes, float emphasis_intensity, int phoneme, float scale, bool newexpression )
{
	int type;

	// Setup weights for any emphasis blends
	bool skip = SetupEmphasisBlend( classes, phoneme );
	// Uh-oh, missing or unknown phoneme???
	if ( skip )
	{
		return;
	}
		
	// Compute blend weights
	ComputeBlendedSetting( classes, emphasis_intensity );

	for ( type = 0; type < NUM_PHONEME_CLASSES; type++ )
	{
		Emphasized_Phoneme *info = &classes[ type ];
		if ( !info->valid || info->amount == 0.0f )
			continue;

		const flexsettinghdr_t *actual_flexsetting_header = info->base;
		const flexsetting_t *pSetting = actual_flexsetting_header->pIndexedSetting( phoneme );
		if (!pSetting)
		{
			continue;
		}

		flexweight_t *pWeights = NULL;

		int truecount = pSetting->psetting( (byte *)actual_flexsetting_header, 0, &pWeights );
		if ( pWeights )
		{
			float flPhonemes = npc_wilson_talk_start.GetFloat();
			for ( int i = 0; i < truecount; i++)
			{
				// Translate to global controller number
				//int j = FlexControllerLocalToGlobal( actual_flexsetting_header, pWeights->key );
				flPhonemes += info->amount * scale * pWeights->weight * npc_wilson_talk_scale.GetFloat();
				// Go to next setting
				pWeights++;
			}

			// Lerp it
			m_flTalkGlow = FLerp( npc_wilson_talk_min.GetFloat(), npc_wilson_talk_max.GetFloat(), flPhonemes / m_flTalkGlow );
		}
	}
}

class CWilsonEyeMaterialProxy : public CResultProxy
{
public:
	CWilsonEyeMaterialProxy()
	{
	}
	virtual ~CWilsonEyeMaterialProxy()
	{
	}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues )
	{
		if (!CResultProxy::Init( pMaterial, pKeyValues ))
			return false;

		// Scale is optional
		m_Scale.Init( pMaterial, pKeyValues, "scale", 1.0f );

		return true;
	}
	virtual void OnBind( void *pC_BaseEntity )
	{
		if (!pC_BaseEntity)
			return;

		C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

		// HACKHACK: The Wilson model may be used for dynamic props instead, so make sure it's actually him
		if (FClassnameIs( pEntity, "npc_wilson" ))
		{
			C_NPC_Wilson *pWilson = static_cast<C_NPC_Wilson*>(pEntity);

			float flScale = m_Scale.GetFloat();
			if (flScale != 1.0f)
			{
				// Scale distance from 1.0
				SetFloatResult( ((pWilson->m_flTalkGlow - 1.0f) * flScale) + 1.0f );
			}
			else
			{
				SetFloatResult( pWilson->m_flTalkGlow );
			}
		}
		else
		{
			SetFloatResult( 1.0f );
		}
	}

private:
	CFloatInput	m_Scale;
};

EXPOSE_INTERFACE( CWilsonEyeMaterialProxy, IMaterialProxy, "WilsonEye" IMATERIAL_PROXY_INTERFACE_VERSION );

//=======================================================================================================
//=======================================================================================================

// Currently only used on Arbeit turrets
#define FLOOR_TURRET_CITIZEN_SPRITE_COLOR		255, 240, 0

IMPLEMENT_CLIENTCLASS_DT( C_NPC_Arbeit_FloorTurret, DT_NPC_Arbeit_FloorTurret, CNPC_Arbeit_FloorTurret )
	RecvPropBool( RECVINFO( m_bEyeLightEnabled ) ),
	RecvPropInt( RECVINFO( m_iEyeLightBrightness ) ),
	RecvPropFloat( RECVINFO( m_flRange ) ),
	RecvPropFloat( RECVINFO( m_flFOV ) ),
	RecvPropBool( RECVINFO( m_bLaser ) ),
	RecvPropBool( RECVINFO( m_bGooTurret ) ),
	RecvPropBool( RECVINFO( m_bCitizenTurret ) ),
	RecvPropBool( RECVINFO( m_bClosedIdle ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_NPC_Arbeit_FloorTurret )
END_PREDICTION_DATA()

C_NPC_Arbeit_FloorTurret::C_NPC_Arbeit_FloorTurret()
{
}

C_NPC_Arbeit_FloorTurret::~C_NPC_Arbeit_FloorTurret()
{
	if (m_pLaser)
	{
		m_pLaser->Remove();
		m_pLaser = NULL;
	}
}

void C_NPC_Arbeit_FloorTurret::Activate( void )
{
	BaseClass::Activate();
}

void C_NPC_Arbeit_FloorTurret::Simulate( void )
{
	BaseClass::Simulate();

	if (m_pLaser)
	{
		Vector vecLight;
		QAngle angAngles;
		GetAttachment( m_iLightAttachment, vecLight, angAngles );

		Vector vecDir;
		AngleVectors( angAngles, &vecDir );

		trace_t tr;
		UTIL_TraceLine( vecLight, vecLight + (vecDir * (m_flRange + 8.0f)), MASK_BLOCKLOS_AND_NPCS, this, COLLISION_GROUP_NONE, &tr );

		m_pLaser->PointsInit( vecLight, tr.endpos );
		m_pLaser->SetStartEntity( this );
		m_pLaser->SetStartAttachment( m_iLightAttachment );
	}
}

void C_NPC_Arbeit_FloorTurret::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if (m_bLaser)
	{
		if (!m_pLaser)
		{
			if ( m_iLightAttachment == -1 )
				m_iLightAttachment = LookupAttachment( "light" );

			if ( m_iLightAttachment == -1 )
				return;

			m_pLaser = C_Beam::BeamCreate( "effects/laser1.vmt", 3.0f );
			if ( m_pLaser == NULL )
				return;

			Vector vecLight;
			QAngle angAngles;
			GetAttachment( m_iLightAttachment, vecLight, angAngles );

			Vector vecDir;
			AngleVectors( angAngles, &vecDir );

			trace_t tr;
			UTIL_TraceLine( vecLight, vecLight + (vecDir * (m_flRange + 8.0f)), MASK_BLOCKLOS, this, COLLISION_GROUP_NONE, &tr );

			m_pLaser->PointsInit( vecLight, tr.endpos );
			m_pLaser->SetStartEntity( this );
			m_pLaser->SetStartAttachment( m_iLightAttachment );

			//m_pLaser->FollowEntity( this );
			m_pLaser->SetNoise( 0 );

			if (m_bCitizenTurret)
				m_pLaser->SetColor( FLOOR_TURRET_CITIZEN_SPRITE_COLOR );
			else
				m_pLaser->SetColor( 255, 0, 0 );

			m_pLaser->SetScrollRate( 0 );
			m_pLaser->SetWidth( 3.0f );
			m_pLaser->SetEndWidth( 2.0f );
			m_pLaser->SetBrightness( 200 );
			m_pLaser->SetBeamFlags( SF_BEAM_SHADEIN );
		}
	}
	else if (m_pLaser)
	{
		DevMsg( "Removing m_pLaser\n" );

		m_pLaser->Remove();
		m_pLaser = NULL;
	}

	if (m_bEyeLightEnabled && m_bCitizenTurret)
	{
		// Citizen turrets use a different color
		// (NOTE: update if FLOOR_TURRET_CITIZEN_SPRITE_COLOR is changed)
		m_EyeLightColor[0] = 1.0f;
		m_EyeLightColor[1] = 0.94f;
		m_EyeLightColor[2] = 0.0f;
	}
}

C_RopeKeyframe *C_NPC_Arbeit_FloorTurret::CreateRope( int iStartAttach, int iEndAttach )
{
	C_RopeKeyframe *pRope = NULL;
	if (m_bGooTurret)
	{
		pRope = C_RopeKeyframe::Create( this, this, iStartAttach, iEndAttach, 0.5f, "cable/goocable.vmt", 5, ROPE_SIMULATE | ROPE_USE_WIND );
		pRope->SetSlack( 120 );
	}
	else
	{
		pRope = C_RopeKeyframe::Create( this, this, iStartAttach, iEndAttach, 0.5f, "cable/cable.vmt", 5, ROPE_SIMULATE | ROPE_USE_WIND );
		pRope->SetSlack( 110 );
	}
	return pRope;
}
