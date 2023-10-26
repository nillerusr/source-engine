#include "cbase.h"
#include "c_asw_laser_mine.h"
#include "dlight.h"
#include "iefx.h"
#include "functionproxy.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Laser_Mine, DT_ASW_Laser_Mine, CASW_Laser_Mine)
	RecvPropVector		( RECVINFO( m_angLaserAim ) ),
	RecvPropBool		( RECVINFO( m_bFriendly ) ),
	RecvPropBool		( RECVINFO( m_bMineActive ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Laser_Mine )

END_PREDICTION_DATA()

#define ASW_MINE_FLASH_TIME 0.02f

C_ASW_Laser_Mine::C_ASW_Laser_Mine()
{

}

C_ASW_Laser_Mine::~C_ASW_Laser_Mine()
{

}

void C_ASW_Laser_Mine::ClientThink(void)
{
	UpdateLaser();

	if ( GetMoveParent() )		// if attached to something, update the laser each frame, as that object could be moving.
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
	else
	{
		SetNextClientThink( gpGlobals->curtime + 0.1f );
	}
}

void C_ASW_Laser_Mine::UpdateOnRemove()
{
	RemoveLaserEffect();

	BaseClass::UpdateOnRemove();
}

void C_ASW_Laser_Mine::OnDataChanged(DataUpdateType_t updateType)
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( gpGlobals->curtime + 0.1f );
	}
	BaseClass::OnDataChanged(updateType);
}


void C_ASW_Laser_Mine::CreateLaserEffect()
{
	if ( m_pLaserEffect )
	{
		RemoveLaserEffect();
	}

	if ( !m_pLaserEffect )
	{
		const char *szEffect = m_bFriendly.Get() ? "laser_mine_friendly" : "laser_mine";
		m_pLaserEffect = ParticleProp()->Create( szEffect, PATTACH_ABSORIGIN_FOLLOW, -1 ); // PATTACH_POINT_FOLLOW, iAttachment );	// TODO: use attachment point when laser mine model has one

		if ( m_pLaserEffect )
		{
			ParticleProp()->AddControlPoint( m_pLaserEffect, 1, this, PATTACH_CUSTOMORIGIN );
			ParticleProp()->AddControlPoint( m_pLaserEffect, 2, this, PATTACH_CUSTOMORIGIN );
		}
	}
}

//--------------------------------------------------------------------------------------------------------
void C_ASW_Laser_Mine::RemoveLaserEffect()
{
	if ( m_pLaserEffect )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_pLaserEffect );
		m_pLaserEffect = NULL;
	}
}