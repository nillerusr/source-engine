//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//
//==================================================================================================

#ifndef CRCCHECK_SHARED_H
#define CRCCHECK_SHARED_H
#ifdef _WIN32
#pragma once
#endif


#ifdef STEAM
#define VPCCRCCHECK_EXE_FILENAME	"vpc.exe"
#else
#define VPCCRCCHECK_EXE_FILENAME	"vpccrccheck.exe"
#endif

// The file extension for the file that contains the CRCs that a vcproj depends on.
#define VPCCRCCHECK_FILE_EXTENSION		"vpc_crc"
#define VPCCRCCHECK_FILE_VERSION_STRING	"[vpc crc file version 1]"


void Sys_Error( const char *format, ... );
int Sys_LoadTextFileWithIncludes( const char* filename, char** bufferptr );

bool VPC_CheckProjectDependencyCRCs( const char *pProjectFilename, const char *pReferenceSupplementalString, char *pErrorString, int nErrorStringLength );

// Used by vpccrccheck.exe or by vpc.exe to do the CRC check that's initiated in the pre-build steps.
int VPC_CommandLineCRCChecks( int argc, char **argv );


#endif // CRCCHECK_SHARED_H
