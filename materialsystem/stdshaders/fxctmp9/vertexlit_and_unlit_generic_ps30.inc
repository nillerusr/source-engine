// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ($DETAILTEXTURE == 0 ) && ( $DETAIL_BLEND_MODE != 0 )
// ($DETAILTEXTURE == 0 ) && ( $SEAMLESS_DETAIL )
// ($ENVMAPMASK || $SELFILLUM_ENVMAPMASK_ALPHA) && ($SEAMLESS_BASE || $SEAMLESS_DETAIL)
// $BASEALPHAENVMAPMASK && $ENVMAPMASK
// $BASEALPHAENVMAPMASK && $SELFILLUM
// $SELFILLUM && $SELFILLUM_ENVMAPMASK_ALPHA
// $SELFILLUM_ENVMAPMASK_ALPHA && (! $ENVMAPMASK)
// $ENVMAPMASK && ($FLASHLIGHT || $FLASHLIGHTSHADOWS)
// $BASEALPHAENVMAPMASK && ($SEAMLESS_BASE || $SEAMLESS_DETAIL)
// ($DISTANCEALPHA == 0) && ($DISTANCEALPHAFROMDETAIL || $SOFT_MASK || $OUTLINE || $OUTER_GLOW)
// ($DETAILTEXTURE == 0) && ($DISTANCEALPHAFROMDETAIL)
// ( $FLASHLIGHT == 0 ) && ( $FLASHLIGHTSHADOWS == 1 )
// ( $FLASHLIGHT == 0 ) && ( $FLASHLIGHTDEPTHFILTERMODE != 0 )
// ($DISTANCEALPHA) && ($ENVMAPMASK || $BASEALPHAENVMAPMASK || $SELFILLUM || $SELFILLUM_ENVMAPMASK_ALPHA )
// ($DISTANCEALPHA) && ($SEAMLESS_BASE || $SEAMLESS_DETAIL || $CUBEMAP || $LIGHTING_PREVIEW )
// ($DISTANCEALPHA) && ($WRITEWATERFOGTODESTALPHA || $PIXELFOGTYPE || $FLASHLIGHT || $FLASHLIGHTSHADOWS || $SRGB_INPUT_ADAPTER )
// $SEAMLESS_BASE && $SRGB_INPUT_ADAPTER
// $SEAMLESS_BASE && ($BLENDTINTBYBASEALPHA )
// ($BLENDTINTBYBASEALPHA) && ($SELFILLUM || (($DISTANCEALPHA) && ($DISTANCEALPHAFROMDETAIL == 0)) || $BASEALPHAENVMAPMASK)
// $FLASHLIGHT && $CUBEMAP
// $CUBEMAP_SPHERE_LEGACY && ($CUBEMAP == 0)
// defined $HDRTYPE && defined $HDRENABLED && !$HDRTYPE && $HDRENABLED
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $HDRTYPE && $LIGHTING_PREVIEW && $HDRTYPE != 0
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
// defined $HDRTYPE && defined $HDRENABLED && !$HDRTYPE && $HDRENABLED
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $HDRTYPE && $LIGHTING_PREVIEW && $HDRTYPE != 0
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW

#ifndef VERTEXLIT_AND_UNLIT_GENERIC_PS30_H
#define VERTEXLIT_AND_UNLIT_GENERIC_PS30_H

#include "shaderapi/ishaderapi.h"
#include "shaderapi/ishadershadow.h"
#include "materialsystem/imaterialvar.h"

class vertexlit_and_unlit_generic_ps30_Static_Index
{
	unsigned int m_nDETAILTEXTURE : 2;
	unsigned int m_nCUBEMAP : 2;
	unsigned int m_nDIFFUSELIGHTING : 2;
	unsigned int m_nENVMAPMASK : 2;
	unsigned int m_nBASEALPHAENVMAPMASK : 2;
	unsigned int m_nSELFILLUM : 2;
	unsigned int m_nVERTEXCOLOR : 2;
	unsigned int m_nFLASHLIGHT : 2;
	unsigned int m_nSELFILLUM_ENVMAPMASK_ALPHA : 2;
	unsigned int m_nDETAIL_BLEND_MODE : 4;
	unsigned int m_nSEAMLESS_BASE : 2;
	unsigned int m_nSEAMLESS_DETAIL : 2;
	unsigned int m_nDISTANCEALPHA : 2;
	unsigned int m_nDISTANCEALPHAFROMDETAIL : 2;
	unsigned int m_nSOFT_MASK : 2;
	unsigned int m_nOUTLINE : 2;
	unsigned int m_nOUTER_GLOW : 2;
	unsigned int m_nFLASHLIGHTDEPTHFILTERMODE : 2;
	unsigned int m_nDEPTHBLEND : 2;
	unsigned int m_nBLENDTINTBYBASEALPHA : 2;
	unsigned int m_nCUBEMAP_SPHERE_LEGACY : 2;
#ifdef _DEBUG
	bool m_bDETAILTEXTURE : 1;
	bool m_bCUBEMAP : 1;
	bool m_bDIFFUSELIGHTING : 1;
	bool m_bENVMAPMASK : 1;
	bool m_bBASEALPHAENVMAPMASK : 1;
	bool m_bSELFILLUM : 1;
	bool m_bVERTEXCOLOR : 1;
	bool m_bFLASHLIGHT : 1;
	bool m_bSELFILLUM_ENVMAPMASK_ALPHA : 1;
	bool m_bDETAIL_BLEND_MODE : 1;
	bool m_bSEAMLESS_BASE : 1;
	bool m_bSEAMLESS_DETAIL : 1;
	bool m_bDISTANCEALPHA : 1;
	bool m_bDISTANCEALPHAFROMDETAIL : 1;
	bool m_bSOFT_MASK : 1;
	bool m_bOUTLINE : 1;
	bool m_bOUTER_GLOW : 1;
	bool m_bFLASHLIGHTDEPTHFILTERMODE : 1;
	bool m_bDEPTHBLEND : 1;
	bool m_bBLENDTINTBYBASEALPHA : 1;
	bool m_bCUBEMAP_SPHERE_LEGACY : 1;
#endif	// _DEBUG
public:
	void SetDETAILTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDETAILTEXTURE = i;
#ifdef _DEBUG
		m_bDETAILTEXTURE = true;
#endif	// _DEBUG
	}

	void SetCUBEMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCUBEMAP = i;
#ifdef _DEBUG
		m_bCUBEMAP = true;
#endif	// _DEBUG
	}

	void SetDIFFUSELIGHTING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDIFFUSELIGHTING = i;
#ifdef _DEBUG
		m_bDIFFUSELIGHTING = true;
#endif	// _DEBUG
	}

	void SetENVMAPMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nENVMAPMASK = i;
#ifdef _DEBUG
		m_bENVMAPMASK = true;
#endif	// _DEBUG
	}

	void SetBASEALPHAENVMAPMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASEALPHAENVMAPMASK = i;
#ifdef _DEBUG
		m_bBASEALPHAENVMAPMASK = true;
#endif	// _DEBUG
	}

	void SetSELFILLUM( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSELFILLUM = i;
#ifdef _DEBUG
		m_bSELFILLUM = true;
#endif	// _DEBUG
	}

	void SetVERTEXCOLOR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVERTEXCOLOR = i;
#ifdef _DEBUG
		m_bVERTEXCOLOR = true;
#endif	// _DEBUG
	}

	void SetFLASHLIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLASHLIGHT = i;
#ifdef _DEBUG
		m_bFLASHLIGHT = true;
#endif	// _DEBUG
	}

	void SetSELFILLUM_ENVMAPMASK_ALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSELFILLUM_ENVMAPMASK_ALPHA = i;
#ifdef _DEBUG
		m_bSELFILLUM_ENVMAPMASK_ALPHA = true;
#endif	// _DEBUG
	}

	void SetDETAIL_BLEND_MODE( int i )
	{
		Assert( i >= 0 && i <= 9 );
		m_nDETAIL_BLEND_MODE = i;
#ifdef _DEBUG
		m_bDETAIL_BLEND_MODE = true;
#endif	// _DEBUG
	}

	void SetSEAMLESS_BASE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSEAMLESS_BASE = i;
#ifdef _DEBUG
		m_bSEAMLESS_BASE = true;
#endif	// _DEBUG
	}

	void SetSEAMLESS_DETAIL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSEAMLESS_DETAIL = i;
#ifdef _DEBUG
		m_bSEAMLESS_DETAIL = true;
#endif	// _DEBUG
	}

	void SetDISTANCEALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDISTANCEALPHA = i;
#ifdef _DEBUG
		m_bDISTANCEALPHA = true;
#endif	// _DEBUG
	}

	void SetDISTANCEALPHAFROMDETAIL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDISTANCEALPHAFROMDETAIL = i;
#ifdef _DEBUG
		m_bDISTANCEALPHAFROMDETAIL = true;
#endif	// _DEBUG
	}

	void SetSOFT_MASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSOFT_MASK = i;
#ifdef _DEBUG
		m_bSOFT_MASK = true;
#endif	// _DEBUG
	}

	void SetOUTLINE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nOUTLINE = i;
#ifdef _DEBUG
		m_bOUTLINE = true;
#endif	// _DEBUG
	}

	void SetOUTER_GLOW( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nOUTER_GLOW = i;
#ifdef _DEBUG
		m_bOUTER_GLOW = true;
#endif	// _DEBUG
	}

	void SetFLASHLIGHTDEPTHFILTERMODE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nFLASHLIGHTDEPTHFILTERMODE = i;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = true;
#endif	// _DEBUG
	}

	void SetDEPTHBLEND( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDEPTHBLEND = i;
#ifdef _DEBUG
		m_bDEPTHBLEND = true;
#endif	// _DEBUG
	}

	void SetBLENDTINTBYBASEALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBLENDTINTBYBASEALPHA = i;
#ifdef _DEBUG
		m_bBLENDTINTBYBASEALPHA = true;
#endif	// _DEBUG
	}

	void SetCUBEMAP_SPHERE_LEGACY( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCUBEMAP_SPHERE_LEGACY = i;
#ifdef _DEBUG
		m_bCUBEMAP_SPHERE_LEGACY = true;
#endif	// _DEBUG
	}

	vertexlit_and_unlit_generic_ps30_Static_Index()
	{
		m_nDETAILTEXTURE = 0;
		m_nCUBEMAP = 0;
		m_nDIFFUSELIGHTING = 0;
		m_nENVMAPMASK = 0;
		m_nBASEALPHAENVMAPMASK = 0;
		m_nSELFILLUM = 0;
		m_nVERTEXCOLOR = 0;
		m_nFLASHLIGHT = 0;
		m_nSELFILLUM_ENVMAPMASK_ALPHA = 0;
		m_nDETAIL_BLEND_MODE = 0;
		m_nSEAMLESS_BASE = 0;
		m_nSEAMLESS_DETAIL = 0;
		m_nDISTANCEALPHA = 0;
		m_nDISTANCEALPHAFROMDETAIL = 0;
		m_nSOFT_MASK = 0;
		m_nOUTLINE = 0;
		m_nOUTER_GLOW = 0;
		m_nFLASHLIGHTDEPTHFILTERMODE = 0;
		m_nDEPTHBLEND = 0;
		m_nBLENDTINTBYBASEALPHA = 0;
		m_nCUBEMAP_SPHERE_LEGACY = 0;
#ifdef _DEBUG
		m_bDETAILTEXTURE = false;
		m_bCUBEMAP = false;
		m_bDIFFUSELIGHTING = false;
		m_bENVMAPMASK = false;
		m_bBASEALPHAENVMAPMASK = false;
		m_bSELFILLUM = false;
		m_bVERTEXCOLOR = false;
		m_bFLASHLIGHT = false;
		m_bSELFILLUM_ENVMAPMASK_ALPHA = false;
		m_bDETAIL_BLEND_MODE = false;
		m_bSEAMLESS_BASE = false;
		m_bSEAMLESS_DETAIL = false;
		m_bDISTANCEALPHA = false;
		m_bDISTANCEALPHAFROMDETAIL = false;
		m_bSOFT_MASK = false;
		m_bOUTLINE = false;
		m_bOUTER_GLOW = false;
		m_bFLASHLIGHTDEPTHFILTERMODE = false;
		m_bDEPTHBLEND = false;
		m_bBLENDTINTBYBASEALPHA = false;
		m_bCUBEMAP_SPHERE_LEGACY = false;
#endif	// _DEBUG
	}

	int GetIndex() const
	{
		Assert( m_bDETAILTEXTURE && m_bCUBEMAP && m_bDIFFUSELIGHTING && m_bENVMAPMASK && m_bBASEALPHAENVMAPMASK && m_bSELFILLUM && m_bVERTEXCOLOR && m_bFLASHLIGHT && m_bSELFILLUM_ENVMAPMASK_ALPHA && m_bDETAIL_BLEND_MODE && m_bSEAMLESS_BASE && m_bSEAMLESS_DETAIL && m_bDISTANCEALPHA && m_bDISTANCEALPHAFROMDETAIL && m_bSOFT_MASK && m_bOUTLINE && m_bOUTER_GLOW && m_bFLASHLIGHTDEPTHFILTERMODE && m_bDEPTHBLEND && m_bBLENDTINTBYBASEALPHA && m_bCUBEMAP_SPHERE_LEGACY );
		return ( 6 * m_nDETAILTEXTURE ) + ( 12 * m_nCUBEMAP ) + ( 24 * m_nDIFFUSELIGHTING ) + ( 48 * m_nENVMAPMASK ) + ( 96 * m_nBASEALPHAENVMAPMASK ) + ( 192 * m_nSELFILLUM ) + ( 384 * m_nVERTEXCOLOR ) + ( 768 * m_nFLASHLIGHT ) + ( 1536 * m_nSELFILLUM_ENVMAPMASK_ALPHA ) + ( 3072 * m_nDETAIL_BLEND_MODE ) + ( 30720 * m_nSEAMLESS_BASE ) + ( 61440 * m_nSEAMLESS_DETAIL ) + ( 122880 * m_nDISTANCEALPHA ) + ( 245760 * m_nDISTANCEALPHAFROMDETAIL ) + ( 491520 * m_nSOFT_MASK ) + ( 983040 * m_nOUTLINE ) + ( 1966080 * m_nOUTER_GLOW ) + ( 3932160 * m_nFLASHLIGHTDEPTHFILTERMODE ) + ( 11796480 * m_nDEPTHBLEND ) + ( 23592960 * m_nBLENDTINTBYBASEALPHA ) + ( 47185920 * m_nCUBEMAP_SPHERE_LEGACY ) + 0;
	}
};

#define shaderStaticTest_vertexlit_and_unlit_generic_ps30 psh_forgot_to_set_static_DETAILTEXTURE + psh_forgot_to_set_static_CUBEMAP + psh_forgot_to_set_static_DIFFUSELIGHTING + psh_forgot_to_set_static_ENVMAPMASK + psh_forgot_to_set_static_BASEALPHAENVMAPMASK + psh_forgot_to_set_static_SELFILLUM + psh_forgot_to_set_static_VERTEXCOLOR + psh_forgot_to_set_static_FLASHLIGHT + psh_forgot_to_set_static_SELFILLUM_ENVMAPMASK_ALPHA + psh_forgot_to_set_static_DETAIL_BLEND_MODE + psh_forgot_to_set_static_SEAMLESS_BASE + psh_forgot_to_set_static_SEAMLESS_DETAIL + psh_forgot_to_set_static_DISTANCEALPHA + psh_forgot_to_set_static_DISTANCEALPHAFROMDETAIL + psh_forgot_to_set_static_SOFT_MASK + psh_forgot_to_set_static_OUTLINE + psh_forgot_to_set_static_OUTER_GLOW + psh_forgot_to_set_static_FLASHLIGHTDEPTHFILTERMODE + psh_forgot_to_set_static_DEPTHBLEND + psh_forgot_to_set_static_BLENDTINTBYBASEALPHA + psh_forgot_to_set_static_CUBEMAP_SPHERE_LEGACY


class vertexlit_and_unlit_generic_ps30_Dynamic_Index
{
	unsigned int m_nLIGHTING_PREVIEW : 2;
	unsigned int m_nFLASHLIGHTSHADOWS : 2;
#ifdef _DEBUG
	bool m_bLIGHTING_PREVIEW : 1;
	bool m_bFLASHLIGHTSHADOWS : 1;
#endif	// _DEBUG
public:
	void SetLIGHTING_PREVIEW( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nLIGHTING_PREVIEW = i;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = true;
#endif	// _DEBUG
	}

	void SetFLASHLIGHTSHADOWS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLASHLIGHTSHADOWS = i;
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = true;
#endif	// _DEBUG
	}

	vertexlit_and_unlit_generic_ps30_Dynamic_Index()
	{
		m_nLIGHTING_PREVIEW = 0;
		m_nFLASHLIGHTSHADOWS = 0;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = false;
		m_bFLASHLIGHTSHADOWS = false;
#endif	// _DEBUG
	}

	int GetIndex() const
	{
		Assert( m_bLIGHTING_PREVIEW && m_bFLASHLIGHTSHADOWS );
		return ( 1 * m_nLIGHTING_PREVIEW ) + ( 3 * m_nFLASHLIGHTSHADOWS ) + 0;
	}
};

#define shaderDynamicTest_vertexlit_and_unlit_generic_ps30 psh_forgot_to_set_dynamic_LIGHTING_PREVIEW + psh_forgot_to_set_dynamic_FLASHLIGHTSHADOWS


#endif	// VERTEXLIT_AND_UNLIT_GENERIC_PS30_H
