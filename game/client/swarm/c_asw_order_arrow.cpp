#include "cbase.h"
#include "c_asw_order_arrow.h"
#include "datacache/imdlcache.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// this is the red order arrow shown on the floor when you send an AI marine to a specific place

#define ASW_ORDER_ARROW_SOLID_TIME 2.0f
#define ASW_ORDER_ARROW_FADE_TIME 2.0f

C_ASW_Order_Arrow *C_ASW_Order_Arrow::CreateOrderArrow()
{
	C_ASW_Order_Arrow *pArrow = new C_ASW_Order_Arrow;

	if ( pArrow == NULL )
		return NULL;

	MDLCACHE_CRITICAL_SECTION();
	if ( pArrow->InitializeAsClientEntity( "models/swarm/OrderArrow/OrderArrow.mdl", false ) == false )
	{
		pArrow->Release();
		return NULL;
	}

	pArrow->SetCollisionGroup( COLLISION_GROUP_NONE );
	pArrow->SetSolid(SOLID_NONE);
	pArrow->AddEffects(EF_NODRAW);	// arrows start out invisible
	pArrow->AddEffects(EF_NOSHADOW);
	//pArrow->m_fAmbientLight = 0.5f;
	pArrow->SetRenderMode( kRenderTransAlpha );
	pArrow->SetRenderAlpha( 128 );
	pArrow->RefreshArrow();

	return pArrow;
}

void C_ASW_Order_Arrow::RefreshArrow()
{
	m_fCreatedTime = gpGlobals->curtime;
	SetRenderAlpha( 128 );
	SetRenderFX( kRenderFxFadeOut, m_fCreatedTime + ASW_ORDER_ARROW_SOLID_TIME, ASW_ORDER_ARROW_FADE_TIME );
}
