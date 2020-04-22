//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTEASSETPICKERPANEL_H
#define ATTRIBUTEASSETPICKERPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/AttributeBasePickerPanel.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CBaseAssetPickerFrame;


//-----------------------------------------------------------------------------
// CAttributeAssetPickerPanel
//-----------------------------------------------------------------------------
class CAttributeAssetPickerPanel : public CAttributeBasePickerPanel
{
	DECLARE_CLASS_SIMPLE( CAttributeAssetPickerPanel, CAttributeBasePickerPanel );

public:
	CAttributeAssetPickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );
	~CAttributeAssetPickerPanel();

private:
	virtual CBaseAssetPickerFrame *CreateAssetPickerFrame() = 0;
	MESSAGE_FUNC_PARAMS( OnAssetSelected, "AssetSelected", kv );
	virtual void ShowPickerDialog();
};


//-----------------------------------------------------------------------------
// Macro to quickly make new attribute types
//-----------------------------------------------------------------------------
#define DECLARE_ATTRIBUTE_ASSET_PICKER( _className )							\
	class _className : public CAttributeAssetPickerPanel						\
	{																			\
		DECLARE_CLASS_SIMPLE( _className, CAttributeAssetPickerPanel );			\
	public:																		\
		_className( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :	\
			BaseClass( parent, info ) {}										\
	private:																	\
		virtual CBaseAssetPickerFrame *CreateAssetPickerFrame();				\
	}

#define IMPLEMENT_ATTRIBUTE_ASSET_PICKER( _className, _popupTitle, _assetType, _assetExt, _assetSubDir, _assetTextType )	\
	CBaseAssetPickerFrame *_className::CreateAssetPickerFrame()																	\
	{																														\
 		return new CAssetPickerFrame( this, _popupTitle, _assetType, _assetExt, _assetSubDir, _assetTextType );				\
	}


//-----------------------------------------------------------------------------
// Macro to quickly make new attribute types
//-----------------------------------------------------------------------------
#define DECLARE_ATTRIBUTE_ASSET_PREVIEW_PICKER( _className )					\
	class _className : public CAttributeAssetPickerPanel						\
	{																			\
		DECLARE_CLASS_SIMPLE( _className, CAttributeAssetPickerPanel );			\
	public:																		\
		_className( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :	\
			BaseClass( parent, info ) {}										\
	private:																	\
		virtual CBaseAssetPickerFrame *CreateAssetPickerFrame();					\
	}

#define IMPLEMENT_ATTRIBUTE_ASSET_PREVIEW_PICKER( _className, _pickerClassName, _popupTitle )		\
	CBaseAssetPickerFrame *_className::CreateAssetPickerFrame()					\
	{																			\
 		return new _pickerClassName( this, _popupTitle );						\
	}


//-----------------------------------------------------------------------------
// Assets
//-----------------------------------------------------------------------------
DECLARE_ATTRIBUTE_ASSET_PICKER( CAttributeBspPickerPanel );
DECLARE_ATTRIBUTE_ASSET_PREVIEW_PICKER( CAttributeVmtPickerPanel );
DECLARE_ATTRIBUTE_ASSET_PREVIEW_PICKER( CAttributeVtfPickerPanel );


#endif // ATTRIBUTEASSETPICKERPANEL_H
