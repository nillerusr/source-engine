//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef _ERROR3D_H
#define _ERROR3D_H

typedef struct
{
	DWORD dwObjectID;
	LPCTSTR pszReason;
	PVOID pInfo;
} error3d;

void Add3dError(DWORD dwObjectID, LPCTSTR pszReason, PVOID pInfo = NULL);
error3d * Enum3dErrors(BOOL bStart = FALSE);
int Get3dErrorCount();

#endif // _ERROR3D_H