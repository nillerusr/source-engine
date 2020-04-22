//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: This module is for wrapping Crypto++ functions, including crypto++
//          directly has nasty consequences polluting the global namespace, and
//          conflicting with xdebug and locale stuff, so we only include it here
//          and use this wrapper in the rest of our code.
//
// $NoKeywords: $
//=============================================================================

#ifndef CRYPTO_H
#define CRYPTO_H

#include <tier0/dbg.h>		// for Assert & AssertMsg
#include "tier1/passwordhash.h"
#include "tier1/utlmemory.h"
#include <steam/steamtypes.h> // for Salt_t

extern void FreeListRNG();

const unsigned int k_cubSHA256Hash = 32;
typedef	unsigned char SHA256Digest_t[ k_cubSHA256Hash ];

const int k_nSymmetricBlockSize = 16;					// AES block size (128 bits)
const int k_nSymmetricKeyLen = 32;						// length in bytes of keys used for symmetric encryption

const int k_nRSAKeyLenMax = 1024;						// max length in bytes of keys used for RSA encryption (includes DER encoding)
const int k_nRSAKeyLenMaxEncoded = k_nRSAKeyLenMax*2;	// max length in bytes of hex-encoded key (hex encoding exactly doubles size)
const int k_nRSAKeyBits = 1024;							// length in bits of keys used for RSA encryption
const int k_cubRSAEncryptedBlockSize	= 128;
const int k_cubRSAPlaintextBlockSize	= 86 + 1;		// assume plaintext is text, so add a byte for the trailing \0
const uint32 k_cubRSASignature = k_cubRSAEncryptedBlockSize;

// Simple buffer class to encapsulate output from crypto functions with unknown output size
class CCryptoOutBuffer
{
public:

	CCryptoOutBuffer()
	{
		m_pubData = NULL;
		m_cubData = 0;
	}

	~CCryptoOutBuffer()
	{
		if ( m_pubData )
			delete[] m_pubData;
		m_pubData = NULL;
		m_cubData = 0;
	}

	void Set( uint8 *pubData, uint32 cubData )
	{
		if ( !pubData || !cubData )
			return;

		if ( m_pubData )
			delete[] m_pubData;

		m_pubData = new uint8[ cubData ];
		memcpy( m_pubData, pubData, cubData );
		m_cubData = cubData;
	}

	void Allocate( uint32 cubData )
	{
		if ( m_pubData )
			delete[] m_pubData;

		m_pubData = new uint8[ cubData ];
		m_cubData = cubData;
	}

	void Trim( uint32 cubTrim )
	{
		Assert( cubTrim <= m_cubData );
		m_cubData = cubTrim;
	}

	uint8 *PubData() { return m_pubData; }
	uint32 CubData() { return m_cubData; }

private:
	uint8 *m_pubData;
	uint32 m_cubData;
};

#if !defined(_PS3)
class CCrypto
{
public:
	static uint32 GetSymmetricEncryptedSize( uint32 cubPlaintextData );

	// this method writes the encrypted IV, then the ciphertext
	static bool SymmetricEncryptWithIV( const uint8 * pubPlaintextData, uint32 cubPlaintextData, 
										const uint8 * pIV, uint32 cubIV,
										uint8 * pubEncryptedData, uint32 * pcubEncryptedData,
										const uint8 * pubKey, uint32 cubKey );
	static bool SymmetricEncrypt( const uint8 * pubPlaintextData, uint32 cubPlaintextData, 
								  uint8 * pubEncryptedData, uint32 * pcubEncryptedData,
								  const uint8 * pubKey, uint32 cubKey );
	// this method assumes there is no IV before the payload - dissimilar to SymmetricEncryptWithIV
	static bool SymmetricDecryptWithIV( const uint8 * pubEncryptedData, uint32 cubEncryptedData, 
										const uint8 * pIV, uint32 cubIV,
										uint8 * pubPlaintextData, uint32 * pcubPlaintextData,
										const uint8 * pubKey, uint32 cubKey );
	static bool SymmetricDecrypt( const uint8 * pubEncryptedData, uint32 cubEncryptedData, 
								  uint8 * pubPlaintextData, uint32 * pcubPlaintextData,
								  const uint8 * pubKey, uint32 cubKey );

	// symmetrically encrypt data with a text password. A SHA256 hash of the password
	// is used as an AES encryption key (calls SymmetricEncrypt, above).
	// An HMAC of the ciphertext is appended, for authentication.
	static bool EncryptWithPasswordAndHMAC( const uint8 *pubPlaintextData, uint32 cubPlaintextData,
									 uint8 * pubEncryptedData, uint32 * pcubEncryptedData,
									 const char *pchPassword );

	// Same as above but uses an explicit IV. The format of the ciphertext is the same.
	// Be sure you know what you're doing if you use this - a random IV is much more secure in general!
	static bool EncryptWithPasswordAndHMACWithIV( const uint8 *pubPlaintextData, uint32 cubPlaintextData,
											const uint8 * pIV, uint32 cubIV,
											uint8 * pubEncryptedData, uint32 * pcubEncryptedData,
											const char *pchPassword );

	// Symmetrically decrypt data with the given password (see above).
	// If the HMAC does not match what we expect, then we know that either the password is
	// incorrect or the message is corrupted.
	static bool DecryptWithPasswordAndAuthenticate( const uint8 * pubEncryptedData, uint32 cubEncryptedData, 
									 uint8 * pubPlaintextData, uint32 * pcubPlaintextData,
									 const char *pchPassword );

	static bool RSAGenerateKeys( uint8 *pubPublicKey, uint32 *pcubPublicKey, uint8 *pubPrivateKey, uint32 *pcubPrivateKey );

	static bool RSAEncrypt( const uint8 *pubPlaintextPlaintextData, const uint32 cubData, uint8 *pubEncryptedData, 
							uint32 *pcubEncryptedData, const uint8 *pubPublicKey, const uint32 cubPublicKey );
	static bool RSADecrypt( const uint8 *pubEncryptedData, uint32 cubEncryptedData, 
							uint8 *pubPlaintextData, uint32 *pcubPlaintextData, const uint8 *pubPrivateKey, const uint32 cubPrivateKey );

	// decrypt using a public key, and no padding
	static bool RSAPublicDecrypt_NoPadding( const uint8 *pubEncryptedData, uint32 cubEncryptedData, 
							uint8 *pubPlaintextData, uint32 *pcubPlaintextData, const uint8 *pubPublicKey, const uint32 cubPublicKey );

	static bool RSASign( const uint8 *pubData, const uint32 cubData, 
						 uint8 *pubSignature, uint32 *pcubSignature, 
						const uint8 * pubPrivateKey, const uint32 cubPrivateKey );
	static bool RSAVerifySignature( const uint8 *pubData, const uint32 cubData, 
									const uint8 *pubSignature, const uint32 cubSignature, 
									const uint8 *pubPublicKey, const uint32 cubPublicKey );

	static bool RSASignSHA256( const uint8 *pubData, const uint32 cubData, 
						 uint8 *pubSignature, uint32 *pcubSignature, 
						const uint8 * pubPrivateKey, const uint32 cubPrivateKey );
	static bool RSAVerifySignatureSHA256( const uint8 *pubData, const uint32 cubData, 
									const uint8 *pubSignature, const uint32 cubSignature, 
									const uint8 *pubPublicKey, const uint32 cubPublicKey );

	static bool HexEncode( const uint8 *pubData, const uint32 cubData, char *pchEncodedData, uint32 cchEncodedData );
	static bool HexDecode( const char *pchData, uint8 *pubDecodedData, uint32 *pcubDecodedData );

	static uint32 Base64EncodeMaxOutput( uint32 cubData, const char *pszLineBreakOrNull );
	static bool Base64Encode( const uint8 *pubData, uint32 cubData, char *pchEncodedData, uint32 cchEncodedData, bool bInsertLineBreaks = true ); // legacy, deprecated
	static bool Base64Encode( const uint8 *pubData, uint32 cubData, char *pchEncodedData, uint32 *pcchEncodedData, const char *pszLineBreak = "\n" );

	static uint32 Base64DecodeMaxOutput( uint32 cubData ) { return ( (cubData + 3 ) / 4) * 3 + 1; }
	static bool Base64Decode( const char *pchEncodedData, uint8 *pubDecodedData, uint32 *pcubDecodedData, bool bIgnoreInvalidCharacters = true ); // legacy, deprecated
	static bool Base64Decode( const char *pchEncodedData, uint32 cchEncodedData, uint8 *pubDecodedData, uint32 *pcubDecodedData, bool bIgnoreInvalidCharacters = true );

	static bool GenerateSalt( Salt_t *pSalt );
	static bool GenerateSHA1Digest( const uint8 *pubInput, const int cubInput, SHADigest_t *pOutDigest );
	static bool GenerateSaltedSHA1Digest( const char *pchInput, const Salt_t *pSalt, SHADigest_t *pOutDigest );
	static bool GenerateRandomBlock( uint8 *pubDest, int cubDest );

	static bool GenerateHMAC( const uint8 *pubData, uint32 cubData, const uint8 *pubKey, uint32 cubKey, SHADigest_t *pOutputDigest );
	static bool GenerateHMAC256( const uint8 *pubData, uint32 cubData, const uint8 *pubKey, uint32 cubKey, SHA256Digest_t *pOutputDigest );

	static bool BGeneratePasswordHash( const char *pchInput, EPasswordHashAlg hashType, const Salt_t &Salt, PasswordHash_t &OutPasswordHash );
	static bool BValidatePasswordHash( const char *pchInput, EPasswordHashAlg hashType, const PasswordHash_t &DigestStored, const Salt_t &Salt, PasswordHash_t *pDigestComputed );
	static bool BGeneratePBKDF2Hash( const char *pchInput, const Salt_t &Salt, unsigned int rounds, PasswordHash_t &OutPasswordHash );
	static bool BGenerateWrappedSHA1PasswordHash( const char *pchInput, const Salt_t &Salt, unsigned int rounds, PasswordHash_t &OutPasswordHash );
	static bool BUpgradeOrWrapPasswordHash( PasswordHash_t &InPasswordHash, EPasswordHashAlg hashTypeIn, const Salt_t &Salt, PasswordHash_t &OutPasswordHash, EPasswordHashAlg &hashTypeOut );

	static bool BGzipBuffer( const uint8 *pubData, uint32 cubData, CCryptoOutBuffer &bufOutput );
	static bool BGunzipBuffer( const uint8 *pubData, uint32 cubData, CCryptoOutBuffer &bufOutput );

#ifdef DBGFLAG_VALIDATE
	static void ValidateStatics( CValidator &validator, const char *pchName );
#endif
};

#else

// bugbug ps3 - stub until we implement from PS3 libs
class CCrypto
{
public:
	// ps3 only
	static bool Init();
	static void Shutdown();

	//shared
	static uint32 GetSymmetricEncryptedSize( uint32 cubPlaintextData );

	static bool SymmetricEncrypt( const uint8 * pubPlaintextData, uint32 cubPlaintextData, uint8 * pubEncryptedData, uint32 * pcubEncryptedData, const uint8 * pubKey, uint32 cubKey );
	static bool SymmetricDecrypt( const uint8 * pubEncryptedData, uint32 cubEncryptedData, uint8 * pubPlaintextData, uint32 * pcubPlaintextData, const uint8 * pubKey, uint32 cubKey );
	static bool RSAGenerateKeys( uint8 *pubPublicKey, uint32 *pcubPublicKey, uint8 *pubPrivateKey, uint32 *pcubPrivateKey ) { AssertMsg( false, "RSAGenerateKeys not implemented on PS3" ); return false; }
	static bool RSAEncrypt( const uint8 *pubPlaintextPlaintextData, const uint32 cubData, uint8 *pubEncryptedData, uint32 *pcubEncryptedData, const uint8 *pubPublicKey, const uint32 cubPublicKey );
	static bool RSADecrypt( const uint8 *pubEncryptedData, uint32 cubEncryptedData, uint8 *pubPlaintextData, uint32 *pcubPlaintextData, const uint8 *pubPrivateKey, const uint32 cubPrivateKey );
	static bool RSAPublicDecrypt_NoPadding( const uint8 *pubEncryptedData, uint32 cubEncryptedData, uint8 *pubPlaintextData, uint32 *pcubPlaintextData, const uint8 *pubPublicKey, const uint32 cubPublicKey ) { AssertMsg( false, "RSAPublicDecrypt_NoPadding not implemented on PS3" ); return false; }
	static bool RSASign( const uint8 *pubData, const uint32 cubData, uint8 *pubSignature, uint32 *pcubSignature, const uint8 * pubPrivateKey, const uint32 cubPrivateKey );
	static bool RSAVerifySignature( const uint8 *pubData, const uint32 cubData, const uint8 *pubSignature, const uint32 cubSignature, const uint8 *pubPublicKey, const uint32 cubPublicKey );
	static bool HexEncode( const uint8 *pubData, const uint32 cubData, char *pchEncodedData, uint32 cchEncodedData );
	static bool HexDecode( const char *pchData, uint8 *pubDecodedData, uint32 *pcubDecodedData );
	static bool Base64Encode( const uint8 *pubData, const uint32 cubData, char *pchEncodedData, uint32 cchEncodedData, bool bInsertLineBreaks = true ) { AssertMsg( false, "Base64Encode not implemented on PS3" ); return false; } // cellHttpUtilBase64Encoder()
	static bool Base64Decode( const char *pchData, uint8 *pubDecodedData, uint32 *pcubDecodedData, bool bIgnoreInvalidCharacters = true ) { AssertMsg( false, "Base64Decode not implemented on PS3" ); return false; } // cellHttpUtilBase64Decoder()
	static bool GenerateSalt( Salt_t *pSalt );
	static bool GenerateSHA1Digest( const uint8 *pubInput, const int cubInput, SHADigest_t *pOutDigest );
	static bool GenerateSaltedSHA1Digest( const char *pchInput, const Salt_t *pSalt, SHADigest_t *pOutDigest ) { AssertMsg( false, "GenerateSaltedSHA1Digest not implemented on PS3" ); return false; }
	static bool GenerateRandomBlock( uint8 *pubDest, int cubDest );
	static bool GenerateHMAC( const uint8 *pubData, uint32 cubData, const uint8 *pubKey, uint32 cubKey, SHADigest_t *pOutputDigest ) { AssertMsg( false, "GenerateHMAC not implemented on PS3" ); return false; }
	static bool GenerateHMAC256( const uint8 *pubData, uint32 cubData, const uint8 *pubKey, uint32 cubKey, SHA256Digest_t *pOutputDigest ) { AssertMsg( false, "GenerateHMAC256 not implemented on PS3" ); return false; }
	static bool BGzipBuffer( const uint8 *pubData, uint32 cubData, CCryptoOutBuffer &bufOutput );
	static bool BGunzipBuffer( const uint8 *pubData, uint32 cubData, CCryptoOutBuffer &bufOutput );

#ifdef DBGFLAG_VALIDATE
	static void ValidateStatics( CValidator &validator, const char *pchName );
#endif
};


#endif //!_PS3


class CSimpleBitString;

//-----------------------------------------------------------------------------
// Purpose: Implement hex encoding / decoding using a custom lookup table.
//			This is a class because the decoding is done via a generated 
//			reverse-lookup table, and to save time it's best to just create
//			that table once.
//-----------------------------------------------------------------------------
class CCustomHexEncoder
{
public:
	CCustomHexEncoder( const char *pchEncodingTable );
	~CCustomHexEncoder();

	bool Encode( const uint8 *pubData, const uint32 cubData, char *pchEncodedData, uint32 cchEncodedData );
	bool Decode( const char *pchData, uint8 *pubDecodedData, uint32 *pcubDecodedData );

private:
	bool m_bValidEncoding;
	uint8 m_rgubEncodingTable[16];
	int m_rgnDecodingTable[256];
};

//-----------------------------------------------------------------------------
// Purpose: Implement base32 encoding / decoding using a custom lookup table.
//			This is a class because the decoding is done via a generated 
//			reverse-lookup table, and to save time it's best to just create
//			that table once.
//-----------------------------------------------------------------------------
class CCustomBase32Encoder
{
public:
	CCustomBase32Encoder( const char *pchEncodingTable );
	~CCustomBase32Encoder();

	bool Encode( const uint8 *pubData, const uint32 cubData, char *pchEncodedData, uint32 cchEncodedData );
	bool Decode( const char *pchData, uint8 *pubDecodedData, uint32 *pcubDecodedData );

	bool Encode( CSimpleBitString *pBitStringData, char *pchEncodedData, uint32 cchEncodedData );
	bool Decode( const char *pchData, CSimpleBitString *pBitStringDecodedData );

private:
	bool m_bValidEncoding;
	uint8 m_rgubEncodingTable[32];
	int m_rgnDecodingTable[256];
};

#endif // CRYPTO_H
