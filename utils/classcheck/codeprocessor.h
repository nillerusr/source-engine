//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( CODEPROCESSOR_H )
#define CODEPROCESSOR_H
#ifdef _WIN32
#pragma once
#endif

#include "class.h"
#include "icodeprocessor.h"

#include "utldict.h"

class CCodeProcessor : public ICodeProcessor
{
public:
				CCodeProcessor( void );
				~CCodeProcessor( void );

	void		Process( const char *baseentityclass, const char *gamespecific, const char *sourcetreebase, const char *subdir );
	void		Process( const char *baseentityclass, const char *gamespecific, const char *sourcetreebase, const char *subdir, const char *pFileName );
	CClass		*AddClass( const char *classname );

	void		SetQuiet( bool quiet );
	bool		GetQuiet( void ) const;

	void		SetPrintHierarchy( bool print );
	bool		GetPrintHierarchy( void ) const;

	void		SetPrintMembers( bool print );
	bool		GetPrintMembers( void ) const;

	void		SetPrintTDs( bool print );
	bool		GetPrintTDs( void ) const;

	void		SetPrintPredTDs( bool print );
	bool		GetPrintPredTDs( void ) const;

	void		SetPrintCreateMissingTDs( bool print );
	bool		GetPrintCreateMissingTDs( void ) const;

	void		SetPrintCreateMissingPredTDs( bool print );
	bool		GetPrintCreateMissingPredTDs( void ) const;

	void		SetLogFile( bool log );
	bool		GetLogFile( void ) const;

	void		SetCheckHungarian( bool check );
	bool		GetCheckHungarian() const;

	CClass		*FindClass( const char *name ) const;
	
	void		ResolveBaseClasses( const char *baseentityclass );
	void		Clear( void );
	void		PrintClassList( void ) const;
	void		PrintMissingTDFields( void ) const;
	void		ReportHungarianNotationErrors();
	
	int			Count( void ) const;
	
	void		SortClassList( void );

private:
	char		*ParseTypeDescription( char *current, bool fIsMacroized );
	char		*ParsePredictionTypeDescription( char *current );
	char		*ParseReceiveTable( char *current );

	void		ProcessModule( bool forcequiet, int depth, int& maxdepth, int& numheaders, int& skippedfiles, 
		const char *srcroot, const char *baseroot, const char *root, const char *module );
	void		ProcessModules( const char *srcroot, const char *root, const char *rootmodule );
	void		PrintResults( const char *baseentityclass );
	void		ConstructModuleList_R( int level, const char *baseentityclass, const char *gamespecific, const char *root, char const *srcroot );
	
	void		AddHeader( int depth, const char *filename, const char *rootmodule );

	bool		CheckShouldSkip( bool forcequiet, int depth, char const *filename, int& numheaders, int& skippedfiles);
	bool		LoadFile( char **buffer, char *filename, char const *module, bool forcequiet, 
					int depth, int& filelength, int& numheaders, int& skippedfiles,
					char const *srcroot, char const *root, char const *baseroot );

	// include file path
	void		CleanupIncludePath();
	void		AddIncludePath( const char *pPath );
	void		SetupIncludePath( const char *sourcetreebase, const char *subdir, const char *gamespecific );

	CClass		*m_pClassList;

	typedef struct
	{
		bool	skipped;
	} CODE_MODULE;

	CUtlDict< CODE_MODULE, int >	m_Modules;
	CUtlDict< CODE_MODULE, int >	m_Headers;

	CUtlVector< char * >	m_IncludePath;

	bool		m_bQuiet;
	bool		m_bPrintHierarchy;
	bool		m_bPrintMembers;
	bool		m_bPrintTypedescriptionErrors;
	bool		m_bPrintPredictionDescErrors;
	bool		m_bCreateMissingTDs;
	bool		m_bCreateMissingPredTDs;
	bool		m_bLogToFile;
	bool		m_bCheckHungarian;

	int			m_nFilesProcessed;
	int			m_nHeadersProcessed;
	int			m_nClassesParsed;
	int			m_nOffset;
	int			m_nBytesProcessed;
	int			m_nLinesOfCode;

	double		m_flStart;
	double		m_flEnd;

	char		m_szCurrentCPP[ 128 ];
	char		m_szBaseEntityClass[ 256 ];

};

extern ICodeProcessor *processor;

#endif // CODEPROCESSOR_H