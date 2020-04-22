//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "client_pch.h"

#include <vgui_controls/Frame.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui_controls/BuildGroup.h>
#include "KeyValues.h"
#include <vgui_controls/Label.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/FileOpenDialog.h>
#include <vgui_controls/RadioButton.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/PanelListPanel.h>
#include <vgui_controls/ImageList.h>
#include <vgui/IInput.h>
#include "icolorcorrectiontools.h"
#include "vgui_baseui_interface.h"
#include "ivideomode.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "matsys_controls/curveeditorpanel.h"
#include "matsys_controls/proceduraltexturepanel.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "materialsystem/itexture.h"
#include "vtf/vtf.h"
#include "pixelwriter.h"
#include "UtlSortVector.h"
#include "filesystem_engine.h"
#include "gl_matsysiface.h"
#include "materialsystem/IColorCorrection.h"
#include "tier2/tier2.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

const int g_nPreviewImageWidth  = 128;
const int g_nPreviewImageHeight =  96;

ConVar mat_colorcorrection( "mat_colorcorrection", "0", FCVAR_ARCHIVE );
ConVar mat_colcorrection_disableentities( "mat_colcorrection_disableentities", "0" );

//-----------------------------------------------------------------------------
// CPrecisionSlider
// A drop-in replacement for the slider class that contains a text entry that
// can be used to read and set the current value.
// Also provides mousewheel support.   
//-----------------------------------------------------------------------------
class CPrecisionSlider : public vgui::Slider
{
	DECLARE_CLASS_SIMPLE( CPrecisionSlider, vgui::Slider );

public:
	CPrecisionSlider( Panel *parent, const char *panelName );
   ~CPrecisionSlider( );

	virtual void SetValue( int value, bool bTriggerChangeMessage = true );

	virtual void OnSizeChanged( int wide, int tall );

	virtual void GetTrackRect( int &x, int &y, int &w, int &h );

	virtual void SetEnabled( bool state );

protected:

	MESSAGE_FUNC_PARAMS( OnTextNewLine, "TextNewLine", data );

	virtual void OnMouseWheeled( int delta );

private:

	vgui::TextEntry	*m_pTextEntry;

	int				 m_nTextEntryWidth;
	int				 m_nSpacing;
};

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CPrecisionSlider::CPrecisionSlider( Panel *parent, const char *panelName ) : BaseClass( parent, panelName )
{
	m_pTextEntry = new vgui::TextEntry( this, "PrecisionEditPanel" );
	m_pTextEntry->SendNewLine( true );
	m_pTextEntry->SetCatchEnterKey( true );
	m_pTextEntry->AddActionSignalTarget( this );

	m_nTextEntryWidth = 32;
	m_nSpacing = 8;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CPrecisionSlider::~CPrecisionSlider( )
{
	delete m_pTextEntry;
}

//-----------------------------------------------------------------------------
// Override OnSizeChanged to update text entry size as well
//-----------------------------------------------------------------------------
void CPrecisionSlider::OnSizeChanged( int wide, int tall )
{
	int nSliderWidth, nSliderHeight;
	int nEditWidth, nEditHeight;

	nEditWidth = m_nTextEntryWidth;

	nSliderHeight = tall;
	nEditHeight = tall - 12;
	nSliderWidth  = wide - (m_nSpacing + nEditWidth);
    
	m_pTextEntry->SetBounds( nSliderWidth + m_nSpacing, 0, nEditWidth, nEditHeight );

	BaseClass::OnSizeChanged( wide, tall );
}

//-----------------------------------------------------------------------------
// Override GetTrackRect in order to adjust for the text entry 
//-----------------------------------------------------------------------------
void CPrecisionSlider::GetTrackRect( int &x, int &y, int &w, int &h )
{
	int wide, tall;
	GetPaintSize( wide, tall );

	x = 0;
	y = 8;
	w = wide - ( _nobSize + m_nTextEntryWidth + m_nSpacing );
	h = 4;
}

//-----------------------------------------------------------------------------
// Override SetValue to update the text entry data
//-----------------------------------------------------------------------------
void CPrecisionSlider::SetValue( int value, bool bTriggerChangeMessage )
{
	BaseClass::SetValue( value, bTriggerChangeMessage );

	char szValueString[256];
	sprintf( szValueString, "%d", _value );
	m_pTextEntry->SetText( szValueString );
}

//-----------------------------------------------------------------------------
// Override SetEnabled to also effect the text entry field
//-----------------------------------------------------------------------------
void CPrecisionSlider::SetEnabled( bool state )
{
	BaseClass::SetEnabled( state );
	m_pTextEntry->SetEnabled( state );
}
 
//-----------------------------------------------------------------------------
// Handle updates from the text entry field
//-----------------------------------------------------------------------------
void CPrecisionSlider::OnTextNewLine( KeyValues *data )
{
	char buf[256];
	m_pTextEntry->GetText( buf, 256 );

	int value;
	sscanf( buf, "%d", &value );

	SetValue( value );
}

//-----------------------------------------------------------------------------
// Handle mousewheel updates
//-----------------------------------------------------------------------------
void CPrecisionSlider::OnMouseWheeled( int delta )
{
	BaseClass::OnMouseWheeled( delta );

	if( IsEnabled() )
	{
		int value = GetValue();

		if( input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL ) )
			SetValue( value + delta*4 );
		else
			SetValue( value + delta );
	}
}



enum
{
	IMAGE_BUFFER_MAX_DIM = 128
};

class CColorCorrectionUIPanel;

// If you add a tool, add it to the string list and instance the panel in the constructor below
enum ColorCorrectionTool_t
{
	CC_TOOL_NONE = 0,
	CC_TOOL_CURVES,
	CC_TOOL_LEVELS,
	CC_TOOL_SELECTED_HSV,
	CC_TOOL_LOOKUP,
	CC_TOOL_BALANCE,

	CC_TOOL_COUNT,

	DEFAULT_CC_TOOL = CC_TOOL_NONE,
};

static const char *s_pColorCorrectionToolNames[CC_TOOL_COUNT] = 
{
	"No Tool Active",
	"Curves Tool",
	"Levels Tool",
	"Selected HSV Tool",
	"Lookup Tool",
	"Color Balance Tool",
};


//-----------------------------------------------------------------------------
// Converts RGB to normalized
//-----------------------------------------------------------------------------
static void Color24ToVector( color24 inColor, Vector *pOutVector )
{
	pOutVector->Init( inColor.r / 255.0f, inColor.g / 255.0f, inColor.b / 255.0f ); 
}

static void VectorToColor24( const Vector &inVector, color24 &outColor )
{
	int r = (int)((inVector.x * 255.0f) + 0.5f);
	int g = (int)((inVector.y * 255.0f) + 0.5f);
	int b = (int)((inVector.z * 255.0f) + 0.5f);
	outColor.r = clamp( r, 0, 255 );
	outColor.g = clamp( g, 0, 255 );
	outColor.b = clamp( b, 0, 255 );
}


//-----------------------------------------------------------------------------
// Convert HSV to RGB
//-----------------------------------------------------------------------------
float HueToRGB( float v1, float v2, float vH )
{
	float fResult = v1;

	vH = vH / 360.0f;

	vH = fmod (vH + 1.0f, 1.0f);

	if ( ( vH * 6.0f ) < 1.0f )
	{
		fResult = ( v1 + ( v2 - v1 ) * 6.0f * vH );
	}
	else if ( ( vH * 2.0f ) < 1.0f )
	{
		fResult = ( v2 );
	}
	else if ( ( vH * 3.0f ) < 2.0f )
	{
		fResult = ( v1 + ( v2 - v1 ) * ( ( 2.0f / 3.0f ) - vH ) * 6.0f );
	}

	return fResult;
}


//-----------------------------------------------------------------------------
// Computes the point on the spline whose x value == flInColor
//-----------------------------------------------------------------------------
static void ComputeSplinePoint( float flInColor, Vector *pControlPoints[4], Vector &vecOut )
{
	if ( pControlPoints[2]->x == pControlPoints[1]->x )
	{
		VectorAdd( *pControlPoints[1], *pControlPoints[2], vecOut );
		vecOut *= 0.5f;
		return;
	}

	int nIterCount = 0;

	float flStart = 0.0f;
	float flEnd = 1.0f;
	float flMid = ( flInColor - pControlPoints[1]->x ) / ( pControlPoints[2]->x - pControlPoints[1]->x ); 
	while( true )
	{
		Catmull_Rom_Spline(	*pControlPoints[0], *pControlPoints[1], *pControlPoints[2], *pControlPoints[3], flMid, vecOut );
		if ( fabs( vecOut.x - flInColor ) < 1e-5 )
			return;

		if ( flInColor < vecOut.x )
		{
			flEnd = flMid;
		}
		else
		{
			flStart = flMid;
		}
		
		flMid = (flStart + flEnd) * 0.5f;
		++nIterCount;
	}
}


//-----------------------------------------------------------------------------
// A color operation
//-----------------------------------------------------------------------------
abstract_class IColorOperation
{
public:
	// RGB are in 0-1 space here
	virtual void Apply( const Vector &inRGB, Vector &outRGB ) = 0;

	// Causes the operation to be deleted
	virtual void Release() = 0;

	virtual const char *GetName() = 0;
	virtual void SetName( const char *pName ) = 0;

	virtual IColorOperation *Clone() = 0;

	virtual ColorCorrectionTool_t ToolID() = 0;

	virtual bool IsEnabled() = 0;
	virtual void SetEnabled( bool bEnable ) = 0;

	virtual void SetBlendFactor( float flBlendFactor ) = 0;
	virtual float GetBlendFactor( ) = 0;
};


//-----------------------------------------------------------------------------
// List of color operations
//-----------------------------------------------------------------------------
class CColorOperationList
{
public:
	CColorOperationList();

	// Clears the list
	void Clear();

	// Adds an operation
	void AddOperation( IColorOperation *pOp );

	// Deletes the operation at the specified index
	void DeleteOperation( int opIndex );

	// Applys all operations in the list to the color
	void Apply( color24 in, color24 &out, IColorOperation *pFinalOp=NULL );

	// Queries for the number of operations in the list
	int	GetNumOperations( );

	// Returns the operation at the specified index in the list
	IColorOperation *GetOperation( int opIndex );

	// Move the item at the specified index in the list towards the front
	void BringForward( int opIndex );

	// Move the item at the specified index in the list towards the back
	void PushBack( int opIndex );

private:
	CUtlVector< IColorOperation* > m_OpList;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CColorOperationList::CColorOperationList()
{
}


//-----------------------------------------------------------------------------
// Clears the list
//-----------------------------------------------------------------------------
void CColorOperationList::Clear()
{
	for ( int i = m_OpList.Count(); --i >= 0; )
	{
		m_OpList[i]->Release();
	}
	m_OpList.RemoveAll();
}


//-----------------------------------------------------------------------------
// Adds an operation
//-----------------------------------------------------------------------------
void CColorOperationList::AddOperation( IColorOperation *pOp )
{
	m_OpList.AddToTail( pOp );
}


//-----------------------------------------------------------------------------
// Deletes an operation
//-----------------------------------------------------------------------------
void CColorOperationList::DeleteOperation( int opIndex )
{
	if( !m_OpList.IsValidIndex( opIndex ) )
		return;

	m_OpList.Remove( opIndex );
}

//-----------------------------------------------------------------------------
// Applys all operations in the list to the color
//-----------------------------------------------------------------------------
void CColorOperationList::Apply( color24 in, color24 &out, IColorOperation *pFinalOp )
{
	int nCount = m_OpList.Count();
	if ( nCount == 0 )
	{
		out = in;
		return;
	}

	Vector rgb;
	Color24ToVector( in, &rgb );

	for ( int i = 0; i < nCount && m_OpList[i] != pFinalOp ; ++i )
	{
		Vector temp;
		m_OpList[i]->Apply( rgb, temp );
		rgb = temp;
	}

	VectorToColor24( rgb, out );
}

//-----------------------------------------------------------------------------
// Queries for the number of operations in the list
//-----------------------------------------------------------------------------
int	CColorOperationList::GetNumOperations( )
{
	return m_OpList.Count();
}

//-----------------------------------------------------------------------------
// Returns the operation at the specified index in the list
//-----------------------------------------------------------------------------
IColorOperation *CColorOperationList::GetOperation( int opIndex )
{
	if( !m_OpList.IsValidIndex( opIndex ) )
		return NULL;

	return m_OpList.Element( opIndex );
}


void CColorOperationList::BringForward( int opIndex )
{
	if( !m_OpList.IsValidIndex( opIndex ) || opIndex==0 )
		return;

	IColorOperation *pOp = m_OpList[ opIndex ];
	m_OpList.Remove( opIndex );
	m_OpList.InsertBefore( opIndex-1, pOp ); 
}


void CColorOperationList::PushBack( int opIndex )
{
	if( !m_OpList.IsValidIndex( opIndex ) || opIndex==m_OpList.Count()-1 )
		return;

	IColorOperation *pOp = m_OpList[ opIndex ];
	m_OpList.Remove( opIndex );
	m_OpList.InsertAfter( opIndex, pOp ); 
}

//-----------------------------------------------------------------------------
// Base class for all color correction tool panels 
//-----------------------------------------------------------------------------
class CColorCorrectionUIChildPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CColorCorrectionUIChildPanel, vgui::Frame );

public:
	CColorCorrectionUIChildPanel( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
	{
	}

   ~CColorCorrectionUIChildPanel()
	{
	}

	virtual void OnClose()
	{
		KeyValues *msg = new KeyValues( "OpPanelClose" );
		msg->SetPtr( "panel", this );
		PostMessage( GetParent(), msg );
	}

	virtual void Init()     {}
	virtual void Shutdown() {}

	virtual IColorOperation *GetOperation() { return 0; }

	virtual void OnKeyCodeTyped( KeyCode code ) 
	{
		if( code==KEY_ESCAPE )
		{
			void ShowHideColorCorrectionUI();
			ShowHideColorCorrectionUI();
		}
	}

	virtual void ReadUncorrectedImage( Rect_t *pSrcRect, unsigned char *pPreviewImage ) {}
};

//-----------------------------------------------------------------------------
// Sort function for ControlPoints
//-----------------------------------------------------------------------------
class CCurvesLessFunc
{
public:
	bool Less( const Vector& src1, const Vector& src2, void *pCtx )
	{
		return src1.x < src2.x;
	}
};

//-----------------------------------------------------------------------------
// Similar to the 'curves...' operation from Photoshop
//-----------------------------------------------------------------------------
class CCurvesColorOperation : public IColorOperation
{
public:
	enum Channel_t
	{
		RED_CHANNEL = 0x1,
		GREEN_CHANNEL = 0x2,
		BLUE_CHANNEL = 0x4,
		ALL_CHANNELS = RED_CHANNEL | GREEN_CHANNEL | BLUE_CHANNEL,
	};

	CCurvesColorOperation();

	// Methods of IColorOperation
	virtual void Apply( const Vector &inRGB, Vector &outRGB );
	virtual void Release() { delete this; }

	virtual const char *GetName()			  { return m_pName; }
	virtual void SetName( const char *pName ) { V_strcpy_safe( m_pName, pName ); }

	virtual IColorOperation *Clone();

	virtual ColorCorrectionTool_t ToolID() { return CC_TOOL_CURVES; }

	virtual bool IsEnabled( ) { return m_bEnable; }
	virtual void SetEnabled( bool bEnable ) { m_bEnable = bEnable; }

	// Controls which channels to modify (see Channel_t)
	void SetChannelMask( int nMask );

	// Controls how much this op should take effect (1 = use 100% converted color, 0 = use 100% input color)
	virtual void SetBlendFactor( float flBlend );
	virtual float GetBlendFactor( ) { return m_flBlendFactor; }

	// Compute corrected color
	float ComputeCorrectedColor( float flInColor );

	// Finds or adds a control point
	int FindControlPoint( float flInValue, float flTolerance );

	// Finds or adds a control point
	int FindOrAddControlPoint( float flInValue, float flTolerance, float flOutValue );

	// Modifies a control point
	int ModifyControlPoint( int nPoint, float flInValue, float flOutValue );

	// Removes a control point. Points 0 and Last can't be removed 
	void RemoveControlPoint( int nPoint );

	// Iterates the control points
	int ControlPointCount() const;
	void GetControlPoint( int nPoint, float *pInValue, float *pOutValue );

private:
	// Computes actual corrected color (expensive!!)
	float ComputeActualCorrectedColor( float flInColor );

	// Update the outvalue array
	void UpdateOutColorArray();

	// This is an optimization to avoid a costly lookup
	float m_pOutValue[256];

	// Note: The x component of the control points is the in color
	// and the y component of the control points is the out color
	// z is unused; we use 3d vectors because the mathlib catmull rom
	// spline stuff uses them.
	int m_nChannelMask;

	//-----------------------------------------------------------------------------
	// Sort function for ControlPoints
	//-----------------------------------------------------------------------------
	class CurvesLessFunc
	{
	public:
		bool Less( const Vector& src1, const Vector& src2, void *pCtx )
		{
			return src1.x < src2.x;
		}
	};

	CUtlSortVector< Vector, CurvesLessFunc > m_ControlPoints;
	float m_flBlendFactor;

	char	m_pName[256];

	bool	m_bEnable;
};



//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CCurvesColorOperation::CCurvesColorOperation() : m_ControlPoints()
{
	Vector startpt, endpt;
	startpt.Init( 0, 0, 0 );
	endpt.Init( 1, 1, 0 );
	m_ControlPoints.Insert( startpt );
	m_ControlPoints.Insert( endpt );
	m_flBlendFactor = 1.0f;
	m_nChannelMask = ALL_CHANNELS;
	m_bEnable = true;
	UpdateOutColorArray();

	V_strcpy_safe( m_pName, "Curves" );
}


//-----------------------------------------------------------------------------
// Controls which channels to modify (see Channel_t)
//-----------------------------------------------------------------------------
void CCurvesColorOperation::SetChannelMask( int nMask )
{
	m_nChannelMask = nMask;
	colorcorrectiontools->UpdateColorCorrection();
}


//-----------------------------------------------------------------------------
// Controls how much this op should take effect (1 = use 100% converted color, 0 = use 100% input color)
//-----------------------------------------------------------------------------
void CCurvesColorOperation::SetBlendFactor( float flBlend )
{
	m_flBlendFactor = flBlend;
	colorcorrectiontools->UpdateColorCorrection();
}


//-----------------------------------------------------------------------------
// Iterates the control points
//-----------------------------------------------------------------------------
int CCurvesColorOperation::ControlPointCount() const
{
	return m_ControlPoints.Count();
}

void CCurvesColorOperation::GetControlPoint( int nPoint, float *pInValue, float *pOutValue )
{
	*pInValue = m_ControlPoints[nPoint].x;
	*pOutValue = m_ControlPoints[nPoint].y;
}


//-----------------------------------------------------------------------------
// Finds or adds a control point
//-----------------------------------------------------------------------------
int CCurvesColorOperation::FindControlPoint( float flInValue, float flTolerance )
{
	for ( int i = m_ControlPoints.Count(); --i >= 0; )
	{
		if ( fabs( m_ControlPoints[i].x	- flInValue ) < flTolerance )
			return i;
	}
	return -1;
}

	
//-----------------------------------------------------------------------------
// Finds or adds a control point
//-----------------------------------------------------------------------------
int CCurvesColorOperation::FindOrAddControlPoint( float flInValue, float flTolerance, float flOutValue )
{
	int nPoint = FindControlPoint( flInValue, flTolerance );
	if ( nPoint != -1 )
		return nPoint;

	Vector insert( flInValue, flOutValue, 0.0f );
	m_ControlPoints.Insert( insert );
	int n = m_ControlPoints.Find( insert );
	UpdateOutColorArray();
	colorcorrectiontools->UpdateColorCorrection();
	return n;
}


//-----------------------------------------------------------------------------
// Modifies a control point
//-----------------------------------------------------------------------------
int CCurvesColorOperation::ModifyControlPoint( int nPoint, float flInValue, float flOutValue )
{
	Assert( ( nPoint >= 0 ) && ( nPoint < m_ControlPoints.Count() ) );
	Vector temp = m_ControlPoints[nPoint];
	m_ControlPoints.Remove( nPoint );
	temp.x = flInValue;
	temp.y = flOutValue;
	m_ControlPoints.Insert( temp );
	int nIndex = m_ControlPoints.Find( temp );
	UpdateOutColorArray();
	colorcorrectiontools->UpdateColorCorrection();
	return nIndex;
}


//-----------------------------------------------------------------------------
// Removes a control point. Points 0 and Last can't be removed 
//-----------------------------------------------------------------------------
void CCurvesColorOperation::RemoveControlPoint( int nPoint )
{
	Assert( ( nPoint >= 0 ) && ( nPoint < m_ControlPoints.Count() ) );
	if ( ( nPoint == 0 ) || ( nPoint == m_ControlPoints.Count() - 1 ) )
		return;

	m_ControlPoints.Remove( nPoint );
	UpdateOutColorArray();
	colorcorrectiontools->UpdateColorCorrection();
}


//-----------------------------------------------------------------------------
// Computes actual corrected color (expensive!!)
//-----------------------------------------------------------------------------
float CCurvesColorOperation::ComputeActualCorrectedColor( float flInColor )
{
	flInColor = clamp( flInColor, 0.0f, 1.0f );

	// First find the control points we are between
	Vector find( flInColor, 0, 0 );
	int i = m_ControlPoints.FindLessOrEqual( find );
	if ( i < 0 )
		return m_ControlPoints[0].y;

	int nCount = m_ControlPoints.Count(); 
	if ( i == (nCount - 1) )
		return m_ControlPoints[nCount - 1].y;

	Vector *pControlPoints[4];
	pControlPoints[0] = (i >= 1) ? &m_ControlPoints[i-1] : &m_ControlPoints[0];
	pControlPoints[1] = &m_ControlPoints[i];
	pControlPoints[2] = &m_ControlPoints[i+1];
	pControlPoints[3] = (i + 2 < nCount) ? &m_ControlPoints[i+2] : &m_ControlPoints[nCount-1];

	Vector vecOut;
	ComputeSplinePoint( flInColor, pControlPoints, vecOut );
	AssertFloatEquals( vecOut.x, flInColor, 1e-3 );
	return vecOut.y;
}


//-----------------------------------------------------------------------------
// Update the outvalue array
//-----------------------------------------------------------------------------
void CCurvesColorOperation::UpdateOutColorArray()
{
	for ( int i = 0; i < 256; ++i )
	{
		m_pOutValue[i] = ComputeActualCorrectedColor( (float)i / 255.0f );
	}
}



//-----------------------------------------------------------------------------
// Compute corrected color
//-----------------------------------------------------------------------------
float CCurvesColorOperation::ComputeCorrectedColor( float flInColor )
{
	flInColor *= 255.0f;
	int i = (int)flInColor;
	i = clamp( i, 0, 255 );
	if ( i == 255 )
		return m_pOutValue[i];

	float f = flInColor - i;
	return Lerp( f, m_pOutValue[i], m_pOutValue[i+1] );
}


//-----------------------------------------------------------------------------
// Apply curves
//-----------------------------------------------------------------------------
void CCurvesColorOperation::Apply( const Vector &inRGB, Vector &outRGB )
{
	if( !m_bEnable )
	{
		outRGB = inRGB;
		return;
	}

	if ( m_nChannelMask & RED_CHANNEL )
	{
		outRGB.x = ComputeCorrectedColor( inRGB.x );
	}
	else
	{
		outRGB.x = inRGB.x;
	}

	if ( m_nChannelMask & GREEN_CHANNEL )
	{
		outRGB.y = ComputeCorrectedColor( inRGB.y );
	}
	else
	{
		outRGB.y = inRGB.y;
	}

	if ( m_nChannelMask & BLUE_CHANNEL )
	{
		outRGB.z = ComputeCorrectedColor( inRGB.z );
	}
	else
	{
		outRGB.z = inRGB.z;
	}

	VectorLerp( inRGB, outRGB, m_flBlendFactor, outRGB ); 
}


IColorOperation *CCurvesColorOperation::Clone( )
{
	CCurvesColorOperation *pClone = new CCurvesColorOperation();

	Q_memcpy( pClone->m_pOutValue, m_pOutValue, sizeof(float)*256 );
	pClone->m_nChannelMask = m_nChannelMask;
	pClone->m_ControlPoints = m_ControlPoints;
	pClone->m_flBlendFactor = m_flBlendFactor;
	Q_memcpy( pClone->m_pName, m_pName, sizeof(char)*256 );
	pClone->m_bEnable = m_bEnable;

	return pClone;
}

//-----------------------------------------------------------------------------
// Panel that displays + edits color correction spline curves
//-----------------------------------------------------------------------------
class CColorCurvesEditPanel : public CCurveEditorPanel
{
	DECLARE_CLASS_SIMPLE( CColorCurvesEditPanel, CCurveEditorPanel );

public:
	// constructor
	CColorCurvesEditPanel( vgui::Panel *pParent, const char *pName );
	~CColorCurvesEditPanel();

	// Sets the color curves operation to edit
	void SetCurvesOp( CCurvesColorOperation *pCurvesOp );

protected:
	// Control points + values...
	virtual int FindOrAddControlPoint( float flIn, float flTolerance, float flOut );
	virtual int FindControlPoint( float flIn, float flTolerance );
	virtual int ModifyControlPoint( int nPoint, float flIn, float flOut );
	virtual void RemoveControlPoint( int nPoint );
	virtual float GetValue( float flIn );
	virtual int ControlPointCount();
	virtual void GetControlPoint( int nPoint, float *pIn, float *pOut );

private:
	CCurvesColorOperation *m_pCurvesOp;
};


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CColorCurvesEditPanel::CColorCurvesEditPanel( vgui::Panel *pParent, const char *pName ) : BaseClass( pParent, pName )
{
	m_pCurvesOp = NULL;
	SetVisible( false );
}

CColorCurvesEditPanel::~CColorCurvesEditPanel()
{
}


//-----------------------------------------------------------------------------
// Control points + values...
//-----------------------------------------------------------------------------
int CColorCurvesEditPanel::FindOrAddControlPoint( float flIn, float flTolerance, float flOut )
{
	Assert( m_pCurvesOp );
	return m_pCurvesOp->FindOrAddControlPoint( flIn, flTolerance, flOut );
}

int CColorCurvesEditPanel::FindControlPoint( float flIn, float flTolerance )
{
	Assert( m_pCurvesOp );
	return m_pCurvesOp->FindControlPoint( flIn, flTolerance );
}

int CColorCurvesEditPanel::ModifyControlPoint( int nPoint, float flIn, float flOut )
{
	Assert( m_pCurvesOp );
	m_pCurvesOp->ModifyControlPoint( nPoint, flIn, flOut );
	return nPoint;
}

void CColorCurvesEditPanel::RemoveControlPoint( int nPoint )
{
	Assert( m_pCurvesOp );
	m_pCurvesOp->RemoveControlPoint( nPoint );
}

float CColorCurvesEditPanel::GetValue( float flIn )
{
	Assert( m_pCurvesOp );
	return m_pCurvesOp->ComputeCorrectedColor( flIn );
}

int CColorCurvesEditPanel::ControlPointCount()
{
	Assert( m_pCurvesOp );
	return m_pCurvesOp->ControlPointCount( );
}

void CColorCurvesEditPanel::GetControlPoint( int nPoint, float *pIn, float *pOut )
{
	Assert( m_pCurvesOp );
	m_pCurvesOp->GetControlPoint( nPoint, pIn, pOut );
}


//-----------------------------------------------------------------------------
// Sets the color curves operation to edit
//-----------------------------------------------------------------------------
void CColorCurvesEditPanel::SetCurvesOp( CCurvesColorOperation *pCurvesOp )
{
	m_pCurvesOp = pCurvesOp;
	SetVisible( m_pCurvesOp != NULL );
}


//-----------------------------------------------------------------------------
// Root panel for editing color curves
//-----------------------------------------------------------------------------
class CColorCurvesUIPanel : public CColorCorrectionUIChildPanel
{
	DECLARE_CLASS_SIMPLE( CColorCurvesUIPanel, CColorCorrectionUIChildPanel );

public:
	// constructor
	CColorCurvesUIPanel( vgui::Panel *pParent, CCurvesColorOperation *pOp );
	~CColorCurvesUIPanel();

	virtual void Init() {}
	virtual void Shutdown() {}

	virtual void ReadUncorrectedImage( Rect_t *pSrcRect, unsigned char *pPreviewImage ) {};

	virtual IColorOperation *GetOperation( ) { return (IColorOperation*)m_pColorOp; }

	// Command issued
	virtual void OnMessage(const KeyValues *params,  vgui::VPANEL fromPanel);

	virtual void OnCommand( const char *command );

private:
	enum
	{
		COLOR_MASK_RGB = 0,
		COLOR_MASK_RED,
		COLOR_MASK_GREEN,
		COLOR_MASK_BLUE,

		COLOR_MASK_TYPE_COUNT
	};
    
	// The color mask was changed 
	void OnColorMaskSelected();
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );

	void ResetBlendFactorSlider();

	vgui::ComboBox *m_pColorMask;
	CPrecisionSlider *m_pBlendFactorSlider;
	CColorCurvesEditPanel *m_pCurveEditor;
	CCurvesColorOperation *m_pColorOp;

	static const char *s_pColorMaskLabel[COLOR_MASK_TYPE_COUNT]; 
};


const char *CColorCurvesUIPanel::s_pColorMaskLabel[CColorCurvesUIPanel::COLOR_MASK_TYPE_COUNT] = 
{
	"RGB",
	"Red",
	"Green",
	"Blue"
};


//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CColorCurvesUIPanel::CColorCurvesUIPanel( vgui::Panel *pParent, CCurvesColorOperation *pOp ) : BaseClass( pParent, "ColorCurvesUIPanel" )
{
	m_pColorMask = new ComboBox(this, "ColorMask", COLOR_MASK_TYPE_COUNT, false);
	int i;
	for ( i = 0; i < COLOR_MASK_TYPE_COUNT; i++ )
	{
		m_pColorMask->AddItem( s_pColorMaskLabel[i], NULL );
	}
	m_pColorMask->AddActionSignalTarget( this );
	m_pColorMask->ActivateItem( 0 );

	m_pBlendFactorSlider = new CPrecisionSlider( this, "BlendFactorSlider" );
	m_pBlendFactorSlider->SetRange( 0, 255 );
	m_pBlendFactorSlider->SetValue( 255 );
	m_pBlendFactorSlider->AddActionSignalTarget( this );

	m_pColorOp = pOp;
	m_pCurveEditor = new CColorCurvesEditPanel( this, "CurveEditor" );
	m_pCurveEditor->SetCurvesOp( m_pColorOp );

	LoadControlSettings("Resource\\ColorCurvesUIPanel.res");
}

CColorCurvesUIPanel::~CColorCurvesUIPanel()
{
	if( m_pCurveEditor )
		delete m_pCurveEditor;
}


//-----------------------------------------------------------------------------
// Command issued
//-----------------------------------------------------------------------------
void CColorCurvesUIPanel::OnMessage(const KeyValues *params,  vgui::VPANEL fromPanel)
{
	BaseClass::OnMessage( params, fromPanel );

	if ( !Q_stricmp( "SliderMoved", params->GetName() ) )
	{
		vgui::Panel *pPanel = reinterpret_cast<vgui::Panel *>( const_cast<KeyValues*>(params)->GetPtr("panel") );
		CPrecisionSlider *pSlider = dynamic_cast<CPrecisionSlider *>( pPanel );

		if ( pSlider == m_pBlendFactorSlider )
		{
			m_pColorOp->SetBlendFactor( m_pBlendFactorSlider->GetValue() / 255.0f );
		}

		PostMessage( GetParent(), new KeyValues( "command", "command", "BlendFactorUpdate" ) );
	}
}


void CColorCurvesUIPanel::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );

	if( !Q_stricmp( "BlendFactorUpdate", command ) )
	{
		ResetBlendFactorSlider( );
	}
}

void CColorCurvesUIPanel::ResetBlendFactorSlider()
{
	float flBlend;
	if( m_pColorOp )
		flBlend = m_pColorOp->GetBlendFactor();
	else
		flBlend = 0.0f;

	m_pBlendFactorSlider->SetValue( flBlend*255.0f );
}

//-----------------------------------------------------------------------------
// The color mask was changed 
//-----------------------------------------------------------------------------
void CColorCurvesUIPanel::OnColorMaskSelected()
{
	int nMask = m_pColorMask->GetActiveItem();
	switch( nMask )
	{
	case COLOR_MASK_RGB:
		m_pColorOp->SetChannelMask( CCurvesColorOperation::ALL_CHANNELS );
		break;

	case COLOR_MASK_RED:
		m_pColorOp->SetChannelMask( CCurvesColorOperation::RED_CHANNEL );
		break;

	case COLOR_MASK_GREEN:
		m_pColorOp->SetChannelMask( CCurvesColorOperation::GREEN_CHANNEL );
		break;

	case COLOR_MASK_BLUE:
		m_pColorOp->SetChannelMask( CCurvesColorOperation::BLUE_CHANNEL );
		break;
	}
}


//-----------------------------------------------------------------------------
// A combo box changed 
//-----------------------------------------------------------------------------
void CColorCurvesUIPanel::OnTextChanged( KeyValues *data )
{
	Panel *pPanel = reinterpret_cast<vgui::Panel *>( data->GetPtr("panel") );
	vgui::ComboBox *pBox = dynamic_cast<vgui::ComboBox *>( pPanel );

	if ( pBox == m_pColorMask ) 
	{
		OnColorMaskSelected();
		return;
	}
}

//-----------------------------------------------------------------------------
// Similar to the 'levels...' operation from Photoshop
//-----------------------------------------------------------------------------
class CLevelsColorOperation : public IColorOperation
{
public:
	enum Channel_t
	{
		RED_CHANNEL = 0x1,
		GREEN_CHANNEL = 0x2,
		BLUE_CHANNEL = 0x4,
		ALL_CHANNELS = RED_CHANNEL | GREEN_CHANNEL | BLUE_CHANNEL,
	};

	CLevelsColorOperation();

	// Methods of IColorOperation
	virtual void Apply( const Vector &inRGB, Vector &outRGB );
	virtual void Release() { delete this; }

	virtual const char *GetName()			  { return m_pName; }
	virtual void SetName( const char *pName ) { V_strcpy_safe( m_pName, pName ); }

	virtual ColorCorrectionTool_t ToolID() { return CC_TOOL_LEVELS; }

	virtual IColorOperation *Clone();

	virtual bool IsEnabled( ) { return m_bEnable; }
	virtual void SetEnabled( bool bEnable ) { m_bEnable = bEnable; }

	// Controls which channels to modify (see Channel_t)
	void SetChannelMask( int nMask );

	// Controls how much this op should take effect (1 = use 100% converted color, 0 = use 100% input color)
	virtual void SetBlendFactor( float flBlend );
	virtual float GetBlendFactor( ) { return m_flBlendFactor; }

	// Sets input levels
	void SetInputLevels( float flMinValue, float flMidValue, float flMaxValue );

	// Sets output levels
 	void SetOutputLevels( float flMinValue, float flMaxValue );

	// Used to set/get the list
	CColorOperationList *GetColorOpList()				{ return m_pOpList; }
	void SetColorOpList( CColorOperationList *pList )	{ m_pOpList = pList; }

private:
	// Computes normalized input level (expensive!!)
	float ComputeNormalizedInputLevel( float flInLevel );

	// Compute corrected level
	float ComputeCorrectedLevel( float flInLevel );

	// Update the outvalue array
	void UpdateOutputLevelArray();

	// This is an optimization to avoid a costly lookup
	float m_pOutValue[256];

	int m_nChannelMask;
	float m_flBlendFactor;

	float m_flMinInputLevel;
	float m_flMidInputLevel;
	float m_flMaxInputLevel;

	float m_flMinOutputLevel;
	float m_flMaxOutputLevel;

	bool  m_bEnable;

	char  m_pName[256];

	CColorOperationList *m_pOpList;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CLevelsColorOperation::CLevelsColorOperation()
{
	m_flMinInputLevel = 0.0f;
	m_flMidInputLevel = 0.5f;
	m_flMaxInputLevel = 1.0f;
	
	m_flMinOutputLevel = 0.0f;
	m_flMaxOutputLevel = 1.0f;

	m_flBlendFactor = 1.0f;
	m_nChannelMask = ALL_CHANNELS;

	m_bEnable = true;
	UpdateOutputLevelArray();

	V_strcpy_safe( m_pName, "Levels" );
}


//-----------------------------------------------------------------------------
// Controls which channels to modify (see Channel_t)
//-----------------------------------------------------------------------------
void CLevelsColorOperation::SetChannelMask( int nMask )
{
	m_nChannelMask = nMask;
	colorcorrectiontools->UpdateColorCorrection();
}


//-----------------------------------------------------------------------------
// Controls how much this op should take effect (1 = use 100% converted color, 0 = use 100% input color)
//-----------------------------------------------------------------------------
void CLevelsColorOperation::SetBlendFactor( float flBlend )
{
	m_flBlendFactor = flBlend;
	colorcorrectiontools->UpdateColorCorrection();
}


//-----------------------------------------------------------------------------
// Sets input levels
//-----------------------------------------------------------------------------
void CLevelsColorOperation::SetInputLevels( float flMinValue, float flMidValue, float flMaxValue )
{
	m_flMinInputLevel = clamp( flMinValue, 0.0f, 1.0f );
	m_flMidInputLevel = clamp( flMidValue, 0.0f, 1.0f );
	m_flMaxInputLevel = clamp( flMaxValue, 0.0f, 1.0f );
	UpdateOutputLevelArray();
	colorcorrectiontools->UpdateColorCorrection();
}


//-----------------------------------------------------------------------------
// Sets output levels
//-----------------------------------------------------------------------------
void CLevelsColorOperation::SetOutputLevels( float flMinValue, float flMaxValue )
{
	m_flMinOutputLevel = clamp( flMinValue, 0.0f, 1.0f );
	m_flMaxOutputLevel = clamp( flMaxValue, 0.0f, 1.0f );
	UpdateOutputLevelArray();
	colorcorrectiontools->UpdateColorCorrection();
}


//-----------------------------------------------------------------------------
// Computes actual corrected level (expensive!!)
//-----------------------------------------------------------------------------
float CLevelsColorOperation::ComputeNormalizedInputLevel( float flInLevel )
{
	if ( flInLevel <= m_flMinInputLevel )
		return 0.0f;

	if ( flInLevel >= m_flMaxInputLevel )
		return 1.0f;

	// We effectively have 3 control points; 1 at each end, and 1 in the middle
	// Duplicate the end which is 
	Vector controlPoints[4];
	controlPoints[0].Init( m_flMinInputLevel, 0.0f, 0.0f );
	controlPoints[3].Init( m_flMaxInputLevel, 1.0f, 0.0f );
	if ( flInLevel < m_flMidInputLevel )
	{
		controlPoints[1].Init( m_flMinInputLevel, 0.0f, 0.0f );
		controlPoints[2].Init( m_flMidInputLevel, 0.5f, 0.0f );
	}
	else
	{
		controlPoints[1].Init( m_flMidInputLevel, 0.5f, 0.0f );
		controlPoints[2].Init( m_flMaxInputLevel, 1.0f, 0.0f );
	}

	Vector *pControlPoints[4];
	pControlPoints[0] = &controlPoints[0];
	pControlPoints[1] = &controlPoints[1];
	pControlPoints[2] = &controlPoints[2];
	pControlPoints[3] = &controlPoints[3];

	Vector vecOut;
	ComputeSplinePoint( flInLevel, pControlPoints, vecOut );
	AssertFloatEquals( vecOut.x, flInLevel, 1e-5 );
	return vecOut.y;
}


//-----------------------------------------------------------------------------
// Update the outvalue array
//-----------------------------------------------------------------------------
void CLevelsColorOperation::UpdateOutputLevelArray()
{
	for ( int i = 0; i < 256; ++i )
	{
		m_pOutValue[i] = ComputeNormalizedInputLevel( (float)i / 255.0f );
		m_pOutValue[i] *= m_flMaxOutputLevel - m_flMinOutputLevel; 
		m_pOutValue[i] += m_flMinOutputLevel;
	}
}


//-----------------------------------------------------------------------------
// Compute corrected level
//-----------------------------------------------------------------------------
float CLevelsColorOperation::ComputeCorrectedLevel( float flInLevel )
{
	flInLevel *= 255.0f;
	int i = (int)flInLevel;
	i = clamp( i, 0, 255 );
	if ( i == 255 )
		return m_pOutValue[i];

	float f = flInLevel - i;
	return Lerp( f, m_pOutValue[i], m_pOutValue[i+1] );
}


//-----------------------------------------------------------------------------
// Apply curves
//-----------------------------------------------------------------------------
void CLevelsColorOperation::Apply( const Vector &inRGB, Vector &outRGB )
{
	if( !m_bEnable )
	{
		outRGB = inRGB;
		return;
	}

	if ( m_nChannelMask & RED_CHANNEL )
	{
		outRGB.x = ComputeCorrectedLevel( inRGB.x );
	}
	else
	{
		outRGB.x = inRGB.x;
	}

	if ( m_nChannelMask & GREEN_CHANNEL )
	{
		outRGB.y = ComputeCorrectedLevel( inRGB.y );
	}
	else
	{
		outRGB.y = inRGB.y;
	}

	if ( m_nChannelMask & BLUE_CHANNEL )
	{
		outRGB.z = ComputeCorrectedLevel( inRGB.z );
	}
	else
	{
		outRGB.z = inRGB.z;
	}

	VectorLerp( inRGB, outRGB, m_flBlendFactor, outRGB ); 
}


IColorOperation *CLevelsColorOperation::Clone( )
{
	CLevelsColorOperation *pClone = new CLevelsColorOperation;

	Q_memcpy( pClone->m_pOutValue, m_pOutValue, sizeof(float)*256.0f );

	pClone->m_nChannelMask = m_nChannelMask;
	pClone->m_flBlendFactor = m_flBlendFactor;

	pClone->m_flMinInputLevel = m_flMinInputLevel;
	pClone->m_flMidInputLevel = m_flMidInputLevel;
	pClone->m_flMaxInputLevel = m_flMaxInputLevel;

	pClone->m_flMinOutputLevel = m_flMinOutputLevel;
	pClone->m_flMaxOutputLevel = m_flMaxOutputLevel;

	pClone->m_bEnable = m_bEnable;

	pClone->m_pOpList = m_pOpList;

	Q_memcpy( pClone->m_pName, m_pName, sizeof(char)*256 );

	return pClone;
}

//-----------------------------------------------------------------------------
// Panel that displays a histogram of the color information
//-----------------------------------------------------------------------------
class CColorHistogramPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CColorHistogramPanel, vgui::Panel );

public:
	enum HistogramType_t
	{
		RED = 0,
		GREEN,
		BLUE,
		RGB,

		HISTOGRAM_TYPE_COUNT,
	};

	// constructor
	CColorHistogramPanel( vgui::Panel *pParent, const char *pName, CLevelsColorOperation *pOp );
	~CColorHistogramPanel();

	virtual void Paint( void );
	virtual void PaintBackground( void );

	void SetHistogramType( HistogramType_t type );
	void ComputeHistogram( Rect_t &srcRect, unsigned char *pBits, ImageFormat format, int nStride );

private:
	// Converts screen location to normalized color values   and back
	void ScreenToColor( int x, int y, float *pIn, float *pOut );
	void ColorToScreen( float flIn, float flOut, int *x, int *y );

	// The histogram of the screen image
	float m_pHistogram[256];
	HistogramType_t m_Type;

	CLevelsColorOperation *m_pOp;

	float m_flMax;
};


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CColorHistogramPanel::CColorHistogramPanel( vgui::Panel *pParent, const char *pName, CLevelsColorOperation *pOp ) : BaseClass( pParent, pName )
{	
	for ( int i = 0; i < 256; ++i )
	{
		m_pHistogram[i] = 0.0f;
	}
	m_Type = RGB;

	m_pOp = pOp;
}

CColorHistogramPanel::~CColorHistogramPanel()
{
}


//-----------------------------------------------------------------------------
// Computes histogram
//-----------------------------------------------------------------------------
void CColorHistogramPanel::SetHistogramType( HistogramType_t type )
{
	m_Type = type;
}

void CColorHistogramPanel::ComputeHistogram( Rect_t &srcRect, unsigned char *pBits, ImageFormat format, int nStride )
{
	for ( int i = 0; i < 256; ++i )
	{
		m_pHistogram[i] = 0.0f;
	}

	int nPixelCount = srcRect.width * srcRect.height;

	int nSizeInBytes = ImageLoader::SizeInBytes( format );
	CPixelWriter writer;
	writer.SetPixelMemory( format, pBits + srcRect.y * nStride + srcRect.x * nSizeInBytes, nStride );
	for ( int y = 0; y < srcRect.height; ++y )
	{
		writer.Seek( 0, y );
		for ( int x = 0; x < srcRect.width; ++x )
		{
			int r, g, b, a;
			writer.ReadPixelNoAdvance( r, g, b, a );

			color24 inColor, col;
			inColor.r = clamp( r, 0, 255 );
			inColor.g = clamp( g, 0, 255 );
			inColor.b = clamp( b, 0, 255 );

			m_pOp->GetColorOpList()->Apply( inColor, col, m_pOp );

			switch( m_Type )
			{
			case RED:
				++m_pHistogram[col.r];
				break;

			case GREEN:
				++m_pHistogram[col.g];
				break;

			case BLUE:
				++m_pHistogram[col.b];
				break;

			case RGB:
				{
					float flGreyScale = 0.299f * col.r + 0.587f * col.g + 0.114f * col.b;
					g = (int)(flGreyScale + 0.5f);
					g = clamp( g, 0, 255 );
					++m_pHistogram[g];
				}
				break;
			}

			writer.SkipBytes( nSizeInBytes );
		}
	}

	m_flMax = 0.0f;
	for ( int i = 0; i < 256; ++i )
	{
		m_pHistogram[i] /= (float)nPixelCount;
		if ( m_flMax < m_pHistogram[i] )
		{
			m_flMax = m_pHistogram[i];
		}
	}
}


//-----------------------------------------------------------------------------
// This paints the grid behind the curves
//-----------------------------------------------------------------------------
void CColorHistogramPanel::PaintBackground( void )
{
	int w, h;
	GetSize( w, h );

	vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
	vgui::surface()->DrawFilledRect( 0, 0, w, h );

	vgui::surface()->DrawSetColor( 128, 128, 128, 255 );
	vgui::surface()->DrawLine( 0, h/4, w, h/4 );
	vgui::surface()->DrawLine( 0, h/2, w, h/2 );
	vgui::surface()->DrawLine( 0, 3*h/4, w, 3*h/4 );

	vgui::surface()->DrawLine( w/4, 0, w/4, h );
	vgui::surface()->DrawLine( w/2, 0, w/2, h );
	vgui::surface()->DrawLine( 3*w/4, 0, 3*w/4, h );

	vgui::surface()->DrawSetColor( 0, 0, 0, 255 );
	vgui::surface()->DrawLine( 0, 0, w, 0 );
	vgui::surface()->DrawLine( w, 0, w, h );
	vgui::surface()->DrawLine( w, h, 0, h );
	vgui::surface()->DrawLine( 0, h, 0, 0 );
}


//-----------------------------------------------------------------------------
// Sets the color curves operation to edit
//-----------------------------------------------------------------------------
void CColorHistogramPanel::Paint( void )
{
	int w, h;
	GetSize( w, h );

	// FIXME: Add method to draw multiple lines DrawPolyLine connects the 1st and last points... bleah
	switch( m_Type )
	{
	case RED:
		vgui::surface()->DrawSetColor( 255, 0, 0, 255 );
		break;

	case GREEN:
		vgui::surface()->DrawSetColor( 0, 255, 0, 255 );
		break;

	case BLUE:
		vgui::surface()->DrawSetColor( 0, 0, 255, 255 );
		break;

	case RGB:
		vgui::surface()->DrawSetColor( 0, 0, 0, 255 );
		break;
	}

	float flOOMax = (m_flMax != 0.0f) ? 1.0f / m_flMax : 1.0f;
	for ( int i = 0; i < 256; ++i )
	{
		int x = (float)i * (w-1) / 255.0f;
		int y = (float)m_pHistogram[i] * (h-1) * flOOMax;
		vgui::surface()->DrawLine( x, h - 1, x, h - 1 - y );
	}
}


//-----------------------------------------------------------------------------
// A color slider panel used to control input + output levels
//-----------------------------------------------------------------------------
class CColorSlider : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CColorSlider, vgui::Panel );

public:
	// constructor
	CColorSlider( vgui::Panel *pParent, const char *pName, int nKnobCount );
	~CColorSlider();

	// Painting
	virtual void Paint();

 	void SetValue( int nKnobIndex, int value ); 
	void SetNormalizedValue( int nKnobIndex, float flValue );
	int  GetValue( int nKnobIndex );
	void SetRange( int min, int max );	 // set to max and min range of rows to display
	void GetRange( int &min, int &max );

	virtual void OnCursorMoved( int x,int y );
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseReleased( MouseCode code );

private:
	// Draws a single knob with a particular color
	void PaintKnob( float flPosition, unsigned char r, unsigned char g, unsigned char b );

	// Purpose: Send a message to interested parties when the slider moves
	void SendSliderMovedMessage( int nKnobIndex );

	// Update other sliders
	void UpdateOtherSliders( int nKnobChanged );

	int m_nKnobCount;
	float m_flKnobPosition[3];
	int m_nMinValue;
	int m_nMaxValue;

	int m_nWhiteMaterial;
	int m_nSelectedKnob;
};


//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CColorSlider::CColorSlider( vgui::Panel *pParent, const char *pName, int nKnobCount ) : BaseClass( pParent, pName )
{
	m_nKnobCount = nKnobCount;
	Assert( m_nKnobCount == 2 || m_nKnobCount == 3 );
	m_flKnobPosition[0] = 0.0f;
	m_flKnobPosition[1] = 1.0f;
	m_flKnobPosition[2] = 0.5f;
	m_nMinValue = 0;
	m_nMaxValue = 1;
	m_nSelectedKnob = -1;
	m_nWhiteMaterial = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile( m_nWhiteMaterial, "vgui/white" , true, false );
	SetMouseInputEnabled( true );
}

CColorSlider::~CColorSlider()
{
	if ( vgui::surface() && m_nWhiteMaterial != -1 )
	{
		vgui::surface()->DestroyTextureID( m_nWhiteMaterial );
		m_nWhiteMaterial = -1;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Send a message to interested parties when the slider moves
//-----------------------------------------------------------------------------
void CColorSlider::SendSliderMovedMessage( int nKnobIndex )
{	
	// send a changed message
	PostActionSignal( new KeyValues("SliderMoved", "knob", nKnobIndex) );
}


//-----------------------------------------------------------------------------
// Update other sliders
//-----------------------------------------------------------------------------
void CColorSlider::UpdateOtherSliders( int nKnobChanged )
{
	// Remember: 0 == low, 1 == high, 2 == middle!
	float flValue = m_flKnobPosition[ nKnobChanged ];
	switch (nKnobChanged)
	{
	case 0:
		if ( m_flKnobPosition[1] < flValue )
		{
			m_flKnobPosition[1] = flValue;
		}
		if ( m_flKnobPosition[2] < flValue )
		{
			m_flKnobPosition[2] = flValue;
		}
		break;

	case 1:
		if ( m_flKnobPosition[0] > flValue )
		{
			m_flKnobPosition[0] = flValue;
		}
		if ( m_flKnobPosition[2] > flValue )
		{
			m_flKnobPosition[2] = flValue;
		}
		break;

	case 2:
		if ( m_flKnobPosition[0] > flValue )
		{
			m_flKnobPosition[0] = flValue;
		}
		if ( m_flKnobPosition[1] < flValue )
		{
			m_flKnobPosition[1] = flValue;
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// Value getting + setting
//-----------------------------------------------------------------------------
void CColorSlider::SetNormalizedValue( int nKnobIndex, float flValue )
{
	Assert( m_nKnobCount > nKnobIndex );
	m_flKnobPosition[nKnobIndex] = clamp( flValue, 0.0f, 1.0f );
	UpdateOtherSliders( nKnobIndex );

	SendSliderMovedMessage( nKnobIndex );
}

void CColorSlider::SetValue( int nKnobIndex, int value )
{
	Assert( m_nKnobCount > nKnobIndex );
	SetNormalizedValue( nKnobIndex, (float)(value - m_nMinValue) / (m_nMaxValue - m_nMinValue) );
}

int CColorSlider::GetValue( int nKnobIndex )
{
	Assert( m_nKnobCount > nKnobIndex );
	return m_flKnobPosition[nKnobIndex] * (m_nMaxValue - m_nMinValue) + m_nMinValue;
}

void CColorSlider::SetRange( int minValue, int maxValue )
{
	Assert( maxValue > minValue );
	m_nMinValue = minValue;
	m_nMaxValue = maxValue;
}

void CColorSlider::GetRange( int &minValue, int &maxValue )
{
	minValue = m_nMinValue;
	maxValue = m_nMaxValue;
}


//-----------------------------------------------------------------------------
// Handle input
//-----------------------------------------------------------------------------
void CColorSlider::OnMousePressed( vgui::MouseCode code )
{
	BaseClass::OnMousePressed( code );

	int x, y;
	input()->GetCursorPos( x, y );
	ScreenToLocal( x, y );

	if ( code == MOUSE_LEFT )
	{
		input()->SetMouseCapture(GetVPanel());

		// Choose the closest knob
		int w, h;
		GetSize( w, h );

		float flNormalizedVal = (float)x / (w-1);
		m_nSelectedKnob = 0; 
		for ( int i = 1; i < m_nKnobCount; ++i )
		{
			if ( fabs(flNormalizedVal - m_flKnobPosition[i]) < fabs(flNormalizedVal - m_flKnobPosition[m_nSelectedKnob]) )
			{
				m_nSelectedKnob = i;
			}
		}

		SetNormalizedValue( m_nSelectedKnob, flNormalizedVal );
	}
}

void CColorSlider::OnMouseReleased( vgui::MouseCode code )
{
	BaseClass::OnMouseReleased( code );

	if ( code == MOUSE_LEFT )
	{
		if ( m_nSelectedKnob >= 0 )
		{
			input()->SetMouseCapture( NULL );
			m_nSelectedKnob = -1;
		}
	}
}

void CColorSlider::OnCursorMoved( int x, int y )
{
	BaseClass::OnCursorMoved( x, y );

	if ( m_nSelectedKnob >= 0 )
	{
		int w, h;
		GetSize( w, h );

		float flNormalizedVal = (float)x / (w-1);

		if( m_nSelectedKnob<2 && m_nKnobCount==3 )
		{
			// We need to adjust the grey knob, if active
			float fOldRelGrey = (m_flKnobPosition[2] - m_flKnobPosition[0]) / (m_flKnobPosition[1] - m_flKnobPosition[0]);
			
			SetNormalizedValue( m_nSelectedKnob, flNormalizedVal );
			SetNormalizedValue( 2, fOldRelGrey*(m_flKnobPosition[1]-m_flKnobPosition[0]) + m_flKnobPosition[0] );
		}
		else
		{
			SetNormalizedValue( m_nSelectedKnob, flNormalizedVal );
		}
	}
}


//-----------------------------------------------------------------------------
// Draws a single knob with a particular color
//-----------------------------------------------------------------------------
void CColorSlider::PaintKnob( float flPosition, unsigned char r, unsigned char g, unsigned char b )
{
	int w, h;
	GetSize( w, h );

	Vertex_t triangle[3];
	triangle[0].m_Position.x = flPosition * (w-1);
	triangle[0].m_Position.y = 0.0f;
	triangle[0].m_TexCoord.Init( 0.0f, 0.0f );

	triangle[1].m_Position.x = triangle[0].m_Position.x + (h-1);
	triangle[1].m_Position.y = (h-1);
	triangle[1].m_TexCoord.Init( 0.0f, 0.0f );

	triangle[2].m_Position.x = triangle[0].m_Position.x - (h-1);
	triangle[2].m_Position.y = (h-1);
 	triangle[2].m_TexCoord.Init( 0.0f, 0.0f );

	vgui::surface()->DrawSetColor( r, g, b, 255 );
	vgui::surface()->DrawSetTexture( m_nWhiteMaterial );
	vgui::surface()->DrawTexturedPolygon( 3, triangle );

	vgui::surface()->DrawSetColor( 0, 0, 0, 255 );
	vgui::surface()->DrawTexturedPolyLine( triangle, 3 );
}


//-----------------------------------------------------------------------------
// Painting
//-----------------------------------------------------------------------------
void CColorSlider::Paint()
{
	// Knob 0 is black, knob 1 is white, knob 2 is grey (if active)
	PaintKnob( m_flKnobPosition[0], 0, 0, 0 );

	if ( m_nKnobCount == 3 )
	{
		PaintKnob( m_flKnobPosition[2], 128, 128, 128 );
	}

	PaintKnob( m_flKnobPosition[1], 255, 255, 255 );
}


//-----------------------------------------------------------------------------
// Root panel for editing levels
//-----------------------------------------------------------------------------
class CColorLevelsUIPanel : public CColorCorrectionUIChildPanel
{
	DECLARE_CLASS_SIMPLE( CColorLevelsUIPanel, CColorCorrectionUIChildPanel );

public:
	// constructor
	CColorLevelsUIPanel( vgui::Panel *pParent, CLevelsColorOperation *pOp );
	~CColorLevelsUIPanel();

	// Command issued
	virtual void OnMessage(const KeyValues *params,  vgui::VPANEL fromPanel);

	virtual void OnCommand( const char *command );

	// Reads the uncorrected image + generates a hisogram 
	virtual void ReadUncorrectedImage( Rect_t *pSrcRect, unsigned char *pPreviewImage );

	virtual void Init() {}
	virtual void Shutdown() {}

	virtual IColorOperation *GetOperation( ) { return (IColorOperation*)m_pLevelsOp; }

private:
	enum
	{
		HISTOGRAM_IMAGE_SIZE = 256
	};

	enum
	{
		COLOR_MASK_RGB = 0,
		COLOR_MASK_RED,
		COLOR_MASK_GREEN,
		COLOR_MASK_BLUE,

		COLOR_MASK_TYPE_COUNT
	};

	// The color mask was changed 
	void OnColorMaskSelected();
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );

	void ResetBlendFactorSlider();

	vgui::ComboBox *m_pColorMask;
	CPrecisionSlider *m_pBlendFactorSlider;
	CColorHistogramPanel *m_pHistogramPanel;
	CLevelsColorOperation *m_pLevelsOp;
	CColorSlider *m_pInputLevelSlider;
	CColorSlider *m_pOutputLevelSlider;

	static const char *s_pColorMaskLabel[COLOR_MASK_TYPE_COUNT]; 
};


const char *CColorLevelsUIPanel::s_pColorMaskLabel[CColorLevelsUIPanel::COLOR_MASK_TYPE_COUNT] = 
{
	"RGB",
	"Red",
	"Green",
	"Blue"
};


//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CColorLevelsUIPanel::CColorLevelsUIPanel( vgui::Panel *pParent, CLevelsColorOperation *pOp ) : BaseClass( pParent, "LevelsUIPanel" )
{
	m_pColorMask = new ComboBox(this, "ColorMask", COLOR_MASK_TYPE_COUNT, false);
	int i;
	for ( i = 0; i < COLOR_MASK_TYPE_COUNT; i++ )
	{
		m_pColorMask->AddItem( s_pColorMaskLabel[i], NULL );
	}
	m_pColorMask->AddActionSignalTarget( this );
	m_pColorMask->ActivateItem( 0 );

	m_pBlendFactorSlider = new CPrecisionSlider( this, "BlendFactorSlider" );
	m_pBlendFactorSlider->SetRange( 0, 255 );
	m_pBlendFactorSlider->SetValue( 255 );
	m_pBlendFactorSlider->AddActionSignalTarget( this );

	m_pInputLevelSlider = new CColorSlider( this, "InputLevelSlider", 3 );
	m_pInputLevelSlider->SetRange( 0, 255 );
	m_pInputLevelSlider->AddActionSignalTarget( this );

	m_pOutputLevelSlider = new CColorSlider( this, "OutputLevelSlider", 2 );
	m_pOutputLevelSlider->SetRange( 0, 255 );
	m_pOutputLevelSlider->AddActionSignalTarget( this );

	m_pLevelsOp = new CLevelsColorOperation;
	m_pHistogramPanel = new CColorHistogramPanel( this, "Histogram", pOp );

	m_pLevelsOp = pOp;

	LoadControlSettings("Resource\\ColorLevelsUIPanel.res");

	ResetBlendFactorSlider();
}

CColorLevelsUIPanel::~CColorLevelsUIPanel()
{
}

	
//-----------------------------------------------------------------------------
// Reads the uncorrected image + generates a hisogram 
//-----------------------------------------------------------------------------
void CColorLevelsUIPanel::ReadUncorrectedImage( Rect_t *pSrcRect, unsigned char *pPreviewImage )
{
	Rect_t dstRect;
	dstRect.x = 0;
	dstRect.y = 0;
	dstRect.width  = g_nPreviewImageWidth;
	dstRect.height = g_nPreviewImageHeight;

	m_pHistogramPanel->ComputeHistogram( dstRect, pPreviewImage, IMAGE_FORMAT_BGRX8888, dstRect.width * 4 );
}


//-----------------------------------------------------------------------------
// Command issued
//-----------------------------------------------------------------------------
void CColorLevelsUIPanel::OnMessage(const KeyValues *params,  vgui::VPANEL fromPanel)
{
	BaseClass::OnMessage( params, fromPanel );

	if ( !Q_stricmp( "SliderMoved", params->GetName() ) )
	{
		vgui::Panel *pPanel = reinterpret_cast<vgui::Panel *>( const_cast<KeyValues*>(params)->GetPtr("panel") );
		if ( pPanel == m_pBlendFactorSlider )
		{
			m_pLevelsOp->SetBlendFactor( m_pBlendFactorSlider->GetValue() / 255.0f );

			PostMessage( GetParent(), new KeyValues( "command", "command", "BlendFactorUpdate" ) );

			return;
		}

		if ( pPanel == m_pInputLevelSlider )
		{
			m_pLevelsOp->SetInputLevels( 
				m_pInputLevelSlider->GetValue( 0 ) / 255.0f, 
				m_pInputLevelSlider->GetValue( 2 ) / 255.0f,
				m_pInputLevelSlider->GetValue( 1 ) / 255.0f );
			return;
		}

		if ( pPanel == m_pOutputLevelSlider )
		{
			m_pLevelsOp->SetOutputLevels(
				m_pOutputLevelSlider->GetValue( 0 ) / 255.0f,
				m_pOutputLevelSlider->GetValue( 1 ) / 255.0f );
			return;
		}
	}
}


void CColorLevelsUIPanel::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );

	if( !Q_stricmp( "BlendFactorUpdate", command ) )
	{
		ResetBlendFactorSlider( );
	}
}

void CColorLevelsUIPanel::ResetBlendFactorSlider()
{
	float flBlend;
	if( m_pLevelsOp )
		flBlend = m_pLevelsOp->GetBlendFactor();
	else
		flBlend = 0.0f;

	m_pBlendFactorSlider->SetValue( flBlend*255.0f );
}

	
//-----------------------------------------------------------------------------
// The color mask was changed 
//-----------------------------------------------------------------------------
void CColorLevelsUIPanel::OnColorMaskSelected()
{
	int nMask = m_pColorMask->GetActiveItem();
	switch( nMask )
	{
	case COLOR_MASK_RGB:
		m_pLevelsOp->SetChannelMask( CLevelsColorOperation::ALL_CHANNELS );
		m_pHistogramPanel->SetHistogramType( CColorHistogramPanel::RGB );
		break;

	case COLOR_MASK_RED:
		m_pLevelsOp->SetChannelMask( CLevelsColorOperation::RED_CHANNEL );
		m_pHistogramPanel->SetHistogramType( CColorHistogramPanel::RED );
		break;

	case COLOR_MASK_GREEN:
		m_pLevelsOp->SetChannelMask( CLevelsColorOperation::GREEN_CHANNEL );
		m_pHistogramPanel->SetHistogramType( CColorHistogramPanel::GREEN );
		break;

	case COLOR_MASK_BLUE:
		m_pLevelsOp->SetChannelMask( CLevelsColorOperation::BLUE_CHANNEL );
		m_pHistogramPanel->SetHistogramType( CColorHistogramPanel::BLUE );
		break;
	}
}


//-----------------------------------------------------------------------------
// A combo box changed 
//-----------------------------------------------------------------------------
void CColorLevelsUIPanel::OnTextChanged( KeyValues *data )
{
	Panel *pPanel = reinterpret_cast<vgui::Panel *>( data->GetPtr("panel") );
	vgui::ComboBox *pBox = dynamic_cast<vgui::ComboBox *>( pPanel );

	if ( pBox == m_pColorMask ) 
	{
		OnColorMaskSelected();
		return;
	}
}

//-----------------------------------------------------------------------------
// HSV modification on selected parts of the image
//-----------------------------------------------------------------------------
class CSelectedHSVOperation : public IColorOperation
{
public:
	CSelectedHSVOperation();

	// Selection methods
	enum SelectionMethod_t
	{
		SELECT_NONE = 0,
		SELECT_ALL,
		SELECT_GREATER_RED,
		SELECT_LESSER_RED,
		SELECT_GREATER_GREEN,
		SELECT_LESSER_GREEN,
		SELECT_GREATER_BLUE,
		SELECT_LESSER_BLUE,
		SELECT_NEARBY_RGB,
		SELECT_GREATER_HUE,
		SELECT_LESSER_HUE,
		SELECT_NEARBY_HUE,
		SELECT_GREATER_SATURATION,
		SELECT_LESSER_SATURATION,
		SELECT_NEARBY_SATURATION,
		SELECT_GREATER_VALUE,
		SELECT_LESSER_VALUE,
		SELECT_NEARBY_VALUE,

		SELECTION_METHOD_COUNT,
	};

	// Methods of IColorOperation
	virtual void Apply( const Vector &inRGB, Vector &outRGB );
	virtual void Release() { delete this; }

	virtual const char *GetName()			  { return m_pName; }
	virtual void SetName( const char *pName ) { V_strcpy_safe( m_pName, pName ); }

	virtual ColorCorrectionTool_t ToolID() { return CC_TOOL_SELECTED_HSV; }

	virtual IColorOperation *Clone();

	virtual bool IsEnabled( ) { return m_bEnable; }
	virtual void SetEnabled( bool bEnable ) { m_bEnable = bEnable; }

	void AddSelectedColor( unsigned char r, unsigned char g, unsigned char b );
	void ClearSelectedColors( );

	float GetSelectionAmount( unsigned char r, unsigned char g, unsigned char b ) const;
	float GetSelectionAmount( const Vector &rgb ) const;
	
	void SetSelectionMethod( SelectionMethod_t method );
	SelectionMethod_t GetSelectionMethod( );

	void SetDeltaHSV( const Vector &deltaHSV );
    void GetDeltaHSV( Vector &deltaHSV );

	void SetColorize( bool bColorize );
	bool GetColorize( );

	void SetInvertSelection( bool bInvertSelection );
	bool GetInvertSelection( );

	void SetTolerance( float flTolerance );
	void SetFuzziness( float flFuzziness );

	float GetTolerance( );
	float GetFuzziness( );

	virtual void SetBlendFactor( float blend_factor );
	virtual float GetBlendFactor( ) { return m_flBlendFactor; }

	// Used to set/get the list
	CColorOperationList *GetColorOpList()				{ return m_pOpList; }
	void SetColorOpList( CColorOperationList *pList )	{ m_pOpList = pList; }

private:
	CColorOperationList *m_pOpList;

	CUtlVector<Vector>	m_SelectedRGBs;
	CUtlVector<Vector>	m_SelectedHSVs;

	SelectionMethod_t	m_SelectionMethod;
	Vector				m_DeltaHSV;

	float				m_Tolerance;
	float				m_Fuzziness;

	bool				m_bColorize;
	bool				m_bInvertSelection;

	float				m_flBlendFactor;

	bool				m_bEnable;

	char				m_pName[256];
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSelectedHSVOperation::CSelectedHSVOperation()
{
	m_SelectionMethod = SELECT_NEARBY_RGB;
	m_DeltaHSV.Init( 0, 0, 0 );

	m_Tolerance = 0.2f;
	m_Fuzziness = 0.0f;

	m_bColorize = false;
	m_bInvertSelection = false;

	m_flBlendFactor = 1.0f;

	m_bEnable = true;

	V_strcpy_safe( m_pName, "HSV" );
}


//-----------------------------------------------------------------------------
// Returns the image buffer 
//-----------------------------------------------------------------------------
void CSelectedHSVOperation::SetSelectionMethod( SelectionMethod_t method )
{
	m_SelectionMethod = method;
	colorcorrectiontools->UpdateColorCorrection();
}

CSelectedHSVOperation::SelectionMethod_t CSelectedHSVOperation::GetSelectionMethod( )
{
	return m_SelectionMethod;
}

void CSelectedHSVOperation::SetDeltaHSV( const Vector &deltaHSV )
{
	m_DeltaHSV = deltaHSV;
}

void CSelectedHSVOperation::GetDeltaHSV( Vector &deltaHSV )
{
	deltaHSV = m_DeltaHSV;
}

float FuzzyLessThan( float a, float b, float fuzziness )
{
	if( fuzziness < 1.0f/255.0f )
		return (a <= b) ? 1.0f : 0.0f;

	float min = b - fuzziness;
	float max = b + fuzziness;

	if( a < min )
		return 1.0f;
	if( a > max )
		return 0.0f;

	return 1.0f - (a-min)/(max-min);
}

float FuzzyGreaterThan( float a, float b, float fuzziness )
{
	if( fuzziness < 1.0f/255.0f )
		return (a >= b) ? 1.0f : 0.0f;

	float min = b - fuzziness;
	float max = b + fuzziness;

	if( a > max )
		return 1.0f;
	if( a < min )
		return 0.0f;

	return (a-min)/(max-min);
}

//-----------------------------------------------------------------------------
// Returns the image buffer 
//-----------------------------------------------------------------------------
float CSelectedHSVOperation::GetSelectionAmount( const Vector &rgb ) const
{
	if( m_SelectionMethod==SELECT_ALL )
		return 1.0f;
	else if( m_SelectionMethod==SELECT_NONE )
		return 0.0f;

	float flSelAmount = 0.0f;
	for( int i=0;i<m_SelectedRGBs.Count();i++ )
	{
		Vector hsv;
		float  flCurSelAmount;

		switch ( m_SelectionMethod )
		{
			default:
			case SELECT_GREATER_RED:
				flCurSelAmount = FuzzyGreaterThan( rgb.x, m_SelectedRGBs[i].x, m_Fuzziness );
				break;
			case SELECT_LESSER_RED:
				flCurSelAmount = FuzzyLessThan( rgb.x, m_SelectedRGBs[i].x, m_Fuzziness );
				break;
			case SELECT_GREATER_GREEN:
				flCurSelAmount = FuzzyGreaterThan( rgb.y, m_SelectedRGBs[i].y, m_Fuzziness );
				break;
			case SELECT_LESSER_GREEN:
				flCurSelAmount = FuzzyLessThan( rgb.y, m_SelectedRGBs[i].y, m_Fuzziness );
				break;
			case SELECT_GREATER_BLUE:
				flCurSelAmount = FuzzyGreaterThan( rgb.z, m_SelectedRGBs[i].z, m_Fuzziness );
				break;
			case SELECT_LESSER_BLUE:
				flCurSelAmount = FuzzyLessThan( rgb.z, m_SelectedRGBs[i].z, m_Fuzziness );
				break;
			case SELECT_NEARBY_RGB:
				flCurSelAmount = FuzzyLessThan( rgb.DistTo( m_SelectedRGBs[i] ), m_Tolerance, m_Fuzziness*m_Tolerance );
				break;
			case SELECT_GREATER_HUE:
				RGBtoHSV( rgb, hsv );
				flCurSelAmount = FuzzyGreaterThan( hsv.x, m_SelectedHSVs[i].x, m_Fuzziness );
				break;
			case SELECT_LESSER_HUE:
				RGBtoHSV( rgb, hsv );
				flCurSelAmount = FuzzyLessThan( hsv.x, m_SelectedHSVs[i].x, m_Fuzziness );
				break;
			case SELECT_NEARBY_HUE:
				RGBtoHSV( rgb, hsv );
				flCurSelAmount = FuzzyLessThan( fabsf( hsv.x-m_SelectedHSVs[i].x )/360.0f, m_Tolerance, m_Fuzziness*m_Tolerance );
				break;
			case SELECT_GREATER_SATURATION:
				RGBtoHSV( rgb, hsv );
				flCurSelAmount = FuzzyGreaterThan( hsv.y, m_SelectedHSVs[i].y, m_Fuzziness );
				break;
			case SELECT_LESSER_SATURATION:
				RGBtoHSV( rgb, hsv );
				flCurSelAmount = FuzzyLessThan( hsv.y, m_SelectedHSVs[i].y, m_Fuzziness );
				break;
			case SELECT_NEARBY_SATURATION:
				RGBtoHSV( rgb, hsv );
				flCurSelAmount = FuzzyLessThan( fabsf( hsv.y-m_SelectedHSVs[i].y ), m_Tolerance, m_Fuzziness*m_Tolerance );
				break;
			case SELECT_GREATER_VALUE:
				RGBtoHSV( rgb, hsv );
				flCurSelAmount = FuzzyGreaterThan( hsv.z, m_SelectedHSVs[i].z, m_Fuzziness );
				break;
			case SELECT_LESSER_VALUE:
				RGBtoHSV( rgb, hsv );
				flCurSelAmount = FuzzyLessThan( hsv.z, m_SelectedHSVs[i].z, m_Fuzziness );
				break;
			case SELECT_NEARBY_VALUE:
				RGBtoHSV( rgb, hsv );
				flCurSelAmount = FuzzyLessThan( fabsf( hsv.z-m_SelectedHSVs[i].z ), m_Tolerance, m_Fuzziness*m_Tolerance );
				break;
		}

		if( flCurSelAmount>flSelAmount )
		{
			flSelAmount = flCurSelAmount;
		}
	}

	if( m_bInvertSelection )
		flSelAmount = 1.0f - flSelAmount;

	return flSelAmount;
}

float CSelectedHSVOperation::GetSelectionAmount( unsigned char r, unsigned char g, unsigned char b ) const
{
	Vector rgb( r, g, b );
	rgb /= 255.0f;
	return GetSelectionAmount( rgb );
}

void CSelectedHSVOperation::AddSelectedColor( unsigned char r, unsigned char g, unsigned char b )
{
	Vector color, hsv;
	color.x = r / 255.0f;
	color.y = g / 255.0f;
	color.z = b / 255.0f;
	RGBtoHSV( color, hsv );
	m_SelectedRGBs.AddToTail( color );
	m_SelectedHSVs.AddToTail( hsv );

	colorcorrectiontools->UpdateColorCorrection();
}

void CSelectedHSVOperation::ClearSelectedColors( )
{
	m_SelectedRGBs.RemoveAll();
	m_SelectedHSVs.RemoveAll();
}

void CSelectedHSVOperation::SetBlendFactor( float blend_factor )
{
	m_flBlendFactor = blend_factor;
	colorcorrectiontools->UpdateColorCorrection();
}

void CSelectedHSVOperation::SetColorize( bool bColorize )
{
	m_bColorize = bColorize;
	colorcorrectiontools->UpdateColorCorrection();
}

bool CSelectedHSVOperation::GetColorize( )
{
	return m_bColorize;
}

void CSelectedHSVOperation::SetInvertSelection( bool bInvertSelection )
{
	m_bInvertSelection = bInvertSelection;
	colorcorrectiontools->UpdateColorCorrection();
}

bool CSelectedHSVOperation::GetInvertSelection( )
{
	return m_bInvertSelection;
}

void CSelectedHSVOperation::SetTolerance( float flTolerance )
{
	m_Tolerance = flTolerance;
}

float CSelectedHSVOperation::GetTolerance( )
{
	return m_Tolerance;
}

void CSelectedHSVOperation::SetFuzziness( float flFuzziness )
{
	m_Fuzziness = flFuzziness;
}

float CSelectedHSVOperation::GetFuzziness( )
{
	return m_Fuzziness;
}

//-----------------------------------------------------------------------------
// Applies the color correction 
//-----------------------------------------------------------------------------
void CSelectedHSVOperation::Apply( const Vector &inRGB, Vector &outRGB )
{
	float flSelectionAmount = GetSelectionAmount( inRGB );
	if ( flSelectionAmount == 0.0f || !m_bEnable )
	{
		outRGB = inRGB;
		return;
	}

	Vector hsv;
	RGBtoHSV( inRGB, hsv );
	if( !m_bColorize )
	{
		hsv.x += m_DeltaHSV.x;
		hsv.x = fmod( hsv.x, 360.0f );
		if( hsv.x < 0.0f ) hsv.x = 360.0f + hsv.x;

		hsv.y += m_DeltaHSV.y*hsv.y;
	}
	else
	{
		hsv.x = (m_DeltaHSV.x < 0.0f) ? 360.0f+m_DeltaHSV.x : m_DeltaHSV.x;
		hsv.y = m_DeltaHSV.y;
	}

	hsv.y = clamp( hsv.y, 0.0f, 1.0f );
	
	hsv.z += m_DeltaHSV.z;
 	hsv.z = clamp( hsv.z, 0.0f, 1.0f );
	if ( hsv.y == 0.0F )
	{
		hsv.x = -1.0f;
	}
	HSVtoRGB( hsv, outRGB );

	VectorLerp( inRGB, outRGB, flSelectionAmount * m_flBlendFactor, outRGB ); 
}

IColorOperation *CSelectedHSVOperation::Clone()
{
	CSelectedHSVOperation *pClone = new CSelectedHSVOperation;

	pClone->m_SelectedRGBs = m_SelectedRGBs;
	pClone->m_SelectedHSVs = m_SelectedHSVs;

	pClone->m_SelectionMethod = m_SelectionMethod;

	pClone->m_DeltaHSV = m_DeltaHSV;

	pClone->m_Tolerance = m_Tolerance;
	pClone->m_Fuzziness = m_Fuzziness;

	pClone->m_bColorize = m_bColorize;
	pClone->m_bInvertSelection = m_bInvertSelection;

	pClone->m_flBlendFactor = m_flBlendFactor;

	pClone->m_bEnable = m_bEnable;

	pClone->m_pOpList = m_pOpList;

	Q_memcpy( pClone->m_pName, m_pName, sizeof(char)*256 );

	return pClone;
}

//-----------------------------------------------------------------------------
// Full screen selection panel
//-----------------------------------------------------------------------------
class CFullScreenSelectionPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CFullScreenSelectionPanel, vgui::Panel );

public:
		
	CFullScreenSelectionPanel( const char *pName, CSelectedHSVOperation *pOp, vgui::Panel *pParent );
   ~CFullScreenSelectionPanel( );

	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseReleased( vgui::MouseCode code );

	virtual void OnCursorMoved( int x, int y );

	virtual void OnKeyCodeTyped( KeyCode code );

protected:

	CSelectedHSVOperation *m_pOp;

	bool	m_bMouseDown;

};	

CFullScreenSelectionPanel::CFullScreenSelectionPanel( const char *pName, CSelectedHSVOperation *pOp, vgui::Panel *pParent ) : BaseClass( pParent, pName )
{
	m_bMouseDown = false;

	SetZPos( -1000 );

	m_pOp = pOp;
}

CFullScreenSelectionPanel::~CFullScreenSelectionPanel()
{

}

void CFullScreenSelectionPanel::OnMousePressed( vgui::MouseCode code )
{
	int x, y;
	input()->GetCursorPos( x, y );

	m_bMouseDown = true;

	BaseClass::OnMousePressed( code );
}

void CFullScreenSelectionPanel::OnMouseReleased( vgui::MouseCode code )
{
	m_bMouseDown = false;

	int x, y;
	input()->GetCursorPos( x, y );

	BaseClass::OnMouseReleased( code );
}

void CFullScreenSelectionPanel::OnCursorMoved( int x, int y )
{
	if( m_bMouseDown )
	{
		CMatRenderContextPtr pRenderContext( materials );

		BGRA8888_t pixelValue;
		pRenderContext->ReadPixels( x, y, 1, 1, (unsigned char *)&pixelValue, IMAGE_FORMAT_BGRX8888 );

		bool bCTRLDown = ( input()->IsKeyDown(KEY_LCONTROL) || input()->IsKeyDown(KEY_RCONTROL) );

		if( !bCTRLDown )
			m_pOp->ClearSelectedColors( );

		m_pOp->AddSelectedColor( pixelValue.r, pixelValue.g, pixelValue.b );
	}

	BaseClass::OnCursorMoved( x, y );
}

class CSelectedHSVUIPanel;

void CFullScreenSelectionPanel::OnKeyCodeTyped( KeyCode code ) 
{
	if( code==KEY_ESCAPE )
	{
		PostActionSignal( new KeyValues( "Command", "Command", "ToggleSelection" ) );
	}
}

//-----------------------------------------------------------------------------
// Uncorrected image
//-----------------------------------------------------------------------------
class CUncorrectedImagePanel : public CProceduralTexturePanel
{
	DECLARE_CLASS_SIMPLE( CUncorrectedImagePanel, CProceduralTexturePanel );

public:
	// constructor
	CUncorrectedImagePanel( vgui::Panel *pParent, const char *pName );
	~CUncorrectedImagePanel();

	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect );

	virtual void OnCursorMoved(int x,int y);
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseReleased( MouseCode code );

	// Sets the HSV color operation
	void SetHSVOperation( CSelectedHSVOperation *pOp );

protected:
	CSelectedHSVOperation *m_pHSVOp;

	// Is the mouse down?
	bool m_bMouseDown;
};


//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CUncorrectedImagePanel::CUncorrectedImagePanel( vgui::Panel *pParent, const char *pName ) : BaseClass( pParent, pName )
{
	m_bMouseDown = false;
	SetMouseInputEnabled( true );
	MaintainProportions( true );
}

CUncorrectedImagePanel::~CUncorrectedImagePanel()
{
}


//-----------------------------------------------------------------------------
// Sets the HSV color operation
//-----------------------------------------------------------------------------
void CUncorrectedImagePanel::SetHSVOperation( CSelectedHSVOperation *pOp )
{
	m_pHSVOp = pOp;
}


//-----------------------------------------------------------------------------
// Fills the texture w/ the image buffer 
//-----------------------------------------------------------------------------
void CUncorrectedImagePanel::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
{
	Assert( pVTFTexture->FrameCount() == 1 );
	Assert( pVTFTexture->FaceCount() == 1 );
	Assert( !pTexture->IsMipmapped() );

	int nWidth, nHeight, nDepth;
	pVTFTexture->ComputeMipLevelDimensions( 0, &nWidth, &nHeight, &nDepth );
	Assert( nDepth == 1 );
	Assert( nWidth == m_nWidth && nHeight == m_nHeight );

	CPixelWriter pixelWriter;
	pixelWriter.SetPixelMemory( pVTFTexture->Format(), 
		pVTFTexture->ImageData( 0, 0, 0 ), pVTFTexture->RowSizeInBytes( 0 ) );

	for (int y = 0; y < nHeight; ++y)
	{
		pixelWriter.Seek( 0, y );
		BGRA8888_t *pTexel = &m_pImageBuffer[y * m_nWidth];
		for ( int x = 0; x < nWidth; ++x, ++pTexel )
		{
			color24 inColor, col;
			inColor.r =  pTexel->r;
			inColor.g =  pTexel->g;
			inColor.b =  pTexel->b;

			m_pHSVOp->GetColorOpList()->Apply( inColor, col, m_pHSVOp );
//			col = inColor;

			float flSelectionAmount = m_pHSVOp->GetSelectionAmount( col.r, col.g, col.b );
			flSelectionAmount *= 0.5f;

			// Blend between at most 50% (1,0,0) and the original texel based on selection amount
			Vector rgb( col.r, col.g, col.b );
			rgb *= (1 - flSelectionAmount) / 255.0f;
			rgb.x += flSelectionAmount;

			int r, g, b;
			r = rgb.x * 255.0f;
			g = rgb.y * 255.0f;
			b = rgb.z * 255.0f;
			r = clamp( r, 0, 255 );
			g = clamp( g, 0, 255 );
			b = clamp( b, 0, 255 );

			pixelWriter.WritePixel( r, g, b, pTexel->a );
		}
	}
}


//-----------------------------------------------------------------------------
// Used to control selection 
//-----------------------------------------------------------------------------
void CUncorrectedImagePanel::OnCursorMoved( int x, int y )
{
	if ( !m_bMouseDown )
		return;

	bool bCTRLDown = ( input()->IsKeyDown(KEY_LCONTROL) || input()->IsKeyDown(KEY_RCONTROL) );

//	LocalToScreen( x, y );

	int sx, sy;
	GetSize( sx, sy );

	// Renormalize (x,y) based on actual bits since the image is being stretched
	x = m_TextureSubRect.x + (float)x * m_TextureSubRect.width / sx;
	y = m_TextureSubRect.y + (float)y * m_TextureSubRect.height / sy;

	int nSelectedX = min( x, m_nWidth );
	nSelectedX = max( 0, nSelectedX );
	int nSelectedY = min( y, m_nHeight );
	nSelectedY = max( 0, nSelectedY );
	BGRA8888_t *pTexel = &m_pImageBuffer[(nSelectedY * m_nWidth) + nSelectedX];

	if( !bCTRLDown )
		m_pHSVOp->ClearSelectedColors();

	color24 inColor, outColor;
	inColor.r = pTexel->r;
	inColor.g = pTexel->g;
	inColor.b = pTexel->b;

	m_pHSVOp->GetColorOpList()->Apply( inColor, outColor, m_pHSVOp );

	m_pHSVOp->AddSelectedColor( outColor.r, outColor.g, outColor.b );
}

void CUncorrectedImagePanel::OnMousePressed( vgui::MouseCode code )
{
	BaseClass::OnMousePressed( code );
	if ( code == MOUSE_LEFT )
	{
		m_bMouseDown = true;
		int x, y;
		input()->GetCursorPos( x, y );
		ScreenToLocal( x, y );

		OnCursorMoved( x, y );
	}
}

void CUncorrectedImagePanel::OnMouseReleased( MouseCode code )
{
	BaseClass::OnMouseReleased( code );
	if ( code == MOUSE_LEFT )
	{
		m_bMouseDown = false;
	}
}


//-----------------------------------------------------------------------------
// Main ui panel for dealing with HSV modification
//-----------------------------------------------------------------------------
class CSelectedHSVUIPanel : public CColorCorrectionUIChildPanel
{
	DECLARE_CLASS_SIMPLE( CSelectedHSVUIPanel, CColorCorrectionUIChildPanel );

public:
	CSelectedHSVUIPanel( vgui::Panel *parent, CSelectedHSVOperation *pOp );
	~CSelectedHSVUIPanel();

	// Command issued
	virtual void	OnMessage(const KeyValues *params,  vgui::VPANEL fromPanel);

	virtual void	OnCommand( const char *command );

	virtual void	Init();
	virtual void	Shutdown();

	virtual void	ReadUncorrectedImage( Rect_t *pSrcRect, unsigned char *pPreviewImage );

	virtual IColorOperation *GetOperation( ) { return (IColorOperation*)m_pHSVOperation; }

	void	EnableSelectionMode( bool bEnable );

protected:
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );

private:
	enum
	{
		DEFAULT_SELECTION_METHOD = CSelectedHSVOperation::SELECT_NONE
	};

	void	PopulateControls();
	void	OnSelectionMethodSelected();

	void	ResetBlendFactorSlider();

	// Reset the HSV tools
	void	ResetHSVSliders( );

	// Update delta HSV in the color operation
	void	UpdateDeltaHSV( );

	vgui::ComboBox		*m_pSelectionMethod;
	CPrecisionSlider	*m_pHueSlider;
	CPrecisionSlider	*m_pSaturationSlider;
	CPrecisionSlider	*m_pValueSlider;
	CUncorrectedImagePanel *m_pUncorrectedImage;

	CPrecisionSlider	*m_pToleranceSlider;
	CPrecisionSlider	*m_pFuzzinessSlider;

	CPrecisionSlider	*m_pBlendFactorSlider;
	vgui::CheckButton	*m_pColorizeButton;
	vgui::CheckButton	*m_pInvertSelectionButton;

	vgui::Button		*m_pSelectionButton;

	CFullScreenSelectionPanel *m_pFullScreenSelection;

	CSelectedHSVOperation *m_pHSVOperation;

	bool				m_bSelectionEnable;

	static const char	*s_pSelectionMethodNames[CSelectedHSVOperation::SELECTION_METHOD_COUNT]; 
};


//-----------------------------------------------------------------------------
// If you add a selection method, add it to the string list
//-----------------------------------------------------------------------------
const char *CSelectedHSVUIPanel::s_pSelectionMethodNames[CSelectedHSVOperation::SELECTION_METHOD_COUNT] = 
{
	"Select None",
	"Select All",
	"Select Greater Red Channel",
	"Select Lesser Red Channel",
	"Select Greater Green Channel",
	"Select Lesser Green Channel",
	"Select Greater Blue Channel",
	"Select Lesser Blue Channel",
	"Select Nearby RGB",
	"Select Greater Hue",
	"Select Lesser Hue",
	"Select Nearby Hue",
	"Select Greater Saturation",
	"Select Lesser Saturation",
	"Select Nearby Saturation",
	"Select Greater Value",
	"Select Lesser Value",
	"Select Nearby Value",
};


//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
CSelectedHSVUIPanel::CSelectedHSVUIPanel( vgui::Panel *parent, CSelectedHSVOperation *pOp ) : BaseClass( parent, "SelectedHSVUIPanel")
{
	m_pSelectionMethod = new vgui::ComboBox(this, "SelectionMethod", 10, false);
    m_pHSVOperation = pOp;

	m_pHueSlider = new CPrecisionSlider( this, "HueSlider" );
	m_pSaturationSlider = new CPrecisionSlider( this, "SaturationSlider" );
	m_pValueSlider = new CPrecisionSlider( this, "ValueSlider" );
	m_pToleranceSlider = new CPrecisionSlider( this, "ToleranceSlider" );
	m_pFuzzinessSlider = new CPrecisionSlider( this, "FuzzinessSlider" );
	m_pBlendFactorSlider = new CPrecisionSlider( this, "BlendFactorSlider" );

	m_pColorizeButton = new vgui::CheckButton( this, "ColorizeButton", "Colorize" );
	m_pInvertSelectionButton = new vgui::CheckButton( this, "InvertSelectionButton", "Invert Selection" );

	m_pSelectionButton = new vgui::Button( this, "SelectionButton", "Select" );
	m_pSelectionButton->AddActionSignalTarget( this );

	m_pUncorrectedImage = new CUncorrectedImagePanel( this, "UncorrectedImage" );
	m_pUncorrectedImage->SetHSVOperation( m_pHSVOperation );

	m_pHueSlider->SetRange( -360, 360 );
	m_pHueSlider->AddActionSignalTarget( this );

	m_pSaturationSlider->SetRange( -255, 255 );
	m_pSaturationSlider->AddActionSignalTarget( this );

	m_pValueSlider->SetRange( -255, 255 );
	m_pValueSlider->AddActionSignalTarget( this );
					
	m_pToleranceSlider->SetRange( 0, 255 );
	m_pToleranceSlider->AddActionSignalTarget( this );

	m_pFuzzinessSlider->SetRange( 0, 255 );
	m_pFuzzinessSlider->AddActionSignalTarget( this );
									
	m_pBlendFactorSlider->SetRange( 0, 255 );
	m_pBlendFactorSlider->AddActionSignalTarget( this );

	LoadControlSettings("Resource\\SelectedHSVUIPanel.res");
	PopulateControls();

	m_pColorizeButton->SetSelected( m_pHSVOperation->GetColorize() );
	m_pColorizeButton->AddActionSignalTarget( this );

	m_pInvertSelectionButton->SetSelected( m_pHSVOperation->GetInvertSelection() );
	m_pInvertSelectionButton->AddActionSignalTarget( this );
	ResetHSVSliders();
	ResetBlendFactorSlider();

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	int x, y, w, h;
	pRenderContext->GetViewport( x, y, w, h );

	m_pFullScreenSelection = new CFullScreenSelectionPanel( "SelectionPanel", pOp, this );
	m_pFullScreenSelection->SetSize( w, h );
	m_pFullScreenSelection->SetPos( x, y );
	m_pFullScreenSelection->SetEnabled( false );
	m_pFullScreenSelection->SetVisible( false );
	m_pFullScreenSelection->SetMouseInputEnabled( false );
	m_pFullScreenSelection->MakePopup( true );
	m_pFullScreenSelection->AddActionSignalTarget( this );
	m_bSelectionEnable = false;
}

CSelectedHSVUIPanel::~CSelectedHSVUIPanel()
{
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
void CSelectedHSVUIPanel::Init()
{
	m_pUncorrectedImage->Init( IMAGE_BUFFER_MAX_DIM, IMAGE_BUFFER_MAX_DIM, true );
}

void CSelectedHSVUIPanel::Shutdown()
{
	m_pUncorrectedImage->Shutdown();
}

void CSelectedHSVUIPanel::PopulateControls()
{
	m_pSelectionMethod->DeleteAllItems();
	int i;
	for ( i = 0; i < CSelectedHSVOperation::SELECTION_METHOD_COUNT; i++ )
	{
		m_pSelectionMethod->AddItem( s_pSelectionMethodNames[i], NULL );
	}
	m_pSelectionMethod->AddActionSignalTarget( this );
	m_pSelectionMethod->ActivateItem( m_pHSVOperation->GetSelectionMethod() );
}


//-----------------------------------------------------------------------------
// Update delta HSV in the color operation
//-----------------------------------------------------------------------------
void CSelectedHSVUIPanel::UpdateDeltaHSV( )
{
	Vector deltaHSV;
	deltaHSV.x = m_pHueSlider->GetValue();
	deltaHSV.y = m_pSaturationSlider->GetValue() / 255.0f;
	deltaHSV.z = m_pValueSlider->GetValue() / 255.0f;
	m_pHSVOperation->SetDeltaHSV( deltaHSV );

	m_pHSVOperation->SetTolerance( (float)m_pToleranceSlider->GetValue()/255.0f );
	m_pHSVOperation->SetFuzziness( (float)m_pFuzzinessSlider->GetValue()/255.0f );

	colorcorrectiontools->UpdateColorCorrection();
}


//-----------------------------------------------------------------------------
// Reset the HSV tools
//-----------------------------------------------------------------------------
void CSelectedHSVUIPanel::ResetHSVSliders( )
{
	Vector deltaHSV;
	m_pHSVOperation->GetDeltaHSV( deltaHSV );

	m_pHueSlider->SetValue( deltaHSV.x );
	m_pSaturationSlider->SetValue( deltaHSV.y*255.0f );
	m_pValueSlider->SetValue( deltaHSV.z*255.0f );

	m_pToleranceSlider->SetValue( m_pHSVOperation->GetTolerance()*255.0f );
	m_pFuzzinessSlider->SetValue( m_pHSVOperation->GetFuzziness()*255.0f );
}


void CSelectedHSVUIPanel::ResetBlendFactorSlider()
{
	float flBlend;
	if( m_pHSVOperation )
		flBlend = m_pHSVOperation->GetBlendFactor();
	else
		flBlend = 0.0f;

	m_pBlendFactorSlider->SetValue( flBlend*255.0f );
}


//-----------------------------------------------------------------------------
// A new selection method was selected 
//-----------------------------------------------------------------------------
void CSelectedHSVUIPanel::OnTextChanged( KeyValues *data )
{
	Panel *pPanel = reinterpret_cast<vgui::Panel *>( data->GetPtr("panel") );
	vgui::ComboBox *pBox = dynamic_cast<vgui::ComboBox *>( pPanel );

	if( pBox == m_pSelectionMethod ) // don't change the text in the config setting combo
	{
		OnSelectionMethodSelected();
	}
}

void CSelectedHSVUIPanel::OnSelectionMethodSelected()
{
	ResetHSVSliders();
	
	CSelectedHSVOperation::SelectionMethod_t method = (CSelectedHSVOperation::SelectionMethod_t)m_pSelectionMethod->GetActiveItem();
	m_pHSVOperation->SetSelectionMethod( method );

	if( method == CSelectedHSVOperation::SELECT_NEARBY_RGB ||
		method == CSelectedHSVOperation::SELECT_NEARBY_HUE ||
		method == CSelectedHSVOperation::SELECT_NEARBY_SATURATION ||
		method == CSelectedHSVOperation::SELECT_NEARBY_VALUE )
	{
		m_pToleranceSlider->SetEnabled( true );
		m_pFuzzinessSlider->SetEnabled( true );
	}
	else
	{
		m_pToleranceSlider->SetEnabled( false );
		m_pFuzzinessSlider->SetEnabled( true );
	}
}


//-----------------------------------------------------------------------------
// Returns the image buffer 
//-----------------------------------------------------------------------------
void CSelectedHSVUIPanel::ReadUncorrectedImage( Rect_t *pSrcRect, unsigned char *pPreviewImage )
{
	Rect_t dstRect;
	dstRect.x = 0;
	dstRect.y = 0;
	dstRect.width = g_nPreviewImageWidth;
	dstRect.height = g_nPreviewImageHeight;

	for( int i=0;i<g_nPreviewImageHeight;i++ )
	{
		Q_memcpy( m_pUncorrectedImage->GetImageBuffer()+i*IMAGE_BUFFER_MAX_DIM, pPreviewImage + i*g_nPreviewImageWidth*4, g_nPreviewImageWidth*4 );
	}
	m_pUncorrectedImage->SetTextureSubRect( dstRect );
	m_pUncorrectedImage->DownloadTexture();
}


//-----------------------------------------------------------------------------
// A new performance tool was selected 
//-----------------------------------------------------------------------------
void CSelectedHSVUIPanel::OnMessage(const KeyValues *params, VPANEL fromPanel)
{
	BaseClass::OnMessage( params, fromPanel );

	if ( !Q_stricmp( "SliderMoved", params->GetName() ) )
	{
		vgui::Panel *pPanel = reinterpret_cast<vgui::Panel *>( const_cast<KeyValues*>(params)->GetPtr("panel") );
		CPrecisionSlider *pSlider = dynamic_cast<CPrecisionSlider *>( pPanel );

		if ( pSlider != m_pBlendFactorSlider )
		{
			UpdateDeltaHSV();
		}
		else
		{
			m_pHSVOperation->SetBlendFactor( (float)pSlider->GetValue()/255.0f );

			PostMessage( GetParent(), new KeyValues( "command", "command", "BlendFactorUpdate" ) );
		}
	}
	else if ( !Q_stricmp( "CheckButtonChecked", params->GetName() ) )
	{
		vgui::Panel *pPanel = reinterpret_cast<vgui::Panel *>( const_cast<KeyValues*>(params)->GetPtr("panel") );
		vgui::CheckButton *pButton = dynamic_cast<vgui::CheckButton *>( pPanel );

		if( pButton == m_pColorizeButton )
		{
			m_pHSVOperation->SetColorize( pButton->IsSelected() );
		}
		else if( pButton == m_pInvertSelectionButton )
		{
			m_pHSVOperation->SetInvertSelection( pButton->IsSelected() );
		}
	}

}


void CSelectedHSVUIPanel::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );

	if( !Q_stricmp( "BlendFactorUpdate", command ) )
	{
		ResetBlendFactorSlider( );
	}
	else if( !Q_stricmp( "ToggleSelection", command ) )
	{
		EnableSelectionMode( !m_bSelectionEnable );
	}
}


void CSelectedHSVUIPanel::EnableSelectionMode( bool bEnable )
{
	if( bEnable )
		colorcorrectiontools->SetFinalOperation( m_pHSVOperation );
	else
		colorcorrectiontools->SetFinalOperation( NULL );

	m_bSelectionEnable = bEnable;
	m_pSelectionButton->ForceDepressed( bEnable );
	m_pFullScreenSelection->SetEnabled( bEnable );
	m_pFullScreenSelection->SetVisible( bEnable );
	m_pFullScreenSelection->SetMouseInputEnabled( bEnable );
}


//-----------------------------------------------------------------------------
// Lookup table based IColorOperation
//-----------------------------------------------------------------------------
class CColorLookupOperation : public IColorOperation
{
public:
	CColorLookupOperation();
   ~CColorLookupOperation();

	// Methods of IColorOperation
	virtual void Apply( const Vector &inRGB, Vector &outRGB );
	virtual void Release() { delete this; }

	virtual const char *GetName()			  { return m_pName; }
	virtual void SetName( const char *pName ) { V_strcpy_safe( m_pName, pName ); }

	virtual ColorCorrectionTool_t ToolID() { return CC_TOOL_LOOKUP; }

	virtual IColorOperation *Clone();

	virtual bool IsEnabled( ) { return m_bEnable; }
	virtual void SetEnabled( bool bEnable ) { m_bEnable = bEnable; }

	// Load a lookup table from file pFilename
	void LoadLookupTable( const char *pFilename );

	// Get the floating point color values at a lookup cell
	void GetLookupValue( int r, int g, int b, Vector &out );

	// Controls how much this op should take effect (1 = use 100% converted color, 0 = use 100% input color)
	virtual void SetBlendFactor( float flBlend );
	virtual float GetBlendFactor( ) { return m_flBlendFactor; }

	bool IsFileLoaded( )		{ return m_LookupTable != 0; }
	const char *GetFilename( )	{ return m_pFilename; }

private:

	// Set the resolution of the lookup table, deletes any active data
	void SetResolution( const int res );

	// Deletes any active lookup data
	void DeleteLookupTableData( );

	char	 m_pFilename[ MAX_PATH ];

	int		 m_Resolution;
	color24 *m_LookupTable;

	float	 m_flBlendFactor;

	bool	 m_bEnable;

	char	 m_pName[256];
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CColorLookupOperation::CColorLookupOperation( )
{
	m_Resolution = 0;
	m_LookupTable = 0;
	m_flBlendFactor = 1.0f;

	V_strcpy_safe( m_pName, "Lookup" );
	V_strcpy_safe( m_pFilename, "" );
}


//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CColorLookupOperation::~CColorLookupOperation( )
{
	DeleteLookupTableData( );
}
 

//-----------------------------------------------------------------------------
// Applies the color correction
//-----------------------------------------------------------------------------
void CColorLookupOperation::Apply( const Vector &inRGB, Vector &outRGB )
{
	if( !m_LookupTable || !m_bEnable )
	{
		outRGB = inRGB;
		return;
	}

	float fr = inRGB.x * (m_Resolution-1);
	float fg = inRGB.y * (m_Resolution-1);
	float fb = inRGB.z * (m_Resolution-1);

	int ir = (int)fr;
	fr = fr - (float)ir;

	int ig = (int)fg;
	fg = fg - (float)ig;

	int ib = (int)fb;
	fb = fb - (float)ib;

	Vector interp_cube[ 8 ];
	GetLookupValue( ir  , ig  , ib  , interp_cube[ 0 ] );
	GetLookupValue( ir+1, ig  , ib  , interp_cube[ 1 ] );
	GetLookupValue( ir  , ig+1, ib  , interp_cube[ 2 ] );
	GetLookupValue( ir+1, ig+1, ib  , interp_cube[ 3 ] );
	GetLookupValue( ir  , ig  , ib+1, interp_cube[ 4 ] );
	GetLookupValue( ir+1, ig  , ib+1, interp_cube[ 5 ] );
	GetLookupValue( ir  , ig+1, ib+1, interp_cube[ 6 ] );
	GetLookupValue( ir+1, ig+1, ib+1, interp_cube[ 7 ] );

    Vector a = interp_cube[0] * (1.0f-fr) + interp_cube[1] * fr;
	Vector b = interp_cube[2] * (1.0f-fr) + interp_cube[3] * fr;
	Vector c = interp_cube[4] * (1.0f-fr) + interp_cube[5] * fr;
	Vector d = interp_cube[6] * (1.0f-fr) + interp_cube[7] * fr;

	Vector bottom = a * (1.0f-fg) + b * fg;
	Vector top    = c * (1.0f-fg) + d * fg;

	outRGB = (bottom * (1.0f-fb) + top * fb) * m_flBlendFactor + inRGB - inRGB * m_flBlendFactor;
}


//-----------------------------------------------------------------------------
// Finds the floating point lookup value at the specified cell
//-----------------------------------------------------------------------------
void CColorLookupOperation::GetLookupValue( int r, int g, int b, Vector &out )
{
	if( !m_LookupTable )
		return;

	if( r<0 ) r = 0;
	if( g<0 ) g = 0;
	if( b<0 ) b = 0;

	if( r>m_Resolution-1 ) r = m_Resolution-1;
	if( g>m_Resolution-1 ) g = m_Resolution-1;
	if( b>m_Resolution-1 ) b = m_Resolution-1;

	color24 out_color = m_LookupTable[ r + g*m_Resolution + b*m_Resolution*m_Resolution ];
	out.x = out_color.r / 255.0f;
	out.y = out_color.g / 255.0f;
	out.z = out_color.b / 255.0f;
}


//-----------------------------------------------------------------------------
// Controls how much this op should take effect (1 = use 100% converted color, 0 = use 100% input color)
//-----------------------------------------------------------------------------
void CColorLookupOperation::SetBlendFactor( float flBlend )
{
	m_flBlendFactor = flBlend;

	colorcorrectiontools->UpdateColorCorrection();
}


//-----------------------------------------------------------------------------
// Loads a lookup table from file pFilename
//-----------------------------------------------------------------------------
void CColorLookupOperation::LoadLookupTable( const char *pFilename )
{
	FileHandle_t file_handle = g_pFileSystem->Open( pFilename, "rb" );
	if( !file_handle )
		return;

    unsigned int file_size = g_pFileSystem->Size( file_handle );
    int res = (int)powf( (float)(file_size/sizeof(color24)), 1.0f/3.0f );
	if( res*res*res*sizeof(color24) != file_size )
	{
		g_pFileSystem->Close( file_handle );
		return;
	}

	SetResolution( res );

	for( int i=0;i<res*res*res;i++ )
	{
		color24 color;
		g_pFileSystem->Read( &color, sizeof(color24), file_handle );
		m_LookupTable[i] = color;
	}

	g_pFileSystem->Close( file_handle );

	V_strcpy_safe( m_pFilename, pFilename );

	colorcorrectiontools->UpdateColorCorrection();
}


//-----------------------------------------------------------------------------
// Sets the resolution of the lookup table, deletes any active data
//-----------------------------------------------------------------------------
void CColorLookupOperation::SetResolution( const int res )
{
	DeleteLookupTableData( );

	m_LookupTable = new color24[ res*res*res ];
	m_Resolution = res;
}


//-----------------------------------------------------------------------------
// Deletes the lookup table data
//-----------------------------------------------------------------------------
void CColorLookupOperation::DeleteLookupTableData( )
{
	if( m_LookupTable )
	{
		m_Resolution = 0;
		delete m_LookupTable;
	}
}


IColorOperation *CColorLookupOperation::Clone()
{
	CColorLookupOperation *pClone = new CColorLookupOperation;

	Q_memcpy( pClone->m_pFilename, m_pFilename, sizeof(char)*MAX_PATH );

	pClone->m_Resolution = m_Resolution;
	pClone->m_flBlendFactor = m_flBlendFactor;
	pClone->m_bEnable = m_bEnable;

	Q_memcpy( pClone->m_pName, m_pName, sizeof(char)*256 );

	pClone->m_LookupTable = new color24[m_Resolution*m_Resolution*m_Resolution];
	Q_memcpy( pClone->m_LookupTable, m_LookupTable, sizeof(color24)*m_Resolution*m_Resolution*m_Resolution );

	return pClone;
}


//-----------------------------------------------------------------------------
// Root panel for loading lookup tables
//-----------------------------------------------------------------------------
class CColorLookupUIPanel : public CColorCorrectionUIChildPanel
{
	DECLARE_CLASS_SIMPLE( CColorLookupUIPanel, CColorCorrectionUIChildPanel );

public:
	// constructor
	CColorLookupUIPanel( vgui::Panel *pParent, CColorLookupOperation *pOp );
	~CColorLookupUIPanel();

	virtual void Init() {}
	virtual void Shutdown() {}

	virtual void ReadUncorrectedImage( Rect_t *pSrcRect, unsigned char *pPreviewImage ) {}

	virtual IColorOperation *GetOperation( ) { return (IColorOperation*)m_pLookupOp; }

	// Command issued
	virtual void OnMessage(const KeyValues *params,  vgui::VPANEL fromPanel);
	virtual void OnCommand(const char *command);

private:
	
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );

	void ResetBlendFactorSlider();

	void SetButtonText( );

	vgui::Button *m_pLoadButton;
	CPrecisionSlider *m_pBlendFactorSlider;

	CColorLookupOperation *m_pLookupOp;
};


//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CColorLookupUIPanel::CColorLookupUIPanel( vgui::Panel *pParent, CColorLookupOperation *pOp ) : BaseClass( pParent, "LookupUIPanel" )
{
	m_pLookupOp = pOp;

	m_pLoadButton = new vgui::Button( this, "Load Lookup", "", this, "LoadLookup" );

	m_pBlendFactorSlider = new CPrecisionSlider( this, "BlendFactorSlider" );
	m_pBlendFactorSlider->SetRange( 0, 255 );
	m_pBlendFactorSlider->SetValue( 255 );
	m_pBlendFactorSlider->AddActionSignalTarget( this );

	LoadControlSettings("Resource\\ColorLookupUIPanel.res");

	SetButtonText( );
}


CColorLookupUIPanel::~CColorLookupUIPanel()
{
}


//-----------------------------------------------------------------------------
// Command issued
//-----------------------------------------------------------------------------
void CColorLookupUIPanel::OnMessage(const KeyValues *params,  vgui::VPANEL fromPanel)
{
	BaseClass::OnMessage( params, fromPanel );

	if ( !Q_stricmp( "SliderMoved", params->GetName() ) )
	{
		vgui::Panel *pPanel = reinterpret_cast<vgui::Panel *>( const_cast<KeyValues*>(params)->GetPtr("panel") );
		if ( pPanel == m_pBlendFactorSlider )
		{
			m_pLookupOp->SetBlendFactor( m_pBlendFactorSlider->GetValue() / 255.0f );

			PostMessage( GetParent(), new KeyValues( "command", "command", "BlendFactorUpdate" ) );

			return;
		}
	}
}


void CColorLookupUIPanel::OnCommand( const char *command )
{
	if ( !Q_strcasecmp( command, "LoadLookup" ) )
	{
		FileOpenDialog *open_dialog = new FileOpenDialog( this, "File Open", true );
		open_dialog->AddActionSignalTarget( this );
		open_dialog->AddFilter( "*.raw", ".RAW files", true );
		open_dialog->DoModal( true );
	}
	else if( !Q_stricmp( "BlendFactorUpdate", command ) )
	{
		ResetBlendFactorSlider( );
	}

	BaseClass::OnCommand( command );
}


void CColorLookupUIPanel::ResetBlendFactorSlider()
{
	float flBlend;
	if( m_pLookupOp )
		flBlend = m_pLookupOp->GetBlendFactor();
	else
		flBlend = 0.0f;

	m_pBlendFactorSlider->SetValue( flBlend*255.0f );
}


void CColorLookupUIPanel::OnFileSelected( const char *filename )
{
	m_pLookupOp->LoadLookupTable( filename );

	SetButtonText( );
}


void CColorLookupUIPanel::SetButtonText( )
{
	if( !m_pLookupOp->IsFileLoaded() )
	{
		m_pLoadButton->SetText( "No File Loaded" );
	}
	else
	{
		m_pLoadButton->SetText( m_pLookupOp->GetFilename() );
	}
}


//-----------------------------------------------------------------------------
// Lookup table based IColorOperation
//-----------------------------------------------------------------------------
enum ColorBalanceMode_t
{
	CC_BALANCE_MODE_SHADOWS = 0,
	CC_BALANCE_MODE_MIDTONES,
	CC_BALANCE_MODE_HIGHLIGHTS,
	CC_BALANCE_MODE_COUNT,
};

class CColorBalanceOperation : public IColorOperation
{
public:
	CColorBalanceOperation();
   ~CColorBalanceOperation();

	// Methods of IColorOperation
	virtual void Apply( const Vector &inRGB, Vector &outRGB );
	virtual void Release() { delete this; }

	virtual const char *GetName()			  { return m_pName; }
	virtual void SetName( const char *pName ) { V_strcpy_safe( m_pName, pName ); }

	virtual ColorCorrectionTool_t ToolID() { return CC_TOOL_BALANCE; }

	virtual IColorOperation *Clone();

	virtual bool IsEnabled( ) { return m_bEnable; }
	virtual void SetEnabled( bool bEnable ) { m_bEnable = bEnable; }

	// Controls how much this op should take effect (1 = use 100% converted color, 0 = use 100% input color)
	virtual void SetBlendFactor( float flBlend );
	virtual float GetBlendFactor( ) { return m_flBlendFactor; }

	void SetPreserveLuminosity( bool bPreserveLum ) { m_PreserveLuminosity = bPreserveLum; Update(); }

	void SetCyanRedBalance     ( ColorBalanceMode_t mode, float value ) { m_CyanRedBalance[ (int)mode ]      = value; Update(); }
	void SetMagentaGreenBalance( ColorBalanceMode_t mode, float value ) { m_MagentaGreenBalance[ (int)mode ] = value; Update(); }
	void SetYellowBlueBalance  ( ColorBalanceMode_t mode, float value ) { m_YellowBlueBalance[ (int)mode ]   = value; Update(); }

	float GetCyanRedBalance     ( ColorBalanceMode_t mode ) { return m_CyanRedBalance[ (int)mode ]; }
	float GetMagentaGreenBalance( ColorBalanceMode_t mode ) { return m_MagentaGreenBalance[ (int)mode ]; }
	float GetYellowBlueBalance  ( ColorBalanceMode_t mode ) { return m_YellowBlueBalance[ (int)mode ]; }

private:

	void	Update( );
	void	CreateLookupTables( );

	bool	m_PreserveLuminosity;

	float	m_CyanRedBalance[ CC_BALANCE_MODE_COUNT ];
	float	m_MagentaGreenBalance[ CC_BALANCE_MODE_COUNT ];
	float	m_YellowBlueBalance[ CC_BALANCE_MODE_COUNT ];

	float	m_ShadowsSubTransfer[ 256 ];
	float	m_MidtonesSubTransfer[ 256 ];
	float	m_HighlightsSubTransfer[ 256 ];
	float	m_ShadowsAddTransfer[ 256 ];
	float	m_MidtonesAddTransfer[ 256 ];
	float	m_HighlightsAddTransfer[ 256 ];

	byte	m_pRedLookup[ 256 ];
	byte	m_pBlueLookup[ 256 ];
	byte	m_pGreenLookup[ 256 ];

	float	m_flBlendFactor;

	bool	m_bEnable;

	char	m_pName[256];
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CColorBalanceOperation::CColorBalanceOperation( )
{
	m_PreserveLuminosity = true;

	for( int i=0;i<CC_BALANCE_MODE_COUNT;i++ )
	{
		m_CyanRedBalance[i] = (float)0.0;
		m_MagentaGreenBalance[i] = (float)0.0;
		m_YellowBlueBalance[i] = (float)0.0;
	}

	for( int i=0;i<256;i++ )
	{
		m_HighlightsAddTransfer[i] = m_ShadowsSubTransfer[i] = ( 1.075f - 1.0f / ((float)i/16.0f + 1.0f) );
//		m_HighlightsSubTransfer[i] = m_ShadowsAddTransfer[i] = ( 1.075f - 1.0f / ((float)(255-i)/16.0f + 1.0f) );

		float fi = ((float)i - 127.0f) / 127.0f;
		m_MidtonesAddTransfer[i] = m_MidtonesSubTransfer[i]   = 0.667f * (1.0f - fi*fi);
		m_ShadowsAddTransfer[i]  = m_HighlightsSubTransfer[i] = 0.667f * (1.0f - fi*fi);
	}

	m_bEnable = true;

	m_flBlendFactor = 1.0f;

	CreateLookupTables( );

	V_strcpy_safe( m_pName, "Balance" );
}


//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CColorBalanceOperation::~CColorBalanceOperation( )
{
}
 
//-----------------------------------------------------------------------------
// Returns the luminance of an rgb color
//-----------------------------------------------------------------------------	
int RGBToL( int r, int g, int b )
{
	int imin, imax;

	if( r>g )
	{
		imax = max( r, b );
		imin = min( g, b );
	}
	else
	{
		imax = max( g, b );
		imin = min( r, b );
	}

	return (int)((float)(imax+imin)/2.0f);
}																		

//-----------------------------------------------------------------------------
// HSL conversion utility function
//-----------------------------------------------------------------------------	
int HSLValue( float n1, float n2, float hue )
{
	float value;

	if (hue > 255)
		hue -= 255;
	else if (hue < 0)
		hue += 255;

	if (hue < 42.5)
		value = n1 + (n2 - n1) * (hue / 42.5);
	else if (hue < 127.5)
		value = n2;
	else if (hue < 170)
		value = n1 + (n2 - n1) * ((170 - hue) / 42.5);
	else
		value = n1;

	return (int)(value * 255.0f);
}

//-----------------------------------------------------------------------------
// Converts from HSL space to RGB space with integer inputs/outputs
//-----------------------------------------------------------------------------	
void HSLToRGB( int *hue, int *saturation, int *lightness ) 
{
	float h, s, l;

	h = *hue;
	s = *saturation;
	l = *lightness;

	if (s == 0)
	{
		/*  achromatic case  */
		*hue        = l;
		*lightness  = l;
		*saturation = l;
	}
	else
	{
		float m1, m2;

		if (l < 128)
			m2 = (l * (255 + s)) / 65025.0;
		else
			m2 = (l + s - (l * s) / 255.0) / 255.0;

		m1 = (l / 127.5) - m2;

		/*  chromatic case  */
		*hue        = HSLValue(m1, m2, h + 85);
		*saturation = HSLValue(m1, m2, h);
		*lightness  = HSLValue(m1, m2, h - 85);
	}
}

//-----------------------------------------------------------------------------
// Converts from RGB space to HSL space with integer inputs/outputs
//-----------------------------------------------------------------------------	
void RGBToHSL( int *red, int *green, int *blue )
{
	int   r, g, b;
	float h, s, l;
	int   imin, imax;
	int   delta;

	r = *red;
	g = *green;
	b = *blue;

	if (r > g)
	{
		imax = max (r, b);
		imin = min (g, b);
	}
	else
	{
		imax = max (g, b);
		imin = min (r, b);
	}

	l = (imax + imin) / 2.0;

	if (imax == imin)
	{
		s = 0.0;
		h = 0.0;
	}
	else
	{
		delta = (imax - imin);

		if (l < 128)
			s = 255 * (float) delta / (float) (imax + imin);
		else
			s = 255 * (float) delta / (float) (511 - imax - imin);

		if (r == imax)
			h = (g - b) / (float) delta;
		else if (g == imax)
			h = 2 + (b - r) / (float) delta;
		else
			h = 4 + (r - g) / (float) delta;

		h = h * 42.5;

		if (h < 0)
			h += 255;
		else if (h > 255)
			h -= 255;
	}

	*red   = (int)h;
	*green = (int)s;
	*blue  = (int)l;
}

//-----------------------------------------------------------------------------
// Applies the color correction
//-----------------------------------------------------------------------------	
void CColorBalanceOperation::Apply( const Vector &inRGB, Vector &outRGB )
{
	if( !m_bEnable )
	{
		outRGB = inRGB;
		return;
	}

	int redIn   = (int)(inRGB.x * 255.0f);
	int greenIn = (int)(inRGB.y * 255.0f);
	int blueIn  = (int)(inRGB.z * 255.0f);

	int redOut   = m_pRedLookup[ redIn ];
	int greenOut = m_pGreenLookup[ greenIn ];
	int blueOut  = m_pBlueLookup[ blueIn ];

	if( m_PreserveLuminosity )
	{
		RGBToHSL( &redOut, &greenOut, &blueOut );
		blueOut = RGBToL( redIn, greenIn, blueIn );
		HSLToRGB( &redOut, &greenOut, &blueOut );
	}

	outRGB.x = (float)redOut   / 255.0f;
	outRGB.y = (float)greenOut / 255.0f;
	outRGB.z = (float)blueOut  / 255.0f;

	outRGB = outRGB * m_flBlendFactor + inRGB * (1.0f-m_flBlendFactor);
}


//-----------------------------------------------------------------------------
// Controls how much this op should take effect (1 = use 100% converted color, 0 = use 100% input color)
//-----------------------------------------------------------------------------
void CColorBalanceOperation::SetBlendFactor( float flBlend )
{
	m_flBlendFactor = flBlend;

	colorcorrectiontools->UpdateColorCorrection();
}


//-----------------------------------------------------------------------------
// Update the operator to reflect a change in parameters
//-----------------------------------------------------------------------------
void CColorBalanceOperation::Update( )
{
	CreateLookupTables( );
	colorcorrectiontools->UpdateColorCorrection();
}

//-----------------------------------------------------------------------------
// Create lookup tables used to accelerate balance operation
//-----------------------------------------------------------------------------
void CColorBalanceOperation::CreateLookupTables( )
{
	float *cyan_red_transfer[3];
	float *magenta_green_transfer[3];
	float *yellow_blue_transfer[3];

	cyan_red_transfer[ CC_BALANCE_MODE_SHADOWS ]         = (m_CyanRedBalance[ CC_BALANCE_MODE_SHADOWS ] > 0.0f)         ? m_ShadowsAddTransfer    : m_ShadowsSubTransfer;
	cyan_red_transfer[ CC_BALANCE_MODE_MIDTONES ]        = (m_CyanRedBalance[ CC_BALANCE_MODE_MIDTONES ] > 0.0f)        ? m_MidtonesAddTransfer   : m_MidtonesSubTransfer;
	cyan_red_transfer[ CC_BALANCE_MODE_HIGHLIGHTS ]      = (m_CyanRedBalance[ CC_BALANCE_MODE_HIGHLIGHTS ] > 0.0f)      ? m_HighlightsAddTransfer : m_HighlightsSubTransfer;

	magenta_green_transfer[ CC_BALANCE_MODE_SHADOWS ]    = (m_MagentaGreenBalance[ CC_BALANCE_MODE_SHADOWS ] > 0.0f)    ? m_ShadowsAddTransfer    : m_ShadowsSubTransfer;
	magenta_green_transfer[ CC_BALANCE_MODE_MIDTONES ]   = (m_MagentaGreenBalance[ CC_BALANCE_MODE_MIDTONES ] > 0.0f)   ? m_MidtonesAddTransfer   : m_MidtonesSubTransfer;
	magenta_green_transfer[ CC_BALANCE_MODE_HIGHLIGHTS ] = (m_MagentaGreenBalance[ CC_BALANCE_MODE_HIGHLIGHTS ] > 0.0f) ? m_HighlightsAddTransfer : m_HighlightsSubTransfer;

	yellow_blue_transfer[ CC_BALANCE_MODE_SHADOWS ]      = (m_YellowBlueBalance[ CC_BALANCE_MODE_SHADOWS ] > 0.0f)      ? m_ShadowsAddTransfer    : m_ShadowsSubTransfer;
	yellow_blue_transfer[ CC_BALANCE_MODE_MIDTONES ]     = (m_YellowBlueBalance[ CC_BALANCE_MODE_MIDTONES ] > 0.0f)     ? m_MidtonesAddTransfer   : m_MidtonesSubTransfer;
	yellow_blue_transfer[ CC_BALANCE_MODE_HIGHLIGHTS ]   = (m_YellowBlueBalance[ CC_BALANCE_MODE_HIGHLIGHTS ] > 0.0f)   ? m_HighlightsAddTransfer : m_HighlightsSubTransfer;

	for( int i=0;i<256;i++ )
	{
		int redOut = i;
		int greenOut = i;
		int blueOut = i;

		for( int mode=CC_BALANCE_MODE_SHADOWS;mode<=CC_BALANCE_MODE_HIGHLIGHTS;mode++ )
		{
			redOut   += m_CyanRedBalance[ mode ]      * cyan_red_transfer[ mode ][ redOut ];
    		greenOut += m_MagentaGreenBalance[ mode ] * magenta_green_transfer[ mode ][ greenOut ];
			blueOut  += m_YellowBlueBalance[ mode ]   * yellow_blue_transfer[ mode ][ blueOut ];

			redOut = clamp( redOut, 0, 255 );
			greenOut = clamp( greenOut, 0, 255 );
			blueOut = clamp( blueOut, 0, 255 );
		}

		m_pRedLookup[i] = redOut;
		m_pGreenLookup[i] = greenOut;
		m_pBlueLookup[i] = blueOut;
	}
}

IColorOperation *CColorBalanceOperation::Clone()
{
	CColorBalanceOperation *pClone = new CColorBalanceOperation;

	pClone->m_PreserveLuminosity = m_PreserveLuminosity;
	pClone->m_flBlendFactor = m_flBlendFactor;
	pClone->m_bEnable = m_bEnable;

	Q_memcpy( pClone->m_CyanRedBalance,      m_CyanRedBalance,      sizeof(float)*CC_BALANCE_MODE_COUNT );
	Q_memcpy( pClone->m_MagentaGreenBalance, m_MagentaGreenBalance, sizeof(float)*CC_BALANCE_MODE_COUNT );
	Q_memcpy( pClone->m_YellowBlueBalance,   m_YellowBlueBalance,   sizeof(float)*CC_BALANCE_MODE_COUNT );

	Q_memcpy( pClone->m_ShadowsSubTransfer,    m_ShadowsSubTransfer,    sizeof(float)*256 );
	Q_memcpy( pClone->m_MidtonesSubTransfer,   m_ShadowsSubTransfer,    sizeof(float)*256 );
	Q_memcpy( pClone->m_HighlightsSubTransfer, m_ShadowsSubTransfer,    sizeof(float)*256 );
	Q_memcpy( pClone->m_ShadowsAddTransfer,    m_ShadowsAddTransfer,    sizeof(float)*256 );
	Q_memcpy( pClone->m_MidtonesAddTransfer,   m_MidtonesAddTransfer,   sizeof(float)*256 );
	Q_memcpy( pClone->m_HighlightsAddTransfer, m_HighlightsAddTransfer, sizeof(float)*256 );

	Q_memcpy( pClone->m_pRedLookup,   m_pRedLookup,   sizeof(char)*256 );
	Q_memcpy( pClone->m_pGreenLookup, m_pGreenLookup, sizeof(char)*256 );
	Q_memcpy( pClone->m_pBlueLookup,  m_pBlueLookup,  sizeof(char)*256 );

	Q_memcpy( pClone->m_pName, m_pName, sizeof(char)*256 );

	return pClone;
}

//-----------------------------------------------------------------------------
// Root panel for color balance operations
//-----------------------------------------------------------------------------
class CColorBalanceUIPanel : public CColorCorrectionUIChildPanel
{
	DECLARE_CLASS_SIMPLE( CColorBalanceUIPanel, CColorCorrectionUIChildPanel );

public:
	// constructor
	CColorBalanceUIPanel( vgui::Panel *pParent, CColorBalanceOperation *pOp );
	~CColorBalanceUIPanel();

	virtual void Init() {}
	virtual void Shutdown() {}

	virtual void ReadUncorrectedImage( Rect_t *pSrcRect, unsigned char *pPreviewImage ) {}

	virtual IColorOperation *GetOperation( ) { return (IColorOperation*)m_pBalanceOp; }

	// Command issued
	virtual void OnMessage(const KeyValues *params,  vgui::VPANEL fromPanel);

	virtual void OnCommand( const char *command );

	ColorBalanceMode_t GetCurrentMode();

private:

	MESSAGE_FUNC( OnRadioButtonHit, "RadioButtonChecked" );

	void ResetSliders();
	void ResetBlendFactorSlider();

	vgui::CheckButton *m_pPreserveLuminosityButton;

	vgui::RadioButton *m_pShadowModeButton;
	vgui::RadioButton *m_pMidtoneModeButton;
	vgui::RadioButton *m_pHighlightModeButton;

	CPrecisionSlider  *m_pCyanRedSlider;
	CPrecisionSlider  *m_pMagentaGreenSlider;
	CPrecisionSlider  *m_pYellowBlueSlider;

	CPrecisionSlider  *m_pBlendFactorSlider;

	CColorBalanceOperation *m_pBalanceOp;
};


//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CColorBalanceUIPanel::CColorBalanceUIPanel( vgui::Panel *pParent, CColorBalanceOperation *pOp ) : BaseClass( pParent, "BalanceUIPanel" )
{
	m_pPreserveLuminosityButton = new vgui::CheckButton( this, "PreserveLuminosity", "Preserve Luminosity" );

	m_pShadowModeButton    = new vgui::RadioButton( this, "ShadowMode", "Shadows" );
	m_pMidtoneModeButton   = new vgui::RadioButton( this, "MidtoneMode", "Midtones" );
	m_pHighlightModeButton = new vgui::RadioButton( this, "HighlightMode", "Highlights" );

	m_pCyanRedSlider = new CPrecisionSlider( this, "CyanRedSlider" );
	m_pCyanRedSlider->SetRange( -100, 100 );
	m_pCyanRedSlider->SetValue( 0 );
	m_pCyanRedSlider->AddActionSignalTarget( this );

	m_pMagentaGreenSlider = new CPrecisionSlider( this, "MagentaGreenSlider" );
	m_pMagentaGreenSlider->SetRange( -100, 100 );
	m_pMagentaGreenSlider->SetValue( 0 );
	m_pMagentaGreenSlider->AddActionSignalTarget( this );

	m_pYellowBlueSlider = new CPrecisionSlider( this, "YellowBlueSlider" );
	m_pYellowBlueSlider->SetRange( -100, 100 );
	m_pYellowBlueSlider->SetValue( 0 );
	m_pYellowBlueSlider->AddActionSignalTarget( this );

	m_pBlendFactorSlider = new CPrecisionSlider( this, "BlendFactorSlider" );
	m_pBlendFactorSlider->SetRange( 0, 255 );
	m_pBlendFactorSlider->SetValue( 255 );
	m_pBlendFactorSlider->AddActionSignalTarget( this );

	LoadControlSettings("Resource\\ColorBalanceUIPanel.res");

	m_pBalanceOp = pOp;

	ResetBlendFactorSlider();
}


CColorBalanceUIPanel::~CColorBalanceUIPanel()
{
}


//-----------------------------------------------------------------------------
// Command issued
//-----------------------------------------------------------------------------
void CColorBalanceUIPanel::OnMessage(const KeyValues *params,  vgui::VPANEL fromPanel)
{
	BaseClass::OnMessage( params, fromPanel );

	if ( !Q_stricmp( "SliderMoved", params->GetName() ) )
	{
		vgui::Panel *pPanel = reinterpret_cast<vgui::Panel *>( const_cast<KeyValues*>(params)->GetPtr("panel") );
		if ( pPanel == m_pBlendFactorSlider )
		{
			m_pBalanceOp->SetBlendFactor( m_pBlendFactorSlider->GetValue() / 255.0f );

			PostMessage( GetParent(), new KeyValues( "command", "command", "BlendFactorUpdate" ) );

			return;
		}
		else if ( pPanel == m_pCyanRedSlider )
		{
			m_pBalanceOp->SetCyanRedBalance( GetCurrentMode(), m_pCyanRedSlider->GetValue() / 1.0f );
			return;
		}
		else if ( pPanel == m_pMagentaGreenSlider )
		{
			m_pBalanceOp->SetMagentaGreenBalance( GetCurrentMode(), m_pMagentaGreenSlider->GetValue() / 1.0f );
			return;
		}
		else if ( pPanel == m_pYellowBlueSlider )
		{
			m_pBalanceOp->SetYellowBlueBalance( GetCurrentMode(), m_pYellowBlueSlider->GetValue() / 1.0f );
			return;
		}
	}
	else if ( !Q_stricmp( "CheckButtonChecked", params->GetName() ) )
    {
		vgui::Panel *pPanel = reinterpret_cast<vgui::Panel *>( const_cast<KeyValues*>(params)->GetPtr("panel") );
		if( pPanel == m_pPreserveLuminosityButton )
		{
			m_pBalanceOp->SetPreserveLuminosity( m_pPreserveLuminosityButton->IsSelected() );
			return;
		}
	}
}


void CColorBalanceUIPanel::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );

	if( !Q_stricmp( "BlendFactorUpdate", command ) )
	{
		ResetBlendFactorSlider();
	}
}


void CColorBalanceUIPanel::ResetBlendFactorSlider()
{
	float flBlend;
	if( m_pBalanceOp )
		flBlend = m_pBalanceOp->GetBlendFactor();
	else
		flBlend = 0.0f;

	m_pBlendFactorSlider->SetValue( flBlend*255.0f );
}


void CColorBalanceUIPanel::OnRadioButtonHit()
{
	ResetSliders();
}


ColorBalanceMode_t CColorBalanceUIPanel::GetCurrentMode()
{
	if( m_pShadowModeButton->IsSelected() )
		return CC_BALANCE_MODE_SHADOWS;
	else if( m_pMidtoneModeButton->IsSelected() )
		return CC_BALANCE_MODE_MIDTONES;
	else if( m_pHighlightModeButton->IsSelected() )
		return CC_BALANCE_MODE_HIGHLIGHTS;

	return CC_BALANCE_MODE_SHADOWS;
}


void CColorBalanceUIPanel::ResetSliders()
{
	if( !m_pBalanceOp )
		return;

	ColorBalanceMode_t mode = GetCurrentMode();

	m_pCyanRedSlider->SetValue     ( (int)m_pBalanceOp->GetCyanRedBalance(mode), 0 );
	m_pMagentaGreenSlider->SetValue( (int)m_pBalanceOp->GetMagentaGreenBalance(mode), 0 );
	m_pYellowBlueSlider->SetValue  ( (int)m_pBalanceOp->GetYellowBlueBalance(mode), 0 );
}



class CLookupViewPanel : public CProceduralTexturePanel
{
	DECLARE_CLASS_SIMPLE( CLookupViewPanel, CProceduralTexturePanel );

public:
    CLookupViewPanel( vgui::Panel *parent, ColorCorrectionHandle_t CCHandle );
   ~CLookupViewPanel( );

   void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect );

protected:
	ColorCorrectionHandle_t m_CCHandle;

private:
};

CLookupViewPanel::CLookupViewPanel( vgui::Panel *parent, ColorCorrectionHandle_t CCHandle ) : BaseClass( parent, "LookupViewPanel" )
{
	m_CCHandle = CCHandle;
}

CLookupViewPanel::~CLookupViewPanel()
{
}

void CLookupViewPanel::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
{
	Assert( pVTFTexture->FrameCount() == 1 );
	Assert( pVTFTexture->FaceCount() == 1 );
	Assert( !pTexture->IsMipmapped() );
	
	int nWidth, nHeight, nDepth;
	pVTFTexture->ComputeMipLevelDimensions( 0, &nWidth, &nHeight, &nDepth );
	Assert( nDepth==1 );
	Assert( nWidth*nHeight==32*32*32 );
	
	CPixelWriter pixelWriter;
	pixelWriter.SetPixelMemory( pVTFTexture->Format(), pVTFTexture->ImageData( 0, 0, 0 ), pVTFTexture->RowSizeInBytes( 0 ) );

	for( int y=0;y<256;y++ )
	{
		for( int x=0;x<128;x++ )
		{
			int cx = x>>5;
			int cy = y>>5;

			int dx = x%32;
			int dy = y%32;

			RGBX5551_t inColor;
			inColor.r = cx + cy*4;
			inColor.g = dx;
			inColor.b = dy;

			color24 outColor = colorcorrection->GetLookup( m_CCHandle, inColor );

			pixelWriter.WritePixel( outColor.r, outColor.g, outColor.b );
		}
	}
}

class CLookupViewWindow : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CLookupViewWindow, vgui::Frame );

public:
	CLookupViewWindow( vgui::Panel *parent, ColorCorrectionHandle_t CCHandle );
   ~CLookupViewWindow( );

   virtual void Init();
   virtual void Shutdown();

   void UpdateColorCorrection();

private:

	CLookupViewPanel *m_pLookupPanel;
	ColorCorrectionHandle_t m_CCHandle;
};


CLookupViewWindow::CLookupViewWindow( vgui::Panel *parent, ColorCorrectionHandle_t CCHandle ) : BaseClass( parent, "LookupViewWindow" )
{
	SetSize( 146, 298 );
	SetPos( 32, 32 );

	m_pLookupPanel = new CLookupViewPanel( this, CCHandle );

	LoadControlSettings( "Resource\\LookupViewWindow.res" );

	m_CCHandle = CCHandle;
}

CLookupViewWindow::~CLookupViewWindow()
{
	if( m_pLookupPanel )
		delete m_pLookupPanel;
}

void CLookupViewWindow::Init()
{
	m_pLookupPanel->Init( 128, 256, false );
	Rect_t rect;
	rect.x = 0;
	rect.y = 0;
	rect.width = 128;
	rect.height = 256;
	m_pLookupPanel->SetTextureSubRect( rect );
	m_pLookupPanel->DownloadTexture();

}

void CLookupViewWindow::Shutdown()
{
	m_pLookupPanel->Shutdown();
}

void CLookupViewWindow::UpdateColorCorrection()
{
	m_pLookupPanel->DownloadTexture();
}

class CColorOperationListPanel;

class CNewOperationDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CNewOperationDialog, vgui::Frame );

public:
	CNewOperationDialog( vgui::Panel *parent, CColorOperationList *pOpList );
   ~CNewOperationDialog( );

   virtual void OnCommand( const char *command );

private:

	void PopulateControls( );

	vgui::ComboBox	*m_pOperationType;
	vgui::TextEntry *m_pName;
	vgui::Button	*m_pCreateButton;
	vgui::Button	*m_pCancelButton;

	CColorOperationList *m_pOpList;

};

CNewOperationDialog::CNewOperationDialog( vgui::Panel *parent, CColorOperationList *pOpList ) : BaseClass( parent, "NewOperation" )
{
	m_pOperationType = new vgui::ComboBox( this, "OperationType", 6, false );
	m_pName = new vgui::TextEntry( this, "Name" );
	m_pCreateButton = new vgui::Button( this, "Create", "Create", this, "Create" );
	m_pCancelButton = new vgui::Button( this, "Cancel", "Cancel", this, "Cancel" );

	LoadControlSettings( "Resource\\NewOperationDialog.res" );

	PopulateControls();

	m_pOpList = pOpList;
}

CNewOperationDialog::~CNewOperationDialog( )
{
}

void CNewOperationDialog::OnCommand( const char *command )
{
	if( !Q_stricmp( command, "Create" ) )
	{
		int nSelectedItem = m_pOperationType->GetActiveItem();

		IColorOperation *newOp = 0;
		switch( (ColorCorrectionTool_t)nSelectedItem+1 )
		{
			case CC_TOOL_BALANCE: newOp = new CColorBalanceOperation(); break;
			case CC_TOOL_CURVES:  newOp = new CCurvesColorOperation();  break;
			case CC_TOOL_LOOKUP:  newOp = new CColorLookupOperation();  break;
			case CC_TOOL_LEVELS:       newOp = new CLevelsColorOperation(); ((CLevelsColorOperation *)newOp)->SetColorOpList( m_pOpList ); break;
			case CC_TOOL_SELECTED_HSV: newOp = new CSelectedHSVOperation(); ((CSelectedHSVOperation *)newOp)->SetColorOpList( m_pOpList ); break;
		}

		if( m_pName->GetTextLength()>0 )
		{
			char buf[256];
			m_pName->GetText( buf, 256 );
			newOp->SetName( buf );
		}

		m_pOpList->AddOperation( newOp );

		PostActionSignal( new KeyValues( "Command", "Command", "NewComplete" ) );
	}
	else if( !Q_stricmp( command, "Cancel" ) )
	{
		PostActionSignal( new KeyValues( "Command", "Command", "NewCancel" ) );
	}
}	

void CNewOperationDialog::PopulateControls()
{
	m_pOperationType->DeleteAllItems();
	int i;
	for ( i = 1; i < CC_TOOL_COUNT; i++ )
	{
		m_pOperationType->AddItem( s_pColorCorrectionToolNames[i], NULL );
	}
	m_pOperationType->AddActionSignalTarget( this );
	m_pOperationType->ActivateItem( 0 );
}

//-----------------------------------------------------------------------------
// COperationListPanel 
//-----------------------------------------------------------------------------
class COperationListPanel : public vgui::ListPanel
{
	DECLARE_CLASS_SIMPLE( COperationListPanel, vgui::ListPanel );

public:
	COperationListPanel( vgui::Panel *parent, const char *pName );
   ~COperationListPanel( );

	MESSAGE_FUNC_PARAMS( OnTextNewLine, "TextNewLine", data );

	virtual void OnMousePressed( MouseCode code );
	virtual void OnMouseDoublePressed( MouseCode code );

	virtual void AddSelectedItem( int itemID );
	virtual void ClearSelectedItems( );
	virtual void RemoveItem( int itemID );

	virtual void SortList( );
	virtual void SetSortColumn( int column );

private:

	vgui::TextEntry	*m_pNameEditPanel;
	int				 m_nEditItem;
        
};

COperationListPanel::COperationListPanel( vgui::Panel *parent, const char *pName ) : BaseClass( parent, pName )
{
	m_pNameEditPanel = 0;
	m_nEditItem = -1;
}

COperationListPanel::~COperationListPanel( )
{
}

void COperationListPanel::AddSelectedItem( int itemID )
{
	BaseClass::AddSelectedItem( itemID );

	PostActionSignal( new KeyValues( "Command", "Command", "SelectedItemChanged" ) );
}

void COperationListPanel::ClearSelectedItems( )
{
	BaseClass::ClearSelectedItems( );

	PostActionSignal( new KeyValues( "Command", "Command", "SelectedItemChanged" ) );
}

void COperationListPanel::RemoveItem( int itemID )
{
	BaseClass::RemoveItem( itemID );

	PostActionSignal( new KeyValues( "Command", "Command", "SelectedItemChanged" ) );
}

void COperationListPanel::SortList( )
{
	// Disable sorting of the list
}

void COperationListPanel::SetSortColumn( int column )
{
	if( column==0 )
	{
		bool bAllEnabled = true;
		for( int i=0;i<GetItemCount();i++ )
		{
			IColorOperation *pOp = (IColorOperation *)GetItemUserData( i );

			if( !pOp->IsEnabled() )
			{
				bAllEnabled = false;
				break;
			}
		}

		for( int i=0;i<GetItemCount();i++ )
		{
			KeyValues *kv = GetItem( i );
			kv->SetInt( "image", (!bAllEnabled)?1:0 );

			IColorOperation *pOp = (IColorOperation *)GetItemUserData( i );
			pOp->SetEnabled( !bAllEnabled );
		}

		colorcorrectiontools->UpdateColorCorrection();
	}
}

void COperationListPanel::OnMousePressed( MouseCode code )
{
	if( code==MOUSE_LEFT )
	{
		int x, y;
		input()->GetCursorPos( x, y );

		int row, column;
		GetCellAtPos( x, y, row, column );

		if( column==0 && row!=-1 )
		{
			IColorOperation *pOp = (IColorOperation *)GetItemUserData( row );

			bool bNewEnable = !pOp->IsEnabled();

			KeyValues *kv = GetItem( row );
			kv->SetInt( "image", (bNewEnable)?1:0 );

			pOp->SetEnabled( bNewEnable );

			colorcorrectiontools->UpdateColorCorrection();
		}
		else
		{
			BaseClass::OnMousePressed( code );
		}
	}
}

void COperationListPanel::OnMouseDoublePressed( MouseCode code )
{
	if( code==MOUSE_LEFT )
	{
		int x, y;
		input()->GetCursorPos( x, y );

		int row, column;
		GetCellAtPos( x, y, row, column );

		if( column!=0 && row==-1 )
		{
			PostActionSignal( new KeyValues( "Command", "Command", "NewOperation" ) );
		}
		else
		{
			if( input()->IsKeyDown( KEY_LCONTROL )||input()->IsKeyDown( KEY_RCONTROL ) )
			{
				if( !m_pNameEditPanel )
				{
					m_nEditItem = row;
					m_pNameEditPanel = new vgui::TextEntry( this, "Name" );
					m_pNameEditPanel->SendNewLine( true );
					m_pNameEditPanel->SetCatchEnterKey( true );
					m_pNameEditPanel->AddActionSignalTarget( this );
					m_pNameEditPanel->SetSize( 226, 24 );
					m_pNameEditPanel->SetBgColor( Color(255,255,255,255) );
					EnterEditMode( row, column, m_pNameEditPanel );
				}
			}
			else if( input()->IsKeyDown( KEY_LALT ) || input()->IsKeyDown( KEY_RALT ) )
			{
				PostActionSignal( new KeyValues( "Command", "Command", "CloneOperation" ) );
			}
			else
			{
				BaseClass::OnMouseDoublePressed( code );
			}
		}
	}
}


void COperationListPanel::OnTextNewLine( KeyValues *data )
{
	char newName[256];
	m_pNameEditPanel->GetText( newName, 256 );

	if( m_nEditItem!=-1 )
	{
		IColorOperation *pOp = (IColorOperation *)GetItemUserData( m_nEditItem );

		pOp->SetName( newName );

		PostActionSignal( new KeyValues( "Command", "Command", "UpdateList" ) );
	}

	m_nEditItem = -1;
	LeaveEditMode();

	delete m_pNameEditPanel;
	m_pNameEditPanel = 0;
}

//-----------------------------------------------------------------------------
// CColorOperationListPanel - View window for operations in a CColorOperationList
//-----------------------------------------------------------------------------
class CColorOperationListPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CColorOperationListPanel, vgui::EditablePanel );

public:
	CColorOperationListPanel( vgui::Panel *parent, ColorCorrectionHandle_t CCHandle );
   ~CColorOperationListPanel( );

	void Init( );
	void Shutdown( );

	void PopulateList( );

	CColorOperationList	*GetOperationList( );

	virtual void	OnCommand(const char *command);

	virtual void	OnThink( );

	void			ReadUncorrectedImage( Rect_t *pSrcRect, unsigned char *pPreviewImage );

	void UpdateColorCorrection( );

private:

	MESSAGE_FUNC_PARAMS( OnOpPanelClose, "OpPanelClose", data );
	MESSAGE_FUNC_PARAMS( OnSliderMoved, "SliderMoved", data );
	MESSAGE_FUNC_PARAMS( OnCheckButtonChecked, "CheckButtonChecked", data );
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );

	virtual void OnMouseDoublePressed( MouseCode code );
	virtual void OnKeyCodeTyped( KeyCode code );

	void ResetSlider( );

	void LaunchOperationPanel( IColorOperation *pOp );

	vgui::Button			*m_pNewOperationButton;
	vgui::Button			*m_pDeleteOperationButton;
	vgui::Button			*m_pBringForwardButton;
	vgui::Button			*m_pPushBackButton;
	vgui::Button			*m_pSaveButton;
	vgui::CheckButton		*m_pEnableButton;
	vgui::CheckButton		*m_pEnableEntitiesButton;
	COperationListPanel		*m_pOperationListPanel;
	CPrecisionSlider		*m_pBlendFactorSlider;

	CLookupViewWindow		*m_pLookupViewWindow;

	CNewOperationDialog		*m_pNewDialog;

	CColorOperationList		 m_OperationList;

	ColorCorrectionHandle_t  m_CCHandle;

	CUtlVector< CColorCorrectionUIChildPanel * > m_OpPanelList;

	bool m_bEnable;
	bool m_bEnableEntities;
};

CColorOperationListPanel::CColorOperationListPanel( vgui::Panel *parent, ColorCorrectionHandle_t CCHandle ) : BaseClass( parent, "ColorOperationListPanel" )
{
	m_pNewOperationButton    = new vgui::Button( this, "NewOperation",    "New",           this, "NewOperation" );
	m_pDeleteOperationButton = new vgui::Button( this, "DeleteOperation", "Delete",        this, "DeleteOperation" );
	m_pBringForwardButton    = new vgui::Button( this, "BringForward",    "Up",			   this, "BringForward" );
	m_pPushBackButton        = new vgui::Button( this, "PushBack",        "Down",		   this, "PushBack" );
	m_pSaveButton			 = new vgui::Button( this, "Save",			  "Save",		   this, "Save" );

	m_pEnableButton          = new vgui::CheckButton( this, "Enable", "Enable" );
	m_pEnableButton->SetSelected( false );
	m_pEnableButton->AddActionSignalTarget( this );

	m_pEnableEntitiesButton  = new vgui::CheckButton( this, "EnableEntities", "Enable Entities" );
	m_pEnableEntitiesButton->SetSelected( true );
	m_pEnableEntitiesButton->AddActionSignalTarget( this );

	m_pBlendFactorSlider = new CPrecisionSlider( this, "BlendFactorSlider" );
	m_pBlendFactorSlider->SetRange( 0, 255 );
	m_pBlendFactorSlider->SetValue( 255 );
	m_pBlendFactorSlider->AddActionSignalTarget( this );

	m_pOperationListPanel = new COperationListPanel( this, "OperationList" );
	m_pOperationListPanel->SetBuildModeEditable( true );
	m_pOperationListPanel->AddColumnHeader( 0, "image", "", 24, ListPanel::COLUMN_IMAGE );
	m_pOperationListPanel->AddColumnHeader( 1, "layer", "", 226, 0 );
	m_pOperationListPanel->SetSelectIndividualCells( false );
	m_pOperationListPanel->SetEmptyListText( "" );
	m_pOperationListPanel->SetDragEnabled( false );
	m_pOperationListPanel->SetColumnSortable( 0, true );
	m_pOperationListPanel->SetColumnSortable( 1, false );
	m_pOperationListPanel->AddActionSignalTarget( this );

	vgui::ImageList *pImageList = new vgui::ImageList( false );
	pImageList->AddImage( scheme()->GetImage( "Resource/icon_hlicon1", false ) );
	m_pOperationListPanel->SetImageList( pImageList, true );

	m_pLookupViewWindow = new CLookupViewWindow( this, CCHandle );
	m_pLookupViewWindow->SetTitle( "Lookup View", true );
	m_pLookupViewWindow->SetSize( 148, 298 );
	m_pLookupViewWindow->SetEnabled( true );
	m_pLookupViewWindow->SetSizeable( false );
	m_pLookupViewWindow->AddActionSignalTarget( this );
	m_pLookupViewWindow->Activate();

	m_pNewDialog = 0;
	m_bEnable = true;
	m_bEnableEntities = true;

	LoadControlSettings( "Resource\\ColorOperationListPanel.res" );

	SetVisible( true );

	ResetSlider( );
	PopulateList( );

	m_CCHandle = CCHandle;
}

void CColorOperationListPanel::OnOpPanelClose( KeyValues *data )
{
	CColorCorrectionUIChildPanel *pSender = (CColorCorrectionUIChildPanel *)data->GetPtr( "panel", 0 );
	if( pSender )
	{
		int opPanelIndex = m_OpPanelList.Find( pSender );
        m_OpPanelList.Remove( opPanelIndex );

		pSender->Shutdown();
		delete pSender;
	}
}

void CColorOperationListPanel::OnSliderMoved( KeyValues *data )
{
	vgui::Panel *pPanel = reinterpret_cast<vgui::Panel *>( const_cast<KeyValues*>(data)->GetPtr("panel") );
	if ( pPanel == m_pBlendFactorSlider )
	{
		int nSelectedItem = m_pOperationListPanel->GetSelectedItem( 0 );
		if( nSelectedItem>=0 && nSelectedItem<m_pOperationListPanel->GetItemCount() )
		{
			IColorOperation *pOp = (IColorOperation *)m_pOperationListPanel->GetItemUserData( nSelectedItem );
            pOp->SetBlendFactor( m_pBlendFactorSlider->GetValue() / 255.0f );

			for( int i=0;i<m_OpPanelList.Count();i++ )
			{
				if( m_OpPanelList[i]->GetOperation()==pOp )
				{
					// We have an open edit window for this op
					PostMessage( m_OpPanelList[i], new KeyValues( "command", "command", "BlendFactorUpdate" ) );
				}
			}
		}

		return;
	}
}

void CColorOperationListPanel::OnCheckButtonChecked( KeyValues *data )
{
	vgui::Panel *pPanel = reinterpret_cast<vgui::Panel *>( const_cast<KeyValues*>(data)->GetPtr("panel") );
	if ( pPanel == m_pEnableButton )
	{
		if( m_pEnableButton->IsSelected() )
		{
			PostActionSignal( new KeyValues( "Command", "Command", "EnableColorCorrection" ) );
			m_bEnable = true;
		}
		else
		{
			PostActionSignal( new KeyValues( "Command", "Command", "DisableColorCorrection" ) );
			m_bEnable = false;
		}
	}
	else if ( pPanel == m_pEnableEntitiesButton )
	{
		if( m_pEnableEntitiesButton->IsSelected() )
		{
			m_bEnableEntities = true;
			mat_colcorrection_disableentities.SetValue( 0 );
		}
		else
		{
			m_bEnableEntities = false;
			mat_colcorrection_disableentities.SetValue( 1 );
		}
	}
}

void CColorOperationListPanel::OnCommand( const char *command )
{
	if( !Q_stricmp( command, "NewOperation" ) )
	{
		if( !m_pNewDialog )
		{
			m_pNewDialog = new CNewOperationDialog( this, &m_OperationList );
			m_pNewDialog->AddActionSignalTarget( this );
			m_pNewDialog->Activate();
		}
	}
	else if( !Q_stricmp( command, "DeleteOperation" ) )
	{
		if( m_pOperationListPanel->GetSelectedItemsCount()!=0 )
		{
			int nSelectedItem = m_pOperationListPanel->GetSelectedItem( 0 );

			IColorOperation *pOp = m_OperationList.GetOperation( nSelectedItem );
			m_OperationList.DeleteOperation( nSelectedItem );

			for( int i=0;i<m_OpPanelList.Count();i++ )
			{
				if( m_OpPanelList[i]->GetOperation()==pOp )
				{
					CColorCorrectionUIChildPanel *panel = m_OpPanelList[i];
					delete panel;

					m_OpPanelList.Remove( i );
					break;
				}
			}

			PopulateList( );

			colorcorrectiontools->UpdateColorCorrection( );
		}
	}
	else if( !Q_stricmp( command, "BringForward" ) )
	{
		int nSelectedItem = m_pOperationListPanel->GetSelectedItem( 0 );
		if( nSelectedItem!=0 )
		{
			m_OperationList.BringForward( nSelectedItem );

			PopulateList( );

			colorcorrectiontools->UpdateColorCorrection( );

			m_pOperationListPanel->SetSingleSelectedItem( nSelectedItem-1 );
		}
	}
	else if( !Q_stricmp( command, "PushBack" ) )
	{
		int nSelectedItem = m_pOperationListPanel->GetSelectedItem( 0 );
		if( nSelectedItem<m_OperationList.GetNumOperations()-1 )
		{
			m_OperationList.PushBack( nSelectedItem );

			PopulateList( );

			colorcorrectiontools->UpdateColorCorrection( );

			m_pOperationListPanel->SetSingleSelectedItem( nSelectedItem+1 );
		}
	}
	else if( !Q_stricmp( command, "Save" ) )
	{
 		FileOpenDialog *save_dialog = new FileOpenDialog( this, "File Save", false );
		save_dialog->AddActionSignalTarget( this );
		save_dialog->AddFilter( "*.raw", ".RAW files", true );
		save_dialog->DoModal( true );
	}
	else if( !Q_stricmp( command, "NewComplete" ) )
	{
		if( m_pNewDialog )
		{
			delete m_pNewDialog;
			m_pNewDialog = 0;
		}

		PopulateList( );

		m_pOperationListPanel->SetSingleSelectedItem( m_pOperationListPanel->GetItemCount()-1 );

		OnKeyCodeTyped( KEY_ENTER );
	}
	else if( !Q_stricmp( command, "NewCancel" ) )
	{
		if( m_pNewDialog )
		{
			delete m_pNewDialog;
			m_pNewDialog = 0;
		}
	}
	else if( !Q_stricmp( command, "SelectedItemChanged" ) )
	{
		ResetSlider();
	}
	else if( !Q_stricmp( command, "BlendFactorUpdate" ) )
	{
		ResetSlider();
	}
	else if( !Q_stricmp( command, "UpdateList" ) )
	{
		PopulateList();
	}
	else if( !Q_stricmp( command, "CloneOperation" ) )
	{
		int nSelectedItem = m_pOperationListPanel->GetSelectedItem( 0 );

		IColorOperation *pOp = m_OperationList.GetOperation( nSelectedItem );
		IColorOperation *pCloneOp = pOp->Clone();
        
		m_OperationList.AddOperation( pCloneOp );

		PopulateList();
	}
}

void CColorOperationListPanel::OnThink( )
{
	BaseClass::OnThink();

	if( m_bEnable )
	{
		colorcorrection->SetLookupWeight( m_CCHandle, 1.0f );
	}
	else
	{
		colorcorrection->SetLookupWeight( m_CCHandle, 0.0f );
	}
}

void CColorOperationListPanel::ResetSlider( )
{
	int nSelectedItem = m_pOperationListPanel->GetSelectedItem( 0 );
	if( nSelectedItem>=0 && nSelectedItem<m_pOperationListPanel->GetItemCount() )
	{
		IColorOperation *pOp = (IColorOperation *)m_pOperationListPanel->GetItemUserData( nSelectedItem );
		float flBlend = pOp->GetBlendFactor();

        m_pBlendFactorSlider->SetValue( flBlend*255.0f );
		m_pBlendFactorSlider->SetEnabled( true );
	}
	else
	{
        m_pBlendFactorSlider->SetValue( 0 );
		m_pBlendFactorSlider->SetEnabled( false );
	}
}

void CColorOperationListPanel::PopulateList( )
{
	m_pOperationListPanel->DeleteAllItems( );

	int numItems = m_OperationList.GetNumOperations();

	for( int i=0;i<numItems;i++ )
	{
		IColorOperation *op = m_OperationList.GetOperation( i );
		if( op )
		{
			KeyValues *kv = new KeyValues( "operation", "layer", op->GetName() );
			kv->SetInt( "image", (op->IsEnabled())?1:0 );
			
			m_pOperationListPanel->AddItem( kv, (unsigned int)op, false, false );
		}
	}
}

CColorOperationListPanel::~CColorOperationListPanel()
{
}

void CColorOperationListPanel::Init()
{
	m_pLookupViewWindow->Init();
}

void CColorOperationListPanel::Shutdown()
{
	m_pLookupViewWindow->Shutdown();
	m_OperationList.Clear();
}

CColorOperationList *CColorOperationListPanel::GetOperationList( )
{
	return &m_OperationList;
}

void CColorOperationListPanel::OnMouseDoublePressed( MouseCode code )
{
	BaseClass::OnMouseDoublePressed( code );

	if( code==MOUSE_LEFT )
	{
	}
}

void CColorOperationListPanel::OnKeyCodeTyped( KeyCode code )
{
	if( code==KEY_ENTER )
	{
		int nSelectedItem = m_pOperationListPanel->GetSelectedItem( 0 );
		IColorOperation *pSelectedOp = m_OperationList.GetOperation( nSelectedItem );
		
		LaunchOperationPanel( pSelectedOp );
	}
	else if( code==KEY_ESCAPE )
	{
		void ShowHideColorCorrectionUI();
		ShowHideColorCorrectionUI();
	}

	BaseClass::OnKeyCodeTyped( code );
}

void CColorOperationListPanel::OnFileSelected( const char *pFilename )
{
	FileHandle_t file_handle = g_pFileSystem->Open( pFilename, "wb" );

	colorcorrection->LockLookup( m_CCHandle );
    
	RGBX5551_t inColor;

	inColor.b = 0;
	for ( int b = 0; b < 32; ++b, ++inColor.b )
	{
		inColor.g = 0;
		for ( int g = 0; g < 32; ++g, ++inColor.g )
		{
			inColor.r = 0;
			for ( int r = 0; r < 32; ++r, ++inColor.r )
			{
				color24 outColor;

				outColor = colorcorrection->GetLookup( m_CCHandle, inColor );
				g_pFileSystem->Write( &outColor, sizeof(color24), file_handle );
			}
		}
	}

	colorcorrection->UnlockLookup( m_CCHandle );

	g_pFileSystem->Close( file_handle );
}

void CColorOperationListPanel::LaunchOperationPanel( IColorOperation *pOp )
{
	if( pOp )
	{
		for( int i=0;i<m_OpPanelList.Count();i++ )
		{
			CColorCorrectionUIChildPanel *panel = m_OpPanelList[i];
			if( panel->GetOperation()==pOp )
			{
				panel->Activate();
				return;
			}
		}
		
		CColorCorrectionUIChildPanel *pOpPanel = 0;
		switch( pOp->ToolID() )
		{
			case CC_TOOL_BALANCE: pOpPanel = new CColorBalanceUIPanel( this, (CColorBalanceOperation *)pOp ); break;
			case CC_TOOL_CURVES:  pOpPanel = new CColorCurvesUIPanel(  this, (CCurvesColorOperation *)pOp );  break;
			case CC_TOOL_LEVELS:  pOpPanel = new CColorLevelsUIPanel(  this, (CLevelsColorOperation *)pOp );  break;
			case CC_TOOL_LOOKUP:  pOpPanel = new CColorLookupUIPanel(  this, (CColorLookupOperation *)pOp );  break;
			case CC_TOOL_SELECTED_HSV: pOpPanel = new CSelectedHSVUIPanel( this, (CSelectedHSVOperation *)pOp ); break;
		}

		int parentX, parentY;
		GetParent()->GetPos( parentX, parentY );

		int maxPanels = parentX / 250;
		int panelOffset = (m_OpPanelList.Count()+1<maxPanels)?m_OpPanelList.Count()+1:maxPanels;

		int xPos = parentX - 250*panelOffset;

		pOpPanel->SetPos(  xPos, parentY );
		pOpPanel->SetSize( 250, 480 );
		pOpPanel->SetTitle( pOp->GetName(), true );
		pOpPanel->AddActionSignalTarget( this );
		pOpPanel->SetSizeable( false );
		pOpPanel->SetVisible( true );
		pOpPanel->Init( );

		m_OpPanelList.AddToTail( pOpPanel );
	}
}

void CColorOperationListPanel::ReadUncorrectedImage( Rect_t *pSrcRect, unsigned char *pPreviewImage )
{
	for( int i=0;i<m_OpPanelList.Count();i++ )
	{
		CColorCorrectionUIChildPanel *pPanel = m_OpPanelList[i];
		pPanel->ReadUncorrectedImage( pSrcRect, pPreviewImage );
	}
}

void CColorOperationListPanel::UpdateColorCorrection()
{
	m_pLookupViewWindow->UpdateColorCorrection();
}

//-----------------------------------------------------------------------------
//
// CColorCorrectionUIPanel begins here
//
//-----------------------------------------------------------------------------
class CColorCorrectionUIPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CColorCorrectionUIPanel, vgui::Frame );

public:
	CColorCorrectionUIPanel( vgui::Panel *parent );
	~CColorCorrectionUIPanel();

	// Command issued
	virtual void	OnCommand(const char *command);

	virtual void	Activate();

	void			Init();
	void			Shutdown();

	virtual void	OnKeyCodeTyped(KeyCode code);

	virtual void	OnThink( );

	void			ReadUncorrectedImage( Rect_t *pSrcRect, unsigned char *pPreviewImage );

	// Updates the color correction terms
	void			UpdateColorCorrection( );

	void			SetFinalOperation( IColorOperation *pOp );

protected:

	CColorOperationListPanel *m_pOperationListPanel;

	IColorOperation	*m_pFinalOperation;

	bool			m_bEnable;

	ColorCorrectionHandle_t	m_CCHandle;

	int				m_nRowStep;
	int				m_nCurrentRow;
	color24			m_pLookupCache[ 32*32*32 ];

	bool			m_bForceReset;
    
private:
};


//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
CColorCorrectionUIPanel::CColorCorrectionUIPanel( vgui::Panel *parent ) : BaseClass( parent, "ColorCorrectionUIPanel" )
{
	if ( !colorcorrection )
	{
		m_pOperationListPanel = NULL;
		m_CCHandle = 0;
		Warning( "Could not get the color correction interface!" );
		Shutdown();
		return;
	}

	m_CCHandle = colorcorrection->AddLookup( "editable" );
	colorcorrection->SetResetable( m_CCHandle, true );

	m_bForceReset = true;
	m_bEnable = false;

	SetTitle("Color Correction Tools", true);

	m_pOperationListPanel = new CColorOperationListPanel( this, m_CCHandle );
	m_pOperationListPanel->AddActionSignalTarget( this );

	LoadControlSettings("Resource\\ColorCorrectionUIPanel.res");

	// Hidden by default
	SetVisible( false );

	SetSizeable( false );
	SetMoveable( true );

	int w = 250;
	int h = 480;

	int x = videomode->GetModeStereoWidth() - w - 10;
	int y = videomode->GetModeStereoHeight() - h - 10;
	SetBounds( x, y, w, h );

	m_pOperationListPanel->PopulateList( );

	Q_memset( m_pLookupCache, 0x00, sizeof(color24)*32*32*32 );
	m_nCurrentRow = -1;
	m_nRowStep = 4;

	m_pFinalOperation = NULL;
}

CColorCorrectionUIPanel::~CColorCorrectionUIPanel()
{
	colorcorrection->RemoveLookup( m_CCHandle );
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
void CColorCorrectionUIPanel::Init()
{
	m_pOperationListPanel->Init();
}

void CColorCorrectionUIPanel::Shutdown()
{
	if ( m_pOperationListPanel )
	{
		m_pOperationListPanel->Shutdown();
	}
}

//-----------------------------------------------------------------------------
// Updates the color correction terms
//-----------------------------------------------------------------------------
void CColorCorrectionUIPanel::UpdateColorCorrection( )
{
	if( !m_bEnable )
		return;

	m_nCurrentRow = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Shows the panel
//-----------------------------------------------------------------------------
void CColorCorrectionUIPanel::Activate()
{
	BaseClass::Activate();
}

void CColorCorrectionUIPanel::OnCommand( char const *command )
{
	BaseClass::OnCommand( command );

	if( !Q_stricmp( "EnableColorCorrection", command ) )
	{
		m_bEnable = true;
		UpdateColorCorrection();

		colorcorrection->SetResetable( m_CCHandle, false );
	}
	else if( !Q_stricmp( "DisableColorCorrection", command ) )
	{
		m_bEnable = false;
		UpdateColorCorrection();

		colorcorrection->SetResetable( m_CCHandle, true );
	}
}

void CColorCorrectionUIPanel::OnThink( )
{
	BaseClass::OnThink();

	if( m_bForceReset )
	{
		colorcorrection->LockLookup( m_CCHandle );
		colorcorrection->ResetLookup( m_CCHandle );
		colorcorrection->UnlockLookup( m_CCHandle );
		m_bForceReset = false;
	}

	if( m_nCurrentRow!=-1 )
	{
		RGBX5551_t inColor;

		inColor.r = m_nCurrentRow;
		for ( int r = m_nCurrentRow; r < 32 && r < (m_nCurrentRow+32/m_nRowStep); ++r, ++inColor.r )
		{
			inColor.g = 0;
			for ( int g = 0; g < 32; ++g, ++inColor.g )
			{
				inColor.b = 0;
				for ( int b = 0; b < 32; ++b, ++inColor.b )
				{
					color24 outColor;
					color24 directColor = colorcorrection->ConvertToColor24( inColor );
					if( m_bEnable )
					{
						m_pOperationListPanel->GetOperationList()->Apply( directColor, outColor, m_pFinalOperation );
					}
					else
					{
						outColor = directColor;
					}

					m_pLookupCache[ inColor.r + inColor.g*32 + inColor.b*32*32 ] = outColor;
				}
			}
		}

		m_nCurrentRow+=32/m_nRowStep;
		if( m_nCurrentRow==32 )
		{
			colorcorrection->LockLookup( m_CCHandle );
			colorcorrection->CopyLookup( m_CCHandle, m_pLookupCache );
			colorcorrection->UnlockLookup( m_CCHandle );

			m_pOperationListPanel->UpdateColorCorrection();

			m_nCurrentRow = -1;
		}
	}
}

//-----------------------------------------------------------------------------
// Pass down the uncorrected image for panels that need it 
//-----------------------------------------------------------------------------
void CColorCorrectionUIPanel::ReadUncorrectedImage( Rect_t *pSrcRect, unsigned char *pPreviewImage )
{
	m_pOperationListPanel->ReadUncorrectedImage( pSrcRect, pPreviewImage );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CColorCorrectionUIPanel::OnKeyCodeTyped(KeyCode code)
{
	switch( code )
	{
	case KEY_ESCAPE:
		Close();
		break;

	default:
		BaseClass::OnKeyCodeTyped( code );
		break;
	}
}


void CColorCorrectionUIPanel::SetFinalOperation( IColorOperation *pOp ) 
{ 
	m_pFinalOperation = pOp; 
	UpdateColorCorrection( );
}


//-----------------------------------------------------------------------------
// Main interface to the performance tools 
//-----------------------------------------------------------------------------
static CColorCorrectionUIPanel *g_pColorCorrectionUI = NULL;

class CColorCorrectionTools : public IColorCorrectionTools
{
public:
	virtual void		Init( void );
	virtual void		Shutdown( void );

	virtual void		InstallColorCorrectionUI( vgui::Panel *parent );
	virtual bool		ShouldPause() const;

	virtual void		GrabPreColorCorrectedFrame( int x, int y, int width, int height );
	virtual void		UpdateColorCorrection( );

	virtual void		SetFinalOperation( IColorOperation *pOp );

private:

	BGRA8888_t	*m_pPreviewImage;
};

static CColorCorrectionTools g_ColorCorrectionTools;
IColorCorrectionTools *colorcorrectiontools = &g_ColorCorrectionTools;

void CColorCorrectionTools::Init( void )
{
	if ( g_pColorCorrectionUI )
	{
		g_pColorCorrectionUI->Init();
	}

	m_pPreviewImage = new BGRA8888_t[ g_nPreviewImageWidth*g_nPreviewImageHeight ];
}

void CColorCorrectionTools::Shutdown( void )
{
	if ( g_pColorCorrectionUI )
	{
		g_pColorCorrectionUI->Shutdown();
	}

	delete [] m_pPreviewImage;
}

void CColorCorrectionTools::InstallColorCorrectionUI( vgui::Panel *parent )
{
	if ( g_pColorCorrectionUI )
		return;

	g_pColorCorrectionUI = new CColorCorrectionUIPanel( parent );
	Assert( g_pColorCorrectionUI );
}

bool CColorCorrectionTools::ShouldPause() const
{
	return false;
}

void CColorCorrectionTools::GrabPreColorCorrectedFrame( int x, int y, int width, int height )
{
	if ( !g_pColorCorrectionUI->IsVisible() )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	Rect_t srcRect;
	srcRect.x = y; srcRect.y = y;
	srcRect.width = width; srcRect.height = height;

	Rect_t dstRect;
	dstRect.x = 0;
	dstRect.y = 0;
	dstRect.width  = g_nPreviewImageWidth;
	dstRect.height = g_nPreviewImageHeight;

	pRenderContext->ReadPixelsAndStretch( &srcRect, &dstRect, (unsigned char*)m_pPreviewImage, IMAGE_FORMAT_BGRX8888, g_nPreviewImageWidth*4 );

	g_pColorCorrectionUI->ReadUncorrectedImage( &srcRect, (unsigned char *)m_pPreviewImage );
}

void CColorCorrectionTools::UpdateColorCorrection( )
{
	g_pColorCorrectionUI->UpdateColorCorrection( );
}

void CColorCorrectionTools::SetFinalOperation( IColorOperation *pOp )
{
	g_pColorCorrectionUI->SetFinalOperation( pOp );
}

void ShowHideColorCorrectionUI()
{
	if ( !g_pColorCorrectionUI )
		return;

	bool bWasVisible = g_pColorCorrectionUI->IsVisible();

	if ( bWasVisible )
	{
		// hide
		g_pColorCorrectionUI->Close();
	}
	else
	{
		g_pColorCorrectionUI->Activate();
	}
}

static ConCommand colorcorrectionui( "colorcorrectionui", ShowHideColorCorrectionUI, "Show/hide the color correction tools UI.", FCVAR_CHEAT );

void PrintColorCorrection()
{
	ConMsg( "Default weight : %0.5f\n", colorcorrection->GetLookupWeight(-1) );
	ConMsg( "Weight 0       : %0.5f\n", colorcorrection->GetLookupWeight(0) );
	ConMsg( "Weight 1       : %0.5f\n", colorcorrection->GetLookupWeight(1) );
	ConMsg( "Weight 2       : %0.5f\n", colorcorrection->GetLookupWeight(2) );
	ConMsg( "Weight 3       : %0.5f\n", colorcorrection->GetLookupWeight(3) );
}

static ConCommand print_colorcorrection( "print_colorcorrection", PrintColorCorrection, "Display the color correction layer information.", FCVAR_CHEAT );

