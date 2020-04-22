//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef DMSERIALIZERKEYVALUES2_H
#define DMSERIALIZERKEYVALUES2_H

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
void InstallKeyValues2Serializer( IDataModel *pFactory );


#endif // DMSERIALIZERKEYVALUES2_H