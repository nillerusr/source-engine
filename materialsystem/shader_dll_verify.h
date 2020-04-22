//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: This is temporary obfuscation code to verify that a base shader
// DLL comes from us (because it includes some interfaces that we don't
// give out with the SDK).
//
//=============================================================================

#ifndef SHADER_DLL_VERIFY_H
#define SHADER_DLL_VERIFY_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "tier1/checksum_crc.h"
#include "tier1/checksum_md5.h"


#define SHADER_DLL_VERIFY_DATA_LEN1	4101
#define SHADER_DLL_VERIFY_DATA_PTR_OFFSET	43

#define SHADER_DLL_FNNAME_1	"_ftol3"
typedef void (*ShaderDLLVerifyFn)( char *pData );

abstract_class IShaderDLLVerification
{
public:

	virtual CRC32_t Function1( unsigned char *pData ) = 0;
	virtual void Function2( int a, int b, int c ) = 0;
	virtual void Function3( int a, int b, int c ) = 0;
	virtual void Function4( int a, int b, int c ) = 0;
	virtual CRC32_t Function5() = 0;
};


#endif // SHADER_DLL_VERIFY_H
