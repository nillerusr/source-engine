//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

// Note: not using precompiled headers.  The crypto++ headers create gunk that anyone including them
//       has to link to, so they can't be included in the global project file.  They also contain
//       string functions which are deprecated by the global project file so they can't be included after, either.
//       So we can't use the global precompiled header, need to manually include the things we need.

#include "winlite.h"

#ifdef POSIX
#include <sys/types.h>
#include <sys/socket.h>
#endif

/* this stuff needs to be before the crypto headers, as it relies on memdbg, which doesn't 
   do the right thing in the face of xdebug getting included below */
// tier0
//#include "tier0/tier0.h"
#include "tier0/basetypes.h"
#include "tier0/vprof.h"
//#include "constants.h"
#include "vstdlib/vstdlib.h"
#include "strtools.h"
//#include "version.h"
//#include "globals.h"
//#include "ivalidate.h"
#include "tier1/utlvector.h"
#include "simplebitstring.h"
#include "tier1/checksum_sha1.h"
#include "tier0/memdbgon.h"
#include "tier0/tslist.h"
#include "tier0/memdbgoff.h"


#ifdef ENABLE_OPENSSLCONNECTION
#define USE_OPENSSL_AES_DECRYPT 1
#endif

#ifdef USE_OPENSSL_AES_DECRYPT
// openssl optimized AES routines
#include "openssl/aes.h"
#if defined(_M_IX86) || defined (_M_X64) || defined(__i386__) || defined(__x86_64__)
#include <emmintrin.h>
#endif 
#endif

// crypto ++
#include "tier0/valve_off.h"
#include "../external/crypto++-5.6.3/cryptopushdisablewarnings.h"
#if _MSC_VER < 1400 // doesn't work with vc8, things below need xdebug
#define _XDEBUG_		// keep crypto++-5.2 from including xdebug
// these are defined in xdebug and used in some subsequent headers, define them to be our version
#define _NEW_CRT			new
#define _DELETE_CRT(_P)		delete (_P)
#define _DELETE_CRT_VEC(_P)	delete[] (_P)
#define _STRING_CRT			string
#endif
#define CRYPTOPP_DLL
#undef min
#undef max
#undef Verify
#define VPROF_BUDGETGROUP_ENCRYPTION				_T("Encryption")
#define SPEW_CRYPTO "crypto"
const int k_cMedBuff = 1024;					// medium buffer 

#if defined(GNUC)
#pragma GCC diagnostic ignored "-Wshadow"
#endif

#include "../external/crypto++-5.6.3/cryptlib.h"
#include "../external/crypto++-5.6.3/osrng.h"
#include "../external/crypto++-5.6.3/crc.h"
#include "../external/crypto++-5.6.3/modes.h"
#include "../external/crypto++-5.6.3/files.h"
#include "../external/crypto++-5.6.3/hex.h"
#include "../external/crypto++-5.6.3/base64.h"
#include "../external/crypto++-5.6.3/base32.h"
#include "../external/crypto++-5.6.3/words.h"
#include "../external/crypto++-5.6.3/rsa.h"
#include "../external/crypto++-5.6.3/aes.h"
#include "../external/crypto++-5.6.3/hmac.h"
#include "../external/crypto++-5.6.3/zlib.h"
#include "../external/crypto++-5.6.3/gzip.h"
#include "../external/crypto++-5.6.3/pwdbased.h"
using namespace CryptoPP;
typedef AutoSeededX917RNG<AES> CAutoSeededRNG;
#include "../external/crypto++-5.6.3/cryptopopdisablewarnings.h"

#if defined(GNUC)
#pragma GCC diagnostic warning "-Wshadow"
#endif

#include "tier0/memdbgon.h"
#include "tier0/valve_on.h"

#include "crypto.h"
#define max(a,b)  (((a) > (b)) ? (a) : (b))
#define min(a,b)  (((a) < (b)) ? (a) : (b))


// list of auto-seeded RNG pointers
// these are very expensive to construct, so it makes sense to cache them
CTSList<CAutoSeededRNG> g_tslistPAutoSeededRNG;

// to avoid deconstructor order issuses we allow to manually free the list
void FreeListRNG()
{
	g_tslistPAutoSeededRNG.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: thread-safe access to a pool of cryptoPP random number generators
//-----------------------------------------------------------------------------
class CPoolAllocatedRNG
{
public:
	CPoolAllocatedRNG()
	{
		m_pRNGNode = g_tslistPAutoSeededRNG.Pop();
		if ( !m_pRNGNode )
		{
			m_pRNGNode = new CTSList<CAutoSeededRNG>::Node_t;
		}
	}

	~CPoolAllocatedRNG()
	{
		g_tslistPAutoSeededRNG.Push( m_pRNGNode );
	}

	CAutoSeededRNG &GetRNG()
	{
		return m_pRNGNode->elem;
	}

private:
	CTSList<CAutoSeededRNG>::Node_t *m_pRNGNode;
};

// force run this static construction code 
class CGlobalInitConstructor
{
public:
	CGlobalInitConstructor()
	{
		// we have to use this function once since the underlying static constructor
		// is not thread safe. See use of MicrosoftCryptoProvider in Crypto++
		CAutoSeededRNG rng;
		rng.GenerateByte();
	}
};

volatile static CGlobalInitConstructor s_StaticCryptoConstructor;

//-----------------------------------------------------------------------------
// Purpose: Encrypts the specified data with the specified key.  Uses AES (Rijndael) symmetric
//			encryption.  The encrypted data may then be decrypted by calling SymmetricDecrypt
//			with the same key.
// Input:	pubPlaintextData -	Data to be encrypted
//			cubPlaintextData -	Size of data to be encrypted
//			pIV				 -	Pointer to initialization vector
//			cubIV			 -	Size of initialization vector
//			pubEncryptedData -  Pointer to buffer to receive encrypted data
//			pcubEncryptedData - Pointer to a variable that at time of call contains the size of
//								the receive buffer for encrypted data.  When the method returns, this will contain
//								the actual size of the encrypted data.
//			pubKey -			the key to encrypt the data with
//			cubKey -			Size of the key (must be k_nSymmetricKeyLen)
// Output:  true if successful, false if encryption failed
//-----------------------------------------------------------------------------
bool CCrypto::SymmetricEncryptWithIV( const uint8 *pubPlaintextData, const uint32 cubPlaintextData, 
									  const uint8 *pIV, const uint32 cubIV,
									  uint8 *pubEncryptedData, uint32 *pcubEncryptedData,
									  const uint8 *pubKey, const uint32 cubKey )
{
	VPROF_BUDGET( "CCrypto::SymmetricEncrypt", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pubPlaintextData );
	Assert( cubPlaintextData );
	Assert( pubEncryptedData );
	Assert( pcubEncryptedData );
	Assert( *pcubEncryptedData );
	Assert( pubKey );
	Assert( k_nSymmetricKeyLen == cubKey );	// the only key length supported is k_nSymmetricKeyLen

	bool bRet = false;

	uint32 cubEncryptedData = *pcubEncryptedData;	// remember how big the caller's buffer is
	bool bUseTempBuffer = false;
	uint8 *pTemp = pubEncryptedData;


	//
	// Crypto++ does not play well with overlapping buffers. If the buffers are
	// overlapping, then allocate some temp space to use for the encryption.
	//
	// It does work fine with _identical_ buffers.
	//
	if ( ( pubEncryptedData + cubEncryptedData >= pubPlaintextData ) &&
		( pubPlaintextData + cubPlaintextData >= pubEncryptedData ) )
	{
		pTemp = new uint8[cubEncryptedData];
		bUseTempBuffer = true;
	}

	try		// handle any exceptions crypto++ may throw
	{	
		if ( pTemp != NULL )
		{
			AESEncryption aesEncrypt( pubKey, cubKey );

			byte rgubIVEncrypted[k_cMedBuff];
			Assert( Q_ARRAYSIZE( rgubIVEncrypted ) >= aesEncrypt.BlockSize() );
			Assert( pIV != NULL && cubIV >= aesEncrypt.BlockSize() );

			ArraySink * pOutputSink = new ArraySink( pTemp, *pcubEncryptedData );

			// encrypt the initial vector with the key
			aesEncrypt.ProcessBlock( pIV, rgubIVEncrypted );

			// store the encrypted IV in the output - the recipient will need it
			pOutputSink->Put( rgubIVEncrypted, aesEncrypt.BlockSize() );

			// encrypt the message, given the key & IV
			CBC_Mode_ExternalCipher::Encryption cipher( aesEncrypt, pIV );
			// Note: StreamTransformationFilter now owns the pointer to pOutputSink and will
			// free it when the filter goes out of scope and destructs
			StreamTransformationFilter filter( cipher, pOutputSink );
			filter.Put( (byte *) pubPlaintextData, cubPlaintextData );
			filter.MessageEnd();
			// return length of encrypted data to caller
			*pcubEncryptedData = pOutputSink->TotalPutLength();
			// CryptoPP may leave garbage hanging around in the caller's buffer past the stated output length.
			// Just to be safe, zero out caller's buffer from end out output to end of max buffer
			if ( bUseTempBuffer )
			{
				Q_memcpy( pubEncryptedData, pTemp, *pcubEncryptedData );
			}
			Q_memset( pubEncryptedData + *pcubEncryptedData, 0, (cubEncryptedData - *pcubEncryptedData ) );
			bRet = true;
		}
	}
	catch ( Exception e ) 
	{
		DMsg( SPEW_CRYPTO, 2, "CCrypto::SymmetricEncrypt: crypto++ threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
	}

	if ( bUseTempBuffer )
	{
		delete[] pTemp;
	}

	return bRet;
}

//-----------------------------------------------------------------------------
// Purpose: Encrypts the specified data with the specified key.  Uses AES (Rijndael) symmetric
//			encryption.  The encrypted data may then be decrypted by calling SymmetricDecrypt
//			with the same key.  Generates a random initialization vector of the
//			appropriate size.
// Input:	pubPlaintextData -	Data to be encrypted
//			cubPlaintextData -	Size of data to be encrypted
//			pubEncryptedData -  Pointer to buffer to receive encrypted data
//			pcubEncryptedData - Pointer to a variable that at time of call contains the size of
//								the receive buffer for encrypted data.  When the method returns, this will contain
//								the actual size of the encrypted data.
//			pubKey -			the key to encrypt the data with
//			cubKey -			Size of the key (must be k_nSymmetricKeyLen)
// Output:  true if successful, false if encryption failed
//-----------------------------------------------------------------------------

bool CCrypto::SymmetricEncrypt( const uint8 *pubPlaintextData, const uint32 cubPlaintextData, 
								uint8 *pubEncryptedData, uint32 *pcubEncryptedData,
								const uint8 *pubKey, const uint32 cubKey )
{
	bool bRet = false;

	//
	// Generate a random IV
	//
	AESEncryption aesEncrypt( pubKey, cubKey );
	byte rgubIV[k_cMedBuff];
	CPoolAllocatedRNG rng;
	rng.GetRNG().GenerateBlock( rgubIV, aesEncrypt.BlockSize() );

	bRet = SymmetricEncryptWithIV( pubPlaintextData, cubPlaintextData, rgubIV, aesEncrypt.BlockSize(), pubEncryptedData, pcubEncryptedData, pubKey, cubKey );

	return bRet;
}


#ifdef USE_OPENSSL_AES_DECRYPT

// Local helper to perform AES+CBC decryption using optimized OpenSSL AES routines
static bool BDecryptAESUsingOpenSSL( const uint8 *pubEncryptedData, uint32 cubEncryptedData, uint8 *pubPlaintextData, uint32 *pcubPlaintextData, AES_KEY *key, const uint8 *pIV )
{
	COMPILE_TIME_ASSERT( k_nSymmetricBlockSize == 16 );

	// Block cipher encrypted text must be a multiple of the block size
	if ( cubEncryptedData % k_nSymmetricBlockSize != 0 )
		return false;

	// Enough input? Requirement is one padded final block
	if ( cubEncryptedData < k_nSymmetricBlockSize )
		return false;

	// Enough output space for all the full non-final blocks?
	if ( *pcubPlaintextData < cubEncryptedData - k_nSymmetricBlockSize )
		return false;

	uint8 rgubWorking[k_nSymmetricBlockSize];
	uint32 nDecrypted = 0;

	// Process non-final blocks
#if defined(_M_IX86) || defined (_M_X64) || defined(__i386__) || defined(__x86_64__)
	// ... believe it or not, Steam client on Windows supports Athlon XP without SSE2
	if ( !IsWindows() || GetCPUInformation().m_bSSE2 )
	{
		while ( nDecrypted < cubEncryptedData - k_nSymmetricBlockSize )
		{
			AES_decrypt( pubEncryptedData + nDecrypted, rgubWorking, key );
			__m128i m128Temp = _mm_xor_si128( _mm_loadu_si128( (__m128i*)pIV ), _mm_loadu_si128( (__m128i*)rgubWorking ) );
			pIV = pubEncryptedData + nDecrypted;
			nDecrypted += k_nSymmetricBlockSize;
			_mm_storeu_si128( (__m128i* RESTRICT)( pubPlaintextData + nDecrypted - k_nSymmetricBlockSize ), m128Temp );
		}
	}
	else
#endif
	{
		while ( nDecrypted < cubEncryptedData - k_nSymmetricBlockSize )
		{
			AES_decrypt( pubEncryptedData + nDecrypted, rgubWorking, key );
			for ( int i = 0; i < k_nSymmetricBlockSize; ++i )
				pubPlaintextData[nDecrypted + i] = rgubWorking[i] ^ pIV[i];
			pIV = pubEncryptedData + nDecrypted;
			nDecrypted += k_nSymmetricBlockSize;
		}
	}

	// Process final block into rgubWorking for padding inspection
	Assert( nDecrypted == cubEncryptedData - k_nSymmetricBlockSize );
	AES_decrypt( pubEncryptedData + nDecrypted, rgubWorking, key );
	for ( int i = 0; i < k_nSymmetricBlockSize; ++i )
		rgubWorking[i] ^= pIV[i];
	
	// Get final block padding length and make sure it is backfilled properly (PKCS#5)
	uint8 pad = rgubWorking[ k_nSymmetricBlockSize - 1 ];
	if ( pad < 1 || pad > k_nSymmetricBlockSize )
		return false;
	for ( int i = k_nSymmetricBlockSize - pad; i < k_nSymmetricBlockSize; ++i )
		if ( rgubWorking[i] != pad )
			return false;

    // Check that we have enough space for final bytes
    if ( *pcubPlaintextData < nDecrypted + k_nSymmetricBlockSize - pad )
        return false;
    
	// Write any non-pad bytes from rgubWorking to pubPlaintextData
	for ( int i = 0; i < k_nSymmetricBlockSize - pad; ++i )
		pubPlaintextData[nDecrypted++] = rgubWorking[i];

	// The old CryptoPP path zeros out the entire destination buffer, but that
	// behavior isn't documented or even expected. We'll just zero out one byte
	// in case anyone relies on string termination, but that zero isn't counted.
	if ( *pcubPlaintextData > nDecrypted )
		pubPlaintextData[nDecrypted] = 0;

	*pcubPlaintextData = nDecrypted;
	return true;
}

#else

/* function, not method */
static bool SymmetricDecryptWorker( const uint8 *pubEncryptedData, uint32 cubEncryptedData, 
									const uint8 * pIV, uint32 cubIV,
									uint8 *pubPlaintextData, uint32 *pcubPlaintextData, 
									AESDecryption &aesDecrypt )
{
	VPROF_BUDGET( "CCrypto::SymmetricDecrypt", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pubEncryptedData );
	Assert( cubEncryptedData);
	Assert( pIV );
	Assert( cubIV );
	Assert( pubPlaintextData );
	Assert( pcubPlaintextData );
	Assert( *pcubPlaintextData );

	bool bRet = false;

	uint32 cubPlaintextData = *pcubPlaintextData;	// remember how big the caller's buffer is
	bool bUseTempBuffer = false;
	uint8* pTemp = pubPlaintextData;

	//
	// Crypto++ does not play nice with decrypting in place. If the buffers are
	// overlapping, then allocate some temp space to use for the decryption.
	//
	// It does work fine with _identical_ buffers, but due to the way we store
	// the IV in the returned encrypted data we never actually hit that case.
	//
	if ( ( pubEncryptedData + cubEncryptedData >= pubPlaintextData ) &&
		 ( pubPlaintextData + cubPlaintextData >= pubEncryptedData ) )
	{
		pTemp = new uint8[cubPlaintextData];
		bUseTempBuffer = true;
	}

	try		// handle any exceptions crypto++ may throw
	{
		if ( pTemp != NULL )
		{
			CryptoPP::ArraySink* pOutputSink = new CryptoPP::ArraySink( pTemp, *pcubPlaintextData );
			CryptoPP::CBC_Mode_ExternalCipher::Decryption cbc( aesDecrypt, pIV );
			// Note: StreamTransformationFilter now owns the pointer to pOutputSink and will
			// free it when the filter goes out of scope and destructs
			CryptoPP::StreamTransformationFilter padding( cbc, pOutputSink );
			padding.Put( pubEncryptedData, cubEncryptedData );
			padding.MessageEnd();
			// return length of decrypted data to caller
			*pcubPlaintextData = pOutputSink->TotalPutLength();
			// CryptoPP may leave garbage hanging around in the caller's buffer past the stated output length.
			// Just to be safe, zero out caller's buffer from end out output to end of max buffer
			if ( bUseTempBuffer )
			{
				Q_memcpy( pubPlaintextData, pTemp, *pcubPlaintextData );
			}
			Q_memset( pubPlaintextData + *pcubPlaintextData, 0, (cubPlaintextData - *pcubPlaintextData ) );

			bRet = true;
		}
	}
	catch ( Exception e ) 
	{
		DMsg( SPEW_CRYPTO, 4, "CCrypto::SymmetricDecrypt: crypto++ threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
	}

	if ( bUseTempBuffer )
	{
		delete[] pTemp;
	}

	return bRet;
}

#endif


//-----------------------------------------------------------------------------
// Purpose: Decrypts the specified data with the specified key.  Uses AES (Rijndael) symmetric
//			decryption.  
// Input:	pubEncryptedData -	Data to be decrypted
//			cubEncryptedData -	Size of data to be decrypted
//			pubPlaintextData -  Pointer to buffer to receive decrypted data
//			pcubPlaintextData - Pointer to a variable that at time of call contains the size of
//								the receive buffer for decrypted data.  When the method returns, this will contain
//								the actual size of the decrypted data.
//			pubKey -			the key to decrypt the data with
//			cubKey -			Size of the key (must be k_nSymmetricKeyLen)
// Output:  true if successful, false if decryption failed
//-----------------------------------------------------------------------------
bool CCrypto::SymmetricDecrypt( const uint8 *pubEncryptedData, uint32 cubEncryptedData, 
							   uint8 *pubPlaintextData, uint32 *pcubPlaintextData, 
							   const uint8 *pubKey, const uint32 cubKey )
{
	Assert( pubEncryptedData );
	Assert( cubEncryptedData);
	Assert( pubPlaintextData );
	Assert( pcubPlaintextData );
	Assert( *pcubPlaintextData );
	Assert( pubKey );
	Assert( k_nSymmetricKeyLen == cubKey );	// the only key length supported is k_nSymmetricKeyLen

	// the initialization vector (IV) must be stored in the first block of bytes.
	// If the size of encrypted data is not at least the block size, it is not valid
	if ( cubEncryptedData < k_nSymmetricBlockSize )
		return false;

#ifdef USE_OPENSSL_AES_DECRYPT

	AES_KEY key;
	if ( AES_set_decrypt_key( pubKey, cubKey * 8, &key ) < 0 )
		return false;

	// Our first block is straight AES block encryption of IV with user key, no XOR.
	uint8 rgubIV[ k_nSymmetricBlockSize ];
	AES_decrypt( pubEncryptedData, rgubIV, &key );
	pubEncryptedData += k_nSymmetricBlockSize;
	cubEncryptedData -= k_nSymmetricBlockSize;

	return BDecryptAESUsingOpenSSL( pubEncryptedData, cubEncryptedData, pubPlaintextData, pcubPlaintextData, &key, rgubIV );

#else

	AESDecryption aesDecrypt( pubKey, cubKey );
	Assert( k_nSymmetricBlockSize == aesDecrypt.BlockSize() );

	// Decrypt the IV
	byte rgubIV[k_cMedBuff];
	Assert( Q_ARRAYSIZE( rgubIV ) >= aesDecrypt.BlockSize() );
	aesDecrypt.ProcessBlock( pubEncryptedData, rgubIV );

	// We have now consumed the IV, so remove it from the front of the message
	pubEncryptedData += k_nSymmetricBlockSize;
	cubEncryptedData -= k_nSymmetricBlockSize;

	// given the IV stored in the message, and the key, decrypt the message
	return SymmetricDecryptWorker( pubEncryptedData, cubEncryptedData,
		rgubIV, aesDecrypt.BlockSize(),
		pubPlaintextData, pcubPlaintextData,
		aesDecrypt );

#endif
}

//-----------------------------------------------------------------------------
// Purpose: Decrypts the specified data with the specified key.  Uses AES (Rijndael) symmetric
//			decryption.  
// Input:	pubEncryptedData -	Data to be decrypted
//			cubEncryptedData -	Size of data to be decrypted
//			pIV -				Initialization vector. Byte array one block in size.
//			cubIV -				size of IV. This should be 16 (one block, 128 bits)
//			pubPlaintextData -  Pointer to buffer to receive decrypted data
//			pcubPlaintextData - Pointer to a variable that at time of call contains the size of
//								the receive buffer for decrypted data.  When the method returns, this will contain
//								the actual size of the decrypted data.
//			pubKey -			the key to decrypt the data with
//			cubKey -			Size of the key (must be k_nSymmetricKeyLen)
// Output:  true if successful, false if decryption failed
//-----------------------------------------------------------------------------
bool CCrypto::SymmetricDecryptWithIV( const uint8 *pubEncryptedData, uint32 cubEncryptedData, 
								const uint8 * pIV, uint32 cubIV,
								uint8 *pubPlaintextData, uint32 *pcubPlaintextData, 
								const uint8 *pubKey, const uint32 cubKey )
{
	Assert( pubEncryptedData );
	Assert( cubEncryptedData);
	Assert( pIV );
	Assert( cubIV );
	Assert( pubPlaintextData );
	Assert( pcubPlaintextData );
	Assert( *pcubPlaintextData );
	Assert( pubKey );
	Assert( k_nSymmetricKeyLen == cubKey );	// the only key length supported is k_nSymmetricKeyLen

	// IV input into CBC must be exactly one block size
	if ( cubIV != k_nSymmetricBlockSize )
		return false;

#ifdef USE_OPENSSL_AES_DECRYPT

	AES_KEY key;
	if ( AES_set_decrypt_key( pubKey, cubKey * 8, &key ) < 0 )
		return false;

	return BDecryptAESUsingOpenSSL( pubEncryptedData, cubEncryptedData, pubPlaintextData, pcubPlaintextData, &key, pIV );

#else

	AESDecryption aesDecrypt( pubKey, cubKey );
	Assert( k_nSymmetricBlockSize == aesDecrypt.BlockSize() );

	return SymmetricDecryptWorker( pubEncryptedData, cubEncryptedData,
		pIV, cubIV,
		pubPlaintextData, pcubPlaintextData,
		aesDecrypt );

#endif
}


//-----------------------------------------------------------------------------
// Purpose: For specified plaintext data size, returns what size of symmetric
//			encrypted data will be
//-----------------------------------------------------------------------------
uint32 CCrypto::GetSymmetricEncryptedSize( uint32 cubPlaintextData )
{
	// empirically determined encrypted size as function of plaintext size for AES encryption
	uint k_cubBlock = 16;
	uint k_cubHeader = 16;
	return k_cubHeader + ( ( cubPlaintextData / k_cubBlock ) * k_cubBlock ) + k_cubBlock;
}


//-----------------------------------------------------------------------------
// Purpose: Encrypts the specified data with the specified text password.  
//			Uses the SHA256 hash of the password as the key for AES (Rijndael) symmetric
//			encryption. A SHA1 HMAC of the result is appended, for authentication on 
//			the receiving end.
//			The encrypted data may then be decrypted by calling DecryptWithPasswordAndAuthenticate
//			with the same password.
// Input:	pubPlaintextData -	Data to be encrypted
//			cubPlaintextData -	Size of data to be encrypted
//			pubEncryptedData -  Pointer to buffer to receive encrypted data
//			pcubEncryptedData - Pointer to a variable that at time of call contains the size of
//								the receive buffer for encrypted data.  When the method returns, this will contain
//								the actual size of the encrypted data.
//			pchPassword -		text password
// Output:  true if successful, false if encryption failed
//-----------------------------------------------------------------------------
bool CCrypto::EncryptWithPasswordAndHMAC( const uint8 *pubPlaintextData, uint32 cubPlaintextData,
								 uint8 * pubEncryptedData, uint32 * pcubEncryptedData,
								 const char *pchPassword )
{
	//
	// Generate a random IV
	//
	byte rgubIV[k_nSymmetricBlockSize];
	CPoolAllocatedRNG rng;
	rng.GetRNG().GenerateBlock( rgubIV, k_nSymmetricBlockSize );

	return EncryptWithPasswordAndHMACWithIV( pubPlaintextData, cubPlaintextData, rgubIV, k_nSymmetricBlockSize, pubEncryptedData, pcubEncryptedData, pchPassword );
}


//-----------------------------------------------------------------------------
// Purpose: Encrypts the specified data with the specified text password.  
//			Uses the SHA256 hash of the password as the key for AES (Rijndael) symmetric
//			encryption. A SHA1 HMAC of the result is appended, for authentication on 
//			the receiving end.
//			The encrypted data may then be decrypted by calling DecryptWithPasswordAndAuthenticate
//			with the same password.
// Input:	pubPlaintextData -	Data to be encrypted
//			cubPlaintextData -	Size of data to be encrypted
//			pIV -				IV to use for AES encryption. Should be random and never used before unless you know
//								exactly what you're doing.
//			cubIV -				size of the IV - should be same ase the AES blocksize.
//			pubEncryptedData -  Pointer to buffer to receive encrypted data
//			pcubEncryptedData - Pointer to a variable that at time of call contains the size of
//								the receive buffer for encrypted data.  When the method returns, this will contain
//								the actual size of the encrypted data.
//			pchPassword -		text password
// Output:  true if successful, false if encryption failed
//-----------------------------------------------------------------------------
bool CCrypto::EncryptWithPasswordAndHMACWithIV( const uint8 *pubPlaintextData, uint32 cubPlaintextData,
								 const uint8 * pIV, uint32 cubIV,
								 uint8 * pubEncryptedData, uint32 * pcubEncryptedData,
								 const char *pchPassword )
{
	uint8 rgubKey[k_nSymmetricKeyLen];
	if ( !pchPassword || !pchPassword[0] )
		return false;

	if ( !cubPlaintextData )
		return false;

	uint32 cubBuffer = *pcubEncryptedData;
	uint32 cubExpectedResult = GetSymmetricEncryptedSize( cubPlaintextData ) + sizeof( SHADigest_t );

	if ( cubBuffer < cubExpectedResult )
		return false;

	try
	{
		CryptoPP::SHA256().CalculateDigest( rgubKey, (const uint8 *)pchPassword, Q_strlen( pchPassword ) );
	}
	catch ( Exception e ) 
	{
		DMsg( SPEW_CRYPTO, 4, "CCrypto::EncryptWithPassword: crypto++ threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
		return false;
	}

	bool bRet = SymmetricEncryptWithIV( pubPlaintextData, cubPlaintextData, pIV, cubIV, pubEncryptedData, pcubEncryptedData, rgubKey, k_nSymmetricKeyLen );
	if ( bRet )
	{
		// calc HMAC
		uint32 cubEncrypted = *pcubEncryptedData;
		*pcubEncryptedData += sizeof( SHADigest_t );
		if ( cubBuffer < *pcubEncryptedData )
			return false;

		SHADigest_t *pHMAC = (SHADigest_t*)( pubEncryptedData + cubEncrypted );
		bRet = CCrypto::GenerateHMAC( pubEncryptedData, cubEncrypted, rgubKey, k_nSymmetricKeyLen, pHMAC );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Decrypts the specified data with the specified password.  Uses AES (Rijndael) symmetric
//			decryption. First, the HMAC is verified - if it is not correct, then we know that
//			the key is incorrect or the data is corrupted, and the decryption fails.
// Input:	pubEncryptedData -	Data to be decrypted
//			cubEncryptedData -	Size of data to be decrypted
//			pubPlaintextData -  Pointer to buffer to receive decrypted data
//			pcubPlaintextData - Pointer to a variable that at time of call contains the size of
//								the receive buffer for decrypted data.  When the method returns, this will contain
//								the actual size of the decrypted data.
//			pchPassword -		the text password to decrypt the data with
// Output:  true if successful, false if decryption failed
//-----------------------------------------------------------------------------
bool CCrypto::DecryptWithPasswordAndAuthenticate( const uint8 * pubEncryptedData, uint32 cubEncryptedData, 
								 uint8 * pubPlaintextData, uint32 * pcubPlaintextData,
								 const char *pchPassword )
{
	uint8 rgubKey[k_nSymmetricKeyLen];
	if ( !pchPassword || !pchPassword[0] )
		return false;

	if ( cubEncryptedData <= sizeof( SHADigest_t ) )
		return false;

	try
	{
		CryptoPP::SHA256().CalculateDigest( rgubKey, (const uint8 *)pchPassword, Q_strlen( pchPassword ) );
	}
	catch ( Exception e ) 
	{
		DMsg( SPEW_CRYPTO, 4, "CCrypto::EncryptWithPassword: crypto++ threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
		return false;
	}

	uint32 cubCiphertext = cubEncryptedData - sizeof( SHADigest_t );
	SHADigest_t *pHMAC = (SHADigest_t*)( pubEncryptedData + cubCiphertext  );
	SHADigest_t hmacActual;
	bool bRet = CCrypto::GenerateHMAC( pubEncryptedData, cubCiphertext, rgubKey, k_nSymmetricKeyLen, &hmacActual );

	if ( bRet )
	{
		// invalid ciphertext or key
		if ( Q_memcmp( &hmacActual, pHMAC, sizeof( SHADigest_t ) ) )
			return false;
		
		bRet = SymmetricDecrypt( pubEncryptedData, cubCiphertext, pubPlaintextData, pcubPlaintextData, rgubKey, k_nSymmetricKeyLen );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Generates a new pair of private/public RSA keys
// Input:	pubPublicKey -		Pointer to buffer to receive public key (should be of size k_nRSAKeyLenMax)
//			pcubPublicKey -		Pointer to variable that contains size of pubPublicKey buffer.  At exit,
//								this is filled in with the actual size of the public key
// 			pubPrivateKey -		Pointer to buffer to receive private key (should be of size k_nRSAKeyLenMax)
//			pcubPrivateKey -	Pointer to variable that contains size of pubPrivateKey buffer.  At exit,
//								this is filled in with the actual size of the private key
// Output:  true if successful, false if key generation failed
//-----------------------------------------------------------------------------
bool CCrypto::RSAGenerateKeys( uint8 *pubPublicKey, uint32 *pcubPublicKey, uint8 *pubPrivateKey, uint32 *pcubPrivateKey )
{
	VPROF_BUDGET( "CCrypto::RSAGenerateKeys", VPROF_BUDGETGROUP_ENCRYPTION );
	bool bRet = false;
	Assert( pubPublicKey );
	Assert( pcubPublicKey );
	Assert( pubPrivateKey );
	Assert( pcubPrivateKey );

	try		// handle any exceptions crypto++ may throw
	{
		// generate private key
		ArraySink arraySinkPrivateKey( pubPrivateKey, *pcubPrivateKey );
		CPoolAllocatedRNG rng;
		RSAES_OAEP_SHA_Decryptor priv( rng.GetRNG(), k_nRSAKeyBits );
		priv.DEREncode( arraySinkPrivateKey );
		*pcubPrivateKey = arraySinkPrivateKey.TotalPutLength();
		// generate public key
		ArraySink arraySinkPublicKey( pubPublicKey, *pcubPublicKey );
		RSAES_OAEP_SHA_Encryptor pub(priv);
		pub.DEREncode( arraySinkPublicKey );
		*pcubPublicKey = arraySinkPublicKey.TotalPutLength();
		bRet = true;
	}
	catch ( Exception e ) 
	{
		DMsg( SPEW_CRYPTO, 2, "CCrypto::RSAGenerateKeys: crypto++ threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Encrypts the specified data with the specified RSA public key.  
//			The encrypted data may then be decrypted by calling RSADecrypt with the
//			corresponding RSA private key.
// Input:	pubPlaintextData -	Data to be encrypted
//			cubPlaintextData -	Size of data to be encrypted
//			pubEncryptedData -  Pointer to buffer to receive encrypted data
//			pcubEncryptedData - Pointer to a variable that at time of call contains the size of
//								the receive buffer for encrypted data.  When the method returns, this will contain
//								the actual size of the encrypted data.
//			pubPublicKey -		the RSA public key to encrypt the data with
//			cubPublicKey -		Size of the key (must be k_nSymmetricKeyLen)
// Output:  true if successful, false if encryption failed
//-----------------------------------------------------------------------------
bool CCrypto::RSAEncrypt( const uint8 *pubPlaintextData, uint32 cubPlaintextData, 
						  uint8 *pubEncryptedData, uint32 *pcubEncryptedData, 
						  const uint8 *pubPublicKey, const uint32 cubPublicKey )
{
	VPROF_BUDGET( "CCrypto::RSAEncrypt", VPROF_BUDGETGROUP_ENCRYPTION );
	bool bRet = false;
	Assert( cubPlaintextData > 0 );	// must pass in some data

	try		// handle any exceptions crypto++ may throw
	{
		StringSource stringSourcePublicKey( pubPublicKey, cubPublicKey, true );
		RSAES_OAEP_SHA_Encryptor rsaEncryptor( stringSourcePublicKey );

		// calculate how many blocks of encryption will we need to do
		AssertFatal( rsaEncryptor.FixedMaxPlaintextLength() <= ULONG_MAX );
		uint32 cBlocks = 1 + ( ( cubPlaintextData - 1 ) / (uint32)rsaEncryptor.FixedMaxPlaintextLength() );
		// calculate how big the output will be
		AssertFatal( rsaEncryptor.FixedCiphertextLength() <= ULONG_MAX / cBlocks );
		uint32 cubCipherText = cBlocks * (uint32)rsaEncryptor.FixedCiphertextLength();
		Assert( cubCipherText > 0 );

		// ensure there is sufficient room in output buffer for result
		if ( cubCipherText > ( *pcubEncryptedData ) )
		{
			AssertMsg2( false, "CCrypto::RSAEncrypt: insufficient output buffer for encryption, needed %d got %d\n",
						cubCipherText, *pcubEncryptedData );
			return false;
		}

		// encrypt the message, using as many blocks as required
		CPoolAllocatedRNG rng;
		for ( uint32 nBlock = 0; nBlock < cBlocks; nBlock++ )
		{
			// encrypt either all remaining plaintext, or maximum allowed plaintext per RSA encryption operation
			uint32 cubToEncrypt = min( cubPlaintextData, (uint32)rsaEncryptor.FixedMaxPlaintextLength() );
			// encrypt the plaintext
			rsaEncryptor.Encrypt( rng.GetRNG(), pubPlaintextData, cubToEncrypt, pubEncryptedData );
			// adjust input and output pointers and remaining plaintext byte count
			pubPlaintextData += cubToEncrypt;
			cubPlaintextData -= cubToEncrypt;
			pubEncryptedData += rsaEncryptor.FixedCiphertextLength();
		}
		Assert( 0 == cubPlaintextData );		// should have no remaining plaintext to encrypt
		*pcubEncryptedData = cubCipherText;

		bRet = true;
	}
	catch ( Exception e ) 
	{
		DMsg( SPEW_CRYPTO, 2, "CCrypto::RSAEncrypt: Encrypt() threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Decrypts the specified data with the specified RSA private key 
// Input:	pubEncryptedData -	Data to be decrypted
//			cubEncryptedData -	Size of data to be decrypted
//			pubPlaintextData -  Pointer to buffer to receive decrypted data
//			pcubPlaintextData - Pointer to a variable that at time of call contains the size of
//								the receive buffer for decrypted data.  When the method returns, this will contain
//								the actual size of the decrypted data.
//			pubPrivateKey -		the RSA private key key to decrypt the data with
//			cubPrivateKey -		Size of the key (must be k_nSymmetricKeyLen)
// Output:  true if successful, false if decryption failed
//-----------------------------------------------------------------------------
bool CCrypto::RSADecrypt( const uint8 *pubEncryptedData, uint32 cubEncryptedData, 
						  uint8 *pubPlaintextData, uint32 *pcubPlaintextData, 
						  const uint8 *pubPrivateKey, const uint32 cubPrivateKey )
{
	VPROF_BUDGET( "CCrypto::RSADecrypt", VPROF_BUDGETGROUP_ENCRYPTION );
	bool bRet = false;

	Assert( cubEncryptedData > 0 );	// must pass in some data

	try		// handle any exceptions crypto++ may throw
	{
		StringSource stringSourcePrivateKey( pubPrivateKey, cubPrivateKey, true );
		RSAES_OAEP_SHA_Decryptor rsaDecryptor( stringSourcePrivateKey );

		// calculate how many blocks of decryption will we need to do
		AssertFatal( rsaDecryptor.FixedCiphertextLength() <= ULONG_MAX );
		uint32 cubFixedCiphertextLength = (uint32)rsaDecryptor.FixedCiphertextLength();
		// Ensure encrypted data is valid and has length that is exact multiple of 128 bytes
		if ( 0 != ( cubEncryptedData % cubFixedCiphertextLength ) )
		{
			DMsg( SPEW_CRYPTO, 2, "CCrypto::RSADecrypt: invalid ciphertext length %d, needs to be a multiple of %d\n",
				cubEncryptedData, cubFixedCiphertextLength );
			return false;
		}
		uint32 cBlocks = cubEncryptedData / cubFixedCiphertextLength;

		// calculate how big the maximum output will be
		size_t cubMaxPlaintext = rsaDecryptor.MaxPlaintextLength( rsaDecryptor.FixedCiphertextLength() );
		AssertFatal( cubMaxPlaintext <= ULONG_MAX / cBlocks );
		uint32 cubPlaintextDataMax = cBlocks * (uint32)cubMaxPlaintext;
		Assert( cubPlaintextDataMax > 0 );
		// ensure there is sufficient room in output buffer for result
		if ( cubPlaintextDataMax >= ( *pcubPlaintextData ) )
		{
			AssertMsg2( false, "CCrypto::RSADecrypt: insufficient output buffer for decryption, needed %d got %d\n",
						cubPlaintextDataMax, *pcubPlaintextData );
			return false;
		}

		// decrypt the data, using as many blocks as required
		CPoolAllocatedRNG rng;
		uint32 cubPlaintextData = 0;
		for ( uint32 nBlock = 0; nBlock < cBlocks; nBlock++ )
		{
			// decrypt one block (always of fixed size)
			int cubToDecrypt = cubFixedCiphertextLength;
			DecodingResult decodingResult = rsaDecryptor.Decrypt( rng.GetRNG(), pubEncryptedData, cubToDecrypt, pubPlaintextData );
			if ( !decodingResult.isValidCoding )
			{
				DMsg( SPEW_CRYPTO, 2, "CCrypto::RSADecrypt: failed to decrypt\n" );
				return false;
			}
			// adjust input and output pointers and remaining encrypted byte count
			pubEncryptedData += cubToDecrypt;
			cubEncryptedData -= cubToDecrypt;
			pubPlaintextData += decodingResult.messageLength;
			AssertFatal( decodingResult.messageLength <= ULONG_MAX );
			cubPlaintextData += (uint32)decodingResult.messageLength;
		}
		Assert( 0 == cubEncryptedData );	// should have no remaining encrypted data to decrypt
		*pcubPlaintextData = cubPlaintextData;

		bRet = true;
	}
	catch ( Exception e ) 
	{
		DMsg( SPEW_CRYPTO, 2, "CCrypto::RSADecrypt: Decrypt() threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Decrypts the specified data with the specified RSA PUBLIC key,
//			using no padding (eg un-padded signature).
// Input:	pubEncryptedData -	Data to be decrypted
//			cubEncryptedData -	Size of data to be decrypted
//			pubPlaintextData -  Pointer to buffer to receive decrypted data
//			pcubPlaintextData - Pointer to a variable that at time of call contains the size of
//								the receive buffer for decrypted data.  When the method returns, this will contain
//								the actual size of the decrypted data.
//			pubPublicKey -		the RSA public key key to decrypt the data with
//			cubPublicKey -		Size of the key
// Output:  true if successful, false if decryption failed
//-----------------------------------------------------------------------------
bool CCrypto::RSAPublicDecrypt_NoPadding( const uint8 *pubEncryptedData, uint32 cubEncryptedData, 
						 uint8 *pubPlaintextData, uint32 *pcubPlaintextData, 
						 const uint8 *pubPublicKey, const uint32 cubPublicKey )
{
	VPROF_BUDGET( "CCrypto::RSADecrypt", VPROF_BUDGETGROUP_ENCRYPTION );
	bool bRet = false;

	Assert( cubEncryptedData > 0 );	// must pass in some data

	// BUGBUG taylor
	// This probably only works for reasonably small ciphertext sizes.

	try		// handle any exceptions crypto++-5.2 may throw
	{
		StringSource stringSourcePublicKey( pubPublicKey, cubPublicKey, true );

		// 1. We need to use a Verifier because a Decryptor expects a private key,
		// which is encoded differently
		// 2. We are using neither PKCS1v15 padding nor SHA in any way, we are simply
		// using this object as a means of instantiating the key decryption function
		RSASSA_PKCS1v15_SHA_Verifier pub( stringSourcePublicKey );

		// Ask for the data to be decrypted
		// Caveat: this may "succeed" even if the ciphertext is bogus, so it
		// is up to the caller to do any MAC or other sanity checking
		Integer x = pub.AccessKey().ApplyFunction(Integer(pubEncryptedData, cubEncryptedData));

		// Result is an 'Integer', essentially a string of bytes for our
		// purposes though.
		uint32 nBytes = x.ByteCount();
		if ( nBytes > *pcubPlaintextData )
		{
			return false;
		}

		// don't tell it to encode to the full buffer size, because it will
		// pre-pad with zeros. Just squeeze it in to the first nBytes of the
		// buffer.
		x.Encode( pubPlaintextData, nBytes );
		*pcubPlaintextData = nBytes;

		bRet = true;
	}
	catch ( Exception e ) 
	{
		DMsg( SPEW_CRYPTO, 2, "CCrypto::RSAPublicDecrypt_NoPadding: Decrypt() threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Generates an RSA signature block for the specified data with the specified
//			RSA private key.  The signature can be verified by calling RSAVerifySignature
//			with the RSA public key.
// Input:	pubData -			Data to be signed
//			cubData -			Size of data to be signed
//			pubSignature -		Pointer to buffer to receive signature block
//			pcubSignature -		Pointer to a variable that at time of call contains the size of
//								the pubSignature buffer.  When the method returns, this will contain
//								the actual size of the signature block
//			pubPrivateKey -		The RSA private key to use to sign the data
//			cubPrivateKey -		Size of the key
// Output:  true if successful, false if signature failed
//-----------------------------------------------------------------------------
bool CCrypto::RSASign( const uint8 *pubData, const uint32 cubData, 
					   uint8 *pubSignature, uint32 *pcubSignature,
					   const uint8 *pubPrivateKey, const uint32 cubPrivateKey )
{
	VPROF_BUDGET( "CCrypto::RSASign", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pubData );
	Assert( pubPrivateKey );
	Assert( cubPrivateKey > 0 );
	Assert( pubSignature );
	Assert( pcubSignature );
	bool bRet = false;
	try		// handle any exceptions crypto++ may throw
	{
		StringSource stringSourcePrivateKey( pubPrivateKey, cubPrivateKey, true );
		RSASSA_PKCS1v15_SHA_Signer rsaSigner( stringSourcePrivateKey );
		CPoolAllocatedRNG rng;
		size_t len = rsaSigner.SignMessage( rng.GetRNG(), (byte *)pubData, cubData, pubSignature );
		AssertFatal( len <= ULONG_MAX );
		*pcubSignature = (uint32)len;
		bRet = true;
	}
	catch ( Exception e ) 
	{
		DMsg( SPEW_CRYPTO, 2, "CCrypto::RSASign: SignMessage threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Verifies that signature block is authentic for given data & RSA public key
// Input:	pubData -			Data that was signed
//			cubData -			Size of data that was signed signed
//			pubSignature -		Signature block
//			cubSignature -		Size of signature block
//			pubPublicKey -		The RSA public key to use to verify the signature 
//								(must be from same pair as RSA private key used to generate signature)
//			cubPublicKey -		Size of the key
// Output:  true if successful and signature is authentic, false if signature does not match or other error
//-----------------------------------------------------------------------------
bool CCrypto::RSAVerifySignature( const uint8 *pubData, const uint32 cubData, 
								  const uint8 *pubSignature, const uint32 cubSignature, 
								  const uint8 *pubPublicKey, const uint32 cubPublicKey )
{
	VPROF_BUDGET( "CCrypto::RSAVerifySignature", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pubData );
	Assert( pubSignature );
	Assert( pubPublicKey );

	bool bRet = false;	
	try		// handle any exceptions crypto++ may throw
	{
		StringSource stringSourcePublicKey( pubPublicKey, cubPublicKey, true );
		RSASSA_PKCS1v15_SHA_Verifier pub( stringSourcePublicKey );
		bRet = pub.VerifyMessage( pubData, cubData, pubSignature, cubSignature );
	}
	catch ( Exception e ) 
	{
		DMsg( SPEW_CRYPTO, 2, "CCrypto::RSASign: VerifyMessage threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
	}

	return bRet;
}

//-----------------------------------------------------------------------------
// Purpose: Generates an RSA signature block for the specified data with the specified
//			RSA private key.  The signature can be verified by calling RSAVerifySignature
//			with the RSA public key.
// Input:	pubData -			Data to be signed
//			cubData -			Size of data to be signed
//			pubSignature -		Pointer to buffer to receive signature block
//			pcubSignature -		Pointer to a variable that at time of call contains the size of
//								the pubSignature buffer.  When the method returns, this will contain
//								the actual size of the signature block
//			pubPrivateKey -		The RSA private key to use to sign the data
//			cubPrivateKey -		Size of the key
// Output:  true if successful, false if signature failed
//-----------------------------------------------------------------------------
bool CCrypto::RSASignSHA256( const uint8 *pubData, const uint32 cubData, 
					   uint8 *pubSignature, uint32 *pcubSignature,
					   const uint8 *pubPrivateKey, const uint32 cubPrivateKey )
{
	VPROF_BUDGET( "CCrypto::RSASign", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pubData );
	Assert( pubPrivateKey );
	Assert( cubPrivateKey > 0 );
	Assert( pubSignature );
	Assert( pcubSignature );
	bool bRet = false;
	try		// handle any exceptions crypto++ may throw
	{
		StringSource stringSourcePrivateKey( pubPrivateKey, cubPrivateKey, true );
		RSASS<PKCS1v15, SHA256>::Signer rsaSigner( stringSourcePrivateKey );
		CPoolAllocatedRNG rng;
		size_t len = rsaSigner.SignMessage( rng.GetRNG(), (byte *)pubData, cubData, pubSignature );
		AssertFatal( len <= ULONG_MAX );
		*pcubSignature = (uint32)len;
		bRet = true;
	}
	catch ( Exception e ) 
	{
		DMsg( SPEW_CRYPTO, 2, "CCrypto::RSASign: SignMessage threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Verifies that signature block is authentic for given data & RSA public key
// Input:	pubData -			Data that was signed
//			cubData -			Size of data that was signed signed
//			pubSignature -		Signature block
//			cubSignature -		Size of signature block
//			pubPublicKey -		The RSA public key to use to verify the signature 
//								(must be from same pair as RSA private key used to generate signature)
//			cubPublicKey -		Size of the key
// Output:  true if successful and signature is authentic, false if signature does not match or other error
//-----------------------------------------------------------------------------
bool CCrypto::RSAVerifySignatureSHA256( const uint8 *pubData, const uint32 cubData, 
								  const uint8 *pubSignature, const uint32 cubSignature, 
								  const uint8 *pubPublicKey, const uint32 cubPublicKey )
{
	VPROF_BUDGET( "CCrypto::RSAVerifySignature", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pubData );
	Assert( pubSignature );
	Assert( pubPublicKey );

	bool bRet = false;	
	try		// handle any exceptions crypto++ may throw
	{
		StringSource stringSourcePublicKey( pubPublicKey, cubPublicKey, true );
		RSASS<PKCS1v15, SHA256>::Verifier pub( stringSourcePublicKey );
		bRet = pub.VerifyMessage( pubData, cubData, pubSignature, cubSignature );
	}
	catch ( Exception e ) 
	{
		DMsg( SPEW_CRYPTO, 2, "CCrypto::RSASign: VerifyMessage threw exception %s (%d)\n",
			e.what(), e.GetErrorType() );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Hex-encodes a block of data.  (Binary -> text representation.)  The output
//			is null-terminated and can be treated as a string.
// Input:	pubData -			Data to encode
//			cubData -			Size of data to encode
//			pchEncodedData -	Pointer to string buffer to store output in
//			cchEncodedData -	Size of pchEncodedData buffer
//-----------------------------------------------------------------------------
bool CCrypto::HexEncode( const uint8 *pubData, const uint32 cubData, char *pchEncodedData, uint32 cchEncodedData )
{
	VPROF_BUDGET( "CCrypto::HexEncode", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pubData );
	Assert( cubData );
	Assert( pchEncodedData );
	Assert( cchEncodedData > 0 );

	if ( cchEncodedData < ( ( cubData * 2 ) + 1 ) )
	{
		Assert( cchEncodedData >= ( cubData * 2 ) + 1 );  // expands to 2x input + NULL, must have room in output buffer
		*pchEncodedData = '\0';
		return false;
	}

	ArraySink * pArraySinkOutput = new ArraySink( (byte *) pchEncodedData, cchEncodedData );
	// Note: HexEncoder now owns the pointer to pOutputSink and will free it when the encoder goes out of scope and destructs
	HexEncoder hexEncoder( pArraySinkOutput );
	hexEncoder.Put( pubData, cubData );
	hexEncoder.MessageEnd();

	uint32 len = pArraySinkOutput->TotalPutLength();
	if ( len >= cchEncodedData )
	{
		AssertMsg2( false, "CCrypto::HexEncode: insufficient output buffer for encoding, needed %d got %d\n",
					len, cchEncodedData );
		return false;
	}

	pchEncodedData[len] = 0;	// NULL-terminate
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Hex-decodes a block of data.  (Text -> binary representation.)  
// Input:	pchData -			Null-terminated hex-encoded string 
//			pubDecodedData -	Pointer to buffer to store output in
//			pcubDecodedData -	Pointer to variable that contains size of
//								output buffer.  At exit, is filled in with actual size
//								of decoded data.
//-----------------------------------------------------------------------------
bool CCrypto::HexDecode( const char *pchData, uint8 *pubDecodedData, uint32 *pcubDecodedData )
{
	VPROF_BUDGET( "CCrypto::HexDecode", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pchData );
	Assert( pubDecodedData );
	Assert( pcubDecodedData );
	Assert( *pcubDecodedData );

	ArraySink * pArraySinkOutput = new ArraySink( pubDecodedData, *pcubDecodedData );
	// Note: HexEncoder now owns the pointer to pOutputSink and will free it when the encoder goes out of scope and destructs
	HexDecoder hexDecoder( pArraySinkOutput );
	hexDecoder.Put( (byte *) pchData, Q_strlen( pchData ) );
	hexDecoder.MessageEnd();

	uint32 len = pArraySinkOutput->TotalPutLength();
	if ( len > *pcubDecodedData )
	{
		AssertMsg2( false, "CCrypto::HexDecode: insufficient output buffer for decoding, needed %d got %d\n",
					len, *pcubDecodedData );
		return false;
	}
	*pcubDecodedData = len;

	return true;
}


static const int k_LineBreakEveryNGroups = 18; // line break every 18 groups of 4 characters (every 72 characters)

//-----------------------------------------------------------------------------
// Purpose: Returns the expected buffer size that should be passed to Base64Encode.
// Input:	cubData -			Size of data to encode
//			bInsertLineBreaks -	If line breaks should be inserted automatically
//-----------------------------------------------------------------------------
uint32 CCrypto::Base64EncodeMaxOutput( const uint32 cubData, const char *pszLineBreak )
{
	// terminating null + 4 chars per 3-byte group + line break after every 18 groups (72 output chars) + final line break
	uint32 nGroups = (cubData+2)/3;
	str_size cchRequired = 1 + nGroups*4 + ( pszLineBreak ? Q_strlen(pszLineBreak)*(1+(nGroups-1)/k_LineBreakEveryNGroups) : 0 );
	return cchRequired;
}


//-----------------------------------------------------------------------------
// Purpose: Base64-encodes a block of data.  (Binary -> text representation.)  The output
//			is null-terminated and can be treated as a string.
// Input:	pubData -			Data to encode
//			cubData -			Size of data to encode
//			pchEncodedData -	Pointer to string buffer to store output in
//			cchEncodedData -	Size of pchEncodedData buffer
//			bInsertLineBreaks -	If "\n" line breaks should be inserted automatically
//-----------------------------------------------------------------------------
bool CCrypto::Base64Encode( const uint8 *pubData, uint32 cubData, char *pchEncodedData, uint32 cchEncodedData, bool bInsertLineBreaks )
{
	const char *pszLineBreak = bInsertLineBreaks ? "\n" : NULL;
	uint32 cchRequired = Base64EncodeMaxOutput( cubData, pszLineBreak );
	(void)cchRequired;
	AssertMsg2( cchEncodedData >= cchRequired, "CCrypto::Base64Encode: insufficient output buffer for encoding, needed %d got %d\n", cchRequired, cchEncodedData );
	return Base64Encode( pubData, cubData, pchEncodedData, &cchEncodedData, pszLineBreak );
}

//-----------------------------------------------------------------------------
// Purpose: Base64-encodes a block of data.  (Binary -> text representation.)  The output
//			is null-terminated and can be treated as a string.
// Input:	pubData -			Data to encode
//			cubData -			Size of data to encode
//			pchEncodedData -	Pointer to string buffer to store output in
//			pcchEncodedData -	Pointer to size of pchEncodedData buffer; adjusted to number of characters written (before NULL)
//			pszLineBreak -		String to be inserted every 72 characters; empty string or NULL pointer for no line breaks
// Note: if pchEncodedData is NULL and *pcchEncodedData is zero, *pcchEncodedData is filled with the actual required length
// for output. A simpler approximation for maximum output size is (cubData * 4 / 3) + 5 if there are no linebreaks.
//-----------------------------------------------------------------------------
bool CCrypto::Base64Encode( const uint8 *pubData, uint32 cubData, char *pchEncodedData, uint32* pcchEncodedData, const char *pszLineBreak )
{
	VPROF_BUDGET( "CCrypto::Base64Encode", VPROF_BUDGETGROUP_ENCRYPTION );
	
	if ( pchEncodedData == NULL )
	{
		AssertMsg( *pcchEncodedData == 0, "NULL output buffer with non-zero size passed to Base64Encode" );
		*pcchEncodedData = Base64EncodeMaxOutput( cubData, pszLineBreak );
		return true;
	}
	
	const uint8 *pubDataEnd = pubData + cubData;
	char *pchEncodedDataStart = pchEncodedData;
	str_size unLineBreakLen = pszLineBreak ? Q_strlen( pszLineBreak ) : 0;
	int nNextLineBreak = unLineBreakLen ? k_LineBreakEveryNGroups : INT_MAX;

	const char * const pszBase64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	uint32 cchEncodedData = *pcchEncodedData;
	if ( cchEncodedData == 0 )
		goto out_of_space;

	--cchEncodedData; // pre-decrement for the terminating null so we don't forget about it

	// input 3 x 8-bit, output 4 x 6-bit
	while ( pubDataEnd - pubData >= 3 )
	{
		if ( cchEncodedData < 4 + unLineBreakLen )
			goto out_of_space;
		
		if ( nNextLineBreak == 0 )
		{
			memcpy( pchEncodedData, pszLineBreak, unLineBreakLen );
			pchEncodedData += unLineBreakLen;
			cchEncodedData -= unLineBreakLen;
			nNextLineBreak = k_LineBreakEveryNGroups;
		}

		uint32 un24BitsData;
		un24BitsData  = (uint32) pubData[0] << 16;
		un24BitsData |= (uint32) pubData[1] << 8;
		un24BitsData |= (uint32) pubData[2];
		pubData += 3;

		pchEncodedData[0] = pszBase64Chars[ (un24BitsData >> 18) & 63 ];
		pchEncodedData[1] = pszBase64Chars[ (un24BitsData >> 12) & 63 ];
		pchEncodedData[2] = pszBase64Chars[ (un24BitsData >>  6) & 63 ];
		pchEncodedData[3] = pszBase64Chars[ (un24BitsData      ) & 63 ];
		pchEncodedData += 4;
		cchEncodedData -= 4;
		--nNextLineBreak;
	}

	// Clean up remaining 1 or 2 bytes of input, pad output with '='
	if ( pubData != pubDataEnd )
	{
		if ( cchEncodedData < 4 + unLineBreakLen )
			goto out_of_space;

		if ( nNextLineBreak == 0 )
		{
			memcpy( pchEncodedData, pszLineBreak, unLineBreakLen );
			pchEncodedData += unLineBreakLen;
			cchEncodedData -= unLineBreakLen;
		}

		uint32 un24BitsData;
		un24BitsData = (uint32) pubData[0] << 16;
		if ( pubData+1 != pubDataEnd )
		{
			un24BitsData |= (uint32) pubData[1] << 8;
		}
		pchEncodedData[0] = pszBase64Chars[ (un24BitsData >> 18) & 63 ];
		pchEncodedData[1] = pszBase64Chars[ (un24BitsData >> 12) & 63 ];
		pchEncodedData[2] = pubData+1 != pubDataEnd ? pszBase64Chars[ (un24BitsData >> 6) & 63 ] : '=';
		pchEncodedData[3] = '=';
		pchEncodedData += 4;
		cchEncodedData -= 4;
	}

	if ( unLineBreakLen )
	{
		if ( cchEncodedData < unLineBreakLen )
			goto out_of_space;
		memcpy( pchEncodedData, pszLineBreak, unLineBreakLen );
		pchEncodedData += unLineBreakLen;
		cchEncodedData -= unLineBreakLen;
	}

	*pchEncodedData = 0;
	*pcchEncodedData = pchEncodedData - pchEncodedDataStart;
	return true;

out_of_space:
	*pchEncodedData = 0;
	*pcchEncodedData = Base64EncodeMaxOutput( cubData, pszLineBreak );
	AssertMsg( false, "CCrypto::Base64Encode: insufficient output buffer (up to n*4/3+5 bytes required, plus linebreaks)" );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Base64-decodes a block of data.  (Text -> binary representation.)  
// Input:	pchData -			Null-terminated hex-encoded string 
//			pubDecodedData -	Pointer to buffer to store output in
//			pcubDecodedData -	Pointer to variable that contains size of
//								output buffer.  At exit, is filled in with actual size
//								of decoded data.
// Note: if NULL is passed as the output buffer and *pcubDecodedData is zero, the function
// will calculate the actual required size and place it in *pcubDecodedData. A simpler upper
// bound on the required size is ( strlen(pchData)*3/4 + 1 ).
//-----------------------------------------------------------------------------
bool CCrypto::Base64Decode( const char *pchData, uint8 *pubDecodedData, uint32 *pcubDecodedData, bool bIgnoreInvalidCharacters )
{
	return Base64Decode( pchData, ~0u, pubDecodedData, pcubDecodedData, bIgnoreInvalidCharacters );
}

//-----------------------------------------------------------------------------
// Purpose: Base64-decodes a block of data.  (Text -> binary representation.)  
// Input:	pchData -			base64-encoded string, null terminated
//			cchDataMax -		maximum length of string unless a null is encountered first
//			pubDecodedData -	Pointer to buffer to store output in
//			pcubDecodedData -	Pointer to variable that contains size of
//								output buffer.  At exit, is filled in with actual size
//								of decoded data.
// Note: if NULL is passed as the output buffer and *pcubDecodedData is zero, the function
// will calculate the actual required size and place it in *pcubDecodedData. A simpler upper
// bound on the required size is ( strlen(pchData)*3/4 + 2 ).
//-----------------------------------------------------------------------------
bool CCrypto::Base64Decode( const char *pchData, uint32 cchDataMax, uint8 *pubDecodedData, uint32 *pcubDecodedData, bool bIgnoreInvalidCharacters )
{
	VPROF_BUDGET( "CCrypto::Base64Decode", VPROF_BUDGETGROUP_ENCRYPTION );
	
	uint32 cubDecodedData = *pcubDecodedData;
	uint32 cubDecodedDataOrig = cubDecodedData;

	if ( pubDecodedData == NULL )
	{
		AssertMsg( *pcubDecodedData == 0, "NULL output buffer with non-zero size passed to Base64Decode" );
		cubDecodedDataOrig = cubDecodedData = ~0u;
	}
	
	// valid base64 character range: '+' (0x2B) to 'z' (0x7A)
	// table entries are 0-63, -1 for invalid entries, -2 for '='
	static const char rgchInvBase64[] = {
		62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
		-1, -1, -1, -2, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,
		 8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
		23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31,
		32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
		47, 48, 49, 50, 51
	};
	COMPILE_TIME_ASSERT( Q_ARRAYSIZE(rgchInvBase64) == 0x7A - 0x2B + 1 );

	uint32 un24BitsWithSentinel = 1;
	while ( cchDataMax-- > 0 )
	{
		char c = *pchData++;

		if ( (uint8)(c - 0x2B) >= Q_ARRAYSIZE( rgchInvBase64 ) )
		{
			if ( c == '\0' )
				break;

			if ( !bIgnoreInvalidCharacters && !( c == '\r' || c == '\n' || c == '\t' || c == ' ' ) )
				goto decode_failed;
			else
				continue;
		}

		c = rgchInvBase64[(uint8)(c - 0x2B)];
		if ( c < 0 )
		{
			if ( c == -2 ) // -2 -> terminating '='
				break;

			if ( !bIgnoreInvalidCharacters )
				goto decode_failed;
			else
				continue;
		}

		un24BitsWithSentinel <<= 6;
		un24BitsWithSentinel |= c;
		if ( un24BitsWithSentinel & (1<<24) )
		{
			if ( cubDecodedData < 3 ) // out of space? go to final write logic
				break;
			if ( pubDecodedData )
			{
				pubDecodedData[0] = (uint8)( un24BitsWithSentinel >> 16 );
				pubDecodedData[1] = (uint8)( un24BitsWithSentinel >> 8);
				pubDecodedData[2] = (uint8)( un24BitsWithSentinel );
				pubDecodedData += 3;
			}
			cubDecodedData -= 3;
			un24BitsWithSentinel = 1;
		}
	}

	// If un24BitsWithSentinel contains data, output the remaining full bytes
	if ( un24BitsWithSentinel >= (1<<6) )
	{
		// Possibilities are 3, 2, 1, or 0 full output bytes.
		int nWriteBytes = 3;
		while ( un24BitsWithSentinel < (1<<24) )
		{
			nWriteBytes--;
			un24BitsWithSentinel <<= 6;
		}

		// Write completed bytes to output
		while ( nWriteBytes-- > 0 )
		{
			if ( cubDecodedData == 0 )
			{
				AssertMsg( false, "CCrypto::Base64Decode: insufficient output buffer (up to n*3/4+2 bytes required)" );
				goto decode_failed;
			}
			if ( pubDecodedData )
			{
				*pubDecodedData++ = (uint8)(un24BitsWithSentinel >> 16);
			}
			--cubDecodedData;
			un24BitsWithSentinel <<= 8;
		}
	}
	
	*pcubDecodedData = cubDecodedDataOrig - cubDecodedData;
	return true;

decode_failed:
	*pcubDecodedData = cubDecodedDataOrig - cubDecodedData;
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Generate a SHA1 hash
// Input:	pchInput -			Plaintext string of item to hash (null terminated)
//			pOutDigest -		Pointer to receive hashed digest output
//-----------------------------------------------------------------------------
bool CCrypto::GenerateSHA1Digest( const uint8 *pubInput, const int cubInput, SHADigest_t *pOutDigest )
{
	VPROF_BUDGET( "CCrypto::GenerateSHA1Digest", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pubInput );
	Assert( cubInput > 0 );
	Assert( pOutDigest );

	bool bSuccess = true;
	try
	{
		CryptoPP::SHA().CalculateDigest( *pOutDigest, pubInput, cubInput );
	}
	catch(...)
	{
		bSuccess = false;
	}

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Generate a hash Salt - be careful, over-writing an existing salt
// will render the hashed value unverifiable.
//-----------------------------------------------------------------------------
bool CCrypto::GenerateSalt( Salt_t *pSalt )
{
	VPROF_BUDGET( "CCrypto::GenerateSalt", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pSalt );

	bool bSuccess = true;
	try
	{
		CPoolAllocatedRNG rng;
		rng.GetRNG().GenerateBlock( (byte*)pSalt, sizeof(Salt_t) );
	}
	catch(...)
	{
		bSuccess = false;
	}

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Generate a SHA1 hash using a salt.
// Input:	pchInput -			Plaintext string of item to hash (null terminated)
//			pSalt -				Salt
//			pOutDigest -		Pointer to receive salted digest output
//-----------------------------------------------------------------------------
bool CCrypto::GenerateSaltedSHA1Digest( const char *pchInput, const Salt_t *pSalt, SHADigest_t *pOutDigest )
{
	VPROF_BUDGET( "CCrypto::GenerateSaltedSHA1Digest", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pchInput );
	Assert( pSalt );
	Assert( pOutDigest );

	int iInputLen = Q_strlen( pchInput );
	uint8 *pubSaltedInput = new uint8[ iInputLen + sizeof( Salt_t ) ];

	// Insert half the salt before the input string and half at the end.
	// This is probably unnecessary (to split the salt) but we're stuck with it for historical reasons.
	uint8 *pubCursor = pubSaltedInput;
	Q_memcpy( pubCursor, (uint8 *)pSalt, sizeof(Salt_t) / 2 );
	pubCursor += sizeof( Salt_t ) / 2;
	Q_memcpy( pubCursor, pchInput, iInputLen );
	pubCursor += iInputLen;
	Q_memcpy( pubCursor, (uint8 *)pSalt + sizeof(Salt_t) / 2, sizeof(Salt_t) / 2 );

	bool bSuccess = true;
	try
	{
		CryptoPP::SHA().CalculateDigest( *pOutDigest, pubSaltedInput, iInputLen + sizeof( Salt_t ) );
	}
	catch(...)
	{
		bSuccess = false;
	}

	delete [] pubSaltedInput;

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Generates a random block of data
//-----------------------------------------------------------------------------
bool CCrypto::GenerateRandomBlock( uint8 *pubDest, int cubDest )
{
	CPoolAllocatedRNG rng;
	rng.GetRNG().GenerateBlock( pubDest, cubDest );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Generate a keyed-hash MAC using SHA1
// Input:	pubData -			Plaintext data to digest
//			cubData -			length of data
//			pubKey -			key to use in HMAC
//			cubKey -			length of key
//			pOutDigest -		Pointer to receive hashed digest output
//-----------------------------------------------------------------------------
bool CCrypto::GenerateHMAC( const uint8 *pubData, uint32 cubData, const uint8 *pubKey, uint32 cubKey, SHADigest_t *pOutputDigest )
{
	VPROF_BUDGET( "CCrypto::GenerateHMAC", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pubData );
	Assert( cubData > 0 );
	Assert( pubKey );
	Assert( cubKey > 0 );
	Assert( pOutputDigest );

	bool bSuccess = true;
	try
	{
		CryptoPP::HMAC< CryptoPP::SHA1 > hmac( pubKey, cubKey );
		hmac.CalculateDigest( *pOutputDigest, pubData, cubData );
	}
	catch(...)
	{
		bSuccess = false;
	}

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Generate a keyed-hash MAC using SHA-256
//-----------------------------------------------------------------------------
bool CCrypto::GenerateHMAC256( const uint8 *pubData, uint32 cubData, const uint8 *pubKey, uint32 cubKey, SHA256Digest_t *pOutputDigest )
{
	VPROF_BUDGET( "CCrypto::GenerateHMAC256", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pubData );
	Assert( cubData > 0 );
	Assert( pubKey );
	Assert( cubKey > 0 );
	Assert( pOutputDigest );

	bool bSuccess = true;
	try
	{
		CryptoPP::HMAC< CryptoPP::SHA256 > hmac( pubKey, cubKey );
		hmac.CalculateDigest( *pOutputDigest, pubData, cubData );
	}
	catch(...)
	{
		bSuccess = false;
	}

	return bSuccess;
}


bool CCrypto::BGzipBuffer( const uint8 *pubData, uint32 cubData, CCryptoOutBuffer &bufOutput )
{
	bool bSuccess = true;
	try
	{
		std::string gzip_output;
		StringSource( (byte *)pubData, cubData, true, new Gzip( new StringSink( gzip_output ) ) ); 
		bufOutput.Set( (uint8*)gzip_output.c_str(), (uint32)gzip_output.length() );
	}
	catch( ... )
	{
		bSuccess = false;
	}
	return bSuccess;
}


bool CCrypto::BGunzipBuffer( const uint8 *pubData, uint32 cubData, CCryptoOutBuffer &bufOutput   )
{
	bool bSuccess = true;
	try
	{
		std::string gunzip_output;
		StringSource( (byte *)pubData, cubData, true, new Gunzip( new StringSink( gunzip_output ) ) ); 
		bufOutput.Set( (uint8*)gunzip_output.c_str(), (uint32)gunzip_output.length() );
	}
	catch( ... )
	{
		bSuccess = false;
	}
	return bSuccess;
}


//! These are all needed to get around stack-overflow bug in Initialize()
class HexDecoderTKS : public HexDecoder
{
public:
	HexDecoderTKS(BufferedTransformation *attachment, const int *pnDecodingArray)
		: HexDecoder(attachment) 
	{
		BaseN_Decoder::IsolatedInitialize( MakeParameters( Name::DecodingLookupArray(), pnDecodingArray )( Name::Log2Base(), 4 ) );
	}
};

class Base32DecoderTKS : public Base32Decoder
{
public:
	Base32DecoderTKS(BufferedTransformation *attachment, const int *pnDecodingArray)
		: Base32Decoder(attachment)
	{
		BaseN_Decoder::IsolatedInitialize( MakeParameters( Name::DecodingLookupArray(), pnDecodingArray )( Name::Log2Base(), 5 ) );
	}
};


//-----------------------------------------------------------------------------
// Purpose: Implement hex encoding / decoding using a custom lookup table.
//			This is a class because the decoding is done via a generated 
//			reverse-lookup table, and to save time it's best to just create
//			that table once.
//-----------------------------------------------------------------------------
CCustomHexEncoder::CCustomHexEncoder( const char *pchEncodingTable )
{
	m_bValidEncoding = false;
	if ( Q_strlen( pchEncodingTable ) == sizeof( m_rgubEncodingTable ) )
	{
		Q_memcpy( m_rgubEncodingTable, pchEncodingTable, sizeof( m_rgubEncodingTable ) );

		BaseN_Decoder::InitializeDecodingLookupArray( m_rgnDecodingTable, m_rgubEncodingTable, 16, false );
		m_bValidEncoding = true;
	}
	else
	{
		AssertMsg( false, "CCrypto::CustomHexEncoder: Improper encoding table\n" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCustomHexEncoder::~CCustomHexEncoder()
{
}


//-----------------------------------------------------------------------------
// Purpose: Hex-encodes a block of data.  (Binary -> text representation.)  The output
//			is null-terminated and can be treated as a string.
//			Uses the supplied custom encoding characters
// Input:	pubData -			Data to encode
//			cubData -			Size of data to encode
//			pchEncodedData -	Pointer to string buffer to store output in
//			cchEncodedData -	Size of pchEncodedData buffer
//-----------------------------------------------------------------------------
bool CCustomHexEncoder::Encode( const uint8 *pubData, const uint32 cubData, char *pchEncodedData, uint32 cchEncodedData )
{
	VPROF_BUDGET( "CCrypto::CustomHexEncode", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pubData );
	Assert( cubData );
	Assert( pchEncodedData );
	Assert( cchEncodedData > 0 );

	if ( !m_bValidEncoding )
		return false;

	ArraySink * pArraySinkOutput = new ArraySink( (byte *) pchEncodedData, cchEncodedData );
	// Note: HexEncoder now owns the pointer to pOutputSink and will free it when the encoder goes out of scope and destructs
	HexEncoder hexEncoder( pArraySinkOutput );
	hexEncoder.IsolatedInitialize( MakeParameters( Name::EncodingLookupArray(), (const uint8 *)m_rgubEncodingTable ) );
	hexEncoder.Put( pubData, cubData );
	hexEncoder.MessageEnd();

	uint32 len = pArraySinkOutput->TotalPutLength();
	pchEncodedData[len] = 0;	// NULL-terminate
	if ( len >= cchEncodedData )
	{
		AssertMsg2( false, "CCrypto::CustomHexEncode: insufficient output buffer for encoding, needed %d got %d\n",
			len, cchEncodedData );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Hex-decodes a block of data.  (Text -> binary representation.)  
//			With custom encoding-table
// Input:	pchData -			Null-terminated hex-encoded string 
//			pubDecodedData -	Pointer to buffer to store output in
//			pcubDecodedData -	Pointer to variable that contains size of
//								output buffer.  At exit, is filled in with actual size
//								of decoded data.
//-----------------------------------------------------------------------------
bool CCustomHexEncoder::Decode( const char *pchData, uint8 *pubDecodedData, uint32 *pcubDecodedData )
{
	VPROF_BUDGET( "CCrypto::CustomHexDecode", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pchData );
	Assert( pubDecodedData );
	Assert( pcubDecodedData );
	Assert( *pcubDecodedData );

	if ( !m_bValidEncoding )
		return false;

	ArraySink * pArraySinkOutput = new ArraySink( pubDecodedData, *pcubDecodedData );
	// Note: HexEncoder now owns the pointer to pOutputSink and will free it when the encoder goes out of scope and destructs
	HexDecoderTKS hexDecoder( pArraySinkOutput, (const int *)m_rgnDecodingTable );
	hexDecoder.Put( (byte *) pchData, Q_strlen( pchData ) );
	hexDecoder.MessageEnd();

	uint32 len = pArraySinkOutput->TotalPutLength();
	if ( len > *pcubDecodedData )
	{
		AssertMsg2( false, "CCrypto::CustomHexDecode: insufficient output buffer for decoding, needed %d got %d\n",
			len, *pcubDecodedData );
		return false;
	}
	*pcubDecodedData = len;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Implement hex encoding / decoding using a custom lookup table.
//			This is a class because the decoding is done via a generated 
//			reverse-lookup table, and to save time it's best to just create
//			that table once.
//-----------------------------------------------------------------------------
CCustomBase32Encoder::CCustomBase32Encoder( const char *pchEncodingTable )
{
	m_bValidEncoding = false;
	if ( Q_strlen( pchEncodingTable ) == sizeof( m_rgubEncodingTable ) )
	{
		Q_memcpy( m_rgubEncodingTable, pchEncodingTable, sizeof( m_rgubEncodingTable ) );

		BaseN_Decoder::InitializeDecodingLookupArray( m_rgnDecodingTable, m_rgubEncodingTable, 32, false );
		m_bValidEncoding = true;
	}
	else
	{
		AssertMsg( false, "CCrypto::CustomBase32Encoder: Improper encoding table\n" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCustomBase32Encoder::~CCustomBase32Encoder()
{
}


//-----------------------------------------------------------------------------
// Purpose: Base32-encodes a block of data.  (Binary -> text representation.)  The output
//			is null-terminated and can be treated as a string.
//			Uses the supplied custom encoding table
// Input:	pubData -			Data to encode
//			cubData -			Size of data to encode
//			pchEncodedData -	Pointer to string buffer to store output in
//			cchEncodedData -	Size of pchEncodedData buffer
//-----------------------------------------------------------------------------
bool CCustomBase32Encoder::Encode( const uint8 *pubData, const uint32 cubData, char *pchEncodedData, uint32 cchEncodedData )
{
	VPROF_BUDGET( "CCrypto::CustomBase32Encode", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pubData );
	Assert( cubData );
	Assert( pchEncodedData );
	Assert( cchEncodedData > 0 );

	if ( !m_bValidEncoding )
		return false;

	ArraySink * pArraySinkOutput = new ArraySink( (byte *) pchEncodedData, cchEncodedData );
	// Note: Base32Encoder now owns the pointer to pOutputSink and will free it when the encoder goes out of scope and destructs
	Base32Encoder base32Encoder( pArraySinkOutput );
	base32Encoder.IsolatedInitialize( MakeParameters( Name::EncodingLookupArray(), (const uint8 *)m_rgubEncodingTable ) );
	base32Encoder.Put( pubData, cubData );
	base32Encoder.MessageEnd();

	uint32 len = pArraySinkOutput->TotalPutLength();
	pchEncodedData[len] = 0;	// NULL-terminate
	if ( len >= cchEncodedData )
	{
		AssertMsg2( false, "CCrypto::CustomBase32Encode: insufficient output buffer for encoding, needed %d got %d\n",
			len, cchEncodedData );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Base32-decodes a block of data.  (Text -> binary representation.)  
//			With custom encoding table
// Input:	pchData -			Null-terminated hex-encoded string 
//			pubDecodedData -	Pointer to buffer to store output in
//			pcubDecodedData -	Pointer to variable that contains size of
//								output buffer.  At exit, is filled in with actual size
//								of decoded data.
//-----------------------------------------------------------------------------
bool CCustomBase32Encoder::Decode( const char *pchData, uint8 *pubDecodedData, uint32 *pcubDecodedData )
{
	VPROF_BUDGET( "CCrypto::CustomBase32Decode", VPROF_BUDGETGROUP_ENCRYPTION );
	Assert( pchData );
	Assert( pubDecodedData );
	Assert( pcubDecodedData );
	Assert( *pcubDecodedData );

	if ( !m_bValidEncoding )
		return false;

	ArraySink * pArraySinkOutput = new ArraySink( pubDecodedData, *pcubDecodedData );
	// Note: Base32Encoder now owns the pointer to pOutputSink and will free it when the encoder goes out of scope and destructs
	Base32DecoderTKS base32Decoder( pArraySinkOutput, (const int *)m_rgnDecodingTable );
	base32Decoder.Put( (byte *) pchData, Q_strlen( pchData ) );
	base32Decoder.MessageEnd();

	uint32 len = pArraySinkOutput->TotalPutLength();
	if ( len > *pcubDecodedData )
	{
		AssertMsg2( false, "CCrypto::CustomBase32Decode: insufficient output buffer for decoding, needed %d got %d\n",
			len, *pcubDecodedData );
		return false;
	}
	*pcubDecodedData = len;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Base32-encodes a block of data.  (Binary -> text representation.)  The output
//			is null-terminated and can be treated as a string.
//			Uses the supplied custom encoding table, and a bit-stream input source
//			(not necessarily an integer number of bytes).
// Input:	pBitStringData -	Data to encode
//			pchEncodedData -	Pointer to string buffer to store output in
//			cchEncodedData -	Size of pchEncodedData buffer
//-----------------------------------------------------------------------------
bool CCustomBase32Encoder::Encode( CSimpleBitString *pBitStringData, char *pchEncodedData, uint32 cchEncodedData )
{
	// This is useful if you have, say, 125 bits of information and
	// want to encode them into 25 base32-encoded characters. 
	uint32 cBits = pBitStringData->GetCurrNumBits();

	uint32 cCharacters = (cBits / 5);
	uint32 cBitsRemainder = cBits % 5;
	if ( cBitsRemainder )
		cCharacters++;

	// GTE because of NULL
	if ( cCharacters >= cchEncodedData )
		return false;

	CSimpleBitString::iterator itBitString( *pBitStringData );
	uint ich = 0;
	for ( ; ich < cCharacters; ++ich )
	{
		uint32 unCodon = itBitString.GetNextBits( 5 );
		// Pad w/ zero bits to integer num codons
		if ( ich == (cCharacters - 1) )
			unCodon <<= cBitsRemainder;

		pchEncodedData[ich] = m_rgubEncodingTable[ unCodon ];
	}

	// NULL
	pchEncodedData[ich] = 0;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Base32-decodes a block of data.  (Text -> binary representation.)  
//			With custom encoding table, and a BitString output
// Input:	pchData -				Null-terminated base32-encoded string 
//			pBitStringDecodedData -	Pointer to BitString to receive decoded data
//-----------------------------------------------------------------------------
bool CCustomBase32Encoder::Decode( const char *pchData, CSimpleBitString *pBitStringDecodedData )
{
	// Note: 25 base32-encoded characters contain 125 bits of information.
	// Decoded into a byte buffer, this yields 15 bytes plus 5 bits of padding.
	// Decoded into a CSimpleBitString, it will yield all 125 bits
	while ( *pchData )
	{
		uint32 unData = m_rgnDecodingTable[(unsigned)*pchData++];
		if ( unData == 0xFFFFFFFF )
			return false;

		pBitStringDecodedData->AppendBits( unData, 5 );
	}

	return true;
}

#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validates memory structures
//-----------------------------------------------------------------------------
void CCrypto::ValidateStatics( CValidator &validator, const char *pchName )
{
	ValidateObj( g_tslistPAutoSeededRNG );
}
#endif // DBGFLAG_VALIDATE


//-----------------------------------------------------------------------------
// Purpose: Given a plaintext password, check whether it matches an existing
// hash
//-----------------------------------------------------------------------------
bool CCrypto::BValidatePasswordHash( const char *pchInput, EPasswordHashAlg hashType, const PasswordHash_t &DigestStored, const Salt_t &Salt, PasswordHash_t *pDigestComputed )
{
	VPROF_BUDGET( "CCrypto::BValidatePasswordHash", VPROF_BUDGETGROUP_ENCRYPTION );

	bool bResult = false;
	size_t cDigest = k_HashLengths[hashType];
	Assert( cDigest != 0 );
	PasswordHash_t tmpDigest;
	PasswordHash_t *pOutputDigest = pDigestComputed;
	if ( pOutputDigest == NULL )
	{
		pOutputDigest = &tmpDigest;
	}

	BGeneratePasswordHash( pchInput, hashType, Salt, *pOutputDigest );
	bResult = ( 0 == Q_memcmp( &DigestStored, pOutputDigest, cDigest ) );

	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: Given a plaintext password and salt, generate a password hash of 
// the requested type.
//-----------------------------------------------------------------------------
bool CCrypto::BGeneratePasswordHash( const char *pchInput, EPasswordHashAlg hashType, const Salt_t &Salt, PasswordHash_t &OutPasswordHash )
{
	VPROF_BUDGET( "CCrypto::BGeneratePasswordHash", VPROF_BUDGETGROUP_ENCRYPTION );

	bool bResult = false;
	size_t cDigest = k_HashLengths[hashType];

	switch ( hashType )
	{
	case k_EHashSHA1:
		bResult = CCrypto::GenerateSaltedSHA1Digest( pchInput, &Salt, (SHADigest_t *)&OutPasswordHash.sha );
		break;
	case k_EHashBigPassword:
	{
		//
		// This is a fake algorithm to test widening of the column.  It's a salted SHA-1 hash with 0x01 padding
		// on either side of it.
		//
		size_t cDigestSHA1 = k_HashLengths[k_EHashSHA1];
		size_t cPadding = ( cDigest - cDigestSHA1 ) / 2;
		
		AssertMsg( ( ( cDigest - cDigestSHA1 ) % 2 ) == 0, "Invalid hash width for k_EHashBigPassword, needs to be even." );

		CCrypto::GenerateSaltedSHA1Digest( pchInput, &Salt, (SHADigest_t *)( (uint8 *)&OutPasswordHash.bigpassword + cPadding ) );
		Q_memset( (uint8 *)&OutPasswordHash, 0x01, cPadding );
		Q_memset( (uint8 *)&OutPasswordHash + cPadding + cDigestSHA1 , 0x01, cPadding );
		bResult = true;
		break;
	}
	case k_EHashPBKDF2_1000:
		bResult = CCrypto::BGeneratePBKDF2Hash( pchInput, Salt, 1000, OutPasswordHash );
		break;
	case k_EHashPBKDF2_5000:
		bResult = CCrypto::BGeneratePBKDF2Hash( pchInput, Salt, 5000, OutPasswordHash );
		break;
	case k_EHashPBKDF2_10000:
		bResult = CCrypto::BGeneratePBKDF2Hash( pchInput, Salt, 10000, OutPasswordHash );
		break;
	case k_EHashSHA1WrappedWithPBKDF2_10000:
		bResult = CCrypto::BGenerateWrappedSHA1PasswordHash( pchInput, Salt, 10000, OutPasswordHash );
		break;
	default:
		AssertMsg1( false, "Invalid password hash type %u passed to BGeneratePasswordHash\n", hashType );
		bResult = false;
	}

	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: Given a plaintext password and salt and a count of rounds, generate a PBKDF2 hash
//			with the requested number of rounds.
//-----------------------------------------------------------------------------
bool CCrypto::BGeneratePBKDF2Hash( const char* pchInput, const Salt_t &Salt, unsigned int rounds, PasswordHash_t &OutPasswordHash )
{
	PKCS5_PBKDF2_HMAC<SHA256> pbkdf;

	unsigned int iterations = pbkdf.DeriveKey( (byte *)&OutPasswordHash.pbkdf2, sizeof(OutPasswordHash.pbkdf2), 0, (const byte *)pchInput, Q_strlen(pchInput), (const byte *)&Salt, sizeof(Salt), rounds );

	return ( iterations == rounds );
}


//-----------------------------------------------------------------------------
// Purpose: Given a plaintext password and salt and a count of rounds, generate a SHA1 hash wrapped with
//			a PBKDF2 hash with the specified number of rounds.
//			Used to provide a stronger password hash for accounts that haven't logged in in a while.
//-----------------------------------------------------------------------------
bool CCrypto::BGenerateWrappedSHA1PasswordHash( const char *pchInput, const Salt_t &Salt, unsigned int rounds, PasswordHash_t &OutPasswordHash )
{
	bool bResult;
	bResult = CCrypto::GenerateSaltedSHA1Digest( pchInput, &Salt, (SHADigest_t *)&OutPasswordHash.sha );
	if ( bResult )
	{
		PKCS5_PBKDF2_HMAC<SHA256> pbkdf;
		unsigned int iterations = pbkdf.DeriveKey( (byte *)&OutPasswordHash.pbkdf2, sizeof(OutPasswordHash.pbkdf2), 0, (const byte *)&OutPasswordHash.sha, sizeof(OutPasswordHash.sha), (const byte *)&Salt, sizeof(Salt), rounds );

		bResult = ( iterations == rounds );
	}
	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: Given an existing password hash and salt, attempt to construct a stronger
//			password hash and return the new hash type.
//
//			Currently the only transformation available is from a SHA1 (or BigPassword)
//			hash to a PBKDF2 hash with 10,000 rounds.  In the future this function
//			may be extended to allow additional transformations.
//-----------------------------------------------------------------------------
bool CCrypto::BUpgradeOrWrapPasswordHash( PasswordHash_t &InPasswordHash, EPasswordHashAlg hashTypeIn, const Salt_t &Salt, PasswordHash_t &OutPasswordHash, EPasswordHashAlg &hashTypeOut )
{
	bool bResult = true;;

	if ( hashTypeIn == k_EHashSHA1 || hashTypeIn == k_EHashBigPassword )
	{
		//
		// Can wrap a SHA1 hash with any PBKDF variant, but right now only 10,000 rounds is
		// implemented.
		//
		if ( hashTypeOut == k_EHashPBKDF2_10000 )
		{
			hashTypeOut = k_EHashSHA1WrappedWithPBKDF2_10000;

			byte * pbHash;
			if ( hashTypeIn == k_EHashSHA1 )
			{
				pbHash = (byte *)&InPasswordHash.sha;
			}
			else
			{
				//
				// Need to unroll BigPasswordHash into unpadded SHA1
				//
				size_t cDigest = k_HashLengths[k_EHashBigPassword];
				size_t cDigestSHA1 = k_HashLengths[k_EHashSHA1];
				size_t cPadding = ( cDigest - cDigestSHA1 ) / 2;

				AssertMsg( ( ( cDigest - cDigestSHA1 ) % 2 ) == 0, "Invalid hash width for k_EHashBigPassword, needs to be even." );
				pbHash = (byte *)&InPasswordHash.sha + cPadding;
			}

			PKCS5_PBKDF2_HMAC<SHA256> pbkdf;
			PasswordHash_t passOut;
			unsigned int iterations = pbkdf.DeriveKey( (byte *)passOut.pbkdf2, sizeof(passOut.pbkdf2), 0, pbHash, k_HashLengths[k_EHashSHA1], (const byte *)&Salt, sizeof(Salt), 10000 );

			bResult = ( iterations == 10000 );
			if ( bResult )
			{
				Q_memcpy( &OutPasswordHash, &passOut, sizeof(OutPasswordHash) );
			}
		}
		else
		{
			Assert( hashTypeOut == k_EHashPBKDF2_10000 );
			bResult = false;
		}
	}
	else
	{
		bResult = false;
		Assert( false );
	}

	return bResult;
}

