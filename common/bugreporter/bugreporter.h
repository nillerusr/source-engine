//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef BUGREPORTER_H
#define BUGREPORTER_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

abstract_class IBugReporter : public IBaseInterface
{
public:
	// Initialize and login with default username/password for this computer (from resource/bugreporter.res)
	virtual bool		Init( CreateInterfaceFn engineFactory ) = 0;
	virtual void		Shutdown() = 0;

	virtual bool		IsPublicUI() = 0;

	virtual char const	*GetUserName() = 0;
	virtual char const	*GetUserName_Display() = 0;

	virtual int			GetNameCount() = 0;
	virtual char const	*GetName( int index ) = 0;

	virtual int			GetDisplayNameCount() = 0;
	virtual char const  *GetDisplayName( int index ) = 0;

	virtual char const	*GetDisplayNameForUserName( char const *username ) = 0;
	virtual char const  *GetUserNameForDisplayName( char const *display ) = 0;

	virtual int			GetSeverityCount() = 0;
	virtual char const	*GetSeverity( int index ) = 0;

	virtual int			GetPriorityCount() = 0;
	virtual char const	*GetPriority( int index ) = 0;

	virtual int			GetAreaCount() = 0;
	virtual char const	*GetArea( int index ) = 0;

	virtual int			GetAreaMapCount() = 0;
	virtual char const	*GetAreaMap( int index ) = 0;

	virtual int			GetMapNumberCount() = 0;
	virtual char const	*GetMapNumber( int index ) = 0;

	virtual int			GetReportTypeCount() = 0;
	virtual char const	*GetReportType( int index ) = 0;

	virtual char const *GetRepositoryURL( void ) = 0;
	virtual char const *GetSubmissionURL( void ) = 0;

	virtual int			GetLevelCount(int area) = 0;
	virtual char const	*GetLevel(int area, int index ) = 0;

// Submission API
	virtual void		StartNewBugReport() = 0;
	virtual void		CancelNewBugReport() = 0;
	virtual bool		CommitBugReport( int& bugSubmissionId ) = 0;

	virtual void		SetTitle( char const *title ) = 0;
	virtual void		SetDescription( char const *description ) = 0;

	// NULL for current user
	virtual void		SetSubmitter( char const *username = 0 ) = 0;
	virtual void		SetOwner( char const *username ) = 0;
	virtual void		SetSeverity( char const *severity ) = 0;
	virtual void		SetPriority( char const *priority ) = 0;
	virtual void		SetArea( char const *area ) = 0;
	virtual void		SetMapNumber( char const *area ) = 0;
	virtual void		SetReportType( char const *reporttype ) = 0;

	virtual void		SetLevel( char const *levelnamne ) = 0;
	virtual void		SetPosition( char const *position ) = 0;
	virtual void		SetOrientation( char const *pitch_yaw_roll ) = 0;
	virtual void		SetBuildNumber( char const *build_num ) = 0;

	virtual void		SetScreenShot( char const *screenshot_unc_address ) = 0;
	virtual void		SetSaveGame( char const *savegame_unc_address ) = 0;
	virtual void		SetBSPName( char const *bsp_unc_address ) = 0;
	virtual void		SetVMFName( char const *vmf_unc_address ) = 0;
	virtual void		AddIncludedFile(  char const *filename ) = 0;
	virtual void		ResetIncludedFiles() = 0;

	virtual void		SetZipAttachmentName( char const *zipfilename ) = 0;

	virtual void		SetDriverInfo( char const *info ) = 0;
	virtual void		SetMiscInfo( char const *info ) = 0;

	virtual void		SetCSERAddress( const struct netadr_s& adr ) = 0;
	virtual void		SetExeName( char const *exename ) = 0;
	virtual void		SetGameDirectory( char const *gamedir ) = 0;
	virtual void		SetRAM( int ram ) = 0;
	virtual void		SetCPU( int cpu ) = 0;
	virtual void		SetProcessor( char const *processor ) = 0;
	virtual void		SetDXVersion( unsigned int high, unsigned int low, unsigned int vendor, unsigned int device ) = 0;
	virtual void		SetOSVersion( char const *osversion ) = 0;

	virtual void		SetSteamUserID( void *steamid, int idsize ) = 0;
};

#define INTERFACEVERSION_BUGREPORTER		"BugReporter004"

#endif // BUGREPORTER_H
