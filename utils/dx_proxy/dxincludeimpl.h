//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef DXINCLUDEIMPL_H
#define DXINCLUDEIMPL_H
#ifdef _WIN32
#pragma once
#endif

FileCache s_incFileCache;

struct DxIncludeImpl : public ID3DXInclude
{
	STDMETHOD(Open)(THIS_ D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
	{
		CachedFileData *pFileData = s_incFileCache.Get( pFileName );
		if ( !pFileData || !pFileData->IsValid() )
			return E_FAIL;
		
		*ppData = pFileData->GetDataPtr();
		*pBytes = pFileData->GetDataLen();

		pFileData->UpdateRefCount( +1 );

		return S_OK;
	}

	STDMETHOD(Open)(THIS_ D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData,
		LPCVOID *ppData, UINT *pBytes,
		/* OUT */ LPSTR pFullPath, DWORD cbFullPath)
	{
		if ( pFullPath && cbFullPath ) strncpy( pFullPath, pFileName, cbFullPath );
		return Open( IncludeType, pFileName, pParentData, ppData, pBytes );
	}
	
	STDMETHOD(Close)(THIS_ LPCVOID pData)
	{
		if ( CachedFileData *pFileData = CachedFileData::GetByDataPtr( pData ) )
			pFileData->UpdateRefCount( -1 );

		return S_OK;
	}
};

DxIncludeImpl s_incDxImpl;

#endif // #ifndef DXINCLUDEIMPL_H
