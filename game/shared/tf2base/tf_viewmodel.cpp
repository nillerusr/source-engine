//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:
//
//===========================================================================//
#include "cbase.h"
#include "tf_viewmodel.h"
#include "tf_shareddefs.h"
#include "tf_weapon_minigun.h"

#ifdef CLIENT_DLL
#include "c_tf_player.h"

// for spy material proxy
#include "proxyentity.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "prediction.h"

#endif

#include "bone_setup.h"	//temp

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( tf_viewmodel, CTFViewModel );

IMPLEMENT_NETWORKCLASS_ALIASED( TFViewModel, DT_TFViewModel )

BEGIN_NETWORK_TABLE( CTFViewModel, DT_TFViewModel )
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
CTFViewModel::CTFViewModel() : m_LagAnglesHistory("CPredictedViewModel::m_LagAnglesHistory")
{
	m_vLagAngles.Init();
	m_LagAnglesHistory.Setup( &m_vLagAngles, 0 );
	m_vLoweredWeaponOffset.Init();
}
#else
CTFViewModel::CTFViewModel()
{
}
#endif


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFViewModel::~CTFViewModel()
{
}

#ifdef CLIENT_DLL
// TODO:  Turning this off by setting interp 0.0 instead of 0.1 for now since we have a timing bug to resolve
ConVar cl_wpn_sway_interp( "cl_wpn_sway_interp", "0.0", FCVAR_CLIENTDLL );
ConVar cl_wpn_sway_scale( "cl_wpn_sway_scale", "5.0", FCVAR_CLIENTDLL );
#endif

//-----------------------------------------------------------------------------
// Purpose:  Adds head bob for off hand models
//-----------------------------------------------------------------------------
void CTFViewModel::AddViewModelBob( CBasePlayer *owner, Vector& eyePosition, QAngle& eyeAngles )
{
#ifdef CLIENT_DLL
	// if we are an off hand view model (index 1) and we have a model, add head bob.
	// (Head bob for main hand model added by the weapon itself.)
	if ( ViewModelIndex() == 1 && GetModel() != null )
	{
		CalcViewModelBobHelper( owner, &m_BobState );
		AddViewModelBobHelper( eyePosition, eyeAngles, &m_BobState );
	}
#endif
}

void CTFViewModel::CalcViewModelLag( Vector& origin, QAngle& angles, QAngle& original_angles )
{
#ifdef CLIENT_DLL
	if ( prediction->InPrediction() )
	{
		return;
	}

	if ( cl_wpn_sway_interp.GetFloat() <= 0.0f )
	{
		return;
	}

	// Calculate our drift
	Vector	forward, right, up;
	AngleVectors( angles, &forward, &right, &up );

	// Add an entry to the history.
	m_vLagAngles = angles;
	m_LagAnglesHistory.NoteChanged( gpGlobals->curtime, cl_wpn_sway_interp.GetFloat(), false );

	// Interpolate back 100ms.
	m_LagAnglesHistory.Interpolate( gpGlobals->curtime, cl_wpn_sway_interp.GetFloat() );

	// Now take the 100ms angle difference and figure out how far the forward vector moved in local space.
	Vector vLaggedForward;
	QAngle angleDiff = m_vLagAngles - angles;
	AngleVectors( -angleDiff, &vLaggedForward, 0, 0 );
	Vector vForwardDiff = Vector(1,0,0) - vLaggedForward;

	// Now offset the origin using that.
	vForwardDiff *= cl_wpn_sway_scale.GetFloat();
	origin += forward*vForwardDiff.x + right*-vForwardDiff.y + up*vForwardDiff.z;
#endif
}

#ifdef CLIENT_DLL
ConVar cl_gunlowerangle( "cl_gunlowerangle", "90", FCVAR_CLIENTDLL );
ConVar cl_gunlowerspeed( "cl_gunlowerspeed", "2", FCVAR_CLIENTDLL );
#endif

void CTFViewModel::CalcViewModelView( CBasePlayer *owner, const Vector& eyePosition, const QAngle& eyeAngles )
{
#if defined( CLIENT_DLL )

	Vector vecNewOrigin = eyePosition;
	QAngle vecNewAngles = eyeAngles;

	// Check for lowering the weapon
	C_TFPlayer *pPlayer = ToTFPlayer( owner );

	Assert( pPlayer );

	bool bLowered = pPlayer->IsWeaponLowered();

	QAngle vecLoweredAngles(0,0,0);

	m_vLoweredWeaponOffset.x = Approach( bLowered ? cl_gunlowerangle.GetFloat() : 0, m_vLoweredWeaponOffset.x, cl_gunlowerspeed.GetFloat() );
	vecLoweredAngles.x += m_vLoweredWeaponOffset.x;

	vecNewAngles += vecLoweredAngles;

	BaseClass::CalcViewModelView( owner, vecNewOrigin, vecNewAngles );

#endif
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Don't render the weapon if its supposed to be lowered and we have 
// finished the lowering animation
//-----------------------------------------------------------------------------
int CTFViewModel::DrawModel( int flags )
{
	// Check for lowering the weapon
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	Assert( pPlayer );

	bool bLowered = pPlayer->IsWeaponLowered();

	if ( bLowered && fabs( m_vLoweredWeaponOffset.x - cl_gunlowerangle.GetFloat() ) < 0.1 )
	{
		// fully lowered, stop drawing
		return 1;
	}

	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( pLocalPlayer && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE && 
		pLocalPlayer->GetObserverTarget() && pLocalPlayer->GetObserverTarget()->IsPlayer() )
	{
		pPlayer = ToTFPlayer( pLocalPlayer->GetObserverTarget() );

		if ( pPlayer != GetOwner() )
			return 0;
	}

	if ( pPlayer->IsAlive() == false )
	{
		 return 0;
	}

	return BaseClass::DrawModel( flags );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFViewModel::StandardBlendingRules( CStudioHdr *hdr, Vector pos[], Quaternion q[], float currentTime, int boneMask )
{
	BaseClass::StandardBlendingRules( hdr, pos, q, currentTime, boneMask );

	CTFWeaponBase *pWeapon = ( CTFWeaponBase * )GetOwningWeapon();

	if ( !pWeapon ) 
		return;

	if ( pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN )
	{
		CTFMinigun *pMinigun = ( CTFMinigun * )pWeapon;

		int iBarrelBone = Studio_BoneIndexByName( hdr, "v_minigun_barrel" );

		Assert( iBarrelBone != -1 );

		if ( iBarrelBone != -1 && ( hdr->boneFlags( iBarrelBone ) & boneMask ) )
		{
			RadianEuler a;
			QuaternionAngles( q[iBarrelBone], a );

			a.x = pMinigun->GetBarrelRotation();

			AngleQuaternion( a, q[iBarrelBone] );
		}
	}	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFViewModel::ProcessMuzzleFlashEvent()
{
	CTFWeaponBase *pWeapon = ( CTFWeaponBase * )GetOwningWeapon();

	if ( !pWeapon || C_BasePlayer::ShouldDrawLocalPlayer() ) 
		return;

	pWeapon->ProcessMuzzleFlashEvent();
}


//-----------------------------------------------------------------------------
// Purpose: Used for spy invisiblity material
//-----------------------------------------------------------------------------
int CTFViewModel::GetSkin()
{
	int nSkin = BaseClass::GetSkin();

	CTFWeaponBase *pWeapon = ( CTFWeaponBase * )GetOwningWeapon();

	if ( !pWeapon ) 
		return nSkin;

	CTFPlayer *pPlayer = ToTFPlayer( GetOwner() );
	if ( pPlayer )
	{
		if ( pWeapon->GetTFWpnData().m_bHasTeamSkins_Viewmodel )
		{
			switch( pPlayer->GetTeamNumber() )
			{
			case TF_TEAM_RED:
				nSkin = 0;
				break;
			case TF_TEAM_BLUE:
				nSkin = 1;
				break;
			}
		}	
	}

	return nSkin;
}

//-----------------------------------------------------------------------------
// Purpose: Used for spy invisiblity material
//-----------------------------------------------------------------------------
class CViewModelInvisProxy : public CEntityMaterialProxy
{
public:

	CViewModelInvisProxy( void );
	virtual ~CViewModelInvisProxy( void );
	virtual bool Init( IMaterial *pMaterial, KeyValues* pKeyValues );
	virtual void OnBind( C_BaseEntity *pC_BaseEntity );
	virtual IMaterial * GetMaterial();

private:
	IMaterialVar *m_pPercentInvisible;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CViewModelInvisProxy::CViewModelInvisProxy( void )
{
	m_pPercentInvisible = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CViewModelInvisProxy::~CViewModelInvisProxy( void )
{

}

//-----------------------------------------------------------------------------
// Purpose: Get pointer to the color value
// Input : *pMaterial - 
//-----------------------------------------------------------------------------
bool CViewModelInvisProxy::Init( IMaterial *pMaterial, KeyValues* pKeyValues )
{
	Assert( pMaterial );

	// Need to get the material var
	bool bFound;
	m_pPercentInvisible = pMaterial->FindVar( "$cloakfactor", &bFound );

	return bFound;
}

ConVar tf_vm_min_invis( "tf_vm_min_invis", "0.22", FCVAR_DEVELOPMENTONLY, "minimum invisibility value for view model", true, 0.0, true, 1.0 );
ConVar tf_vm_max_invis( "tf_vm_max_invis", "0.5", FCVAR_DEVELOPMENTONLY, "maximum invisibility value for view model", true, 0.0, true, 1.0 );

//-----------------------------------------------------------------------------
// Purpose: 
// Input :
//-----------------------------------------------------------------------------
void CViewModelInvisProxy::OnBind( C_BaseEntity *pEnt )
{
	if ( !m_pPercentInvisible )
		return;

	if ( !pEnt )
		return;

	CTFViewModel *pVM = dynamic_cast<CTFViewModel *>( pEnt );
	if ( !pVM )
	{
		m_pPercentInvisible->SetFloatValue( 0.0f );
		return;
	}

	CTFPlayer *pPlayer = ToTFPlayer( pVM->GetOwner() );

	if ( !pPlayer )
	{
		m_pPercentInvisible->SetFloatValue( 0.0f );
		return;
	}

	float flPercentInvisible = pPlayer->GetPercentInvisible();

	// remap from 0.22 to 0.5
	// but drop to 0.0 if we're not invis at all
	float flWeaponInvis = ( flPercentInvisible < 0.01 ) ?
		0.0 :
		RemapVal( flPercentInvisible, 0.0, 1.0, tf_vm_min_invis.GetFloat(), tf_vm_max_invis.GetFloat() );

	m_pPercentInvisible->SetFloatValue( flWeaponInvis );
}

IMaterial *CViewModelInvisProxy::GetMaterial()
{
	if ( !m_pPercentInvisible )
		return NULL;

	return m_pPercentInvisible->GetOwningMaterial();
}

EXPOSE_INTERFACE( CViewModelInvisProxy, IMaterialProxy, "vm_invis" IMATERIAL_PROXY_INTERFACE_VERSION );


#endif // CLIENT_DLL
