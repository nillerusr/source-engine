//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICSCLONEAREA_H
#define PHYSICSCLONEAREA_H

#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"

class CProp_Portal;
class CPortalSimulator;

class CPhysicsCloneArea : public CBaseEntity
{
public:
	DECLARE_CLASS( CPhysicsCloneArea, CBaseEntity );

	static const Vector		vLocalMins;
	static const Vector		vLocalMaxs;

	virtual void			StartTouch( CBaseEntity *pOther );
	virtual void			Touch( CBaseEntity *pOther ); 
	virtual void			EndTouch( CBaseEntity *pOther );

	virtual void			Spawn( void );
	virtual void			Activate( void );

	virtual int				ObjectCaps( void );
	void					UpdatePosition( void );

	void					CloneTouchingEntities( void );
	void					CloneNearbyEntities( void );
	static CPhysicsCloneArea *CreatePhysicsCloneArea( CProp_Portal *pFollowPortal );	
private:

	CProp_Portal			*m_pAttachedPortal;
	CPortalSimulator		*m_pAttachedSimulator;
	bool					m_bActive;


};

#endif //#ifndef PHYSICSCLONEAREA_H

