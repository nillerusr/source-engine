//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Outputs directly into a binary format
//
//=============================================================================

#ifndef DMSERIALIZERBINARY_H
#define DMSERIALIZERBINARY_H

#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IDataModel;


//-----------------------------------------------------------------------------
// Installation methods for standard serializers
//-----------------------------------------------------------------------------
void InstallBinarySerializer( IDataModel *pFactory );


#endif // DMSERIALIZERBINARY_H