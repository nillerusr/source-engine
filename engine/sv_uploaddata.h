//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SV_UPLOADDATA_H
#define SV_UPLOADDATA_H
#ifdef _WIN32
#pragma once
#endif


class KeyValues;

bool UploadData( char const *cserIP, char const *tablename, KeyValues *fields );

#endif // SV_UPLOADDATA_H
