//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=====================================================================================//
#include "cbase.h"
#include "c_physicsprop.h"
#include "portal_gamerules.h"
#include "igameevents.h"
#include "soundenvelope.h"
#include "beamdraw.h"


//#define RADIO_DEBUG_CLIENT


class C_Dinosaur_Signal : public C_BaseEntity
{
public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( C_Dinosaur_Signal, C_BaseEntity );

	char  m_szSoundName[128];
	float m_flInnerRadius;
	float m_flOuterRadius;
	int	  m_nSignalID;
};

IMPLEMENT_CLIENTCLASS_DT( C_Dinosaur_Signal, DT_DinosaurSignal, CDinosaurSignal )
	RecvPropString( RECVINFO(m_szSoundName) ), 
	RecvPropFloat( RECVINFO(m_flOuterRadius) ),
	RecvPropFloat( RECVINFO(m_flInnerRadius) ),
	RecvPropInt( RECVINFO(m_nSignalID) ),
END_RECV_TABLE()

class C_Portal_Dinosaur : public C_PhysicsProp
{
public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( C_Portal_Dinosaur, C_PhysicsProp );

				~C_Portal_Dinosaur( void );

	virtual void Spawn();
	virtual void Precache();
	virtual void ClientThink();
	virtual	int DrawModel( int flags );

	void ScanForSounds();
	void SetupSounds();

	CHandle<C_Dinosaur_Signal> m_hDinosaur_Signal;
	float	m_flOldBlend;
	bool	m_bAlreadyDiscovered;
	bool	m_bDinosaurExtinct;

	// Sound envelopes
	CSoundPatch		*m_pNormalSound;
	CSoundPatch		*m_pStaticSound;
	CSoundPatch		*m_pSignalSound;
};

IMPLEMENT_CLIENTCLASS_DT( C_Portal_Dinosaur, DT_PropDinosaur, CPortal_Dinosaur )
	RecvPropEHandle( RECVINFO( m_hDinosaur_Signal) ),
	RecvPropBool( RECVINFO(m_bAlreadyDiscovered) ),
END_RECV_TABLE()

void C_Portal_Dinosaur::Spawn()
{
	Precache();

	m_flOldBlend = 0.0f;
	BaseClass::Spawn();
	SetThink( &C_Portal_Dinosaur::ClientThink );
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

C_Portal_Dinosaur::~C_Portal_Dinosaur( void )
{
	if ( m_pNormalSound != NULL )
	{
		CSoundEnvelopeController::GetController().Shutdown( m_pNormalSound );
	}

	if ( m_pStaticSound != NULL )
	{
		CSoundEnvelopeController::GetController().Shutdown( m_pStaticSound );
	}

	if ( m_pSignalSound != NULL )
	{
		CSoundEnvelopeController::GetController().Shutdown( m_pSignalSound );
	}
}

void C_Portal_Dinosaur::Precache( void )
{
	PrecacheScriptSound( "Portal.room1_radio" );
	PrecacheScriptSound( "UpdateItem.Static" );
	PrecacheScriptSound( "UpdateItem.Dinosaur01" );
	PrecacheScriptSound( "UpdateItem.Dinosaur02" );
	PrecacheScriptSound( "UpdateItem.Dinosaur03" );
	PrecacheScriptSound( "UpdateItem.Dinosaur04" );
	PrecacheScriptSound( "UpdateItem.Dinosaur05" );
	PrecacheScriptSound( "UpdateItem.Dinosaur06" );
	PrecacheScriptSound( "UpdateItem.Dinosaur07" );
	PrecacheScriptSound( "UpdateItem.Dinosaur08" );
	PrecacheScriptSound( "UpdateItem.Dinosaur09" );
	PrecacheScriptSound( "UpdateItem.Dinosaur10" );
	PrecacheScriptSound( "UpdateItem.Dinosaur11" );
	PrecacheScriptSound( "UpdateItem.Dinosaur12" );
	PrecacheScriptSound( "UpdateItem.Dinosaur13" );
	PrecacheScriptSound( "UpdateItem.Dinosaur14" );
	PrecacheScriptSound( "UpdateItem.Dinosaur15" );
	PrecacheScriptSound( "UpdateItem.Dinosaur16" );
	PrecacheScriptSound( "UpdateItem.Dinosaur17" );
	PrecacheScriptSound( "UpdateItem.Dinosaur18" );
	PrecacheScriptSound( "UpdateItem.Dinosaur19" );
	PrecacheScriptSound( "UpdateItem.Dinosaur20" );
	PrecacheScriptSound( "UpdateItem.Dinosaur21" );
	PrecacheScriptSound( "UpdateItem.Dinosaur22" );
	PrecacheScriptSound( "UpdateItem.Dinosaur23" );
	PrecacheScriptSound( "UpdateItem.Dinosaur24" );
	PrecacheScriptSound( "UpdateItem.Dinosaur25" );
	PrecacheScriptSound( "UpdateItem.Dinosaur26" );
}

void C_Portal_Dinosaur::SetupSounds()
{
	CPASAttenuationFilter filter( this );

	if ( m_pNormalSound == NULL )
	{
		m_pNormalSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "Portal.room1_radio" );
		CSoundEnvelopeController::GetController().Play( m_pNormalSound, 0.0, PITCH_NORM );
	}

	if ( m_pStaticSound == NULL )
	{
		m_pStaticSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "UpdateItem.Static" );
		CSoundEnvelopeController::GetController().Play( m_pStaticSound, 0.0, PITCH_NORM );
	}

	if ( m_pSignalSound == NULL && 	m_hDinosaur_Signal.Get() != NULL )
	{
		m_pSignalSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), m_hDinosaur_Signal->m_szSoundName );
		//m_pSignalSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "UpdateItem.Signal" );
		CSoundEnvelopeController::GetController().Play( m_pSignalSound, 0.0, PITCH_NORM );
	}
}

void C_Portal_Dinosaur::ClientThink()
{
	SetupSounds();

	//if ( V_stristr( engine->GetLevelName(), "testchmb_a_00" ) != 0 )
	if ( 0 )//m_hDinosaur_Signal.Get() && m_hDinosaur_Signal.Get()->m_nSignalID == 0 )
	{
		// TODO:Play either morse code or special sstv content based on 
		// game account client state given from gc.
	}
	else
	{
		// On any other map, we scan for the partnered Dinosaur_Signal signal
		ScanForSounds();
	}

	if ( m_bDinosaurExtinct == false && m_bDinosaurExtinct != m_bAlreadyDiscovered )
		m_bDinosaurExtinct = m_bAlreadyDiscovered;
}

void C_Portal_Dinosaur::ScanForSounds()
{
	if ( m_hDinosaur_Signal.Get() == NULL )
	{
		AssertMsgOnce( 0, "Unpaired radio exists on this map." );
		return;
	}

	// How much of the real signal to blend in
	// 1.0 at > outerrad, 0.0 at < inner rad, blend when in between the two.
	float flRadiusDelta = fabs( m_hDinosaur_Signal.Get()->m_flOuterRadius - m_hDinosaur_Signal.Get()->m_flInnerRadius );
	float flDist = m_hDinosaur_Signal.Get()->GetAbsOrigin().DistTo( GetAbsOrigin() );

	float flOuterBlend = RemapValClamped( flDist, m_hDinosaur_Signal.Get()->m_flOuterRadius, m_hDinosaur_Signal.Get()->m_flOuterRadius-(flRadiusDelta*0.5f), 1.0f, 0.0f );
	float flInnerBlend = RemapValClamped( flDist, m_hDinosaur_Signal.Get()->m_flOuterRadius-(flRadiusDelta*0.5f), m_hDinosaur_Signal.Get()->m_flInnerRadius, 0.0f, 1.0f );
	flInnerBlend = Bias( flInnerBlend, 0.1f );

	if ( m_pNormalSound )
	{
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pNormalSound, flOuterBlend, 0.1f );
	}

	float flMidBlend = 0.0f;
	if ( m_pStaticSound )
	{
		if ( flOuterBlend <= 0.0f )
		{
			flMidBlend = RemapValClamped( flDist, m_hDinosaur_Signal.Get()->m_flOuterRadius-(flRadiusDelta*0.5f), m_hDinosaur_Signal.Get()->m_flInnerRadius, 1.0f, 0.0f );
		}
		else
		{
			flMidBlend = RemapValClamped( flDist, m_hDinosaur_Signal.Get()->m_flOuterRadius, m_hDinosaur_Signal.Get()->m_flOuterRadius-(flRadiusDelta*0.5f), 0.0f, 1.0f );
			flMidBlend = Bias( flMidBlend, 0.9f );
		}
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pStaticSound, flMidBlend, 0.1f );
	}

	if ( m_pSignalSound )
	{
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pSignalSound, flInnerBlend, 0.1f );
	}

#if defined ( RADIO_DEBUG_CLIENT )
	// Msg( "Dinosaur(%d) listening for signal... dist: %f, blend amt: %f\n", entindex(), flDist, flMidBlend );
#endif
	
	// If we've fully heard this signal, mark the achievement
	if ( flInnerBlend >= 1.0f && m_flOldBlend < 1.0f )
	{
		m_bDinosaurExtinct = true;
		int id = m_hDinosaur_Signal.Get()->m_nSignalID;

		IGameEvent *event = gameeventmanager->CreateEvent( "dinosaur_signal_found" );
		if ( event )
		{
			event->SetInt( "id", id );
			gameeventmanager->FireEvent( event );
		}
	}

	m_flOldBlend = flInnerBlend;
}

int C_Portal_Dinosaur::DrawModel( int flags )
{
	int nRet = BaseClass::DrawModel( flags );

	CMaterialReference	hMaterial;
	hMaterial.Init( "sprites/grav_light", TEXTURE_GROUP_CLIENT_EFFECTS );

	// Draw the sprite
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( hMaterial, this );
	color32 color;
	color.r = ( m_bDinosaurExtinct ) ? 0 : 255;
	color.g = ( m_bDinosaurExtinct ) ? 255 : 0;
	color.b = 0;
	color.a = 128;

	Vector vForward, vRight, vUp;
	GetVectors( &vForward, &vRight, &vUp );
	Vector vOffset = GetAbsOrigin() + ( vForward * 4.0f ) + ( vUp * 1.85f );
	DrawSprite( vOffset, 6.0f, 6.0f, color );

	return nRet;
}
