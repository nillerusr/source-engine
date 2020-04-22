//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "bsplib.h"
#include "ibsppack.h"
#include "cmdlib.h"
#include "zip_utils.h"

class CBSPPack : public IBSPPack 
{
public:
	void LoadBSPFile( IFileSystem *pFileSystem, char *filename );
	void WriteBSPFile( char *filename );
	void ClearPackFile( void );
	void AddFileToPack( const char *relativename, const char *fullpath );
	void AddBufferToPack( const char *relativename, void *data, int length, bool bTextMode );
	void SetHDRMode( bool bHDR );
	bool SwapBSPFile( IFileSystem *pFileSystem, const char *filename, const char *swapFilename, bool bSwapOnLoad, VTFConvertFunc_t pVTFConvertFunc, VHVFixupFunc_t pVHVFixupFunc, CompressFunc_t pCompressFunc );
	bool RepackBSP( CUtlBuffer &inputBuffer, CUtlBuffer &outputBuffer, eRepackBSPFlags repackFlags );
	bool GetPakFileLump( IFileSystem *pFileSystem, const char *pBSPFilename, void **pPakData, int *pPakSize );
	bool SetPakFileLump( IFileSystem *pFileSystem, const char *pBSPFilename, const char *pNewFilename, void *pPakData, int pakSize );
	bool GetBSPDependants( IFileSystem *pFileSystem, const char *pBSPFilename, CUtlVector< CUtlString > *pList );
};

void CBSPPack::LoadBSPFile( IFileSystem *pFileSystem, char *filename )
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
	
	// This is shady, but the engine is the only client here and we want the same search paths it has.
	g_pFileSystem = g_pFullFileSystem = pFileSystem;
	
	::LoadBSPFile( filename );
}

void CBSPPack::WriteBSPFile( char *filename )
{
	::WriteBSPFile( filename );
}

void CBSPPack::ClearPackFile( void )
{
	::ClearPakFile( GetPakFile() );
}

void CBSPPack::AddFileToPack( const char *relativename, const char *fullpath )
{
	// Compressing at this point would work, but the usual usage is creating a BSP and using RepackBSP() to apply lump
	// and pack compression as a final pass
	::AddFileToPak( GetPakFile(), relativename, fullpath, IZip::eCompressionType_None );
}

void CBSPPack::AddBufferToPack( const char *relativename, void *data, int length, bool bTextMode )
{
	// Compressing at this point would work, but the usual usage is creating a BSP and using RepackBSP() to apply lump
	// and pack compression as a final pass
	::AddBufferToPak( GetPakFile(), relativename, data, length, bTextMode, IZip::eCompressionType_None );
}

void CBSPPack::SetHDRMode( bool bHDR )
{
	::SetHDRMode( bHDR );
}

bool CBSPPack::SwapBSPFile( 
IFileSystem			*pFileSystem, 
const char			*filename, 
const char			*swapFilename, 
bool				bSwapOnLoad, 
VTFConvertFunc_t	pVTFConvertFunc,
VHVFixupFunc_t		pVHVFixupFunc,
CompressFunc_t		pCompressFunc )
{
	// This is shady, but the engine is the only client here and we want the same search paths it has.
	g_pFileSystem = g_pFullFileSystem = pFileSystem;

	return ::SwapBSPFile( filename, swapFilename, bSwapOnLoad, pVTFConvertFunc, pVHVFixupFunc, pCompressFunc );
}

bool CBSPPack::RepackBSP( CUtlBuffer &inputBuffer, CUtlBuffer &outputBuffer, eRepackBSPFlags repackFlags )
{
	return ::RepackBSP( inputBuffer, outputBuffer,
						  ( repackFlags & eRepackBSP_CompressLumps ) ? RepackBSPCallback_LZMA : NULL,
						  ( repackFlags & eRepackBSP_CompressPackfile ) ? IZip::eCompressionType_LZMA : IZip::eCompressionType_None );
}

bool CBSPPack::GetPakFileLump( IFileSystem *pFileSystem, const char *pBSPFilename, void **pPakData, int *pPakSize )
{
	g_pFileSystem = g_pFullFileSystem = pFileSystem;

	return ::GetPakFileLump( pBSPFilename, pPakData, pPakSize );
}

bool CBSPPack::SetPakFileLump( IFileSystem *pFileSystem, const char *pBSPFilename, const char *pNewFilename, void *pPakData, int pakSize )
{
	g_pFileSystem = g_pFullFileSystem = pFileSystem;

	return ::SetPakFileLump( pBSPFilename, pNewFilename, pPakData, pakSize );
}

bool CBSPPack::GetBSPDependants( IFileSystem *pFileSystem, const char *pBSPFilename, CUtlVector< CUtlString > *pList )
{
	g_pFileSystem = g_pFullFileSystem = pFileSystem;

	return ::GetBSPDependants( pBSPFilename, pList );
}

EXPOSE_SINGLE_INTERFACE( CBSPPack, IBSPPack, IBSPPACK_VERSION_STRING );

