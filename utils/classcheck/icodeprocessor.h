//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( ICODEPROCESSOR_H )
#define ICODEPROCESSOR_H
#ifdef _WIN32
#pragma once
#endif

#pragma warning( disable : 4127 )

class CClass;

class ICodeProcessor
{
public:
	// Process all files in a directory
	virtual void		Process( const char *baseentityclass, const char *gamespecific, const char *sourcetreebase, const char *subdir ) = 0;

	// Process a single file
	virtual void		Process( const char *baseentityclass, const char *gamespecific, const char *sourcetreebase, const char *subdir, const char *pFileName ) = 0;

	virtual CClass		*AddClass( const char *classname ) = 0;
	virtual CClass		*FindClass( const char *name ) const = 0;

	virtual void		SetQuiet( bool quiet ) = 0;
	virtual bool		GetQuiet( void ) const = 0;

	virtual void		SetPrintHierarchy( bool print ) = 0;
	virtual bool		GetPrintHierarchy( void ) const = 0;

	virtual void		SetPrintMembers( bool print ) = 0;
	virtual bool		GetPrintMembers( void ) const = 0;

	virtual void		SetPrintTDs( bool print ) = 0;
	virtual bool		GetPrintTDs( void ) const = 0;

	virtual void		SetPrintPredTDs( bool print ) = 0;
	virtual bool		GetPrintPredTDs( void ) const = 0;

	virtual void		SetPrintCreateMissingTDs( bool print ) = 0;
	virtual bool		GetPrintCreateMissingTDs( void ) const = 0;

	virtual void		SetPrintCreateMissingPredTDs( bool print ) = 0;
	virtual bool		GetPrintCreateMissingPredTDs( void ) const = 0;

	virtual void		SetLogFile( bool log ) = 0;
	virtual bool		GetLogFile( void ) const = 0;

	virtual void		SetCheckHungarian( bool check ) = 0;
	virtual bool		GetCheckHungarian() const = 0;
};

extern ICodeProcessor *processor;

#endif // ICODEPROCESSOR_H