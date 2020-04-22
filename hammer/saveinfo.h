//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds information relevant to saving the document, such as the
//			rules for which objects to save.
//
// $NoKeywords: $
//=============================================================================//

#ifndef SAVEINFO_H
#define SAVEINFO_H
#pragma once


#include "ChunkFile.h"


class CMapClass;


class CSaveInfo
{
	public:

		inline CSaveInfo(void);
		inline void SetVisiblesOnly(bool bVisiblesOnly);

		bool ShouldSaveObject(CMapClass *pObject);

	protected:

		bool m_bVisiblesOnly;
};


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CSaveInfo::CSaveInfo(void)
{
	m_bVisiblesOnly = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bVisiblesOnly - 
//-----------------------------------------------------------------------------
void CSaveInfo::SetVisiblesOnly(bool bVisiblesOnly)
{
	m_bVisiblesOnly = bVisiblesOnly;
}


#endif // SAVEINFO_H
