//========= Copyright Valve Corporation, All rights reserved. ============//
//
// download_internal.h
//
// Header file for optional HTTP asset downloading
// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2004
//
//=======================================================================================//

#ifndef DOWNLOAD_INTERNAL_H
#define DOWNLOAD_INTERNAL_H

/**
 * -------------------
 * Download overview:
 *  HTTP downloading is done outside the main thread, to increase responsiveness
 *  and for code clarity (the download can be a linear sequence of blocking calls).
 *
 *  The process is as follows, from the perspective of the main thread:
 *   1. Downloads are queued up when connecting to a server, if the server sends
 *      a download URL.
 *   2. If HTTP downloads are queued up, the client disconnects from the server,
 *      and puts up a progress bar dialog box for the transfers.
 *   3. The CL_HTTPUpdate() function is called every frame, and it does the following:
 *      a) Starts a download thread if none are active, and at least one is queued up.
 *      b) Checks on status of the current transfer and updates the progress bar.
 *         i. If the thread has finished, the file is written out to disk and another
 *            transfer is started.
 *         ii. If the thread has aborted/errored out, any partial data is written to
 *             the cache, the progress bar is removed, and an error message is
 *             shown, if neccessary.
 *      c) Checks for old transfers to clean up where the download thread has exited.
 *   4. If the user hits Cancel on the progress bar, the current download is told to
 *      abort, and the progress bar is removed.
 *
 *  The process is as follows, from the perspective of the download thread:
 *   1. A complete URL is constructed and verified for correctness.
 *   2. The connection is established with the server.
 *   3. The file is requested from the server, including the file timestamp and
 *      byte range for partial transfers (resuming an aborted transfer).  A buffer
 *      for the data is allocated in this thread.
 *   4. while the main thread has not requested an abort, read data from the server.
 *   5. Upon completion, abort, or an error, the thread does the following:
 *      a) close any open handles
 *      b) wait for the main thread to request an exit (so the main thread can read
 *         any data.)
 *      c) delete the data buffer (previously allocated in this thread)
 *      d) sets a flag letting the main thread know this thread is finished.
 *      e) exit
 *
 * -------------------
 * Thread interaction:
 *  All thread interaction is handled via a shared RequestContext_t.  Interaction is
 *  structured so that at all times, only one thread can be writing to any given
 *  variable.
 *
 *  This is an attempt to enumerate all cases of thread interaction:
 *   1. Before download thread creation
 *      a) main thread allocates the RequestContext_t, and zeroes it out.
 *      b) main thread fills in the baseURL and gamePath strings.
 *      c) main thread sets cachedTimestamp, nBytesCached, and allocates/fills
 *         cacheData if there is data from a previous aborted download.
 *   2. During thread operation:
 *      a) download thread can do the following:
 *         i.   for HTTP_CONNECTING, download thread can set nBytesTotal,
 *              nBytesCached, and nBytesCurrent.  It can allocate data, and
 *              set the status to HTTP_FETCH, HTTP_ABORTED, or HTTP_ERROR.
 *         ii.  for HTTP_FETCH, download thread can write to data, set
 *              nBytesCurrent, and set status to HTTP_DONE, HTTP_ABORTED,
 *              or HTTP_ERROR.
 *         iii. for HTTP_DONE, HTTP_ABORTED, and HTTP_ERROR, the download thread
 *              can only look at shouldStop until it is set by the main thread.
 *      b) main thread can look at status.
 *         i.   for HTTP_CONNECTING, nothing is read, and only shouldStop can be set.
 *         ii.  for HTTP_FETCH, nBytesTotal and nBytesCurrent are read, and
 *              shouldStop can be set.
 *         iii. for HTTP_DONE, nBytesTotal, nBytesCurrent, and data are read.  Also,
 *              shouldStop can be set.
 *         iv.  for HTTP_ABORTED, nothing is read, and only shouldStop can be set.
 *         v.   for HTTP_ERROR, nBytesTotal, nBytesCurrent, and data are read.  Also,
 *              shouldStop can be set.
 *   3. After shouldStop is set by main thread:
 *      a) if the download thread is in status HTTP_FETCH, it will cease operations
 *         and set status to HTTP_ABORTED.
 *      b) download thread will delete data, if it exists.
 *      c) download thread will set threadDone.
 *      d) after the main thread set shouldStop, it will only look at threadDone.
 *   4. After threadDone is set by download thread:
 *      a) download thread can safely exit, and will not access the RequestContext_t.
 *      b) main thread can delete the cacheData, if any exists, and delete the
 *         RequestContext_t itself.  Thus ends the download.
 *   5. SPECIAL CASE: if the user hits Cancel during a download, the main thread will
 *      look at nBytesTotal and nBytesCurrent to determine if there is any data
 *      present, and read from data if there is.  The download thread will most likely
 *      be in HTTP_CONNECTING or HTTP_FETCH, but could also be in HTTP_DONE or
 *      HTTP_ERROR at this time.  Regardless, this should be safe because of the
 *      following reasons:
 *      a) if nBytesCurrent is non-zero, this means data has been allocated, and
 *         nBytesTotal has been set and won't change.
 *      b) nBytesCurrent increases monotonically, so the contents of
 *         data[0..nBytesCurrent] is complete and unchanging.
 *
 */

//--------------------------------------------------------------------------------------------------------------

#include "engine/requestcontext.h"

//--------------------------------------------------------------------------------------------------------------

#ifdef POSIX
void DownloadThread( void *voidPtr );
#else
DWORD __stdcall DownloadThread( void *voidPtr );
#endif
#endif // DOWNLOAD_INTERNAL_H

//--------------------------------------------------------------------------------------------------------------