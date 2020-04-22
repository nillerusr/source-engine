//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IFACEPOSERWORKSPACE_H
#define IFACEPOSERWORKSPACE_H
#ifdef _WIN32
#pragma once
#endif

class IWorkspaceFiles
{
public:
	enum
	{
		EXPRESSION = 0,
		CHOREODATA,
		MODELDATA,

		NUM_FILE_TYPES
	};

	virtual void			Init( char const *pchShortName ) = 0;

	// Restore
	virtual int				GetNumStoredFiles( int type ) = 0;
	virtual const char		*GetStoredFile( int type, int number ) = 0;

	// Save
	virtual void			StartStoringFiles( int type ) = 0;
	virtual void			FinishStoringFiles( int type ) = 0;
	virtual void			StoreFile( int type, const char *filename ) = 0;
};

extern IWorkspaceFiles *workspacefiles;

#endif // IFACEPOSERWORKSPACE_H
