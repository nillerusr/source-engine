//=============================================================================//
//
// Purpose:		Clientside implementation of the Crab Synth NPC.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "c_npc_crabsynth.h"
#include "basegrenade_shared.h"
#include "particle_parse.h"
#include "functionproxy.h"
#include "toolframework_client.h"

IMPLEMENT_CLIENTCLASS_DT( C_NPC_CrabSynth, DT_NPC_CrabSynth, CNPC_CrabSynth )
	RecvPropBool( RECVINFO( m_bProjectedLightEnabled ) ),
	RecvPropBool( RECVINFO( m_bProjectedLightShadowsEnabled ) ),
	RecvPropFloat( RECVINFO( m_flProjectedLightBrightnessScale ) ),
	RecvPropFloat( RECVINFO( m_flProjectedLightFOV ) ),
	RecvPropFloat( RECVINFO( m_flProjectedLightHorzFOV ) ),
END_RECV_TABLE()

LINK_ENTITY_TO_CLASS( npc_crabsynth, C_NPC_CrabSynth )

C_NPC_CrabSynth::C_NPC_CrabSynth()
{
}

C_NPC_CrabSynth::~C_NPC_CrabSynth()
{
	if (m_LightL)
	{
		// Turned off the flashlight; delete it.
		delete m_LightL;
		m_LightL = NULL;
	}
	if (m_LightR)
	{
		// Turned off the flashlight; delete it.
		delete m_LightR;
		m_LightR = NULL;
	}
}

void C_NPC_CrabSynth::UpdateLight( CCrabSynthLightEffect **ppLight, int nAttachment )
{
	CCrabSynthLightEffect *pLight = *ppLight;
	if ( pLight == NULL )
	{
		// Turned on the headlight; create it.
		*ppLight = new CCrabSynthLightEffect;
		pLight = *ppLight;

		if ( pLight == NULL )
			return;

		pLight->m_bShadowsEnabled = m_bProjectedLightShadowsEnabled;
		pLight->m_flBrightnessScale = m_flProjectedLightBrightnessScale;
		pLight->m_flFOV = m_flProjectedLightFOV;
		pLight->m_flHorzFOV = m_flProjectedLightHorzFOV;

		pLight->TurnOn();
	}
	else
	{
		if (pLight->m_flBrightnessScale != m_flProjectedLightBrightnessScale)
			pLight->m_flBrightnessScale = FLerp( pLight->m_flBrightnessScale, m_flProjectedLightBrightnessScale, 0.1f );

		if (pLight->m_bShadowsEnabled != m_bProjectedLightShadowsEnabled)
			pLight->m_bShadowsEnabled = m_bProjectedLightShadowsEnabled;

		if (pLight->m_flFOV != m_flProjectedLightFOV)
			pLight->m_flFOV = m_flProjectedLightFOV;

		if (pLight->m_flHorzFOV != m_flProjectedLightHorzFOV)
			pLight->m_flHorzFOV = m_flProjectedLightHorzFOV;
	}

	if ( nAttachment != -1 )
	{
		matrix3x4_t matAttachment;
		GetAttachment( nAttachment, matAttachment );

		Vector vVector;
		Vector vecForward, vecRight, vecUp;

		MatrixGetColumn( matAttachment, 0, vecForward );
		MatrixGetColumn( matAttachment, 1, vecRight );
		MatrixGetColumn( matAttachment, 2, vecUp );
		MatrixGetColumn( matAttachment, 3, vVector );

		// Update the flashlight
		pLight->UpdateLight( vVector, vecForward, vecRight, vecUp, 40 );
	}
}

void C_NPC_CrabSynth::Simulate( void )
{
	// The dim light is the flashlight.
	if ( m_bProjectedLightEnabled )
	{
		if (m_LightL == NULL || m_LightL->m_flBrightnessScale > 0.0f)
		{
			if (m_nAttachmentLightL == -1)
				m_nAttachmentLightL = LookupAttachment( "eye_l" );

			UpdateLight( &m_LightL, m_nAttachmentLightL );
		}
		else if (m_LightL)
		{
			// Turned off the flashlight; delete it.
			delete m_LightL;
			m_LightL = NULL;
		}

		if (m_LightR == NULL || m_LightR->m_flBrightnessScale > 0.0f)
		{
			if (m_nAttachmentLightR == -1)
				m_nAttachmentLightR = LookupAttachment( "eye_r" );

			UpdateLight( &m_LightR, m_nAttachmentLightR );
		}
		else if (m_LightR)
		{
			// Turned off the flashlight; delete it.
			delete m_LightR;
			m_LightR = NULL;
		}
	}
	else
	{
		if (m_LightL)
		{
			// Turned off the flashlight; delete it.
			delete m_LightL;
			m_LightL = NULL;
		}
		if (m_LightR)
		{
			// Turned off the flashlight; delete it.
			delete m_LightR;
			m_LightR = NULL;
		}
	}

	BaseClass::Simulate();
}

void C_NPC_CrabSynth::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
}

//-----------------------------------------------------------------------------
// Crab Grenade
//-----------------------------------------------------------------------------
class C_CrabGrenade : public C_BaseGrenade
{
public:
	DECLARE_CLASS( C_CrabGrenade, C_BaseGrenade );
	DECLARE_CLIENTCLASS();

	//C_CrabGrenade();
	//~C_CrabGrenade();

	float m_flStartTime;
	float m_flClientDetonateTime;
};

LINK_ENTITY_TO_CLASS( npc_crab_grenade, C_CrabGrenade );

IMPLEMENT_CLIENTCLASS_DT( C_CrabGrenade, DT_CrabGrenade, CCrabGrenade )

	RecvPropFloat( RECVINFO( m_flStartTime ) ),
	RecvPropFloat( RECVINFO( m_flClientDetonateTime ) ),

END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Controls the crab grenade
//-----------------------------------------------------------------------------
class CProxyCrabGrenade : public IMaterialProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
	void Release();
	IMaterial *GetMaterial();

	IMaterialVar *m_RefractAmount;
	float m_flStartRefractAmount;

	IMaterialVar *m_RefractTint;
	Vector m_vecStartRefractTint;

	IMaterialVar *m_WaveSineMax;
	float m_flStartWaveSineMax;
};

bool CProxyCrabGrenade::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool foundVar;
	m_RefractAmount = pMaterial->FindVar( "$refractamount", &foundVar );
	if ( !m_RefractAmount )
		return false;

	m_flStartRefractAmount = m_RefractAmount->GetFloatValue();
	
	m_RefractTint = pMaterial->FindVar( "$refracttint", &foundVar );
	if ( !m_RefractTint)
		return false;

	const float *flRefractTint = m_RefractTint->GetVecValue();
	m_vecStartRefractTint[0] = flRefractTint[0];
	m_vecStartRefractTint[1] = flRefractTint[1];
	m_vecStartRefractTint[2] = flRefractTint[2];
	
	m_WaveSineMax = pMaterial->FindVar( "$TempMax", &foundVar );
	if ( !m_WaveSineMax )
		return false;

	m_flStartWaveSineMax = m_WaveSineMax->GetFloatValue();

	return true;
}

void CProxyCrabGrenade::OnBind( void *pC_BaseEntity )
{
	if ( !pC_BaseEntity )
		return;

	C_BaseEntity *pEntity = ((IClientRenderable*)pC_BaseEntity)->GetIClientUnknown()->GetBaseEntity();
	if ( pEntity )
	{
		C_CrabGrenade *pGrenade = dynamic_cast<C_CrabGrenade*>( pEntity );
		if ( pGrenade && pGrenade->m_bIsLive )
		{
			float flProgress = RemapVal( gpGlobals->curtime, pGrenade->m_flStartTime, pGrenade->m_flClientDetonateTime, 0.0f, 1.0f );

			m_RefractAmount->SetFloatValue( RemapVal( flProgress, 0.0f, 1.0f, m_flStartRefractAmount, 1.0f ) );
			m_WaveSineMax->SetFloatValue( RemapVal( flProgress, 0.0f, 1.0f, m_flStartWaveSineMax, 10.0f ) );

			m_RefractTint->SetVecValue( RemapVal( flProgress, 0.0f, 1.0f, m_vecStartRefractTint[0], 10.0f ),
				RemapVal( flProgress, 0.0f, 1.0f, m_vecStartRefractTint[1], 10.0f ),
				RemapVal( flProgress, 0.0f, 1.0f, m_vecStartRefractTint[2], 10.0f ) );
		}
		else
		{
			m_RefractAmount->SetFloatValue( m_flStartRefractAmount );
			m_WaveSineMax->SetFloatValue( m_flStartWaveSineMax );

			m_RefractTint->SetVecValue( m_vecStartRefractTint.Base(), 3);
		}
	}

	if ( ToolsEnabled() )
	{
		ToolFramework_RecordMaterialParams( GetMaterial() );
	}
}

void CProxyCrabGrenade::Release( void )
{ 
	delete this; 
}

IMaterial *CProxyCrabGrenade::GetMaterial()
{
	return m_RefractAmount->GetOwningMaterial();
}

EXPOSE_INTERFACE( CProxyCrabGrenade, IMaterialProxy, "CrabGrenade" IMATERIAL_PROXY_INTERFACE_VERSION );
