//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Main header file for the serializers DLL
//
//=============================================================================

#ifndef DMSERIALIZERS_H
#define DMSERIALIZERS_H

#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IDataModel;


//-----------------------------------------------------------------------------
// Externally defined importers
//-----------------------------------------------------------------------------
void InstallActBusyImporter( IDataModel *pFactory );
void InstallVMTImporter( IDataModel *pFactory );
void InstallSFMV1Importer( IDataModel *pFactory );
void InstallSFMV2Importer( IDataModel *pFactory );
void InstallSFMV3Importer( IDataModel *pFactory );
void InstallSFMV4Importer( IDataModel *pFactory );
void InstallSFMV5Importer( IDataModel *pFactory );
void InstallSFMV6Importer( IDataModel *pFactory );
void InstallSFMV7Importer( IDataModel *pFactory );
void InstallSFMV8Importer( IDataModel *pFactory );
void InstallSFMV9Importer( IDataModel *pFactory );
void InstallVMFImporter( IDataModel *pFactory );

void InstallDMXUpdater( IDataModel *pFactory );
void InstallSFMSessionUpdater( IDataModel *pFactory );
void InstallPCFUpdater( IDataModel *pFactory );


#endif // DMSERIALIZERS_H


