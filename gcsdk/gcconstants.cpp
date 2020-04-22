//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Enum name to string and vice versa code
//
//=============================================================================


#include "stdafx.h"
#include "enumutils.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{

#ifdef GC
	ENUMSTRINGS_START( EForeignKeyAction )
	{k_EForeignKeyActionNoAction, "NO ACTION" },
	{k_EForeignKeyActionCascade,  "CASCADE" },
	{k_EForeignKeyActionSetNULL,  "SET NULL" },
	ENUMSTRINGS_REVERSE( EForeignKeyAction, k_EForeignKeyActionNoAction )

	ENUMSTRINGS_START( EGCSQLType )
	{k_EGCSQLTypeInvalid, "k_EGCSQLTypeInvalid" },
	{k_EGCSQLType_Blob,  "k_EGCSQLType_Blob" },
	{k_EGCSQLType_String,  "k_EGCSQLType_String" },
	{k_EGCSQLType_int8, "k_EGCSQLType_int8" },
	{k_EGCSQLType_int16,  "k_EGCSQLType_int16" },
	{k_EGCSQLType_int32,  "k_EGCSQLType_int32" },
	{k_EGCSQLType_int64, "k_EGCSQLType_int64" },
	{k_EGCSQLType_float,  "k_EGCSQLType_float" },
	{k_EGCSQLType_double,  "k_EGCSQLType_double" },
	{k_EGCSQLType_Image,  "k_EGCSQLType_Image" },
	ENUMSTRINGS_REVERSE( EGCSQLType, k_EGCSQLTypeInvalid )
#endif

	ENUMSTRINGS_START( EUniverse )
	{ k_EUniverseInvalid, "Invalid" },
	{ k_EUniversePublic, "Public" },
	{ k_EUniverseBeta, "Beta" },
	{ k_EUniverseInternal, "Internal" },
	{ k_EUniverseDev, "Dev" },
	ENUMSTRINGS_REVERSE( EUniverse, k_EUniverseInvalid )

} // namespace GCSDK
