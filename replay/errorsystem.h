//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef ERRORSYSTEM_H
#define ERRORSYSTEM_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/ireplayerrorsystem.h"
#include "basethinker.h"
#include "utllinkedlist.h"

//----------------------------------------------------------------------------------------

class KeyValues;
class CClientRecordingSessionBlock;
class CHttpDownloader;

//----------------------------------------------------------------------------------------

class IErrorReporter
{
public:
	virtual void			ReportErrorsToUser( wchar_t *pErrorText ) = 0;
};

//----------------------------------------------------------------------------------------

class CErrorSystem	: public CBaseThinker,
					  public IReplayErrorSystem
{
public:
	CErrorSystem( IErrorReporter *pErrorReporter );
	~CErrorSystem();

	virtual void	AddErrorFromTokenName( const char *pToken );
	virtual void	AddFormattedErrorFromTokenName( const char *pFormatToken, KeyValues *pFormatArgs );

#if !defined( DEDICATED )
	void			OGS_ReportSessionBlockDownloadError( const CHttpDownloader *pDownloader, const CClientRecordingSessionBlock *pBlock,
														 int nLocalFileSize, int nMaxBlock, const bool *pSizesDiffer,
														 const bool *pHashFail, uint8 *pLocalHash );
	void			OGS_ReportSessioInfoDownloadError( const CHttpDownloader *pDownloader, const char *pErrorToken );
	void			OGS_ReportGenericError( const char *pGenericErrorToken );
#endif

private:
	void			AddError( const wchar_t *pError );
	void			AddError( const char *pError );

	float			GetNextThinkTime() const;
	void			Think();

	void			Clear();

	IErrorReporter	*m_pErrorReporter;
	CUtlLinkedList< wchar_t *, int > m_lstErrors;
};


//----------------------------------------------------------------------------------------

#endif	// ERRORSYSTEM_H
