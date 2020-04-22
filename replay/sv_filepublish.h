//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef SV_FILEPUBLISH_H
#define SV_FILEPUBLISH_H

//----------------------------------------------------------------------------------------

#include "platform.h"
#include "qlimits.h"
#include "spew.h"
#include "sv_basejob.h"
#include "compression.h"

//----------------------------------------------------------------------------------------

class IFilePublisher;

//----------------------------------------------------------------------------------------

class IPublishCallbackHandler
{
public:
	virtual ~IPublishCallbackHandler() {}

	virtual void	OnPublishComplete( const IFilePublisher *pPublisher, void *pUserData ) = 0;
	virtual void	OnPublishAborted( const IFilePublisher *pPublisher ) = 0;
	virtual void	AdjustHeader( const IFilePublisher *pPublisher, void *pHeaderData ) = 0; 
};

//----------------------------------------------------------------------------------------

struct PublishFileParams_t
{
	inline PublishFileParams_t()
	{
		V_memset( this, 0, sizeof( PublishFileParams_t ) );
		m_nCompressorType = COMPRESSORTYPE_BZ2;
	}

	IPublishCallbackHandler	*m_pCallbackHandler;
	const char				*m_pOutFilename;
	uint8					*m_pSrcData;
	int						m_nSrcSize;
	bool					m_bHash;
	bool					m_bFreeSrcData;
	bool					m_bDeleteFile;
	void					*m_pHeaderData;
	int						m_nHeaderSize;
	void					*m_pUserData;
	CompressorType_t		m_nCompressorType;
};

//----------------------------------------------------------------------------------------

//
// Interface for publishing files to fileserver.
//
// NOTE: You can force an IFilePublisher to delete itself by calling EnableAutoDelete().
//
class IFilePublisher
{
public:
	virtual ~IFilePublisher() {}

	enum PublishStatus_t
	{
		PUBLISHSTATUS_INVALID,
		PUBLISHSTATUS_OK,
		PUBLISHSTATUS_FAILED,
		PUBLISHSTATUS_ABORTED,
	};

	//
	// NOTE: Call Compressed() and Hashed() when IsDone() is true or in OnPublishComplete() to find
	// out if compression/hashing succeeded.
	//
	// Setting bFreeSrcData to false will keep the publisher from deleting the initial source data.
	// Otherwise, the process is that buffers will be cleaned up as they are no longer needed - for
	// example, if compression is enabled, the initial buffer will be free'd if bFreeSrcData is true.
	//
	virtual void				Publish( const PublishFileParams_t &params ) = 0;
	virtual void				AbortAndCleanup() = 0;
	virtual void				FinishSynchronouslyAndCleanup() = 0;

	virtual void				Think() = 0;
	virtual PublishStatus_t		GetStatus() const = 0;
	virtual bool				IsDone() const = 0;
	virtual bool				Compressed() const = 0;	// If compression was requested, did it succeed?
	virtual bool				Hashed() const = 0;
	virtual void				GetHash( uint8 *pOut ) const = 0;	// Writes 16 bytes to pOut
	virtual CompressorType_t	GetCompressorType() const = 0;
	virtual int					GetCompressedSize() const = 0;
};

//----------------------------------------------------------------------------------------

IFilePublisher *SV_PublishFile( const PublishFileParams_t &params );

//----------------------------------------------------------------------------------------

class CBasePublishJob : public CBaseJob
{
public:
	CBasePublishJob( JobPriority_t nPriority = JP_NORMAL, ISpewer *pSpewer = g_pDefaultSpewer );

	virtual void		GetOutputData( uint8 **ppData, uint32 *pDataSize ) const { *ppData = NULL; *pDataSize = 0; }
	virtual const char *GetOutputFilename() const { return NULL; }

protected:
	void SimulateDelay( int nDelay, const char *pThreadName );	// Seconds
};

//----------------------------------------------------------------------------------------

class CLocalPublishJob : public CBasePublishJob
{
public:
	CLocalPublishJob( const char *pLocalFilename );

	enum LocalPublishError_t
	{
		ERROR_SOURCE_FILE_DOES_NOT_EXIST,
		ERROR_INVALID_FILESERVER_PATH,
		ERROR_COULD_NOT_DELETE_TARGET_FILE,
		ERROR_RENAME_FAILED,
	};

private:
	virtual JobStatus_t	DoExecute();

	char				m_szLocalFilename[MAX_OSPATH];
};

CLocalPublishJob *SV_CreateLocalPublishJob( const char *pLocalFilename );

//----------------------------------------------------------------------------------------

class ICompressor;

class CCompressionJob : public CBasePublishJob
{
public:
	CCompressionJob( const uint8 *pSrcData, uint32 nSrcSize, CompressorType_t nType,
		bool *pOutCompressed, uint32 *pCompressedSize );

	enum CompressionError_t
	{
		ERROR_FAILED_ZERO_LENGTH_DATA,
		ERROR_OK_COULDNOTCOMPRESS,
		ERROR_FAILED_OPENOUTFILE,
	};

private:
	virtual JobStatus_t	DoExecute();
	virtual void		GetOutputData( uint8 **ppData, uint32 *pDataSize ) const;

	const uint8			*m_pSrcData;
	uint32				m_nSrcSize;
	char				m_szOutFilename[ MAX_OSPATH ];
	bool				*m_pCompressionResult;
	uint8				*m_pResult;
	unsigned int		*m_pResultSize;
	ICompressor			*m_pCompressor;
};

//----------------------------------------------------------------------------------------

class CMd5Job : public CBasePublishJob
{
public:
	CMd5Job( const void *pSrcData, int nSrcSize, bool *pOutHashed, uint8 pOutHash[16],
			 unsigned int pSeed[4] = NULL );

private:
	virtual JobStatus_t DoExecute();

	const void			*m_pSrcData;
	int					m_nSrcSize;
	bool				*m_pHashed;
	uint8				*m_pHash;
	unsigned int		*m_pSeed;
};

//----------------------------------------------------------------------------------------

class CDeleteLocalFileJob : public CBasePublishJob
{
public:
	CDeleteLocalFileJob( const char *pFilename );

	enum CompressionError_t
	{
		ERROR_FILE_DOES_NOT_EXISTS,
		ERROR_COULD_NOT_DELETE,
	};

private:
	virtual JobStatus_t DoExecute();

	char				m_szFilename[ MAX_OSPATH ];
};

//----------------------------------------------------------------------------------------

#endif // SV_FILEPUBLISH_H
