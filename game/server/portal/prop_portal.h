//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef PROP_PORTAL_H
#define PROP_PORTAL_H
#ifdef _WIN32
#pragma once
#endif

#include "baseanimating.h"
#include "PortalSimulation.h"

// FIX ME
#include "portal_shareddefs.h"

static const char *s_pDelayedPlacementContext = "DelayedPlacementContext";
static const char *s_pTestRestingSurfaceContext = "TestRestingSurfaceContext";
static const char *s_pFizzleThink = "FizzleThink";

class CPhysicsCloneArea;

class CProp_Portal : public CBaseAnimating, public CPortalSimulatorEventCallbacks
{
public:
	DECLARE_CLASS( CProp_Portal, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

							CProp_Portal( void );
	virtual					~CProp_Portal( void );

	CNetworkHandle( CProp_Portal, m_hLinkedPortal ); //the portal this portal is linked to
	

	VMatrix					m_matrixThisToLinked; //the matrix that will transform a point relative to this portal, to a point relative to the linked portal
	CNetworkVar( bool, m_bActivated ); //a portal can exist and not be active
	CNetworkVar( bool, m_bIsPortal2 ); //For teleportation, this doesn't matter, but for drawing and moving, it matters
	Vector	m_vPrevForward; //used for the indecisive push in find closest passable spaces when portal is moved

	bool	m_bSharedEnvironmentConfiguration; //this will be set by an instance of CPortal_Environment when two environments are in close proximity

	EHANDLE	m_hMicrophone; //the microphone for teleporting sound
	EHANDLE	m_hSpeaker; //the speaker for teleported sound
	
	CSoundPatch		*m_pAmbientSound;

	Vector		m_vAudioOrigin;
	Vector		m_vDelayedPosition;
	QAngle		m_qDelayedAngles;
	int			m_iDelayedFailure;
	EHANDLE		m_hPlacedBy;

	COutputEvent m_OnPlacedSuccessfully;		// Output in hammer for when this portal was successfully placed (not attempted and fizzed).

	cplane_t m_plane_Origin; //a portal plane on the entity origin

	CPhysicsCloneArea		*m_pAttachedCloningArea;
	
	bool	IsPortal2() const;
	void	SetIsPortal2( bool bIsPortal2 );
	const VMatrix& MatrixThisToLinked() const;

	virtual int UpdateTransmitState( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}


	virtual void			Precache( void );
	virtual void			CreateSounds( void );
	virtual void			StopLoopingSounds( void );
	virtual void			Spawn( void );
	virtual void			Activate( void );
	virtual void			OnRestore( void );

	virtual void			UpdateOnRemove( void );

	void					DelayedPlacementThink( void );
	void					TestRestingSurfaceThink ( void );
	void					FizzleThink( void );

	bool					IsActivedAndLinked( void ) const;

    void					WakeNearbyEntities( void ); //wakes all nearby entities in-case there's been a significant change in how they can rest near a portal

	void					ForceEntityToFitInPortalWall( CBaseEntity *pEntity ); //projects an object's center into the middle of the portal wall hall, and traces back to where it wants to be

	void					PlacePortal( const Vector &vOrigin, const QAngle &qAngles, float fPlacementSuccess, bool bDelay = false );
	void					NewLocation( const Vector &vOrigin, const QAngle &qAngles );

	void					ResetModel( void ); //sets the model and bounding box
	void					DoFizzleEffect( int iEffect, bool bDelayedPos = true ); //display cool visual effect
	void					Fizzle( void ); //go inactive
	void					PunchPenetratingPlayer( CBaseEntity *pPlayer ); // adds outward force to player intersecting the portal plane
	void					PunchAllPenetratingPlayers( void ); // adds outward force to player intersecting the portal plane

	virtual void			StartTouch( CBaseEntity *pOther );
	virtual void			Touch( CBaseEntity *pOther ); 
	virtual void			EndTouch( CBaseEntity *pOther );
	bool					ShouldTeleportTouchingEntity( CBaseEntity *pOther ); //assuming the entity is or was just touching the portal, check for teleportation conditions
	void					TeleportTouchingEntity( CBaseEntity *pOther );
	void					InputSetActivatedState( inputdata_t &inputdata );
	void					InputFizzle( inputdata_t &inputdata );
	void					InputNewLocation( inputdata_t &inputdata );

	void					UpdatePortalLinkage( void );
	void					UpdatePortalTeleportMatrix( void ); //computes the transformation from this portal to the linked portal, and will update the remote matrix as well

	//void					SendInteractionMessage( CBaseEntity *pEntity, bool bEntering ); //informs clients that the entity is interacting with a portal (mostly used for clip planes)

	bool					SharedEnvironmentCheck( CBaseEntity *pEntity ); //does all testing to verify that the object is better handled with this portal instead of the other

	// The four corners of the portal in worldspace, updated on placement. The four points will be coplanar on the portal plane.
	Vector m_vPortalCorners[4];

	CPortalSimulator		m_PortalSimulator;

	//virtual bool			CreateVPhysics( void );
	//virtual void			VPhysicsDestroyObject( void );

	virtual bool			TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

	virtual void			PortalSimulator_TookOwnershipOfEntity( CBaseEntity *pEntity );
	virtual void			PortalSimulator_ReleasedOwnershipOfEntity( CBaseEntity *pEntity );

private:
	unsigned char			m_iLinkageGroupID; //a group ID specifying which portals this one can possibly link to

	CPhysCollide			*m_pCollisionShape;
	void					RemovePortalMicAndSpeaker();	// Cleans up the portal's internal audio members
	void					UpdateCorners( void );			// Updates the four corners of this portal on spawn and placement

public:
	inline unsigned char	GetLinkageGroup( void ) const { return m_iLinkageGroupID; };
	void					ChangeLinkageGroup( unsigned char iLinkageGroupID );

	//find a portal with the designated attributes, or creates one with them, favors active portals over inactive
	static CProp_Portal		*FindPortal( unsigned char iLinkageGroupID, bool bPortal2, bool bCreateIfNothingFound = false );
	static const CUtlVector<CProp_Portal *> *GetPortalLinkageGroup( unsigned char iLinkageGroupID );
};


//-----------------------------------------------------------------------------
// inline state querying methods
//-----------------------------------------------------------------------------
inline bool	CProp_Portal::IsPortal2() const
{
	return m_bIsPortal2;
}

inline void	CProp_Portal::SetIsPortal2( bool bIsPortal2 )
{
	m_bIsPortal2 = bIsPortal2;
}

inline const VMatrix& CProp_Portal::MatrixThisToLinked() const
{
	return m_matrixThisToLinked;
}


#endif //#ifndef PROP_PORTAL_H
