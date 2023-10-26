#include "cbase.h"
#include "c_asw_sentry_top.h"
#include "c_asw_sentry_base.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "c_te_legacytempents.h"
#include "c_asw_fx.h"
#include "c_user_message_register.h"
#include "ai_debug_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Sentry_Top, DT_ASW_Sentry_Top, CASW_Sentry_Top)
	RecvPropEHandle( RECVINFO( m_hSentryBase ) ),
	RecvPropInt( RECVINFO( m_iSentryAngle ) ),	
	RecvPropFloat( RECVINFO( m_fDeployYaw ) ),	
	RecvPropBool( RECVINFO( m_bLowAmmo ) ),	
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Sentry_Top )

END_PREDICTION_DATA()

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
//BEGIN_DATADESC( C_ASW_Sentry_Top )
//	DEFINE_FIELD( m_hSentryBase, FIELD_EHANDLE ),
//END_DATADESC()

C_ASW_Sentry_Top::C_ASW_Sentry_Top()
{
	m_bSpawnedDisplayEffects = false;
}

C_ASW_Sentry_Top::~C_ASW_Sentry_Top()
{
	if ( m_hRadiusDisplay )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_hRadiusDisplay );
		m_hRadiusDisplay = NULL;
	}

	if ( m_hWarningLight )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_hWarningLight );
		m_hWarningLight = NULL;
	}

	if ( m_hPilotLight )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_hPilotLight );
		m_hPilotLight = NULL;
	}
}

void C_ASW_Sentry_Top::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink(gpGlobals->curtime);
		m_fPrevDeployYaw = m_fDeployYaw;
	}

	if ( m_bLowAmmo && !m_hWarningLight )
	{
		m_hWarningLight = ParticleProp()->Create( "sentry_light_lowammo", PATTACH_ABSORIGIN_FOLLOW );
		if ( m_hWarningLight )
		{
			ParticleProp()->AddControlPoint( m_hWarningLight, 0, this, PATTACH_POINT_FOLLOW, "attach_light" );
		}
	}

	BaseClass::OnDataChanged( updateType );
}

void C_ASW_Sentry_Top::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();
	
	if ( m_hRadiusDisplay )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_hRadiusDisplay );
		m_hRadiusDisplay = NULL;
	}

	if ( m_hWarningLight )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_hWarningLight );
		m_hWarningLight = NULL;
	}

	if ( m_hPilotLight )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_hPilotLight );
		m_hPilotLight = NULL;
	}
}

void C_ASW_Sentry_Top::ClientThink()
{
	BaseClass::ClientThink();

	Scan();

	SetNextClientThink( gpGlobals->curtime + 0.05f );
}

int C_ASW_Sentry_Top::GetMuzzleAttachment( void )
{
	return LookupAttachment( "muzzle" );
}

float C_ASW_Sentry_Top::GetMuzzleFlashScale( void )
{
	return 2.0f;
}

void C_ASW_Sentry_Top::ProcessMuzzleFlashEvent()
{
	// attach muzzle flash particle system effect
	int iAttachment = GetMuzzleAttachment();		
	
	if ( iAttachment > 0 )
	{		
		float flScale = GetMuzzleFlashScale();
		FX_ASW_MuzzleEffectAttached( flScale, GetRefEHandle(), iAttachment, NULL, false );	
		//EjectParticleBrass( "weapon_shell_casing_rifle", iAttachment );
//#endif
	}
	BaseClass::ProcessMuzzleFlashEvent();
}

extern void ASWDoParticleTracer( const char *pTracerEffectName, const Vector &vecStart, const Vector &vecEnd, bool bRedTracer, int iAttributeEffects = 0 );
void C_ASW_Sentry_Top::ASWSentryTracer( const Vector &vecEnd )
{
	MDLCACHE_CRITICAL_SECTION();
	Vector vecStart;
	QAngle vecAngles;

	if ( IsDormant() )
		return;

	C_BaseAnimating::PushAllowBoneAccess( true, false, "sentgun" );

	// Get the muzzle origin
	if ( !GetAttachment( GetMuzzleAttachment(), vecStart, vecAngles ) )
	{
		return;
	}
	
	ASWDoParticleTracer( "tracer_autogun", vecStart, vecEnd, false );

	C_BaseAnimating::PopBoneAccess( "sentgun" );
}

void __MsgFunc_ASWSentryTracer( bf_read &msg )
{
	int iSentry = msg.ReadShort();		
	C_ASW_Sentry_Top *pSentry = dynamic_cast<C_ASW_Sentry_Top*>( ClientEntityList().GetEnt( iSentry ) );		// turn iMarine ent index into the marine

	Vector vecEnd;
	vecEnd.x = msg.ReadFloat();
	vecEnd.y = msg.ReadFloat();
	vecEnd.z = msg.ReadFloat();

	if ( pSentry )
	{
		pSentry->ASWSentryTracer( vecEnd );
	}
}
USER_MESSAGE_REGISTER( ASWSentryTracer );

C_ASW_Sentry_Base* C_ASW_Sentry_Top::GetSentryBase()
{	
	return dynamic_cast<C_ASW_Sentry_Base*>(m_hSentryBase.Get());
}

int C_ASW_Sentry_Top::GetSentryAngle( void )
{ 
	return m_iSentryAngle;
}

void C_ASW_Sentry_Top::Scan()
{
	C_ASW_Sentry_Base * RESTRICT pBase = GetSentryBase();
	if ( !pBase )
	{
		if ( m_hRadiusDisplay )
		{
			ParticleProp()->StopEmissionAndDestroyImmediately( m_hRadiusDisplay );
			m_hRadiusDisplay = NULL;
		}
		if ( m_hWarningLight )
		{
			ParticleProp()->StopEmissionAndDestroyImmediately( m_hWarningLight );
			m_hWarningLight = NULL;
		}
		return;
	}

	if ( pBase->GetAmmo() <= 0 )
	{
		if ( m_hRadiusDisplay )
		{
			ParticleProp()->StopEmissionAndDestroyImmediately( m_hRadiusDisplay );
			m_hRadiusDisplay = NULL;

			if ( m_hWarningLight )
			{
				ParticleProp()->StopEmissionAndDestroyImmediately( m_hWarningLight );
				m_hWarningLight = NULL;
			}

			if ( !m_hWarningLight )
			{
				m_hWarningLight = ParticleProp()->Create( "sentry_light_noammo", PATTACH_ABSORIGIN_FOLLOW );
				if ( m_hWarningLight )
				{
					ParticleProp()->AddControlPoint( m_hWarningLight, 0, this, PATTACH_POINT_FOLLOW, "attach_light" );
				}
			}
		}
		return;
	}
	else if ( !m_bLowAmmo && m_hWarningLight )
	{
		if ( m_hWarningLight )
		{
			m_hWarningLight->StopEmission(false, false , true);
			m_hWarningLight = NULL;
		}
	}

	//if( gpGlobals->curtime >= m_flTimeNextScanPing )
	//{
	//	m_flTimeNextScanPing = gpGlobals->curtime + 1.0f;
	//}

	QAngle	scanAngle;
	Vector	forward;
	Vector	vecEye = pBase->GetAbsOrigin() + Vector( 0, 0, 38 );// + m_vecLightOffset;

	if ( !m_hRadiusDisplay )
	{
		// this sets up the outter, "heavier" lines that make up the edges and the middle
		m_hRadiusDisplay = ParticleProp()->Create( "sentry_radius_display_beam", PATTACH_CUSTOMORIGIN );
		m_hRadiusDisplay->SetControlPoint( 0, vecEye );

		// Draw the outer extents
		scanAngle = pBase->GetAbsAngles();
		scanAngle.y -= GetSentryAngle();
		AngleVectors( scanAngle, &forward, NULL, NULL );
		CreateRadiusBeamEdges( vecEye, forward, 1 );

		scanAngle = pBase->GetAbsAngles();
		scanAngle.y += GetSentryAngle();
		AngleVectors( scanAngle, &forward, NULL, NULL );
		CreateRadiusBeamEdges( vecEye, forward, 2 );

		// scanAngle = pBase->GetAbsAngles();
		// AngleVectors( scanAngle, &forward, NULL, NULL );
		CreateRadiusBeamEdges( vecEye, pBase->Forward(), 3 );

		// create the sweeping beam control points
		ParticleProp()->AddControlPoint( m_hRadiusDisplay, 4, this, PATTACH_CUSTOMORIGIN );
		m_hRadiusDisplay->SetControlPointEntity( 4, this );
		ParticleProp()->AddControlPoint( m_hRadiusDisplay, 5, this, PATTACH_CUSTOMORIGIN );
		m_hRadiusDisplay->SetControlPointEntity( 5, this );
	}

	if ( m_hRadiusDisplay )
	{
		// now move the sweeping beams
		QAngle baseAngle = pBase->GetAbsAngles();
		baseAngle.y = m_fPrevDeployYaw;
		if ( m_fDeployYaw != m_fPrevDeployYaw )
		{
			//baseAngle.y = Approach( m_fDeployYaw, m_fPrevDeployYaw, 20.0f );
			//m_fPrevDeployYaw = baseAngle.y;

			float flDeltatime = 1.0f;
			float flDir = m_fDeployYaw > m_fPrevDeployYaw ? 1 : -1 ;
			float flDist = fabs(m_fDeployYaw - m_fPrevDeployYaw);

			if (flDist > 180)
			{
				flDist = 360 - flDist;
				flDir = -flDir;
			}

			// set our turn rate depending on if we have an enemy or not
			float fTurnRate = 10.0f;//ASW_SENTRY_TURNRATE * 0.5f;

			if (fabs(flDist) < flDeltatime * fTurnRate)
			{
				m_fPrevDeployYaw = m_fDeployYaw;
			}
			else
			{
				// turn it		
				m_fPrevDeployYaw += flDeltatime * fTurnRate * flDir;
			}

			if (m_fPrevDeployYaw < 0)
				m_fPrevDeployYaw += 360;
			else if (m_fPrevDeployYaw >= 360)
				m_fPrevDeployYaw -= 360;

			baseAngle.y = m_fPrevDeployYaw;
		}

		AngleVectors( baseAngle + QAngle( 0, -GetSentryAngle(), 0), &forward, NULL, NULL );
		AdjustRadiusBeamEdges( vecEye, forward, 1 );

		AngleVectors( baseAngle + QAngle( 0, GetSentryAngle(), 0), &forward, NULL, NULL );
		AdjustRadiusBeamEdges( vecEye, forward, 2 );

		AngleVectors( baseAngle, &forward, NULL, NULL );
		AdjustRadiusBeamEdges( vecEye, forward, 3 );

		scanAngle = baseAngle;
		scanAngle.y += GetSentryAngle() * sin( gpGlobals->curtime * 3.0f );

		Vector	vecBase = pBase->GetAbsOrigin() + Vector( 0, 0, 2 );// + m_vecLightOffset;
		AngleVectors( scanAngle, &forward, NULL, NULL );

		m_hRadiusDisplay->SetControlPoint( 4, vecBase + forward * 190.0f );
		m_hRadiusDisplay->SetControlPoint( 5, vecBase + forward * 80.0f );
	}
}

void C_ASW_Sentry_Top::CreateRadiusBeamEdges( const Vector &vecStart, const Vector &vecDir, int iControlPoint )
{
	if ( !m_hRadiusDisplay )
		return;

	ParticleProp()->AddControlPoint( m_hRadiusDisplay, iControlPoint, this, PATTACH_CUSTOMORIGIN );
	m_hRadiusDisplay->SetControlPointEntity( iControlPoint, this );

	AdjustRadiusBeamEdges( vecStart, vecDir, iControlPoint );
}

void C_ASW_Sentry_Top::AdjustRadiusBeamEdges( const Vector &vecStart, const Vector &vecDir, int iControlPoint )
{
	CTraceFilterSkipTwoEntities traceFilter( this, GetSentryBase(), COLLISION_GROUP_DEBRIS );

	trace_t tr;
	AI_TraceLine( vecStart, vecStart + vecDir * 500.0f, MASK_SHOT, &traceFilter, &tr );

	m_hRadiusDisplay->SetControlPoint( iControlPoint, tr.endpos );
}


