
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*******************************************************************************
**
** Contents:
**
**		This file provides the public interface to the Steam service.  This
**		interface is described in the SDK documentation.
**
******************************************************************************/


#ifndef INCLUDED_VALIDATENEWVALVECDKEYCLIENT_H
#define INCLUDED_VALIDATENEWVALVECDKEYCLIENT_H


#if defined(_MSC_VER) && (_MSC_VER > 1000)
#pragma once
#endif


#ifndef INCLUDED_STEAM_COMMON_STEAMCOMMON_H
	#include "steamcommon.h"
#endif


#ifdef __cplusplus
extern "C"
{
#endif


	// Note: this function expects the CDkey to contain dashes to split it into groups of 5 characters.
	//
	// e.g.		5WV2E-CDL82-89AQ4-ZZKH8-L27IB		(note: this example is not a valid key)
	//
	ESteamError	SteamWeakVerifyNewValveCDKey
					(
						const char *	pszCDKeyFormattedForCDLabel,
						uint *			pReceiveGameCode,
						uint *			pReceiveSalesTerritoryCode,
						uint *			pReceiveUniqueSerialNumber
					);

	// This returns data ready to send to the validation server.  
	// If you want to store a CDKey in the registry, consider passing the 
	// output of this function through one of the functions below, 
	// to make life harder for registry-harvesting trojans.
	//
	// This also does a WeakVerify.
	ESteamError	SteamGetEncryptedNewValveCDKey
					(
						const char *	pszCDKeyFormattedForCDLabel,
						unsigned int	ClientLocalIPAddr,
						const void *	pEncryptionKeyReceivedFromAppServer, 
						unsigned int	uEncryptionKeyLength, 
						void *			pOutputBuffer, 
						unsigned int	uSizeOfOutputBuffer, 
						unsigned int *	pReceiveSizeOfEncryptedNewValveCDKey
					);


	// This pair of functions encrypt and decrypt data using a key based on unique 
	// properties of this machine.  The resulting encrypted data can only be (easily) 
	// decrypted on the same machine.  Note: it is NOT impossible to decrypt the data
	// elsewhere, just inconvenient.
	ESteamError	SteamEncryptDataForThisMachine
					(
						const char *	pDataToEncrypt, 
						unsigned int	uSizeOfDataToEncrypt, 
						void *			pOutputBuffer, 
						unsigned int	uSizeOfOutputBuffer, 
						unsigned int *	pReceiveSizeOfEncryptedData
					);

	ESteamError	SteamDecryptDataForThisMachine
					(
						const char *	pDataToDecrypt, 
						unsigned int	uSizeOfDataToDecrypt, 
						void *			pOutputBuffer, 
						unsigned int	uSizeOfOutputBuffer, 
						unsigned int *	pReceiveSizeOfDecryptedData
					);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef INCLUDED_STEAM_COMMON_STEAMCOMMON_H */
