//--------------------------------------------------------------------------------------------------------
// Copyright 2007 Turtle Rock Studios, Inc.

#ifndef ENV_TONEMAP_CONTROLLER_H
#define ENV_TONEMAP_CONTROLLER_H

#include "triggers.h"

//--------------------------------------------------------------------------------------------------------
class CTonemapTrigger : public CBaseTrigger
{
public:
	DECLARE_CLASS( CTonemapTrigger, CBaseTrigger );
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void StartTouch( CBaseEntity *other );
	virtual void EndTouch( CBaseEntity *other );

	CBaseEntity *GetTonemapController( void ) const;

private:
	string_t m_tonemapControllerName;
	EHANDLE m_hTonemapController;
};


//--------------------------------------------------------------------------------------------------------
inline CBaseEntity *CTonemapTrigger::GetTonemapController( void ) const
{
	return m_hTonemapController.Get();
}


//--------------------------------------------------------------------------------------------------------
// Tonemap Controller System.
class CTonemapSystem : public CAutoGameSystem
{
public:

	// Creation/Init.
	CTonemapSystem( char const *name ) : CAutoGameSystem( name ) 
	{
		m_hMasterController = NULL;
	}

	~CTonemapSystem()
	{
		m_hMasterController = NULL;
	}

	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	CBaseEntity *GetMasterTonemapController( void ) const;

private:

	EHANDLE m_hMasterController;
};


//--------------------------------------------------------------------------------------------------------
inline CBaseEntity *CTonemapSystem::GetMasterTonemapController( void ) const
{
	return m_hMasterController.Get();
}

//--------------------------------------------------------------------------------------------------------
CTonemapSystem *TheTonemapSystem( void );


#endif //ENV_TONEMAP_CONTROLLER_H