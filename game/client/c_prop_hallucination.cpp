//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "cbase.h"
#include "c_baseanimating.h"
#include "materialsystem/imaterialsystem.h"
#include "model_types.h"
#include "viewrender.h"
#include "c_pixel_visibility.h"

extern view_id_t CurrentViewID();
extern bool IsMainView( view_id_t id );


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


class C_Prop_Hallucination : public C_BaseAnimating
{
public:
	DECLARE_CLASS( C_Prop_Hallucination, C_BaseAnimating );
	DECLARE_CLIENTCLASS();

	virtual void	Spawn( void );
	virtual void	UpdateOnRemove( void );
	//virtual bool	ShouldDraw( void );
	virtual int		DrawModel( int flags, const RenderableInstance_t &instance );
	virtual ShadowType_t ShadowCastType( void );
	virtual void OnDataChanged( DataUpdateType_t type );
	

	COcclusionQuerySet m_OcclusionSet;
	int m_iLastDrawCallFrame;
	bool m_bVisibleInAnyViewLastFrame;
	//float m_fVisibilityRemaining; //an ill defined measure of how much longer we're visible. When it reaches 0 we no longer show the hallucination until it's recharged to 1. Being occluded before reaching 0 is going to be up in the air on what to do.
	double m_fLastStateChangeTime;
	bool m_bVisibleEligible;

	bool m_bEnabled;
	float m_fVisibleTime;
	float m_fRechargeTime;

	static CMaterialReference sm_OcclusionProxyMaterial;
};

CMaterialReference C_Prop_Hallucination::sm_OcclusionProxyMaterial;

IMPLEMENT_CLIENTCLASS_DT( C_Prop_Hallucination, DT_Prop_Hallucination, CProp_Hallucination )
	RecvPropBool( RECVINFO(m_bEnabled) ),
	RecvPropFloat( RECVINFO(m_fVisibleTime) ),
	RecvPropFloat( RECVINFO(m_fRechargeTime) ),
END_RECV_TABLE()

//ConVar cl_hallucination_dischargescale( "cl_hallucination_dischargescale", "3000.0", FCVAR_CHEAT, "(delta time) * (percentage of screen drawn to) * (this scale) is how much visibility charge (0.0-1.0) we use up per frame. When it's 0.0 the hallucination is invisible" );
//ConVar cl_hallucination_visibletime( "cl_hallucination_visibletime", "0.215", FCVAR_CHEAT, "the maximum time (in seconds) from first visible frame to when we stop drawing the hallucination" );
//ConVar cl_hallucination_rechargetime( "cl_hallucination_rechargetime", "2.0", FCVAR_CHEAT, "How much time must pass without drawing the hallucination before it's eligble to draw again" );


void C_Prop_Hallucination::Spawn( void )
{
	if( !sm_OcclusionProxyMaterial.IsValid() )
	{
		sm_OcclusionProxyMaterial.Init( "engine/occlusionproxy", TEXTURE_GROUP_CLIENT_EFFECTS );
	}

	BaseClass::Spawn();
}

void C_Prop_Hallucination::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();
}

void C_Prop_Hallucination::OnDataChanged( DataUpdateType_t type )
{
	if( !m_bEnabled )
	{
		if( m_bVisibleEligible && (m_fLastStateChangeTime != 0.0f) )
		{
			m_fLastStateChangeTime = Plat_FloatTime();
		}
		m_bVisibleEligible = false;
	}
}

int C_Prop_Hallucination::DrawModel( int flags, const RenderableInstance_t &instance )
{
	switch( CurrentViewID() )
	{
	case VIEW_SHADOW_DEPTH_TEXTURE:
		if( m_bVisibleEligible )
		{
			return BaseClass::DrawModel( flags, instance );
		}

	default:
		if( IsMainView( CurrentViewID() ) ) //VIEW_MAIN and portal views are true
		{
			break;
		}
		else
		{
			return 0;
		}
	};

	
	bool bSameFrame = (m_iLastDrawCallFrame == gpGlobals->framecount);

	if( (flags & STUDIO_RENDER) && !bSameFrame )
	{
		bool bDrewLastFrame = (m_iLastDrawCallFrame == (gpGlobals->framecount - 1));
		if( m_bVisibleEligible )
		{
			if( m_fLastStateChangeTime == 0.0 ) //waiting for the first frame of actual visibility mark the state change
			{
				if( bDrewLastFrame && (m_OcclusionSet.QueryNumPixelsRenderedForAllViewsLastFrame() > 0) )
				{
					m_fLastStateChangeTime = Plat_FloatTime();// - gpGlobals->realtime; //state change occurred last frame
				}
			}
			else if( (Plat_FloatTime() - m_fLastStateChangeTime) > m_fVisibleTime )
			{
				m_bVisibleEligible = false;
			}
		}
		else
		{
			if( ((m_fLastStateChangeTime == 0.0f) || (Plat_FloatTime() - m_fLastStateChangeTime) > m_fRechargeTime) && 
				((m_OcclusionSet.QueryNumPixelsRenderedForAllViewsLastFrame() == 0) || !bDrewLastFrame) )
			{
				m_bVisibleEligible = true;
				m_fLastStateChangeTime = 0.0f;
			}
		}
	}
	
	
	if( flags & STUDIO_RENDER )
	{
		if( !m_bVisibleEligible )
		{
			modelrender->ForcedMaterialOverride( sm_OcclusionProxyMaterial );
		}
		m_iLastDrawCallFrame = gpGlobals->framecount;
		m_OcclusionSet.BeginQueryDrawing();
		int iRetVal = BaseClass::DrawModel( flags, instance );
		m_OcclusionSet.EndQueryDrawing();

		if( !m_bVisibleEligible )
		{
			modelrender->ForcedMaterialOverride( NULL );
		}

		return iRetVal;
	}
	else
	{
		return BaseClass::DrawModel( flags, instance );
	}
}

ShadowType_t C_Prop_Hallucination::ShadowCastType( void )
{
	return SHADOWS_NONE;
}
