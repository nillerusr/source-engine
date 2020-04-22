//========= Copyright Valve Corporation, All rights reserved. ============//
//
// See download overview in src/engine/download_internal.h.
//
//=======================================================================================//

#ifndef REQUESTCONTEXT_H
#define REQUESTCONTEXT_H

//--------------------------------------------------------------------------------------------------------------

#include "engine/http.h"

//--------------------------------------------------------------------------------------------------------------

enum { BufferSize = 256 };	///< BufferSize is used extensively within the download system to size char buffers.

//--------------------------------------------------------------------------------------------------------------

#ifdef POSIX
typedef void *LPVOID;
#endif
#if defined( _X360 ) || defined( POSIX )
typedef LPVOID HINTERNET;
#endif

//--------------------------------------------------------------------------------------------------------------

struct RequestContext_t
{
	inline RequestContext_t()
	{
		memset( this, 0, sizeof( RequestContext_t ) );
	}

	/**
	 * The main thread sets this when it wants to abort the download,
	 * or it is done reading data from a finished download.
	 */
	bool			shouldStop;

	/**
	 * The download thread sets this when it is exiting, so the main thread
	 * can delete the RequestContext_t.
	 */
	bool			threadDone;

	bool			bIsBZ2;					///< true if the file is a .bz2 file that should be uncompressed at the end of the download.  Set and used by main thread.
	bool			bAsHTTP;				///< true if downloaded via HTTP and not ingame protocol.  Set and used by main thread
	unsigned int	nRequestID;				///< request ID for ingame download

	HTTPStatus_t	status;					///< Download thread status
	DWORD			fetchStatus;			///< Detailed status info for the download
	HTTPError_t		error;					///< Detailed error info

	char			baseURL[BufferSize];	///< Base URL (including http://).  Set by main thread.
	char			urlPath[BufferSize];	///< Path to be appended to base URL.  Set by main thread.
	char			absLocalPath[BufferSize];	///< Full local path where the file should go.  Set by main thread.
	char			gamePath[BufferSize];	///< Game path to be appended to base path.  Set by main thread.
	char			serverURL[BufferSize];	///< Server URL (IP:port, loopback, etc).  Set by main thread, and used for HTTP Referer header.

	bool			bSuppressFileWrite;		///< Set to true by main thread if we do not wish to write the file to disk

	/**
	 * The file's timestamp, as returned in the HTTP Last-Modified header.
	 * Saved to ensure partial download resumes match the original cached data.
	 */
	char			cachedTimestamp[BufferSize];

	DWORD			nBytesTotal;			///< Total bytes in the file
	DWORD			nBytesCurrent;			///< Current bytes downloaded
	DWORD			nBytesCached;			///< Amount of data present in cacheData.

	/**
	 * Buffer for the full file data.  Allocated/deleted by the download thread
	 * (which idles until the data is not needed anymore)
	 */
	unsigned char	*data;

	/**
	 * Buffer for partial data from previous failed downloads.
	 * Allocated/deleted by the main thread (deleted after download thread is done)
	 */
	unsigned char	*cacheData;

	// Used purely by the download thread - internal data -------------------
	HINTERNET		hOpenResource;			///< Handle created by InternetOpen
	HINTERNET		hDataResource;			///< Handle created by InternetOpenUrl

	/**
	 * For any user data we may want to attach to a context
	 */
	void			*pUserData;
};

//--------------------------------------------------------------------------------------------------------------

#endif // REQUESTCONTEXT_H
