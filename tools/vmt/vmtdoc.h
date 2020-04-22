//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef VMTDOC_H
#define VMTDOC_H

#ifdef _WIN32
#pragma once
#endif


#include "dme_controls/inotifyui.h"
#include "datamodel/dmehandle.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "tier1/utlstring.h"


//-----------------------------------------------------------------------------
// Forward declarations 
//-----------------------------------------------------------------------------
class IVMTDocCallback;
class IShader;
enum ShaderParamType_t;
class IMaterial;
class IShader;


//-----------------------------------------------------------------------------
// Contains all editable state 
//-----------------------------------------------------------------------------
class CVMTDoc : public IDmNotify
{
public:
	CVMTDoc( IVMTDocCallback *pCallback );
	~CVMTDoc();

	// Inherited from INotifyUI
	virtual void NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags );

	// Sets/Gets the file name
	const char *GetFileName();
	void SetFileName( const char *pFileName );

	// Dirty bits (has it changed since the last time it was saved?)
	void	SetDirty( bool bDirty );
	bool	IsDirty() const;

	// Creates a new act busy list
	void	CreateNew();

	// Saves/loads from file
	bool	LoadFromFile( const char *pFileName );
	bool	SaveToFile( );

	// Returns the root object
	CDmElement *GetRootObject();

	// Called when data changes (see INotifyUI for flags)
	void	OnDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags );

	// Sets the shader in the material
	void	SetShader( const char *pShaderName );

	// Gets the preview material
	IMaterial *GetPreviewMaterial();

	// Sets shader parameters to the default for that shader
	void	SetParamsToDefault();

private:
	// Creates the root element
	bool CreateRootElement();

	// Add attribute for shader parameter
	CDmAttribute* AddAttributeForShaderParameter( CDmElement *pMaterial, const char *pParamName, ShaderParamType_t paramType );

	// Add a single shader parameter if it doesn't exist
	void AddNewShaderParam( CDmElement *pMaterial, const char *pParamName, ShaderParamType_t paramType, const char *pValue );

	// Add all shader parameters that don't currently exist
	void AddNewShaderParams( CDmElement *pMaterial, IShader *pShader );

	// Is this attribute a shader parameter?
	bool IsShaderParam( CDmAttribute* pAttribute );

	// Remove all shader parameters that don't exist in the new shader
	void RemoveUnusedShaderParams( CDmElement *pMaterial, IShader *pShader, IShader *pOldShader );

	// Remove all shader parameters 
	void RemoveAllShaderParams( CDmElement *pMaterial );

	// Finds a shader
	IShader *FindShader( const char *pShaderName );

	// Updates the preview material
	void UpdatePreviewMaterial();

	// Copies VMT parameters into the root
	void CopyParamsFromVMT( CDmElement *pVMT );

	// A couple methods to set param values from strings (OLD METHOD!)
	bool SetVMatrixParamValue( CDmAttribute *pAttribute, const char *pValue );
	bool SetVector2DParamValue( CDmAttribute *pAttribute, const char *pValue );
	bool SetVector3DParamValue( CDmAttribute *pAttribute, const char *pValue );
	bool SetVector4DParamValue( CDmAttribute *pAttribute, const char *pValue );
	bool SetColorParamValue( CDmAttribute *pAttribute, const char *pValue );

	// Sets an attribute value from the shader param default
	void SetAttributeValueFromDefault( CDmElement *pMaterial, CDmAttribute *pAttribute, const char *pValue );

	// Hooks the preview to an existing material, if there is one
	void SetupPreviewMaterial( );

	// Prior to saving to disk, extract all shader parameters which == the default
	CDmElement* ExtractDefaultParameters( );

	IVMTDocCallback *m_pCallback;
	CMaterialReference m_pScratchMaterial;
	CMaterialReference m_pPreviewMaterial;
	CDmeHandle< CDmElement > m_hRoot;
	CUtlString m_CurrentShader;
	IShader *m_pCurrentIShader;
	char m_pFileName[512];
	bool m_bDirty;
};


#endif // VMTDOC_H
