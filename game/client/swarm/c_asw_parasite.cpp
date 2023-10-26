#include "cbase.h"
#include "c_asw_parasite.h"
#include "soundenvelope.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Parasite, DT_ASW_Parasite, CASW_Parasite)
	RecvPropBool(RECVINFO(m_bStartIdleSound)),
	RecvPropBool(RECVINFO(m_bDoEggIdle)),
	RecvPropBool(RECVINFO(m_bInfesting)),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Parasite )

END_PREDICTION_DATA()

C_ASW_Parasite::C_ASW_Parasite()
{
	m_pLoopingSound = NULL;
	m_bStartIdleSound = false;
	m_bDoEggIdle = false;
	m_bInfesting = false;
}


C_ASW_Parasite::~C_ASW_Parasite()
{

}

void C_ASW_Parasite::OnRestore()
{
	BaseClass::OnRestore();
	if (m_bStartIdleSound)
	{
		SoundInit();
	}
}

void C_ASW_Parasite::OnDataChanged( DataUpdateType_t updateType )
{
	if (m_bStartIdleSound)
	{
		SoundInit();
	}

	BaseClass::OnDataChanged( updateType );
}

void C_ASW_Parasite::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();
	SoundShutdown();
}

void C_ASW_Parasite::SoundInit()
{
	CPASAttenuationFilter filter( this );

	// Start the parasite's looping sound
	if( !m_pLoopingSound )
	{
		m_pLoopingSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "ASW_Parasite.Idle" );
		CSoundEnvelopeController::GetController().Play( m_pLoopingSound, 0.0, 100 );
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pLoopingSound, 1.0, 1.0 );
	}
}

void C_ASW_Parasite::SoundShutdown()
{	
	if ( m_pLoopingSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pLoopingSound );
		m_pLoopingSound = NULL;
	}
}

bool C_ASW_Parasite::IsAimTarget()
{
	return BaseClass::IsAimTarget() && !m_bDoEggIdle && !m_bInfesting;
}