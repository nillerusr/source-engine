// Orders nearby aliens to a named info_target

#include "cbase.h"
#include "asw_gamerules.h"
#include "iasw_spawnable_npc.h"
#include "asw_spawn_manager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CASW_Order_Nearby_Aliens : public CPointEntity
{
private:			
	DECLARE_DATADESC();

public:
	DECLARE_CLASS( CASW_Order_Nearby_Aliens, CPointEntity );

	~CASW_Order_Nearby_Aliens( void );
	virtual void	Spawn( void );	

	CBaseEntity* GetOrderTarget();
	EHANDLE m_hAlienOrderTarget;

	const char* GetAlienClass();
	AlienOrder_t m_AlienOrders;
	string_t m_AlienOrderTargetName;		// name of our moveto target
	int m_AlienClassNum;
	float m_fRadius;

	// Input handlers
	void InputOrderAliens( inputdata_t &inputdata );
};

LINK_ENTITY_TO_CLASS( asw_order_nearby_aliens, CASW_Order_Nearby_Aliens );

BEGIN_DATADESC( CASW_Order_Nearby_Aliens )
	DEFINE_KEYFIELD( m_fRadius,		FIELD_FLOAT, "radius" ),
	DEFINE_KEYFIELD( m_AlienClassNum,			FIELD_INTEGER,	"AlienClass" ),
	DEFINE_KEYFIELD( m_AlienOrders,				FIELD_INTEGER,	"AlienOrders" ),	
	DEFINE_KEYFIELD( m_AlienOrderTargetName,	FIELD_STRING,	"AlienOrderTargetName" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SendOrders", InputOrderAliens ),	
END_DATADESC()

CASW_Order_Nearby_Aliens::~CASW_Order_Nearby_Aliens( void )
{
	
}

void CASW_Order_Nearby_Aliens::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
}


void CASW_Order_Nearby_Aliens::InputOrderAliens( inputdata_t &inputdata )
{
	// find all aliens within our radius
	const char* szAlienClass = GetAlienClass();
	CBaseEntity* pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, szAlienClass )) != NULL)
	{
		float dist = (GetAbsOrigin() - pEntity->GetAbsOrigin()).Length2D();
		if (dist <= m_fRadius)
		{
			IASW_Spawnable_NPC* pAlien = dynamic_cast<IASW_Spawnable_NPC*>(pEntity);			
			if (pAlien)
			{
				// give the orders
				pAlien->SetAlienOrders(m_AlienOrders, vec3_origin, GetOrderTarget());
			}
		}
	}
}

const char* CASW_Order_Nearby_Aliens::GetAlienClass()
{
	if ( m_AlienClassNum < 0 || m_AlienClassNum >= ASWSpawnManager()->GetNumAlienClasses() )
	{
		m_AlienClassNum = 0;
	}

	return ASWSpawnManager()->GetAlienClass( m_AlienClassNum )->m_pszAlienClass;
}

CBaseEntity* CASW_Order_Nearby_Aliens::GetOrderTarget()
{
	if (m_hAlienOrderTarget.Get() == NULL &&
		(m_AlienOrders == AOT_MoveTo || m_AlienOrders == AOT_MoveToIgnoringMarines )
		)
	{
		m_hAlienOrderTarget = gEntList.FindEntityByName( NULL, m_AlienOrderTargetName, NULL );

		if( !m_hAlienOrderTarget.Get() )
		{
			DevWarning("asw_spawner can't find order object\n", GetDebugName() );
			return NULL;
		}
	}
	
	return m_hAlienOrderTarget.Get();
}
