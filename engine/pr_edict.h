//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Expose things from pr_edict.cpp.
//
// $NoKeywords: $
//===========================================================================//

#ifndef PR_EDICT_H
#define PR_EDICT_H


#include "edict.h"


void InitializeEntityDLLFields( edict_t *pEdict );

// If iForceEdictIndex is not -1, then it will return the edict with that index. If that edict index
// is already used, it'll return null.
edict_t *ED_Alloc( int iForceEdictIndex = -1 );
void	ED_Free( edict_t *ed );

// Clear the FL_EDICT_FREE flag and the g_FreeEdicts bit.
void	ED_ClearFreeFlag( edict_t *pEdict );

edict_t *EDICT_NUM(int n);
int NUM_FOR_EDICT(const edict_t *e);

void ED_AllowImmediateReuse();
void ED_ClearFreeEdictList();

#endif


