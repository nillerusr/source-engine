
#include "cbase.h"
#include "c_weapon__stubs.h"
#include "c_basehlcombatweapon.h"
#include "basehlcombatweapon_shared.h"
#include "iviewrender_beams.h"
#include "beam_shared.h"
#include "iefx.h"
#include "dlight.h"


extern bool UTIL_GetWeaponAttachment( C_BaseCombatWeapon *pWeapon, int attachmentID, Vector &absOrigin, QAngle &absAngles );
extern void FormatViewModelAttachment( Vector &vOrigin, bool bInverse );

class C_WeaponSniperRifle : public C_BaseHLCombatWeapon
{
	DECLARE_CLASS( C_WeaponSniperRifle, C_BaseHLCombatWeapon );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_WeaponSniperRifle();

	virtual bool		VisibleInWeaponSelection( void ) { return true; }
	virtual bool		CanBeSelected( void ) { return true; }
	virtual bool		HasAnyAmmo( void ) { return true; }
	virtual bool		HasAmmo( void ) {  return true; }

	void				OnRestore( void );
	void				UpdateOnRemove( void );

	bool				IsCarriedByLocalPlayer();

	virtual void ClientThink( void );
	virtual void OnDataChanged( DataUpdateType_t updateType );

	virtual bool IsWeaponCamera() { return (m_nZoomLevel == 0); }

	void SetupAttachmentPoints();

protected:

	float m_fNextZoom;
	int m_nZoomLevel;

	int iLaserAttachment;

	//dlight_t						*m_pELight;
};

STUB_WEAPON_CLASS_IMPLEMENT( weapon_sniperrifle, C_WeaponSniperRifle );

IMPLEMENT_CLIENTCLASS_DT(C_WeaponSniperRifle, DT_WeaponSniperRifle, CWeaponSniperRifle)
	RecvPropFloat( RECVINFO( m_fNextZoom ) ),
	RecvPropInt( RECVINFO( m_nZoomLevel ) ),
END_RECV_TABLE()

C_WeaponSniperRifle::C_WeaponSniperRifle()
{
	iLaserAttachment = -1;

	m_nZoomLevel = 0;

	//m_pELight = NULL;
}

void C_WeaponSniperRifle::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();

	/*if (m_pELight)
	{
		m_pELight->die = gpGlobals->curtime;
		m_pELight = NULL;
	}*/
}

void C_WeaponSniperRifle::OnRestore( void )
{
	SetupAttachmentPoints();

	BaseClass::OnRestore();
}

//-----------------------------------------------------------------------------
// Purpose: Starts the client-side version thinking
//-----------------------------------------------------------------------------
void C_WeaponSniperRifle::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( iLaserAttachment == -1 )
	{
		SetupAttachmentPoints();
	}

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

bool C_WeaponSniperRifle::IsCarriedByLocalPlayer( void )
{
	CBaseViewModel *vm = NULL;
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner )
	{
		if ( pOwner->GetActiveWeapon() != this )
			return false;

		vm = pOwner->GetViewModel( m_nViewModelIndex );
		if ( vm )
			return ( !vm->IsEffectActive(EF_NODRAW) );
	}

	return false;
}

void C_WeaponSniperRifle::ClientThink( void )
{
	if (iLaserAttachment == -1)
	{
		SetNextClientThink( CLIENT_THINK_NEVER );

		/*if (m_pELight)
		{
			m_pELight->die = gpGlobals->curtime;
			m_pELight = NULL;
		}*/
		return;
	}

	if (IsCarriedByLocalPlayer() && !IsEffectActive( EF_NODRAW ) && gpGlobals->frametime != 0.0f && m_fNextZoom <= gpGlobals->curtime )
	{
		

		Vector	vecOrigin, vecAngles;
		QAngle	angAngles;

		// Inner beams
		BeamInfo_t beamInfo;

		trace_t tr;

		if (m_nZoomLevel == 0)
		{
			UTIL_GetWeaponAttachment( this, iLaserAttachment, vecOrigin, angAngles );
			::FormatViewModelAttachment( vecOrigin, false );

			CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
			CBaseEntity *pBeamEnt = pOwner->GetViewModel();

			beamInfo.m_vecEnd = vec3_origin;
			beamInfo.m_pEndEnt = pBeamEnt;
			beamInfo.m_nEndAttachment = iLaserAttachment;

			AngleVectors(angAngles, &vecAngles);

			UTIL_TraceLine( vecOrigin, vecOrigin + (vecAngles * MAX_TRACE_LENGTH), MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr);
	
			/*if (tr.m_pEnt)
			{
				DevMsg("tr.m_pEnt angles: x %f, y %f, z %f\n", tr.m_pEnt->GetAbsAngles().x, tr.m_pEnt->GetAbsAngles().y, tr.m_pEnt->GetAbsAngles().z);
			}*/

			beamInfo.m_vecStart = tr.endpos;
			beamInfo.m_pStartEnt = NULL;
			beamInfo.m_nStartAttachment = -1;

			/*if (!m_pELight)
			{
				m_pELight = effects->CL_AllocElight( entindex() );

				m_pELight->minlight = (5/255.0f);
				m_pELight->die		= FLT_MAX;
			}

			if (m_pELight)
			{
				m_pELight->origin	= vecOrigin;
				m_pELight->radius	= random->RandomFloat( 40.0f, 62.0f );
			}*/
		}
		else
		{
			CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
			Vector vecOrigin, vecForward, vecRight, vecUp;
			pOwner->EyePositionAndVectors(&vecOrigin, &vecForward, &vecRight, &vecUp);

			Vector vecStart = vecOrigin + (vecRight * 4) - (vecUp * 4);

			beamInfo.m_vecEnd = vecStart;
			beamInfo.m_pEndEnt = NULL;
			beamInfo.m_nEndAttachment = -1;

			UTIL_TraceLine( vecOrigin, vecOrigin + (vecForward * MAX_TRACE_LENGTH), MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr);
				
			beamInfo.m_vecStart = tr.endpos;
			beamInfo.m_pStartEnt = NULL;
			beamInfo.m_nStartAttachment = -1;

			/*if (m_pELight)
			{
				m_pELight->die = gpGlobals->curtime;
				m_pELight = NULL;
			}*/
		}


		beamInfo.m_pszModelName = "effects/bluelaser1.vmt";

		if ( !(tr.surface.flags & SURF_SKY) )
		{
			beamInfo.m_pszHaloName = "sprites/light_glow03.vmt";
			beamInfo.m_flHaloScale = 4.0f;
		}

		beamInfo.m_flLife = 0.01f;
		beamInfo.m_flWidth = 1.0f; //random->RandomFloat( 1.0f, 2.0f );
		beamInfo.m_flEndWidth = 0;
		beamInfo.m_flFadeLength = 0.0f;
		beamInfo.m_flAmplitude = 0.0f; //random->RandomFloat( 16, 32 );
		beamInfo.m_flBrightness = 255.0;
		beamInfo.m_flSpeed = 0.0;
		beamInfo.m_nStartFrame = 0.0;
		beamInfo.m_flFrameRate = 1.0f;
		beamInfo.m_flRed = 0.0f;;
		beamInfo.m_flGreen = 100.0f;
		beamInfo.m_flBlue = 255.0f;
		beamInfo.m_nSegments = 0;
		beamInfo.m_bRenderable = true;
		beamInfo.m_nFlags = 0;
			
		beams->CreateBeamEntPoint( beamInfo );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets up the attachment point lookup for the model
//-----------------------------------------------------------------------------
void C_WeaponSniperRifle::SetupAttachmentPoints( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner != NULL && pOwner->GetViewModel() != NULL )
	{
			// Setup the center beam point
		iLaserAttachment = pOwner->GetViewModel()->LookupAttachment( "Laser" );
	}
	else
	{
		iLaserAttachment = -1;
	}
}