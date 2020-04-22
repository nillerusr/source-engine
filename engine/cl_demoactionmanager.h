//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CL_DEMOACTIONMANAGER_H
#define CL_DEMOACTIONMANAGER_H
#ifdef _WIN32
#pragma once
#endif

class CUtlBuffer;

namespace vgui
{
	class Panel;
};

class CBaseDemoAction;
struct democmdinfo_t;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
abstract_class IDemoActionManager
{
public:
	virtual				~IDemoActionManager( void ) {}

	virtual void		Init( void ) = 0;
	virtual void		Shutdown( void ) = 0;

	virtual void		StartPlaying( char const *demfilename ) = 0;
	virtual void		StopPlaying() = 0;

	virtual void		Update( bool newframe, int demoframe, float demotime ) = 0;

	virtual void		SaveToBuffer( CUtlBuffer& buf ) = 0;
	virtual void		SaveToFile( void ) = 0;

	virtual char const	*GetCurrentDemoFile( void ) = 0;

	virtual int			GetActionCount( void ) = 0;
	virtual CBaseDemoAction *GetAction( int index ) = 0;
	virtual void		AddAction( CBaseDemoAction *action ) = 0;
	virtual void		RemoveAction( CBaseDemoAction *action ) = 0;

	virtual bool		IsDirty( void ) const = 0;
	virtual void		SetDirty( bool dirty ) = 0;

	virtual void		ReloadFromDisk( void ) = 0;

	virtual void		DispatchEvents() = 0;
	virtual void		InsertFireEvent( CBaseDemoAction *action ) = 0;

	virtual bool		OverrideView( democmdinfo_t& info, int tick ) = 0;
};

extern IDemoActionManager *demoaction;

#endif // CL_DEMOACTIONMANAGER_H
