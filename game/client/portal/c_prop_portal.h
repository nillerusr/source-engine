//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_PROP_PORTAL_H
#define C_PROP_PORTAL_H

#ifdef _WIN32
#pragma once
#endif

#include "portalrenderable_flatbasic.h"
#include "iviewrender.h"
#include "view_shared.h"
#include "viewrender.h"
#include "PortalSimulation.h"
#include "C_PortalGhostRenderable.h" 

struct dlight_t;
class C_DynamicLight;

class C_Prop_Portal : public CPortalRenderable_FlatBasic
{
public:
	DECLARE_CLASS( C_Prop_Portal, CPortalRenderable_FlatBasic );
	DECLARE_CLIENTCLASS();

							C_Prop_Portal( void );
	virtual					~C_Prop_Portal( void );

	// Handle recording for the SFM
	virtual void GetToolRecordingState( KeyValues *msg );

	CHandle<C_Prop_Portal>	m_hLinkedPortal; //the portal this portal is linked to
	bool					m_bActivated; //a portal can exist and not be active
	
	bool					m_bSharedEnvironmentConfiguration; //this will be set by an instance of CPortal_Environment when two environments are in close proximity
	
	cplane_t				m_plane_Origin;	// The plane on which this portal is placed, normal facing outward (matching model forward vec)

	virtual void			Spawn( void );
	virtual void			Activate( void );
	virtual void			ClientThink( void );

	virtual void			Simulate();

	virtual void			UpdateOnRemove( void );

	virtual void			OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );

	struct Portal_PreDataChanged
	{
		bool					m_bActivated;
		bool					m_bIsPortal2;
		Vector					m_vOrigin;
		QAngle					m_qAngles;
		EHANDLE					m_hLinkedTo;
	} PreDataChanged;

	struct TransformedLightingData_t
	{
		ClientShadowHandle_t	m_LightShadowHandle;
		dlight_t				*m_pEntityLight;
	} TransformedLighting;

	virtual void			OnPreDataChanged( DataUpdateType_t updateType );
	virtual void			OnDataChanged( DataUpdateType_t updateType );
	virtual int				DrawModel( int flags );
	void					UpdateOriginPlane( void );
	void					UpdateGhostRenderables( void );

	void					SetIsPortal2( bool bValue );

	bool					IsActivedAndLinked( void ) const;

	CPortalSimulator		m_PortalSimulator;

	virtual C_BaseEntity *	PortalRenderable_GetPairedEntity( void ) { return this; };

private:

	CUtlVector<EHANDLE>		m_hGhostingEntities;
	CUtlVector<C_PortalGhostRenderable *>		m_GhostRenderables;
	float					m_fGhostRenderablesClip[4];


	friend void __MsgFunc_EntityPortalled(bf_read &msg);

};

typedef C_Prop_Portal CProp_Portal;

#endif //#ifndef C_PROP_PORTAL_H
