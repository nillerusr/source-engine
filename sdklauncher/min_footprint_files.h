//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef MIN_FOOTPRINT_FILES_H
#define MIN_FOOTPRINT_FILES_H
#ifdef _WIN32
#pragma once
#endif


// If bForceRefresh is true, then it will dump out all the files
// regardless of what version we think we currently have.
void DumpMinFootprintFiles( bool bForceRefresh );


#endif // MIN_FOOTPRINT_FILES_H
