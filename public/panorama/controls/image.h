//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_IMAGEPANEL_H
#define PANORAMA_IMAGEPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"
#include "../data/iimagesource.h"

namespace panorama
{

DECLARE_PANEL_EVENT1( SetImageSource, const char * );
DECLARE_PANEL_EVENT0( ClearImageSource );
enum EImageScaling
{
	k_EImageScalingNone,
	k_EImageScalingStretchBoth,
	k_EImageScalingStretchX,
	k_EImageScalingStretchY,
	k_EImageScalingStretchBothToFitPreserveAspectRatio,
	k_EImageScalingStretchXToFitPreserveAspectRatio,
	k_EImageScalingStretchYToFitPreserveAspectRatio,
	k_EImageScalingStretchBothToCoverPreserveAspectRatio
};

enum EImageHorizontalAlignment
{
	k_EImageHorizontalAlignmentCenter,
	k_EImageHorizontalAlignmentLeft,
	k_EImageHorizontalAlignmentRight,
};

enum EImageVerticalAlignment
{
	k_EImageVerticalAlignmentCenter,
	k_EImageVerticalAlignmentTop,
	k_EImageVerticalAlignmentBottom,
};

//-----------------------------------------------------------------------------
// Purpose: ImagePanel
//-----------------------------------------------------------------------------
class CImagePanel : public CPanel2D
{
	DECLARE_PANEL2D( CImagePanel, CPanel2D );

public:
	CImagePanel( CPanel2D *parent, const char * pchPanelID );
	virtual ~CImagePanel();

	virtual void Paint();
	virtual bool BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties );

	bool OnImageLoaded( const CPanelPtr< IUIPanel > &pPanel, IImageSource *pImage );
	bool OnSetImageSource( const CPanelPtr<IUIPanel> &pPanel, const char *pchImageSource );
	bool OnClearImageSource( const CPanelPtr<IUIPanel> &pPanel );

	IImageSource *GetImage() { return m_pImage; }

	// Set an image from a URL (file://, http://), if pchDefaultImage is specified it must be a file:// url and will be
	// used while the actual image is loaded asynchronously, it will also remain in use if the actual image fails to load
	void SetImage( const char *pchImageURL, const char *pchDefaultImageURL = NULL, bool bPrioritizeLoad = false, int nResizeWidth = -1, int nResizeHeight = -1 );

	// Set an image from an already created IImageSource, you should almost always use the simpler SetImage( pchImageURL, pchDefaultImageURL ) call.
	void SetImage( IImageSource *pImage );

	void Clear();
	bool IsSet() { return (m_pImage != NULL); }

	void SetScaling( EImageScaling eScale );
	void SetScaling( CPanoramaSymbol symScale );
	void SetAlignment( EImageHorizontalAlignment horAlign, EImageVerticalAlignment verAlign );
	void SetVisibleImageSlice( int nX, int nY, int nWidth, int nHeight );

	virtual void  SetupJavascriptObjectTemplate() OVERRIDE;

	void GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties );

	virtual bool BRequiresContentClipLayer();

	void SetImageJS( const char *pchImageURL );

	virtual bool IsClonable() OVERRIDE { return AreChildrenClonable(); }
	virtual CPanel2D *Clone() OVERRIDE;

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

protected:
	virtual void OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions );

	virtual void InitClonedPanel( CPanel2D *pPanel ) OVERRIDE;
private:

	EImageScaling m_eScaling;
	EImageHorizontalAlignment m_eHorAlignment;
	EImageVerticalAlignment m_eVerAlignment;
	int m_nVisibleSliceX;
	int m_nVisibleSliceY;
	int m_nVisibleSliceWidth;
	int m_nVisibleSliceHeight;
	CUtlString m_strSource;
	CUtlString m_strSourceDefault;
	bool m_bAnimate;

	IImageSource *m_pImage;
	float m_flPrevAnimateWidth;
	float m_flPrevAnimateHeight;
};

} // namespace panorama

#endif // PANORAMA_IMAGEPANEL_H
