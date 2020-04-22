//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Clients CBaseObject
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_baseobject.h"
#include "c_basetfplayer.h"
#include "hud.h"
#include "c_tfteam.h"
#include "engine/IEngineSound.h"
#include "particles_simple.h"
#include "functionproxy.h"
#include "IEffects.h"
#include "c_hint_events.h"
#include "model_types.h"
#include "particlemgr.h"
#include "particle_collision.h"
#include "env_objecteffects.h"
#include "basetfvehicle.h"
#include "c_weapon_builder.h"
#include "ivrenderview.h"
#include "ObjectControlPanel.h"
#include "engine/ivmodelinfo.h"
#include "c_te_effect_dispatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MAX_VISIBLE_BUILDPOINT_DISTANCE		(400 * 400)

// Remove aliasing of name due to shared code
#undef CBaseObject

IMPLEMENT_CLIENTCLASS_DT(C_BaseObject, DT_BaseObject, CBaseObject)
	RecvPropInt(RECVINFO(m_iHealth)),
	RecvPropInt(RECVINFO(m_iMaxHealth)),
	RecvPropInt(RECVINFO(m_bHasSapper)),
	RecvPropInt(RECVINFO(m_iObjectType)),
	RecvPropInt(RECVINFO(m_bBuilding)),
	RecvPropInt(RECVINFO(m_bPlacing)),
	RecvPropFloat(RECVINFO(m_flPercentageConstructed)),
	RecvPropInt(RECVINFO(m_fObjectFlags)),
	RecvPropInt(RECVINFO(m_bDeteriorating)),
	RecvPropEHandle(RECVINFO(m_hBuiltOnEntity)),
	RecvPropInt(RECVINFO( m_takedamage ) ),
	RecvPropInt( RECVINFO( m_bDisabled ) ),
	RecvPropEHandle( RECVINFO( m_hBuilder ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BaseObject::C_BaseObject(  )
{
	m_flDamageFlash = 0;
	m_YawPreviewState = YAW_PREVIEW_OFF;
	m_bBuilding = false;
	m_bPlacing = false;
	m_flPercentageConstructed = 0;
	m_flNextEffect = 0;
	m_bOldSapper = m_bHasSapper = false;
	m_fObjectFlags = 0;
	m_bDeteriorating = false;
	m_ThermalMaterial.Init("player/thermal/thermal",TEXTURE_GROUP_CLIENT_EFFECTS);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BaseObject::~C_BaseObject( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseObject::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );

	m_iOldHealth = m_iHealth;
 	m_bOldSapper = m_bHasSapper;
	m_hOldOwner = GetOwner();
	m_bWasActive = ShouldBeActive();
	m_bWasBuilding = m_bBuilding;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseObject::OnDataChanged( DataUpdateType_t updateType )
{
	if (updateType == DATA_UPDATE_CREATED)
	{
		if ( !IS_MINIMAP_PANEL_DEFINED( ) && !(m_fObjectFlags & OF_SUPPRESS_APPEAR_ON_MINIMAP) )
		{
			CONSTRUCT_MINIMAP_PANEL( "minimap_object", MINIMAP_OBJECTS );
		}

		CreateBuildPoints();
	}

	BaseClass::OnDataChanged( updateType );

	// Did we just finish building?
	if ( m_bWasBuilding && !m_bBuilding )
	{
		FinishedBuilding();
	}

	// Did we just go active?
	bool bShouldBeActive = ShouldBeActive();
	if ( !m_bWasActive && bShouldBeActive )
	{
		OnGoActive();
	}
	else if ( m_bWasActive && !bShouldBeActive )
	{
		OnGoInactive();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseObject::SetDormant( bool bDormant )
{
	BaseClass::SetDormant( bDormant );
	//ENTITY_PANEL_ACTIVATE( "analyzed_object", !bDormant );
}

#define TF_OBJ_BODYGROUPTURNON			1
#define TF_OBJ_BODYGROUPTURNOFF			0

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			angles - 
//			event - 
//			*options - 
//-----------------------------------------------------------------------------
void C_BaseObject::FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options )
{
	switch ( event )
	{
	default:
		{
			BaseClass::FireEvent( origin, angles, event, options );
		}
		break;
	case TF_OBJ_PLAYBUILDSOUND:
		{
			EmitSound( options );
		}
		break;
	case TF_OBJ_ENABLEBODYGROUP:
		{
			int index = FindBodygroupByName( options );
			if ( index >= 0 )
			{
				SetBodygroup( index, TF_OBJ_BODYGROUPTURNON );
			}
		}
		break;
	case TF_OBJ_DISABLEBODYGROUP:
		{
			int index = FindBodygroupByName( options );
			if ( index >= 0 )
			{
				SetBodygroup( index, TF_OBJ_BODYGROUPTURNOFF );
			}
		}
		break;
	case TF_OBJ_ENABLEALLBODYGROUPS:
	case TF_OBJ_DISABLEALLBODYGROUPS:
		{
			// Start at 1, because body 0 is the main .mdl body...
			// Is this the way we want to do this?
			int count = GetNumBodyGroups();
			for ( int i = 1; i < count; i++ )
			{
				int subpartcount = GetBodygroupCount( i );
				if ( subpartcount == 2 )
				{
					SetBodygroup( i, 
						( event == TF_OBJ_ENABLEALLBODYGROUPS ) ?
						TF_OBJ_BODYGROUPTURNON : TF_OBJ_BODYGROUPTURNOFF );
				}
				else
				{
					DevMsg( "TF_OBJ_ENABLE/DISABLEBODY GROUP:  %s has a group with %i subparts, should be exactly 2\n",
						GetClassname(), subpartcount );
				}
			}
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseObject::OffsetObjectOrigin( Vector& origin )
{
	if ( !m_bBuilding )
		return false;
	
	if ( inv_demo.GetBool() )
		return false;

	Vector vecWorldMins, vecWorldMaxs;
	CollisionProp()->WorldSpaceAABB( &vecWorldMins, &vecWorldMaxs );
	float flSize = vecWorldMaxs.z - vecWorldMins.z;
	origin.z -= (flSize * (1 - m_flPercentageConstructed));

	// If we're building, fake sliding the object out of the ground
	return true;
}


const char* C_BaseObject::GetStatusName() const
{
	return GetObjectInfo( GetType() )->m_pStatusName;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_BaseObject::DrawModel( int flags )
{
	Vector vRealOrigin = GetLocalOrigin();
	Vector vOrigin = vRealOrigin;
	bool needOriginReset = OffsetObjectOrigin( vOrigin );
	if ( needOriginReset )
	{
		SetLocalOrigin( vOrigin );
		InvalidateBoneCache();
	}

	int drawn;
	C_BaseTFPlayer *pLocal = C_BaseTFPlayer::GetLocalPlayer();
	if ( pLocal && pLocal->IsUsingThermalVision() )
	{
		modelrender->ForcedMaterialOverride( m_ThermalMaterial );
		drawn = BaseClass::DrawModel(flags);
		modelrender->ForcedMaterialOverride( NULL );
	}
	else
	{
		// If we're a brush-built, map-defined object chain up to baseentity draw
		if ( modelinfo->GetModelType( GetModel() ) == mod_brush )
		{
			drawn = CBaseEntity::DrawModel(flags);
		}
		else
		{
			drawn = BaseClass::DrawModel(flags);
		}
	}

	// Restore faked origin
	if ( needOriginReset )
	{
		SetLocalOrigin( vRealOrigin );
	}

	// If we were drawn, draw building effects if we're building, or damage effects if we're damaged
	if ( drawn && (m_flNextEffect < gpGlobals->curtime) )
	{
		// Haxory LOD
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		if ( (GetAbsOrigin() - pPlayer->GetAbsOrigin()).LengthSqr() < lod_effect_distance.GetFloat() )
		{
			if ( IsBuilding() )
			{
				DrawBuildEffects();
			}
			if ( !m_bPlacing && !m_bBuilding )
			{
				if ( !HasPowerup( POWERUP_EMP ) )
				{
					DrawRunningEffects();
				}

				if ( GetHealth() < GetMaxHealth() )
				{
					DrawDamageEffects();
				}
			}
		}
	}

	HighlightBuildPoints( flags );

	return drawn;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseObject::HighlightBuildPoints( int flags )
{
	C_BaseTFPlayer *pLocal = C_BaseTFPlayer::GetLocalPlayer();
	if ( !pLocal )
		return;

	if ( !GetNumBuildPoints() || !InLocalTeam() )
		return;

	C_WeaponBuilder *pBuilderWpn = dynamic_cast< C_WeaponBuilder * >( pLocal->GetActiveWeaponForSelection() );
	if ( !pBuilderWpn )
		return;
	if ( !pBuilderWpn->IsPlacingObject() )
		return;
	C_BaseObject *pPlacementObj = pBuilderWpn->GetPlacementModel();
	if ( !pPlacementObj || pPlacementObj == this )
		return;

	// Near enough?
	if ( (GetAbsOrigin() - pLocal->GetAbsOrigin()).LengthSqr() < MAX_VISIBLE_BUILDPOINT_DISTANCE )
	{
		bool bRestoreModel = false;
		Vector vecPrevAbsOrigin = pPlacementObj->GetAbsOrigin();
		QAngle vecPrevAbsAngles = pPlacementObj->GetAbsAngles();

		Vector orgColor;
		render->GetColorModulation( orgColor.Base() );
		float orgBlend = render->GetBlend();

		// Any empty buildpoints?
		for ( int i = 0; i < GetNumBuildPoints(); i++ )
		{
			// Can this object build on this point?
			if ( CanBuildObjectOnBuildPoint( i, pPlacementObj->GetType() ) )
			{
				Vector vecBPOrigin;
				QAngle vecBPAngles;
				if ( GetBuildPoint(i, vecBPOrigin, vecBPAngles) )
				{
					pPlacementObj->InvalidateBoneCaches();

					Vector color( 0, 255, 0 );
					render->SetColorModulation(	color.Base() );
					float frac = fmod( gpGlobals->curtime, 3 );
					frac *= 2 * M_PI;
					frac = cos( frac );
					render->SetBlend( (175 + (int)( frac * 75.0f )) / 255.0 );

					// HACK: Fixup angles on the HL2 model we're using
					if ( !strcmp( modelinfo->GetModelName( pPlacementObj->GetModel() ), "models/items/HealthKit.mdl" ) )
					{
						vecBPAngles.x += 90;
					}

					// FIXME: This truly sucks! The bone cache should use
					// render location for this computation instead of directly accessing AbsAngles
					// Necessary for bone cache computations to work
					pPlacementObj->SetAbsOrigin( vecBPOrigin );
					pPlacementObj->SetAbsAngles( vecBPAngles );


					modelrender->DrawModel( 
						flags, 
						pPlacementObj,
						pPlacementObj->GetModelInstance(),
						pPlacementObj->index, 
						pPlacementObj->GetModel(),
						vecBPOrigin,
						vecBPAngles,
						pPlacementObj->m_nSkin,
						pPlacementObj->m_nBody,
						pPlacementObj->m_nHitboxSet
						);

					bRestoreModel = true;
				}
			}
		}

		if ( bRestoreModel )
		{
			pPlacementObj->SetAbsOrigin(vecPrevAbsOrigin);
			pPlacementObj->SetAbsAngles(vecPrevAbsAngles);
			pPlacementObj->InvalidateBoneCaches();

			render->SetColorModulation( orgColor.Base() );
			render->SetBlend( orgBlend );
		}
	}
}


//-----------------------------------------------------------------------------
// Exit points for mounted vehicles....
//-----------------------------------------------------------------------------
void C_BaseObject::GetExitPoint( CBaseEntity *pPlayer, int nBuildPoint, Vector *pAbsPosition, QAngle *pAbsAngles )
{
	Assert(0);
}


//-----------------------------------------------------------------------------
// Purpose: Overridden to allow for brush-built map defined objects
//-----------------------------------------------------------------------------
bool C_BaseObject::IsIdentityBrush( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Builder preview...
//-----------------------------------------------------------------------------
void C_BaseObject::ActivateYawPreview( bool enable )
{
	m_YawPreviewState = enable ? YAW_PREVIEW_ON : YAW_PREVIEW_WAITING_FOR_UPDATE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseObject::PreviewYaw( float yaw )
{
	m_fYawPreview = yaw;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseObject::IsPreviewingYaw() const
{
	return m_YawPreviewState != YAW_PREVIEW_OFF;
}

//-----------------------------------------------------------------------------
// Purpose: This is called to get the initial builder yaw...
//-----------------------------------------------------------------------------
float C_BaseObject::GetInitialBuilderYaw()
{
	return GetAbsAngles().y;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseObject::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	bool bNewEntity = (updateType == DATA_UPDATE_CREATED);
	if ( bNewEntity )
	{
		m_flAttackTime = -1000;
	}

	// Determine if we're under attack
	if ( !bNewEntity )
	{
		if ( m_iHealth < m_iOldHealth )
		{
			// Deteriorating objects don't play sounds
			if ( !IsDeteriorating() )
			{
				m_flAttackTime = gpGlobals->curtime;
			}
		}
		else if ( m_iHealth > m_iOldHealth && m_iHealth == m_iMaxHealth )
		{
			// If we were just fully healed, remove all decals
			RemoveAllDecals();
		}
	}

	if ( m_bHasSapper ) 
	{
		// Play a specific sound for a sapper...
		if ( m_bOldSapper != m_bHasSapper )
		{
			// Don't create these for dragonsteeth
			if ( InLocalTeam() && GetType() != OBJ_DRAGONSTEETH )
			{
				// Play a sound.
				CLocalPlayerFilter filter;
				EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "BaseObject.SapperDestroyingTeamBuilding" );

				MinimapCreateTempTrace( "minimap_under_attack", MINIMAP_PERSONAL_ORDERS, GetAbsOrigin() );
			}
		}
	}

	// Notify the hint system of the object being built.
	if ( bNewEntity && GetOwner() && ( GetOwner() == C_BasePlayer::GetLocalPlayer() ) )
	{
		C_HintEvent_ObjectBuiltByLocalPlayer event( this );
		GlobalHintEvent( &event );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseObject::Release( void )
{
	// Remove any reticles on this entity
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		pPlayer->Remove_Target( this );
	}

	BaseClass::Release();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseObject::IsUnderAttack( )
{
	// It's under attack for the 3 seconds after the last attack time
	return (gpGlobals->curtime - m_flAttackTime) < 5.0f;
}

//-----------------------------------------------------------------------------
// Ownership: 
//-----------------------------------------------------------------------------
C_BaseTFPlayer *C_BaseObject::GetOwner()
{
	return m_hBuilder;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseObject::IsOwnedByLocalPlayer() const
{
	if ( !m_hBuilder )
		return false;

	return ( m_hBuilder == C_BaseTFPlayer::GetLocalPlayer() );
}

//-----------------------------------------------------------------------------
// Purpose: Add entity to visibile entities list
//-----------------------------------------------------------------------------
void C_BaseObject::AddEntity( void )
{
	// If set to invisible, skip. Do this before resetting the entity pointer so it has 
	// valid data to decide whether it's visible.
	if ( !ShouldDraw() )
	{
		return;
	}

	// Update the entity position
	UpdatePosition();

	// Yaw preview
	if (m_YawPreviewState != YAW_PREVIEW_OFF)
	{
		// This piece of code makes it so we keep using the preview
		// until we get a network update which matches the update value
		if (m_YawPreviewState == YAW_PREVIEW_WAITING_FOR_UPDATE)
		{
			if (fmod( fabs(GetLocalAngles().y - m_fYawPreview), 360.0f) < 1.0f)
			{
				m_YawPreviewState = YAW_PREVIEW_OFF;
			}
		}

		if (GetLocalOrigin().y != m_fYawPreview)
		{
			SetLocalAnglesDim( Y_INDEX, m_fYawPreview );
			InvalidateBoneCache();
		}
	}

	// Create flashlight effects, etc.
	CreateLightEffects();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseObject::Select( void )
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	pPlayer->SetSelectedObject( this );
}

//-----------------------------------------------------------------------------
// Sends client commands back to the server: 
//-----------------------------------------------------------------------------
void C_BaseObject::SendClientCommand( const char *pCmd )
{
	char szbuf[128];
	Q_snprintf( szbuf, sizeof( szbuf ), "objcmd %d %s", entindex(), pCmd );
  	engine->ClientCmd(szbuf);
}


//-----------------------------------------------------------------------------
// Purpose: Get a text description for the object target
//-----------------------------------------------------------------------------
const char *C_BaseObject::GetTargetDescription( void ) const
{
	return GetStatusName();
}

//-----------------------------------------------------------------------------
// Purpose: Get a text description for the object target (more verbose)
//-----------------------------------------------------------------------------
char *C_BaseObject::GetIDString( void )
{
	m_szIDString[0] = 0;
	RecalculateIDString();
	return m_szIDString;
}


//-----------------------------------------------------------------------------
// It's a valid ID target when it's building 
//-----------------------------------------------------------------------------
bool C_BaseObject::IsValidIDTarget( void )
{
	return InSameTeam( C_BaseTFPlayer::GetLocalPlayer() ) && m_bBuilding;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseObject::RecalculateIDString( void )
{
	// Subclasses may have filled this out with a string
	if ( !m_szIDString[0] )
	{
		Q_strncpy( m_szIDString, GetTargetDescription(), sizeof(m_szIDString) );
	}

	// Have I taken damage?
	if ( m_iHealth < m_iMaxHealth )
	{
		char szHealth[ MAX_ID_STRING ];
		if ( IsDeteriorating() )
		{
			Q_snprintf( szHealth, sizeof(szHealth), "\nBUILDER LOST, DETERIORATING... %.0f percent", ceil(((float)m_iHealth / (float)m_iMaxHealth) * 100) );
		}
		else if ( m_bBuilding )
		{
			Q_snprintf( szHealth, sizeof(szHealth), "\nConstruction at %.0f percent\nHealth at %.0f percent", (m_flPercentageConstructed * 100), ceil(((float)m_iHealth / (float)m_iMaxHealth) * 100) );
		}
		else
		{
			Q_snprintf( szHealth, sizeof(szHealth), "\nHealth at %.0f percent", ceil(((float)m_iHealth / (float)m_iMaxHealth) * 100) );
		}
		Q_strncat( m_szIDString, szHealth, sizeof(m_szIDString), COPY_ALL_CHARACTERS );
	}

	if ( m_bHasSapper )
	{
		Q_strncat( m_szIDString, "\nUse it to remove the attached enemy object", sizeof(m_szIDString), COPY_ALL_CHARACTERS );
	}

	// If it's deteriorating, and I can buy it, tell me
	C_BaseTFPlayer *pLocalPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( IsDeteriorating() && pLocalPlayer && ClassCanBuild( pLocalPlayer->PlayerClass(), GetType() ) )
	{
		char szBuy[ MAX_ID_STRING ];
		int iCost = CalculateObjectCost( GetType(), pLocalPlayer->GetNumObjects( GetType() ), pLocalPlayer->GetTeamNumber() );
		Q_snprintf( szBuy, sizeof(szBuy), "\nBUY THIS OBJECT FOR %d RESOURCES", iCost );
		Q_strncat( m_szIDString, szBuy, sizeof(m_szIDString), COPY_ALL_CHARACTERS );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Effects created when the object's running
//-----------------------------------------------------------------------------
void C_BaseObject::DrawRunningEffects( void )
{
	if ( !GetMaxHealth() )
		return;

	// Get the overall damage percentage
	float flDamaged = 1.0 - ((float)GetHealth() / (float)GetMaxHealth());

	// Damage attachment points
	int iSmokeAttachment, iSparkAttachment;
	Vector vecSmoke, vecSpark, vecSmokeDir, dir;
	QAngle angSmoke, angSpark;

	// Look for damage points
	iSmokeAttachment = LookupRandomAttachment( "r_smoke" );

	// Get the points
	if ( GetAttachment( iSmokeAttachment, vecSmoke, angSmoke ) )
	{
		AngleVectors( angSmoke, &vecSmokeDir);
	
		float r, g, b;
		r = g = b = random->RandomFloat( 16, 92 );

		// Smoke
		CSmartPtr<CObjectSmokeParticles> pSmokeEmitter = CObjectSmokeParticles::Create( "DrawRunningEffects 1" );
		pSmokeEmitter->SetSortOrigin( vecSmoke );
		ObjectSmokeParticle *pParticle = (ObjectSmokeParticle *) pSmokeEmitter->AddParticle( sizeof(ObjectSmokeParticle), g_Mat_DustPuff[1], vecSmoke );
		if ( pParticle )
		{
			pParticle->m_flLifetime	= 0.0f;
			pParticle->m_flDieTime	= random->RandomFloat( 2.0f, 3.0f );
			pParticle->m_uchStartSize = random->RandomFloat( 2, 3 );
			pParticle->m_uchEndSize = random->RandomFloat( 5, 10 );
			dir[0] = vecSmokeDir[0] + random->RandomFloat( -0.1f, 0.1f );
			dir[1] = vecSmokeDir[1] + random->RandomFloat( -0.1f, 0.1f );
			dir[2] = vecSmokeDir[2] + random->RandomFloat( -0.1f, 0.1f );
			pParticle->m_vecVelocity = dir * random->RandomFloat( 30.0f, 40.0f );
			pParticle->m_uchStartAlpha	= random->RandomFloat( 128,255 );
			pParticle->m_uchEndAlpha	= 0;
			pParticle->m_flRoll			= random->RandomFloat( 180, 360 );
			pParticle->m_flRollDelta	= random->RandomFloat( -1, 1 );
			pParticle->m_uchColor[0] = r;
			pParticle->m_uchColor[1] = g;
			pParticle->m_uchColor[2] = b;
			pParticle->m_vecAcceleration = Vector(0,0,10);
		}
	}

	// Sparks
	for ( float flSparks = flDamaged - 0.3; flSparks > 0; flSparks -= 0.3 )
	{
		// Get random spark attachment point
		iSparkAttachment = LookupRandomAttachment( "r_spark" );
		if ( GetAttachment( iSparkAttachment, vecSpark, angSpark ) )
		{
			g_pEffects->Sparks( vecSpark );
		}
	}

	m_flNextEffect = gpGlobals->curtime + random->RandomFloat( 0.05, 0.1 );
}

//-----------------------------------------------------------------------------
// Purpose: Effects created while the object's building itself
//-----------------------------------------------------------------------------
void C_BaseObject::DrawBuildEffects( void )
{
	m_flNextEffect = gpGlobals->curtime + 10;
}

//-----------------------------------------------------------------------------
// Purpose: Effects created when the object's damaged
//-----------------------------------------------------------------------------
void C_BaseObject::DrawDamageEffects( void )
{
	if ( !GetMaxHealth() )
		return;

	// Get the overall damage percentage
	float flDamaged = 1.0 - ((float)GetHealth() / (float)GetMaxHealth());

	// Damage attachment points
	int iSmokeAttachment, iFireAttachment, iSparkAttachment;
	Vector vecSmoke, vecFire, vecSpark, vecSmokeDir, dir;
	QAngle angSmoke, angFire, angSpark;

	// HACK: Calculate a random origin
	// This can go away when we require all objects to have damage attachment points
	Vector vecOrigin;
	CollisionProp()->RandomPointInBounds( vec3_origin, Vector( 1, 1, 1 ), &vecOrigin );

	// Look for damage points
	iSmokeAttachment = LookupRandomAttachment( "d_smoke" );
	iFireAttachment = LookupRandomAttachment( "d_fire" );

	// Get the points, and if we can't find 'em, use the random origin
	if ( GetAttachment( iSmokeAttachment, vecSmoke, angSmoke ) )
	{
		AngleVectors( angSmoke, &vecSmokeDir );
	}
	else
	{
		vecSmoke = vecOrigin;
		vecSmokeDir = Vector(0,0,1);
	}
	if ( !GetAttachment( iFireAttachment, vecFire, angFire ) )
	{
		vecFire = vecOrigin;
		angFire = QAngle(0,0,0);
	}
	
	float r, g, b;
	r = g = b = random->RandomFloat( 16, 92 );

	// Smoke
	CSmartPtr<CSimpleEmitter> pSmokeEmitter = CSimpleEmitter::Create( "DrawDamageEffects 1" );
	pSmokeEmitter->SetSortOrigin( vecSmoke );
	SimpleParticle *pParticle = (SimpleParticle *) pSmokeEmitter->AddParticle( sizeof(SimpleParticle), g_Mat_DustPuff[1], vecSmoke );
	if ( pParticle )
	{
		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= random->RandomFloat( 2.0f, 3.0f );
		pParticle->m_uchStartSize = MAX( 1, 20 * flDamaged );
		pParticle->m_uchEndSize = MAX( 10, 80 * flDamaged );
		dir[0] = vecSmokeDir[0] + random->RandomFloat( -0.2f, 0.2f );
		dir[1] = vecSmokeDir[1] + random->RandomFloat( -0.2f, 0.2f );
		dir[2] = vecSmokeDir[2] + random->RandomFloat( -0.2f, 0.2f );
		pParticle->m_vecVelocity = dir * random->RandomFloat( 60.0f, 80.0f );
		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 0;
		pParticle->m_flRoll			= random->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -1, 1 );
		pParticle->m_uchColor[0] = r;
		pParticle->m_uchColor[1] = g;
		pParticle->m_uchColor[2] = b;
	}

	// If we're really hurt, start burning
	if ( flDamaged > 0.25 )
	{
		CSmartPtr<CObjectFireParticles> pFireEmitter = CObjectFireParticles::Create( "DrawDamageEffects 1" );
		pFireEmitter->SetSortOrigin( vecFire );
		PMaterialHandle	hSphereMaterial = pFireEmitter->GetPMaterial( "sprites/floorflame" );
		ObjectFireParticle *pParticle = (ObjectFireParticle *) pFireEmitter->AddParticle( sizeof(ObjectFireParticle), hSphereMaterial, vecFire );
		if ( pParticle )
		{
			pParticle->m_hParent = this;
			pParticle->m_iAttachmentPoint = iFireAttachment;

			pParticle->m_flLifetime	= 0.0f;
			pParticle->m_flDieTime	= 1.0;
			pParticle->m_uchStartSize = MAX( 5, 30 * (flDamaged - 0.25) );
			pParticle->m_uchEndSize = pParticle->m_uchStartSize;
			pParticle->m_vecVelocity = Vector(0,0,1);
			pParticle->m_uchStartAlpha = 255;
			pParticle->m_uchEndAlpha = 255;
			pParticle->m_flRoll	= 0;
			pParticle->m_flRollDelta = 0;
		}
	}

	// Sparks
	for ( float flSparks = flDamaged - 0.3; flSparks > 0; flSparks -= 0.3 )
	{
		// Get random spark attachment point
		iSparkAttachment = LookupRandomAttachment( "d_spark" );
		if ( !GetAttachment( iSparkAttachment, vecSpark, angSpark ) )
		{
			vecSpark = vecOrigin;
			angSpark = QAngle(0,0,0);
		}

		g_pEffects->Sparks( vecSpark );
	}

	m_flNextEffect = gpGlobals->curtime + random->RandomFloat( 0.2, 0.5 );
}

//============================================================================================================
// POWER PROXY
//============================================================================================================
class CObjectPowerProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_Factor;
};

bool CObjectPowerProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ))
		return false;

	return true;
}

void CObjectPowerProxy::OnBind( void *pRenderable )
{
	// Find the view angle between the player and this entity....
	IClientRenderable *pRend = (IClientRenderable *)pRenderable;
	C_BaseEntity *pEntity = pRend->GetIClientUnknown()->GetBaseEntity();
	C_BaseObject *pObject = dynamic_cast<C_BaseObject*>(pEntity);
	if (!pObject)
		return;

	int iPowered = pObject->IsPowered();

	SetFloatResult( iPowered * m_Factor.GetFloat() );
}

EXPOSE_INTERFACE( CObjectPowerProxy, IMaterialProxy, "ObjectPower" IMATERIAL_PROXY_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CBasicControlPanel : public CObjectControlPanel
{
	DECLARE_CLASS( CBasicControlPanel, CObjectControlPanel );

public:
	CBasicControlPanel( vgui::Panel *parent, const char *panelName );
};


DECLARE_VGUI_SCREEN_FACTORY( CBasicControlPanel, "basic_control_panel" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CBasicControlPanel::CBasicControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CBasicControlPanel" ) 
{
}
