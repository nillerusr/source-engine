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

#include "icodeprocessor.h"

class CCodeProcessor : public ICodeProcessor
{
public:
				CCodeProcessor( void );
				~CCodeProcessor( void );

	void		Process( const char *gamespecific, const char *root, const char *dsp, const char *config );

	void		SetQuiet( bool quiet );
	bool		GetQuiet( void ) const;

	void		SetLogFile( bool log );
	bool		GetLogFile( void ) const;

private:
	void		ProcessModule( bool forcequiet, int depth, int& maxdepth, int& numheaders, int& skippedfiles, const char *baseroot, const char *root, const char *module );
	void		ProcessModules( const char *root, const char *rootmodule );
	void		PrintResults( void );
	void		ConstructModuleList_R( int level, const char *gamespecific, const char *root );
	
	void		AddHeader( int depth, const char *filename, const char *rootmodule );

	void		CreateBackup( const char *filename, bool& wasreadonly );
	void		RestoreBackup( const char *filename, bool makereadonly );

	bool		TryBuild( const char *rootdir, const char *filename, unsigned char *buffer, int filelength );

	int			m_nModuleCount;

	typedef struct
	{
		char	name[ 256 ];
		bool	skipped;
	} CODE_MODULE;

	CODE_MODULE m_Modules[ 16384 ];

	int			m_nHeaderCount;
	CODE_MODULE	m_Headers[ 16384 ];

	bool		m_bQuiet;
	bool		m_bLogToFile;

	int			m_nFilesProcessed;
	int			m_nHeadersProcessed;
	int			m_nOffset;
	int			m_nBytesProcessed;
	int			m_nLinesOfCode;

	double		m_flStart;
	double		m_flEnd;

	char		m_szCurrentCPP[ 128 ];
	char		m_szDSP[ 128 ];
	char		m_szConfig[ 128 ];
};

extern ICodeProcessor *processor;

#endif // CODEPROCESSOR_H