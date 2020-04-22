//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef SV_FILESERVERCLEANUP_H
#define SV_FILESERVERCLEANUP_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "basethinker.h"
#include "spew.h"
#include "sv_basejob.h"

//----------------------------------------------------------------------------------------

bool		SV_DoFileserverCleanup( bool bForceCleanAll, ISpewer *pSpewer/*=g_pDefaultSpewer*/ );
CBaseJob *SV_CreateDeleteFileJob();

//----------------------------------------------------------------------------------------

class IFileserverCleanerJob
{
public:
	virtual ~IFileserverCleanerJob() {}

	virtual void		AddFileForDelete( const char *pFilename ) = 0;
	virtual int			GetNumFilesDeleted() const = 0;
};

IFileserverCleanerJob *SV_CastJobToIFileserverCleanerJob( CBaseJob *pJob );

//----------------------------------------------------------------------------------------

class CFileserverCleaner : public CBaseThinker
{
public:
	CFileserverCleaner();

	void				MarkFileForDelete( const char *pFilename );

	int					GetNumFilesDeleted() const		{ return m_nNumFilesDeleted; }
	bool				HasFilesQueuedForDelete() const	{ return m_pCleanerJob != NULL; }

	void				BlockForCompletion();
	void				DoCleanAsynchronous( bool bPrintResult = false, ISpewer *pSpewer = g_pDefaultSpewer );

private:
	void				Clear();
	void				PrintResult();

	virtual void		Think();
	virtual float		GetNextThinkTime() const;

	CBaseJob			*m_pCleanerJob;
	bool				m_bRunning;
	bool				m_bPrintResult;
	int					m_nNumFilesDeleted;
	ISpewer				*m_pSpewer;
};

//----------------------------------------------------------------------------------------

class CLocalFileDeleterJob : public CBaseJob,
							 public IFileserverCleanerJob
{
public:
	CLocalFileDeleterJob();

	virtual void		AddFileForDelete( const char *pFilename );
	virtual int			GetNumFilesDeleted() const	{ return m_nNumDeleted; }

	enum DeleteError_t
	{
		ERROR_FILE_DOES_NOT_EXIST,
	};

private:
	virtual JobStatus_t	DoExecute();

	CUtlStringList		m_vecFiles;
	int					m_nNumDeleted;
};

CLocalFileDeleterJob *SV_CreateLocalFileDeleterJob();

//----------------------------------------------------------------------------------------

#endif	// SV_FILESERVERCLEANUP_H
