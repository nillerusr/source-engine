//=============================================================================
//
//========= Copyright Valve Corporation, All rights reserved. ============//
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Some junk to isolate <windows.h> from polluting everything
//
//=============================================================================

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // #ifdef WIN32


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void MyGetUserName( char *pszBuf, unsigned long *pBufSiz )
{
	GetUserName( pszBuf, pBufSiz );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void MyGetComputerName( char *pszBuf, unsigned long *pBufSiz )
{
	GetComputerName( pszBuf, pBufSiz );
}