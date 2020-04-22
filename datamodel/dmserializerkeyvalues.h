//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef DMSERIALIZERKEYVALUES_H
#define DMSERIALIZERKEYVALUES_H

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
void InstallKeyValuesSerializer( IDataModel *pFactory );


#endif // DMSERIALIZER_H