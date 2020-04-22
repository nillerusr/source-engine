//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//--------------------------------------------------------------------------------------------------------------
// downloadthread.cpp
// 
// Implementation file for optional HTTP asset downloading thread
// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2004
//--------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// Includes
//--------------------------------------------------------------------------------------------------------------


#if defined( WIN32 ) && !defined( _X360 )
#include "winlite.h"
#include <WinInet.h>
#endif
#include <assert.h>
#include <sys/stat.h>
#include <stdio.h>

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "download_internal.h"
#include "tier1/strtools.h"
#include "tier0/threadtools.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------

void WriteFileFromRequestContext( const RequestContext_t &rc )
{
	struct stat buf;
	int rt = stat(rc.absLocalPath, &buf);
	if ( rt == -1 && !rc.bSuppressFileWrite )
	{
		FILE *fp = fopen( rc.absLocalPath, "wb" );
		if ( fp )
		{
			if ( rc.data )
			{
				fwrite( rc.data, rc.nBytesTotal, 1, fp );
			}
			fclose( fp );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------

#ifdef _WIN32
//--------------------------------------------------------------------------------------------------------------
/**
 * Formats a string to spit out via OutputDebugString (only in debug).  OutputDebugString
 * is threadsafe, so this should be fine.
 *
 * Since I don't want to be playing with the developer cvar in the other thread, I'll use
 * the presence of _DEBUG as my developer flag.
 */
void Thread_DPrintf (char *fmt, ...)
{
#ifdef _DEBUG
	va_list		argptr;
	char		msg[4096];
		
	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof(msg), fmt, argptr );
	va_end( argptr );
	OutputDebugString( msg );
#endif // _DEBUG
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Convenience function to name status states for debugging
 */
static const char *StateString( DWORD dwStatus )
{
	switch (dwStatus)
	{
		case INTERNET_STATUS_RESOLVING_NAME:
			return "INTERNET_STATUS_RESOLVING_NAME";
		case INTERNET_STATUS_NAME_RESOLVED:
			return "INTERNET_STATUS_NAME_RESOLVED";
		case INTERNET_STATUS_CONNECTING_TO_SERVER:
			return "INTERNET_STATUS_CONNECTING_TO_SERVER";
		case INTERNET_STATUS_CONNECTED_TO_SERVER:
			return "INTERNET_STATUS_CONNECTED_TO_SERVER";
		case INTERNET_STATUS_SENDING_REQUEST:
			return "INTERNET_STATUS_SENDING_REQUEST";
		case INTERNET_STATUS_REQUEST_SENT:
			return "INTERNET_STATUS_REQUEST_SENT";
		case INTERNET_STATUS_REQUEST_COMPLETE:
			return "INTERNET_STATUS_REQUEST_COMPLETE";
		case INTERNET_STATUS_CLOSING_CONNECTION:
			return "INTERNET_STATUS_CLOSING_CONNECTION";
		case INTERNET_STATUS_CONNECTION_CLOSED:
			return "INTERNET_STATUS_CONNECTION_CLOSED";
		case INTERNET_STATUS_RECEIVING_RESPONSE:
			return "INTERNET_STATUS_RECEIVING_RESPONSE";
		case INTERNET_STATUS_RESPONSE_RECEIVED:
			return "INTERNET_STATUS_RESPONSE_RECEIVED";
		case INTERNET_STATUS_HANDLE_CLOSING:
			return "INTERNET_STATUS_HANDLE_CLOSING";
		case INTERNET_STATUS_HANDLE_CREATED:
			return "INTERNET_STATUS_HANDLE_CREATED";
		case INTERNET_STATUS_INTERMEDIATE_RESPONSE:
			return "INTERNET_STATUS_INTERMEDIATE_RESPONSE";
		case INTERNET_STATUS_REDIRECT:
			return "INTERNET_STATUS_REDIRECT";
		case INTERNET_STATUS_STATE_CHANGE:
			return "INTERNET_STATUS_STATE_CHANGE";
	}
	return "Unknown";
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Callback function to update status information for a download (connecting to server, etc)
 */
void __stdcall DownloadStatusCallback( HINTERNET hOpenResource, DWORD dwContext, DWORD dwStatus, LPVOID pStatusInfo, DWORD dwStatusInfoLength )
{
	RequestContext_t *rc = (RequestContext_t*)pStatusInfo;

	switch (dwStatus)
	{
		case INTERNET_STATUS_RESOLVING_NAME:
		case INTERNET_STATUS_NAME_RESOLVED:
		case INTERNET_STATUS_CONNECTING_TO_SERVER:
		case INTERNET_STATUS_CONNECTED_TO_SERVER:
		case INTERNET_STATUS_SENDING_REQUEST:
		case INTERNET_STATUS_REQUEST_SENT:
		case INTERNET_STATUS_REQUEST_COMPLETE:
		case INTERNET_STATUS_CLOSING_CONNECTION:
		case INTERNET_STATUS_CONNECTION_CLOSED:
			if ( rc )
			{
				rc->fetchStatus = dwStatus;
			}
			else
			{
				//Thread_DPrintf( "** No RequestContext_t **\n" );
			}
			//Thread_DPrintf( "DownloadStatusCallback %s\n", StateString(dwStatus) );
			break;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Reads data from a handle opened by InternetOpenUrl().
 */
void ReadData( RequestContext_t& rc )
{
	const int BufferSize = 2048;
	unsigned char data[BufferSize];
	DWORD dwSize = 0;

	if ( !rc.nBytesTotal )
	{
		rc.status = HTTP_ERROR;
		rc.error = HTTP_ERROR_ZERO_LENGTH_FILE;
		return;
	}
	rc.nBytesCurrent = rc.nBytesCached;
	rc.status = HTTP_FETCH;

	while ( !rc.shouldStop )
	{
		// InternetReadFile() will block until there is data, or the socket gets closed.  This means the
		// main thread could request an abort while we're blocked here.  This is okay, because the main
		// thread will not wait for this thread to finish, but will clean up the RequestContext_t at some
		// later point when InternetReadFile() has returned and this thread has finished.
		if ( !InternetReadFile( rc.hDataResource, (LPVOID)data, BufferSize, &dwSize ) )
		{
			// if InternetReadFile() returns 0, there was a socket error (connection closed, etc)
			rc.status = HTTP_ERROR;
			rc.error = HTTP_ERROR_CONNECTION_CLOSED;
			return;
		}
		if ( !dwSize )
		{
			// if InternetReadFile() succeeded, but we read 0 bytes, we're at the end of the file.

			// if the file doesn't exist, write it out
			WriteFileFromRequestContext( rc );

			// Let the main thread know we finished reading data, and wait for it to let us exit.
			rc.status = HTTP_DONE;
			return;
		}
		else
		{
			// We've read some data.  Make sure we won't walk off the end of our buffer, then
			// use memcpy() to save the data off.
			DWORD safeSize = (DWORD) min( rc.nBytesTotal - rc.nBytesCurrent, (int)dwSize );
			//Thread_DPrintf( "Read %d bytes @ offset %d\n", safeSize, rc.nBytesCurrent );
			if ( safeSize != dwSize )
			{
				//Thread_DPrintf( "Warning - read more data than expected!\n" );
			}
			if ( rc.data && safeSize > 0 )
			{
				memcpy( rc.data + rc.nBytesCurrent, data, safeSize );
			}
			rc.nBytesCurrent += safeSize;
		}
	}

	// If we get here, rc.shouldStop was set early (user hit cancel).
	// Let the main thread know we aborted properly.
	rc.status = HTTP_ABORTED;
}

//--------------------------------------------------------------------------------------------------------------
const char *StatusString[] =
{
	"HTTP_CONNECTING",
	"HTTP_FETCH",
	"HTTP_DONE",
	"HTTP_ABORTED",
	"HTTP_ERROR",
};

//--------------------------------------------------------------------------------------------------------------
const char *ErrorString[] =
{
	"HTTP_ERROR_NONE",
	"HTTP_ERROR_ZERO_LENGTH_FILE",
	"HTTP_ERROR_CONNECTION_CLOSED",
	"HTTP_ERROR_INVALID_URL",
	"HTTP_ERROR_INVALID_PROTOCOL",
	"HTTP_ERROR_CANT_BIND_SOCKET",
	"HTTP_ERROR_CANT_CONNECT",
	"HTTP_ERROR_NO_HEADERS",
	"HTTP_ERROR_FILE_NONEXISTENT",
	"HTTP_ERROR_MAX",
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Closes all open handles, and waits until the main thread has given the OK
 * to quit.
 */
void CleanUpDownload( RequestContext_t& rc, HTTPStatus_t status, HTTPError_t error = HTTP_ERROR_NONE )
{
	if ( status != HTTP_DONE || error != HTTP_ERROR_NONE )
	{
		//Thread_DPrintf( "CleanUpDownload() - http status is %s, error state is %s\n",
		//	StatusString[status], ErrorString[error] );
	}

	rc.status = status;
	rc.error = error;

	// Close all HINTERNET handles we have open
	if ( rc.hDataResource && !InternetCloseHandle(rc.hDataResource) )
	{
		//Thread_DPrintf( "Failed to close data resource for %s%s\n", rc.baseURL, rc.gamePath );
	}
	else if ( rc.hOpenResource && !InternetCloseHandle(rc.hOpenResource) )
	{
		//Thread_DPrintf( "Failed to close open resource for %s%s\n", rc.baseURL, rc.gamePath );
	}
	rc.hDataResource = NULL;
	rc.hOpenResource = NULL;

	// wait until the main thread says we can go away (so it can look at rc.data).
	while ( !rc.shouldStop )
	{
		Sleep( 100 );
	}

	// Delete rc.data, which was allocated in this thread
	if ( rc.data != NULL )
	{
		delete[] rc.data;
		rc.data = NULL;
	}

	// and tell the main thread we're exiting, so it can delete rc.cachedData and rc itself.
	rc.threadDone = true;
}

//--------------------------------------------------------------------------------------------------------------
static void DumpHeaders( RequestContext_t& rc )
{
#ifdef _DEBUG
	DWORD dwSize;

	// First time we will find out the size of the headers.
	HttpQueryInfo ( rc.hDataResource, HTTP_QUERY_RAW_HEADERS_CRLF, NULL, &dwSize, NULL );
	char *lpBuffer =  new char [dwSize + 2];

	// Now we call HttpQueryInfo again to get the headers.
	if (!HttpQueryInfo ( rc.hDataResource, HTTP_QUERY_RAW_HEADERS_CRLF, (LPVOID)lpBuffer, &dwSize, NULL))
	{
		return;
	}
	*(lpBuffer + dwSize) = '\n';
	*(lpBuffer + dwSize + 1) = '\0';

	Thread_DPrintf( "------------------------------\n%s%s\n%s------------------------------\n",
		rc.baseURL, rc.gamePath, lpBuffer );
#endif
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Main download thread function - implements a (partial) synchronous HTTP download.
 */
DWORD __stdcall DownloadThread( void *voidPtr )
{
	RequestContext_t& rc = *(RequestContext_t *)voidPtr;

	URL_COMPONENTS url;
	char urlBuf[6][BufferSize];
	url.dwStructSize = sizeof(url);

	url.dwSchemeLength    = BufferSize;
	url.dwHostNameLength  = BufferSize;
	url.dwUserNameLength  = BufferSize;
	url.dwPasswordLength  = BufferSize;
	url.dwUrlPathLength   = BufferSize;
	url.dwExtraInfoLength = BufferSize;

	url.lpszScheme     = urlBuf[0];
	url.lpszHostName   = urlBuf[1];
	url.lpszUserName   = urlBuf[2];
	url.lpszPassword   = urlBuf[3];
	url.lpszUrlPath    = urlBuf[4];
	url.lpszExtraInfo  = urlBuf[5];

	char fullURL[BufferSize*2];
	DWORD fullURLLength = BufferSize*2;
	Q_snprintf( fullURL, fullURLLength, "%s%s", rc.baseURL, rc.urlPath );

	if ( !InternetCrackUrl( fullURL, fullURLLength, 0, &url ) )
	{
		CleanUpDownload( rc, HTTP_ERROR, HTTP_ERROR_INVALID_URL );
		return rc.status;
	}

	/// @TODO: Add FTP support (including restart of aborted transfers) -MDC 2004/01/08
	// We should be able to handle FTP downloads as well, but I don't have a server to check against, so
	// I'm gonna disallow it in case something bad would happen.
	if ( url.nScheme != INTERNET_SCHEME_HTTP && url.nScheme != INTERNET_SCHEME_HTTPS )
	{
		CleanUpDownload( rc, HTTP_ERROR, HTTP_ERROR_INVALID_PROTOCOL );
		return rc.status;
	}

	// Open a socket etc for the download.
	// The first parameter, "Half-Life", is the User-Agent that gets sent with HTTP requests.
	// INTERNET_OPEN_TYPE_PRECONFIG specifies using IE's proxy info from the registry for HTTP downloads.
	rc.hOpenResource = InternetOpen( "Half-Life 2", INTERNET_OPEN_TYPE_PRECONFIG ,NULL, NULL, 0);

	if ( !rc.hOpenResource )
	{
		CleanUpDownload( rc, HTTP_ERROR, HTTP_ERROR_CANT_BIND_SOCKET );
		return rc.status;
	}

	InternetSetStatusCallback( rc.hOpenResource, (INTERNET_STATUS_CALLBACK)DownloadStatusCallback );

	if ( rc.shouldStop )
	{
		CleanUpDownload( rc, HTTP_ABORTED );
		return rc.status;
	}

	// Set up some flags
	DWORD flags = 0;
	flags |= INTERNET_FLAG_RELOAD;			// Get from server, not IE's cache
	flags |= INTERNET_FLAG_NO_CACHE_WRITE;	// Don't write to IE's cache, since we're doing our own caching of partial downloads
	flags |= INTERNET_FLAG_KEEP_CONNECTION;	// Use keep-alive semantics.  Since each file downloaded is a separate connection
											//   from a separate thread, I don't think this does much.  Can't hurt, though.
	if ( url.nScheme == INTERNET_SCHEME_HTTPS )
	{
		// The following flags allow us to use https:// URLs, but don't provide much in the way of authentication.
		// In other words, this allows people with only access to https:// servers (?!?) to host files as transparently
		// as possible.
		flags |= INTERNET_FLAG_SECURE;						// Use SSL, etc.  Kinda need this for HTTPS URLs.
		flags |= INTERNET_FLAG_IGNORE_CERT_CN_INVALID;		// Don't check hostname on the SSL cert.
		flags |= INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;	// Don't check for expired SSL certs.
	}

	// Request a partial if we have the data
	char headers[BufferSize] = "";
	DWORD headerLen = 0;
	char *headerPtr = NULL;
	if ( *rc.cachedTimestamp && rc.nBytesCached )
	{
		if ( *rc.serverURL )
		{
			Q_snprintf( headers, BufferSize, "If-Range: %s\nRange: bytes=%d-\nReferer: hl2://%s\n",
				rc.cachedTimestamp, rc.nBytesCached, rc.serverURL );
		}
		else
		{
			Q_snprintf( headers, BufferSize, "If-Range: %s\nRange: bytes=%d-\n",
				rc.cachedTimestamp, rc.nBytesCached );
		}
		headerPtr = headers;
		headerLen = (DWORD)-1L; // the DWORD cast is because we get a signed/unsigned mismatch even with an L on the -1.
		//Thread_DPrintf( "Requesting partial download\n%s", headers );
	}
	else if ( *rc.serverURL )
	{
		Q_snprintf( headers, BufferSize, "Referer: hl2://%s\n", rc.serverURL );
		headerPtr = headers;
		headerLen = (DWORD)-1L; // the DWORD cast is because we get a signed/unsigned mismatch even with an L on the -1.
		//Thread_DPrintf( "Requesting full download\n%s", headers );
	}

	rc.hDataResource = InternetOpenUrl(rc.hOpenResource, fullURL, headerPtr, headerLen, flags,(DWORD)(&rc) );

	// send the request off
	if ( !rc.hDataResource )
	{
		CleanUpDownload( rc, HTTP_ERROR, HTTP_ERROR_CANT_CONNECT );
		return rc.status;
	}

	if ( rc.shouldStop )
	{
		CleanUpDownload( rc, HTTP_ABORTED );
		return rc.status;
	}

	//DumpHeaders( rc ); // debug

	// check the status (are we gonna get anything?)
	DWORD size = sizeof(DWORD);
	DWORD code;
	if ( !HttpQueryInfo( rc.hDataResource, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &code, &size, NULL ) )
	{
		CleanUpDownload( rc, HTTP_ERROR, HTTP_ERROR_NO_HEADERS );
		return rc.status;
	}

	// Only status codes we're looking for are HTTP_STATUS_OK (200) and HTTP_STATUS_PARTIAL_CONTENT (206)
	if ( code != HTTP_STATUS_OK && code != HTTP_STATUS_PARTIAL_CONTENT )
	{
		CleanUpDownload( rc, HTTP_ERROR, HTTP_ERROR_FILE_NONEXISTENT );
		return rc.status;
	}

	// get the timestamp, and save it off for future resumes, in case we abort this transfer later.
	size = BufferSize;
	if ( !HttpQueryInfo( rc.hDataResource, HTTP_QUERY_LAST_MODIFIED, rc.cachedTimestamp, &size, NULL ) )
	{
		rc.cachedTimestamp[0] = 0;
	}
	rc.cachedTimestamp[BufferSize-1] = 0;

	// If we're not getting a partial download, don't use any cached data, even if we have some.
	if ( code != HTTP_STATUS_PARTIAL_CONTENT )
	{
		if ( rc.nBytesCached )
		{
			//Thread_DPrintf( "Partial download refused - getting full version\n" );
		}
		rc.nBytesCached = 0; // start from the beginning
	}

	// Check the resource size, and allocate a buffer
	size = sizeof(code);
	if ( HttpQueryInfo( rc.hDataResource, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &code, &size, NULL ) )
	{
		rc.nBytesTotal = code + rc.nBytesCached;
		if ( code > 0 )
		{
			rc.data = new unsigned char[rc.nBytesTotal + 1];	// Extra byte for NULL terminator
			if ( !rc.data )
			{
				// We're seeing crazy large numbers being returned in code (0x48e1e22), the new fails, and we crash.
				// This should probably be a different error?
				CleanUpDownload( rc, HTTP_ERROR, HTTP_ERROR_ZERO_LENGTH_FILE );
				return rc.status;
			}
			rc.data[ rc.nBytesTotal ] = 0;
		}
	}
	else
	{
		CleanUpDownload( rc, HTTP_ERROR, HTTP_ERROR_ZERO_LENGTH_FILE );
		return rc.status;
	}

	// copy cached data into buffer
	if ( rc.cacheData && rc.nBytesCached )
	{
		int len = min( rc.nBytesCached, rc.nBytesTotal );
		memcpy( rc.data, rc.cacheData, len );
	}

	if ( rc.shouldStop )
	{
		CleanUpDownload( rc, HTTP_ABORTED );
		return rc.status;
	}

	// now download the actual data
	ReadData( rc );

	// and stick around until the main thread has gotten the data.
	CleanUpDownload( rc, rc.status, rc.error );
	return rc.status;
}


#elif defined( POSIX )

#include "curl/curl.h"

// curl callback functions

static size_t curlWriteFn( void *ptr, size_t size, size_t nmemb, void *stream)
{
	RequestContext_t *pRC = (RequestContext_t *) stream;
	if ( pRC->nBytesTotal && pRC->nBytesCurrent + ( size * nmemb ) <= pRC->nBytesTotal )
	{
		Q_memcpy( pRC->data + pRC->nBytesCurrent, ptr, ( size * nmemb ) );
		pRC->nBytesCurrent += size * nmemb;
	}
	return size * nmemb;
}


int Q_StrTrim( char *pStr )
{
	char *pSource = pStr;
	char *pDest = pStr;
	
	// skip white space at the beginning
	while ( *pSource != 0 && isspace( *pSource ) )
	{
		pSource++;
	}
	
	// copy everything else
	char *pLastWhiteBlock = NULL;
	char *pStart = pDest;
	while ( *pSource != 0 )
	{
		*pDest = *pSource++;
		if ( isspace( *pDest ) )
		{
			if ( pLastWhiteBlock == NULL )
				pLastWhiteBlock = pDest;
		}
		else
		{
			pLastWhiteBlock = NULL;
		}
		pDest++;
	}
	*pDest = 0;
	
	// did we end in a whitespace block?
	if ( pLastWhiteBlock != NULL )
	{
		// yep; shorten the string
		pDest = pLastWhiteBlock;
		*pLastWhiteBlock = 0;
	}
	
	return pDest - pStart;
}

static size_t curlHeaderFn( void *ptr, size_t size, size_t nmemb, void *stream)
{
	char *pszHeader = (char*)ptr;
	char *pszValue = NULL;
	RequestContext_t *pRC = (RequestContext_t *) stream;
	
	pszHeader[ ( size * nmemb - 1 ) ] = NULL;
	pszValue = Q_strstr( pszHeader, ":" );
	if ( pszValue )
	{
		// null terminate the header name, and point pszValue at it's value
		*pszValue = NULL;
		pszValue++;
		Q_StrTrim( pszValue );
	}
	if ( 0 == Q_stricmp( pszHeader, "Content-Length" ) )
	{
		size_t len = atol( pszValue );
		if ( pRC && len )
		{
			pRC->nBytesTotal = len;
			pRC->data = (byte*)malloc( len );
		}
	}
	
	return size * nmemb;
}


// we're going to abuse this by using it for proxy pac fetching
// the cacheData field will hold the URL of the PAC, and the data
// field the contents of the pac
RequestContext_t g_pacRequestCtx;

// system specific headers for proxy configuration
#if defined(OSX)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <SystemConfiguration/SystemConfiguration.h>
#endif


void SetProxiesForURL( CURL *hMasterCURL, const char *pszURL )
{
	uint32 uProxyPort = 0;
	char rgchProxyHost[1024]; 
	char *pszProxyExceptionList = NULL;
	rgchProxyHost[0] = '\0';
	
#if defined(OSX)
	
	// create an urlref around the raw URL
	CFURLRef url = CFURLCreateWithBytes( NULL, ( const UInt8 * ) pszURL, strlen( pszURL ), kCFStringEncodingASCII, NULL );
	// copy the proxies dictionary 
	CFDictionaryRef proxyDict = SCDynamicStoreCopyProxies(NULL);
	// and ask the system what proxies it thinks I should consider for the given URL
	CFArrayRef proxies = CFNetworkCopyProxiesForURL( url, proxyDict );
	
	CFIndex iProxy;
	// walk through the returned set, looking for any types we (and lib curl) can handle
	// the list is returned in "preference order", but we can only handle http, and pac urls
	for( iProxy = 0; iProxy < CFArrayGetCount( proxies ); iProxy++ )
	{
		CFDictionaryRef proxy =	(CFDictionaryRef) CFArrayGetValueAtIndex( proxies, iProxy );
		
		if ( proxy == NULL )
			break;
		
		// what type of proxy is this one?
		CFStringRef proxyType = (CFStringRef) CFDictionaryGetValue( proxy, kCFProxyTypeKey );
		
		if ( CFEqual( proxyType, kCFProxyTypeNone ) )
		{
			// no proxy should be used - we're done.
			break;
		}
		else if ( CFEqual( proxyType, kCFProxyTypeHTTP ) )
		{
			// manually configured HTTP proxy settings.
			const void *val = NULL;
			
			// grab the proxy port
			val = CFDictionaryGetValue( proxy, kCFProxyPortNumberKey );
			if ( val == NULL || !CFNumberGetValue( (CFNumberRef) val, kCFNumberIntType, &uProxyPort ) )
				// either we failed, or the port was invalid
				continue;
			
			// no port specified - use the default http port
			if ( uProxyPort == 0 )
				uProxyPort = 80;
			
			int cbOffset = 0;
			// see if they've specified authentication (username/password)
			val = CFDictionaryGetValue( proxy, kCFProxyUsernameKey );
			if ( val != NULL && CFStringGetCString( (CFStringRef) val, rgchProxyHost + cbOffset, sizeof(rgchProxyHost) - cbOffset, kCFStringEncodingASCII ) && rgchProxyHost[cbOffset] != '\0' )
			{
				// we've got "username" in rgchProxyHost
				cbOffset = Q_strlen( rgchProxyHost );
				val = CFDictionaryGetValue( proxy, kCFProxyPasswordKey );
				if ( val != NULL && CFStringGetLength( (CFStringRef) val ) ) 
				{
					// and there's a non-null password value - put a colon after username
					rgchProxyHost[cbOffset++] = ':';
					CFStringGetCString( (CFStringRef) val, rgchProxyHost + cbOffset, sizeof(rgchProxyHost) - cbOffset, kCFStringEncodingASCII );
					// now we've got user:password in rgchProxyHost
					cbOffset = Q_strlen( rgchProxyHost );
				}
				// since we've got at least a username, we need an @
				rgchProxyHost[cbOffset++] = '@';
			}
			
			val = CFDictionaryGetValue( proxy, kCFProxyHostNameKey );
			if ( val == NULL || !CFStringGetCString( (CFStringRef) val, rgchProxyHost + cbOffset, sizeof(rgchProxyHost) - cbOffset, kCFStringEncodingASCII ) || rgchProxyHost[cbOffset] == '\0' )
				continue;
			
			break;
		} 
		else if ( CFEqual( proxyType, kCFProxyTypeAutoConfigurationURL ) )
		{
			// a proxy autoconfig URL has been provided
			char rgchPacURL[1024];
			// get the url (as an urlref) and turn it into a string
			CFURLRef cfUrl = (CFURLRef) CFDictionaryGetValue( proxy, kCFProxyAutoConfigurationURLKey );
			CFStringGetCString( (CFStringRef) CFStringCreateWithFormat( NULL, NULL, CFSTR("%@"), cfUrl ),
							   rgchPacURL, sizeof( rgchPacURL ), kCFStringEncodingASCII );
			
			CURLcode res = CURLE_OK;
			// see if we've not yet fetched this pac file
			if ( !g_pacRequestCtx.cacheData || Q_strcmp( (const char *)g_pacRequestCtx.cacheData, rgchPacURL ) )
			{			
				if ( g_pacRequestCtx.cacheData )
				{
					free( g_pacRequestCtx.cacheData ); 
					g_pacRequestCtx.cacheData = NULL;
				}				
				if ( g_pacRequestCtx.data )
				{
					free( g_pacRequestCtx.data ); 
					g_pacRequestCtx.data = NULL;
				}
				
				// grab the data, using the same request context structure (and callbacks) we use for real downloads
				CURL *hCURL = curl_easy_init();
				if ( !hCURL )
				{
					AssertMsg( hCURL, "failed to initialize curl handle" );
					break;
				}
				curl_easy_setopt( hCURL, CURLOPT_NOPROGRESS, 1 );
				curl_easy_setopt( hCURL, CURLOPT_NOSIGNAL, 1 );
				curl_easy_setopt( hCURL, CURLOPT_CONNECTTIMEOUT, 30 );
				curl_easy_setopt( hCURL, CURLOPT_FOLLOWLOCATION, 1 ); // follow 30x redirections from the web server
				
				// and setup the callback fns
				curl_easy_setopt( hCURL, CURLOPT_HEADERFUNCTION, &curlHeaderFn );
				curl_easy_setopt( hCURL, CURLOPT_WRITEFUNCTION, &curlWriteFn );
				
				// setup callback stream pointers
				curl_easy_setopt( hCURL, CURLOPT_WRITEHEADER, &g_pacRequestCtx );
				curl_easy_setopt( hCURL, CURLOPT_WRITEDATA, &g_pacRequestCtx );
				
				curl_easy_setopt( hCURL, CURLOPT_URL, rgchPacURL );
				
				res = curl_easy_perform( hCURL );
				curl_easy_cleanup( hCURL );
			}
			if ( res == CURLE_OK )
			{
				// copy the URL into the "pac cache", if necessary
				if ( !g_pacRequestCtx.cacheData )
				{
					g_pacRequestCtx.cacheData = (unsigned char*) malloc( Q_strlen( rgchPacURL ) + 1 );
					Q_memcpy( g_pacRequestCtx.cacheData, rgchPacURL, Q_strlen( rgchPacURL ) );
				}
				
				if ( !g_pacRequestCtx.data ) // no data in the proxy.pac they have, so just ignore it
					return;
				
				// wrap the data (the pac contents) into a cfstring
				CFStringRef cfPacStr = CFStringCreateWithCString( kCFAllocatorDefault, (const char *)g_pacRequestCtx.data, kCFStringEncodingASCII );
				
				// and ask the system, given this proxy pac, what (list of) proxies should I consider for this URL?
				CFErrorRef err;
				CFArrayRef proxiesForUrl = CFNetworkCopyProxiesForAutoConfigurationScript( cfPacStr, cfUrl, &err );
				if ( proxiesForUrl )
				{
					// we're re-assigning the value that the loop is iterating over, the postincrement will fire after we do this,
					// hence the -1 (rather than 0) assignment to iProxy
					proxies = proxiesForUrl;
					iProxy = -1;					
				}
				continue;
			}
			else
			{
				if ( g_pacRequestCtx.cacheData )
				{
					free( g_pacRequestCtx.cacheData );
					g_pacRequestCtx.cacheData = NULL;
				}
			}
		}
		else
		{
			Msg( "unsupported proxy type\n" );
			break;
		}		
	}
#else
#warning "CHTTPDownloadThread doesn't know how to set proxy config"
#endif
	
	if ( rgchProxyHost[0] == '\0' || uProxyPort <= 0 )
	{
		if ( pszProxyExceptionList )
			free( pszProxyExceptionList );
		return;
	}
	
	curl_easy_setopt( hMasterCURL, CURLOPT_PROXY, rgchProxyHost );
	curl_easy_setopt( hMasterCURL, CURLOPT_PROXYPORT, uProxyPort );
	if ( pszProxyExceptionList )
	{
		curl_easy_setopt( hMasterCURL, (CURLoption) (CURLOPTTYPE_OBJECTPOINT + 177) /*CURLOPT_NOPROXY*/ , pszProxyExceptionList );
		free( pszProxyExceptionList );
	}
	
}


void DownloadThread( void *voidPtr )
{
	static bool bDoneInit = false;
	if ( !bDoneInit )
	{
		bDoneInit = true;
		curl_global_init( CURL_GLOBAL_SSL );
	}
	
	RequestContext_t& rc = *(RequestContext_t *)voidPtr;
	
	rc.status = HTTP_FETCH;

	CURL *hCURL = curl_easy_init();
	if ( !hCURL )
	{
		rc.error = HTTP_ERROR_INVALID_URL;
		rc.status = HTTP_ERROR;
		rc.threadDone = true;
		return;
	}
	
	curl_easy_setopt( hCURL, CURLOPT_NOPROGRESS, 1 );
	curl_easy_setopt( hCURL, CURLOPT_NOSIGNAL, 1 );
	curl_easy_setopt( hCURL, CURLOPT_CONNECTTIMEOUT, 30 );
	curl_easy_setopt( hCURL, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4 );

	// turn off certificate verification similar to how we set INTERNET_FLAG_IGNORE_CERT_CN_INVALID and INTERNET_FLAG_IGNORE_CERT_DATE_INVALID on Windows
	curl_easy_setopt( hCURL, CURLOPT_SSL_VERIFYHOST, 0 );
	curl_easy_setopt( hCURL, CURLOPT_SSL_VERIFYPEER, 0 );
	
	// and now the callback fns
	curl_easy_setopt( hCURL, CURLOPT_HEADERFUNCTION, &curlHeaderFn );
	curl_easy_setopt( hCURL, CURLOPT_WRITEFUNCTION, &curlWriteFn );

	
	uint32 cubURL = Q_strlen( rc.baseURL ) + Q_strlen( rc.urlPath )  + 2 /*one for the /, one for the null*/;
	char *pchURL = (char *) malloc( cubURL );
	Q_snprintf( pchURL, cubURL, "%s%s", rc.baseURL, rc.urlPath );

	uint32 cubReferer = 0;
	char *pchReferer = NULL;
	if ( *rc.serverURL )
	{
		cubReferer = Q_strlen( rc.serverURL ) + 8;
		pchReferer = (char *) malloc( cubReferer );
		Q_snprintf( pchReferer, cubURL, "hl2://%s", rc.serverURL );
	}

	// setup proxies
	SetProxiesForURL( hCURL, pchURL );
	
	// set the url
	curl_easy_setopt( hCURL, CURLOPT_URL, pchURL );
	
	// setup callback stream pointers
	curl_easy_setopt( hCURL, CURLOPT_WRITEHEADER, &rc );
	curl_easy_setopt( hCURL, CURLOPT_WRITEDATA, &rc );
	
	curl_easy_setopt( hCURL, CURLOPT_FOLLOWLOCATION, 1 );
	curl_easy_setopt( hCURL, CURLOPT_MAXREDIRS, 1 );
	curl_easy_setopt( hCURL, CURLOPT_UNRESTRICTED_AUTH, 1 );
	curl_easy_setopt( hCURL, CURLOPT_USERAGENT, "Half-Life 2" );
	if ( pchReferer )
	{
		curl_easy_setopt( hCURL, CURLOPT_REFERER, pchReferer );
	}


	// g0g0g0
	CURLcode res = curl_easy_perform( hCURL );

	free( pchURL );
	if ( pchReferer )
	{
		free( pchReferer );
	}
	
	if ( res == CURLE_OK )
	{
		curl_easy_getinfo( hCURL , CURLINFO_RESPONSE_CODE , &rc.status );
		if ( rc.status == HTTPStatus_t(200) || rc.status == HTTPStatus_t(206) )
		{
			// write the file before we change the status to DONE, so that the write
			// will finish before the main thread goes on and starts messing with the file
			WriteFileFromRequestContext( rc );
	
			rc.status = HTTP_DONE;
			rc.error = HTTP_ERROR_NONE;
		}
		else
		{
			rc.status = HTTP_ERROR;
			rc.error = HTTP_ERROR_FILE_NONEXISTENT;
		}
	}
	else 
	{
		rc.status = HTTP_ERROR;
	}

	// wait until the main thread says we can go away (so it can look at rc.data).
	while ( !rc.shouldStop )
	{
		ThreadSleep( 100 );
	}

	// Delete rc.data, which was allocated in this thread
	if ( rc.data != NULL )
	{
		delete[] rc.data;
		rc.data = NULL;
	}
	
	
	curl_easy_cleanup( hCURL );
	
	rc.threadDone = true;	
}
#else
void DownloadThread( void *voidPtr )
{
#warning "DownloadThread Not Implemented"
	Assert( !"Implement me" );
}
#endif

