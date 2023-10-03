//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"

#include "c_entityfreezing.h"
#include "studio.h"
#include "bone_setup.h"
#include "c_surfacerender.h"
#include "engine/ivdebugoverlay.h"
#include "dt_utlvector_recv.h"
#include "debugoverlay_shared.h"
#include "animation.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar cl_blobulator_freezing_max_metaball_radius( "cl_blobulator_freezing_max_metaball_radius", 
												#ifdef INFESTED_DLL
												  "25.0", // Don't need as much precision in Alien swarm because everything is zoomed out
												#else
												  "12.0", 
												#endif
												  FCVAR_NONE, "Setting this can create more complex surfaces on large hitboxes at the cost of performance.", true, 12.0f, true, 100.0f );


//PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectFreezing )
//	PRECACHE( MATERIAL,"effects/tesla_glow_noz" )
//	PRECACHE( MATERIAL,"effects/spark" )
//	PRECACHE( MATERIAL,"effects/combinemuzzle2" )
//PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_CLIENTCLASS_DT( C_EntityFreezing, DT_EntityFreezing, CEntityFreezing )
	RecvPropVector( RECVINFO(m_vFreezingOrigin) ),
	RecvPropArray3( RECVINFO_ARRAY(m_flFrozenPerHitbox), RecvPropFloat( RECVINFO( m_flFrozenPerHitbox[0] ) ) ),
	RecvPropFloat( RECVINFO(m_flFrozen) ),
	RecvPropBool( RECVINFO(m_bFinishFreezing) ),
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EntityFreezing::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	if ( GetMoveParent() )
	{
		GetMoveParent()->GetRenderBounds( theMins, theMaxs );
	}
	else
	{
		theMins = GetAbsOrigin();
		theMaxs = theMaxs;
	}
}


//-----------------------------------------------------------------------------
// Yes we bloody are
//-----------------------------------------------------------------------------
RenderableTranslucencyType_t C_EntityFreezing::ComputeTranslucencyType( )
{
	return RENDERABLE_IS_TRANSLUCENT;
}


//-----------------------------------------------------------------------------
// On data changed
//-----------------------------------------------------------------------------
void C_EntityFreezing::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EntityFreezing::ClientThink( void )
{
	__asm nop;
	//C_BaseAnimating *pAnimating = GetMoveParent() ? GetMoveParent()->GetBaseAnimating() : NULL;
	//if (!pAnimating)
	//	return;

	//color32 color = pAnimating->GetRenderColor();

	//color.r = color.g = ( 1.0f - m_flFrozen ) * 255.0f;

	//// Setup the entity fade
	//pAnimating->SetRenderMode( kRenderTransColor );
	//pAnimating->SetRenderColor( color.r, color.g, color.b, color.a );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flags - 
// Output : int
//-----------------------------------------------------------------------------
int C_EntityFreezing::DrawModel( int flags, const RenderableInstance_t &instance )
{


	return 1;
}
