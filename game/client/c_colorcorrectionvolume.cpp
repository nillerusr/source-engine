//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Color correction entity.
//
 // $NoKeywords: $
//===========================================================================//
#include "cbase.h"

#include "filesystem.h"
#include "cdll_client_int.h"
#include "materialsystem/materialsystemutil.h"
#include "colorcorrectionmgr.h"
#include "c_triggers.h"



// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Purpose : Shadow control entity
//------------------------------------------------------------------------------
class C_ColorCorrectionVolume : public C_BaseTrigger
{
public:
	DECLARE_CLASS( C_ColorCorrectionVolume, C_BaseTrigger );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_ColorCorrectionVolume();
	virtual ~C_ColorCorrectionVolume();

	void OnDataChanged(DataUpdateType_t updateType);
	bool ShouldDraw();

	void Update( C_BasePlayer *pPlayer, float ccScale );

	void StartTouch( C_BaseEntity *pOther );
	void EndTouch( C_BaseEntity *pOther );

private:
	float	m_LastEnterWeight;
	float	m_LastEnterTime;

	float	m_LastExitWeight;
	float	m_LastExitTime;
	bool	m_bEnabled;
	float	m_MaxWeight;
	float	m_FadeDuration;
	float	m_Weight;
	char	m_lookupFilename[MAX_PATH];

	ClientCCHandle_t m_CCHandle;
};

IMPLEMENT_CLIENTCLASS_DT(C_ColorCorrectionVolume, DT_ColorCorrectionVolume, CColorCorrectionVolume)
	RecvPropBool( RECVINFO(m_bEnabled) ),
	RecvPropFloat( RECVINFO(m_MaxWeight) ),
	RecvPropFloat( RECVINFO(m_FadeDuration) ),
	RecvPropFloat( RECVINFO(m_Weight) ),
	RecvPropString( RECVINFO(m_lookupFilename) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ColorCorrectionVolume )
	DEFINE_PRED_FIELD( m_Weight, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()


//------------------------------------------------------------------------------
// Constructor, destructor
//------------------------------------------------------------------------------
C_ColorCorrectionVolume::C_ColorCorrectionVolume()
{
	m_CCHandle = INVALID_CLIENT_CCHANDLE;
}

C_ColorCorrectionVolume::~C_ColorCorrectionVolume()
{
	g_pColorCorrectionMgr->RemoveColorCorrectionVolume( this, m_CCHandle );
}


//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void C_ColorCorrectionVolume::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		if ( m_CCHandle == INVALID_CLIENT_CCHANDLE )
		{
			// forming a unique name without extension
			char cleanName[MAX_PATH];
			V_StripExtension( m_lookupFilename, cleanName, sizeof( cleanName ) );
			char name[MAX_PATH];
			Q_snprintf( name, MAX_PATH, "%s_%d", cleanName, entindex() );

			m_CCHandle = g_pColorCorrectionMgr->AddColorCorrectionVolume( this, name, m_lookupFilename );

			SetSolid( SOLID_BSP );
			SetSolidFlags( FSOLID_TRIGGER | FSOLID_NOT_SOLID );
		}
	}
}

//------------------------------------------------------------------------------
// We don't draw...
//------------------------------------------------------------------------------
bool C_ColorCorrectionVolume::ShouldDraw()
{
	return false;
}


//--------------------------------------------------------------------------------------------------------
void C_ColorCorrectionVolume::StartTouch( CBaseEntity *pEntity )
{
	m_LastEnterTime = gpGlobals->curtime;
	m_LastEnterWeight = m_Weight;
}


//--------------------------------------------------------------------------------------------------------
void C_ColorCorrectionVolume::EndTouch( CBaseEntity *pEntity )
{
	m_LastExitTime = gpGlobals->curtime;
	m_LastExitWeight = m_Weight;
}


void C_ColorCorrectionVolume::Update( C_BasePlayer *pPlayer, float ccScale )
{
	if ( pPlayer )
	{
		bool isTouching = CollisionProp()->IsPointInBounds( pPlayer->EyePosition() );
		bool wasTouching = m_LastEnterTime > m_LastExitTime;

		if ( isTouching && !wasTouching )
		{
			StartTouch( pPlayer );
		}
		else if ( !isTouching && wasTouching )
		{
			EndTouch( pPlayer );
		}
	}

	if( !m_bEnabled )
	{
		m_Weight = 0.0f;
	}
	else
	{
		if( m_LastEnterTime > m_LastExitTime )
		{
			// we most recently entered the volume

			if( m_Weight < 1.0f )
			{
				float dt = gpGlobals->curtime - m_LastEnterTime;
				float weight = m_LastEnterWeight + dt / ((1.0f-m_LastEnterWeight)*m_FadeDuration);
				if( weight>1.0f )
					weight = 1.0f;

				m_Weight = weight;
			}
		}
		else
		{
			// we most recently exitted the volume

			if( m_Weight > 0.0f )
			{
				float dt = gpGlobals->curtime - m_LastExitTime;
				float weight = (1.0f-m_LastExitWeight) + dt / (m_LastExitWeight*m_FadeDuration);
				if( weight>1.0f )
					weight = 1.0f;

				m_Weight = 1.0f - weight;
			}
		}
	}

	//	Vector entityPosition = GetAbsOrigin();
	g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCHandle, m_Weight * ccScale );
}


void UpdateColorCorrectionVolumes( C_BasePlayer *pPlayer, float ccScale, C_ColorCorrectionVolume **pList, int listCount )
{
	for ( int i = 0; i < listCount; i++ )
	{
		pList[i]->Update(pPlayer, ccScale);
	}
}
