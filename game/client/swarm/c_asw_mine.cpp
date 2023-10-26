#include "cbase.h"
#include "c_asw_mine.h"
#include "dlight.h"
#include "iefx.h"
#include "functionproxy.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Mine, DT_ASW_Mine, CASW_Mine)
	RecvPropBool( RECVINFO( m_bMineTriggered ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Mine )

END_PREDICTION_DATA()

#define ASW_MINE_FLASH_TIME 0.02f

C_ASW_Mine::C_ASW_Mine()
{
	//m_fAmbientLight = 0.3;
}

C_ASW_Mine::~C_ASW_Mine()
{

}

void C_ASW_Mine::ClientThink(void)
{
	if (m_bMineTriggered)
	{
		// flash the mine
		/*
		if (m_fAmbientLight != 1.0f)
		{
			m_fAmbientLight = 1.0f;
		}
		else
		{
			m_fAmbientLight = 0.3;
		}
		*/
	}

	SetNextClientThink(gpGlobals->curtime + ASW_MINE_FLASH_TIME);
}

void C_ASW_Mine::OnDataChanged(DataUpdateType_t updateType)
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink(gpGlobals->curtime);
	}
	BaseClass::OnDataChanged(updateType);
}



//-----------------------------------------------------------------------------
// Material proxy for mine flash
//-----------------------------------------------------------------------------

// sinePeriod: time that it takes to go through whole sine wave in seconds (default: 1.0f)
// sineMax : the max value for the sine wave (default: 1.0f )
// sineMin: the min value for the sine wave  (default: 0.0f )
class CASW_Mine_Proxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

private:
	CFloatInput m_SinePeriod;
	CFloatInput m_SineMax;
	CFloatInput m_SineMin;
	CFloatInput m_SineTimeOffset;
};


bool CASW_Mine_Proxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_SinePeriod.Init( pMaterial, pKeyValues, "sinePeriod", 1.0f ))
		return false;
	if (!m_SineMax.Init( pMaterial, pKeyValues, "sineMax", 1.0f ))
		return false;
	if (!m_SineMin.Init( pMaterial, pKeyValues, "sineMin", 0.0f ))
		return false;
	if (!m_SineTimeOffset.Init( pMaterial, pKeyValues, "timeOffset", 0.0f ))
		return false;

	return true;
}

void CASW_Mine_Proxy::OnBind( void *pC_BaseEntity )
{
	Assert( m_pResult );

	C_ASW_Mine *pMine = dynamic_cast<C_ASW_Mine*>( BindArgToEntity( pC_BaseEntity ) );
	if ( !pMine || !pMine->m_bMineTriggered )
	{
		SetFloatResult( 0 );
		return;
	}

	float flValue;
	float flSineTimeOffset = m_SineTimeOffset.GetFloat();
	float flSineMax = m_SineMax.GetFloat();
	float flSineMin = m_SineMin.GetFloat();
	float flSinePeriod = m_SinePeriod.GetFloat();
	if (flSinePeriod == 0)
		flSinePeriod = 1;

	// get a value in [0,1]
	flValue = ( sin( 2.0f * M_PI * (gpGlobals->curtime - flSineTimeOffset) / flSinePeriod ) * 0.5f ) + 0.5f;
	// get a value in [min,max]	
	flValue = ( flSineMax - flSineMin ) * flValue + flSineMin;	

	SetFloatResult( flValue );
}

EXPOSE_INTERFACE( CASW_Mine_Proxy, IMaterialProxy, "MineFlashSine" IMATERIAL_PROXY_INTERFACE_VERSION );