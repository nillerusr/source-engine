//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "OptionsSubVideo.h"
#include "cvarslider.h"
#include "EngineInterface.h"
#include "BasePanel.h"
#include "IGameUIFuncs.h"
#include "modes.h"
#include "materialsystem/materialsystem_config.h"
#include "filesystem.h"
#include "GameUI_Interface.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/QueryBox.h"
#include "CvarToggleCheckButton.h"
#include "tier1/KeyValues.h"
#include "vgui/IInput.h"
#include "vgui/ILocalize.h"
#include "vgui/ISystem.h"
#include "tier0/icommandline.h"
#include "tier1/convar.h"
#include "ModInfo.h"
#include "vgui_controls/Tooltip.h"
#include "sourcevr/isourcevirtualreality.h"

#if defined( USE_SDL )
#include "SDL.h"
#endif

#include "inetchannelinfo.h"

extern IMaterialSystem *materials;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: aspect ratio mappings (for normal/widescreen combo)
//-----------------------------------------------------------------------------
struct RatioToAspectMode_t
{
	int anamorphic;
	float aspectRatio;
};
RatioToAspectMode_t g_RatioToAspectModes[] =
{
	{	0,		4.0f / 3.0f },
	{	1,		16.0f / 9.0f },
	{	2,		16.0f / 10.0f },
	{	2,		1.0f },
};

struct AAMode_t
{
	int m_nNumSamples;
	int m_nQualityLevel;
};

//-----------------------------------------------------------------------------
// Purpose: list of valid dx levels
//-----------------------------------------------------------------------------
int g_DirectXLevels[] =
{
	70,
	80,
	81,
	90,
#if DX_TO_GL_ABSTRACTION
	92,
#endif
	95,
};

//-----------------------------------------------------------------------------
// Purpose: returns the string name of a given dxlevel
//-----------------------------------------------------------------------------
void GetNameForDXLevel( int dxlevel, char *name, int bufferSize)
{
	if ( ( dxlevel >= 92 ) && ( dxlevel <= 95 ) )
	{
		Q_snprintf( name, bufferSize, "DirectX v9.0+" );
	}
	else
	{
		Q_snprintf( name, bufferSize, "DirectX v%.1f", dxlevel / 10.0f );
	}
}
	
//-----------------------------------------------------------------------------
// Purpose: returns the aspect ratio mode number for the given resolution
//-----------------------------------------------------------------------------
int GetScreenAspectMode( int width, int height )
{
	float aspectRatio = (float)width / (float)height;

	// just find the closest ratio
	float closestAspectRatioDist = 99999.0f;
	int closestAnamorphic = 0;
	for (int i = 0; i < ARRAYSIZE(g_RatioToAspectModes); i++)
	{
		float dist = fabs( g_RatioToAspectModes[i].aspectRatio - aspectRatio );
		if (dist < closestAspectRatioDist)
		{
			closestAspectRatioDist = dist;
			closestAnamorphic = g_RatioToAspectModes[i].anamorphic;
		}
	}

	return closestAnamorphic;
}

//-----------------------------------------------------------------------------
// Purpose: returns the string name of the specified resolution mode
//-----------------------------------------------------------------------------
static void GetResolutionName( vmode_t *mode, char *sz, int sizeofsz, int desktopWidth, int desktopHeight )
{
	Q_snprintf( sz, sizeofsz, "%i x %i%s", mode->width, mode->height,
				( mode->width == desktopWidth ) && ( mode->height == desktopHeight ) ? " (native)": "" );
}

//-----------------------------------------------------------------------------
// Purpose: Gamma-adjust dialog
//-----------------------------------------------------------------------------
class CGammaDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CGammaDialog, vgui::Frame );
public:
	CGammaDialog( vgui::VPANEL hParent ) : BaseClass( NULL, "OptionsSubVideoGammaDlg" )
	{
		// parent is ignored, since we want look like we're steal focus from the parent (we'll become modal below)
		SetTitle("#GameUI_AdjustGamma_Title", true);
		SetSize( 400, 260 );
		SetDeleteSelfOnClose( true );

		m_pGammaSlider = new CCvarSlider( this, "Gamma", "#GameUI_Gamma", 1.6f, 2.6f, "mat_monitorgamma" );
		m_pGammaLabel = new Label( this, "Gamma label", "#GameUI_Gamma" );
		m_pGammaEntry = new TextEntry( this, "GammaEntry" );

		Button *ok = new Button( this, "OKButton", "#vgui_ok" );
		ok->SetCommand( new KeyValues("OK") );

		LoadControlSettings( "resource/OptionsSubVideoGammaDlg.res" );
		MoveToCenterOfScreen();
		SetSizeable( false );

		m_pGammaSlider->SetTickCaptions( "#GameUI_Light", "#GameUI_Dark" );
	}

	MESSAGE_FUNC_PTR( OnGammaChanged, "SliderMoved", panel )
	{
		if (panel == m_pGammaSlider)
		{
			m_pGammaSlider->ApplyChanges();
		}
	}

	virtual void Activate()
	{
		BaseClass::Activate();
		m_flOriginalGamma = m_pGammaSlider->GetValue();
		UpdateGammaLabel();
	}

	MESSAGE_FUNC( OnOK, "OK" )
	{
		// make the gamma stick
		m_flOriginalGamma = m_pGammaSlider->GetValue();
		Close();
	}

	virtual void OnClose()
	{
		// reset to the original gamma
		m_pGammaSlider->SetValue( m_flOriginalGamma );
		m_pGammaSlider->ApplyChanges();
		BaseClass::OnClose();
	}

	void OnKeyCodeTyped(KeyCode code)
	{
		// force ourselves to be closed if the escape key it pressed
		if (code == KEY_ESCAPE)
		{
			Close();
		}
		else
		{
			BaseClass::OnKeyCodeTyped(code);
		}
	}

	MESSAGE_FUNC_PTR( OnControlModified, "ControlModified", panel )
	{
		// the HasBeenModified() check is so that if the value is outside of the range of the
		// slider, it won't use the slider to determine the display value but leave the
		// real value that we determined in the constructor
		if (panel == m_pGammaSlider && m_pGammaSlider->HasBeenModified())
		{
			UpdateGammaLabel();
		}
	}

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel )
	{
		if (panel == m_pGammaEntry)
		{
			char buf[64];
			m_pGammaEntry->GetText(buf, 64);

			float fValue = (float) atof(buf);
			if (fValue >= 1.0)
			{
				m_pGammaSlider->SetSliderValue(fValue);
				PostActionSignal(new KeyValues("ApplyButtonEnable"));
			}
		}
	}

	void UpdateGammaLabel()
	{
		char buf[64];
		Q_snprintf(buf, sizeof( buf ), " %.1f", m_pGammaSlider->GetSliderValue());
		m_pGammaEntry->SetText(buf);
	}


private:
	CCvarSlider			*m_pGammaSlider;
	vgui::Label			*m_pGammaLabel;
	vgui::TextEntry		*m_pGammaEntry;
	float				m_flOriginalGamma;
};


//-----------------------------------------------------------------------------
// Purpose: advanced keyboard settings dialog
//-----------------------------------------------------------------------------
class COptionsSubVideoAdvancedDlg : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( COptionsSubVideoAdvancedDlg, vgui::Frame );
public:
	COptionsSubVideoAdvancedDlg( vgui::Panel *parent ) : BaseClass( parent , "OptionsSubVideoAdvancedDlg" )
	{
		SetTitle("#GameUI_VideoAdvanced_Title", true);
		SetSize( 260, 400 );

		m_pDXLevel = new ComboBox(this, "dxlabel", 6, false );
		const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
		KeyValues *pKeyValues = new KeyValues( "config" );
		materials->GetRecommendedConfigurationInfo( 0, pKeyValues );
		m_pDXLevel->DeleteAllItems();
		for (int i = 0; i < ARRAYSIZE(g_DirectXLevels); i++)
		{
			// don't allow choice of lower dxlevels than the default, 
			// unless we're already at that lower level or have it forced
			if (!CommandLine()->CheckParm("-dxlevel") &&
				g_DirectXLevels[i] != config.dxSupportLevel &&
				g_DirectXLevels[i] < pKeyValues->GetInt("ConVar.mat_dxlevel"))
				continue;

			KeyValues *pTempKV = new KeyValues("config");
			if (g_DirectXLevels[i] == pKeyValues->GetInt("ConVar.mat_dxlevel")
				|| materials->GetRecommendedConfigurationInfo( g_DirectXLevels[i], pTempKV ))
			{
				// add the configuration in the combo
				char szDXLevelName[64];
				GetNameForDXLevel( g_DirectXLevels[i], szDXLevelName, sizeof(szDXLevelName) );
				m_pDXLevel->AddItem( szDXLevelName, new KeyValues("dxlevel", "dxlevel", g_DirectXLevels[i]) );
			}

			pTempKV->deleteThis();
		}
		pKeyValues->deleteThis();

		m_pModelDetail = new ComboBox( this, "ModelDetail", 6, false );
		m_pModelDetail->AddItem("#gameui_low", NULL);
		m_pModelDetail->AddItem("#gameui_medium", NULL);
		m_pModelDetail->AddItem("#gameui_high", NULL);

		m_pTextureDetail = new ComboBox( this, "TextureDetail", 6, false );
		m_pTextureDetail->AddItem("#gameui_low", NULL);
		m_pTextureDetail->AddItem("#gameui_medium", NULL);
		m_pTextureDetail->AddItem("#gameui_high", NULL);
		m_pTextureDetail->AddItem("#gameui_ultra", NULL);

		// Build list of MSAA and CSAA modes, based upon those which are supported by the device
		//
		// The modes that we've seen in the wild to date are as follows (in perf order, fastest to slowest)
		//
		//								2x	4x	6x	8x	16x	8x	16xQ
		//		Texture/Shader Samples	1	1	1	1	1	1	1
		//		Stored Color/Z Samples	2	4	6	4	4	8	8
		//		Coverage Samples		2	4	6	8	16	8	16
		//		MSAA or CSAA			M	M	M	C	C	M	C
		//
		//	The CSAA modes are nVidia only (added in the G80 generation of GPUs)
		//
		m_nNumAAModes = 0;
		m_pAntialiasingMode = new ComboBox( this, "AntialiasingMode", 10, false );
		m_pAntialiasingMode->AddItem("#GameUI_None", NULL);
		m_nAAModes[m_nNumAAModes].m_nNumSamples = 1;
		m_nAAModes[m_nNumAAModes].m_nQualityLevel = 0;
		m_nNumAAModes++;

		if ( materials->SupportsMSAAMode(2) )
		{
			m_pAntialiasingMode->AddItem("#GameUI_2X", NULL);
			m_nAAModes[m_nNumAAModes].m_nNumSamples = 2;
			m_nAAModes[m_nNumAAModes].m_nQualityLevel = 0;
			m_nNumAAModes++;
		}

		if ( materials->SupportsMSAAMode(4) )
		{
			m_pAntialiasingMode->AddItem("#GameUI_4X", NULL);
			m_nAAModes[m_nNumAAModes].m_nNumSamples = 4;
			m_nAAModes[m_nNumAAModes].m_nQualityLevel = 0;
			m_nNumAAModes++;
		}

		if ( materials->SupportsMSAAMode(6) )
		{
			m_pAntialiasingMode->AddItem("#GameUI_6X", NULL);
			m_nAAModes[m_nNumAAModes].m_nNumSamples = 6;
			m_nAAModes[m_nNumAAModes].m_nQualityLevel = 0;
			m_nNumAAModes++;
		}

		if ( materials->SupportsCSAAMode(4, 2) )							// nVidia CSAA			"8x"
		{
			m_pAntialiasingMode->AddItem("#GameUI_8X_CSAA", NULL);
			m_nAAModes[m_nNumAAModes].m_nNumSamples = 4;
			m_nAAModes[m_nNumAAModes].m_nQualityLevel = 2;
			m_nNumAAModes++;
		}

		if ( materials->SupportsCSAAMode(4, 4) )							// nVidia CSAA			"16x"
		{
			m_pAntialiasingMode->AddItem("#GameUI_16X_CSAA", NULL);
			m_nAAModes[m_nNumAAModes].m_nNumSamples = 4;
			m_nAAModes[m_nNumAAModes].m_nQualityLevel = 4;
			m_nNumAAModes++;
		}

		if ( materials->SupportsMSAAMode(8) )
		{
			m_pAntialiasingMode->AddItem("#GameUI_8X", NULL);
			m_nAAModes[m_nNumAAModes].m_nNumSamples = 8;
			m_nAAModes[m_nNumAAModes].m_nQualityLevel = 0;
			m_nNumAAModes++;
		}

		if ( materials->SupportsCSAAMode(8, 2) )							// nVidia CSAA			"16xQ"
		{
			m_pAntialiasingMode->AddItem("#GameUI_16XQ_CSAA", NULL);
			m_nAAModes[m_nNumAAModes].m_nNumSamples = 8;
			m_nAAModes[m_nNumAAModes].m_nQualityLevel = 2;
			m_nNumAAModes++;
		}

		m_pFilteringMode = new ComboBox( this, "FilteringMode", 6, false );
		m_pFilteringMode->AddItem("#GameUI_Bilinear", NULL);
		m_pFilteringMode->AddItem("#GameUI_Trilinear", NULL);
		m_pFilteringMode->AddItem("#GameUI_Anisotropic2X", NULL);
		m_pFilteringMode->AddItem("#GameUI_Anisotropic4X", NULL);
		m_pFilteringMode->AddItem("#GameUI_Anisotropic8X", NULL);
		m_pFilteringMode->AddItem("#GameUI_Anisotropic16X", NULL);

		m_pShadowDetail = new ComboBox( this, "ShadowDetail", 6, false );
		m_pShadowDetail->AddItem("#gameui_low", NULL);
		m_pShadowDetail->AddItem("#gameui_medium", NULL);
		if ( materials->SupportsShadowDepthTextures() )
		{
			m_pShadowDetail->AddItem("#gameui_high", NULL);
		}

		ConVarRef mat_dxlevel( "mat_dxlevel" );

		m_pHDR = new ComboBox( this, "HDR", 6, false );
		m_pHDR->AddItem("#GameUI_hdr_level0", NULL);
		m_pHDR->AddItem("#GameUI_hdr_level1", NULL);

		if ( materials->SupportsHDRMode( HDR_TYPE_INTEGER ) )
		{
			m_pHDR->AddItem("#GameUI_hdr_level2", NULL);
		}
#if 0
		if ( materials->SupportsHDRMode( HDR_TYPE_FLOAT ) )
		{
			m_pHDR->AddItem("#GameUI_hdr_level3", NULL);
		}
#endif

		m_pHDR->SetEnabled( mat_dxlevel.GetInt() >= 80 );

		m_pWaterDetail = new ComboBox( this, "WaterDetail", 6, false );
		m_pWaterDetail->AddItem("#gameui_noreflections", NULL);
		m_pWaterDetail->AddItem("#gameui_reflectonlyworld", NULL);
		m_pWaterDetail->AddItem("#gameui_reflectall", NULL);

		m_pVSync = new ComboBox( this, "VSync", 2, false );
		m_pVSync->AddItem("#gameui_disabled", NULL);
		m_pVSync->AddItem("#gameui_enabled", NULL);

		m_pMulticore = new ComboBox( this, "Multicore", 2, false );
		m_pMulticore->AddItem("#gameui_disabled", NULL);
		m_pMulticore->AddItem("#gameui_enabled", NULL);

		m_pShaderDetail = new ComboBox( this, "ShaderDetail", 6, false );
		m_pShaderDetail->AddItem("#gameui_low", NULL);
		m_pShaderDetail->AddItem("#gameui_high", NULL);

		m_pColorCorrection = new ComboBox( this, "ColorCorrection", 2, false );
		m_pColorCorrection->AddItem("#gameui_disabled", NULL);
		m_pColorCorrection->AddItem("#gameui_enabled", NULL);

		m_pMotionBlur = new ComboBox( this, "MotionBlur", 2, false );
		m_pMotionBlur->AddItem("#gameui_disabled", NULL);
		m_pMotionBlur->AddItem("#gameui_enabled", NULL);

		LoadControlSettings( "resource/OptionsSubVideoAdvancedDlg.res" );
		MoveToCenterOfScreen();
		SetSizeable( false );

		m_pDXLevel->SetEnabled(false);
		
		m_pColorCorrection->SetEnabled( mat_dxlevel.GetInt() >= 90 );
		m_pMotionBlur->SetEnabled( mat_dxlevel.GetInt() >= 90 );
		
		if ( g_pCVar->FindVar( "fov_desired" ) == NULL )
		{
			Panel *pFOV = FindChildByName( "FovSlider" );
			if ( pFOV )
			{
				pFOV->SetVisible( false );
			}

			pFOV = FindChildByName( "FovLabel" );
			if ( pFOV )
			{
				pFOV->SetVisible( false );
			}
		}
		
		MarkDefaultSettingsAsRecommended();

		m_bUseChanges = false;
	}

	virtual void Activate()
	{
		BaseClass::Activate();

		input()->SetAppModalSurface(GetVPanel());

		if (!m_bUseChanges)
		{
			// reset the data
			OnResetData();
		}
	}

	void SetComboItemAsRecommended( vgui::ComboBox *combo, int iItem )
	{
		// get the item text
		wchar_t text[512];
		combo->GetItemText(iItem, text, sizeof(text));

		// append the recommended flag
		wchar_t newText[512];
		_snwprintf( newText, sizeof(newText) / sizeof(wchar_t), L"%ls *", text );

		// reset
		combo->UpdateItem(iItem, newText, NULL);
	}

	int FindMSAAMode( int nAASamples, int nAAQuality )
	{
		// Run through the AA Modes supported by the device
        for ( int nAAMode = 0; nAAMode < m_nNumAAModes; nAAMode++ )
		{
			// If we found the mode that matches what we're looking for, return the index
			if ( ( m_nAAModes[nAAMode].m_nNumSamples == nAASamples) && ( m_nAAModes[nAAMode].m_nQualityLevel == nAAQuality) )
			{
				return nAAMode;
			}
		}

		return 0;	// Didn't find what we're looking for, so no AA
	}

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel )
	{
		if ( panel == m_pDXLevel && RequiresRestart() )
		{
			// notify the user that this will require a disconnect
			QueryBox *box = new QueryBox("#GameUI_SettingRequiresDisconnect_Title", "#GameUI_SettingRequiresDisconnect_Info");
			box->AddActionSignalTarget( this );
			box->SetCancelCommand(new KeyValues("ResetDXLevelCombo"));
			box->DoModal();
		}
	}

	MESSAGE_FUNC( OnGameUIHidden, "GameUIHidden" )	// called when the GameUI is hidden
	{
		Close();
	}

	MESSAGE_FUNC( ResetDXLevelCombo, "ResetDXLevelCombo" )
	{
		ConVarRef mat_dxlevel( "mat_dxlevel" );
		for (int i = 0; i < m_pDXLevel->GetItemCount(); i++)
		{
			KeyValues *kv = m_pDXLevel->GetItemUserData(i);
			if ( kv->GetInt("dxlevel") == mat_dxlevel.GetInt( ) )
			{
				m_pDXLevel->ActivateItem( i );
				break;
			}
		}

		// Reset HDR too
		if ( m_pHDR->IsEnabled() )
		{
			ConVarRef mat_hdr_level("mat_hdr_level");
			Assert( mat_hdr_level.IsValid() );
			m_pHDR->ActivateItem( clamp( mat_hdr_level.GetInt(), 0, 2 ) );
		}
	}

	MESSAGE_FUNC( OK_Confirmed, "OK_Confirmed" )
	{
		m_bUseChanges = true;
		Close();
	}

	void MarkDefaultSettingsAsRecommended()
	{
		// Pull in data from dxsupport.cfg database (includes fine-grained per-vendor/per-device config data)
		KeyValues *pKeyValues = new KeyValues( "config" );
		materials->GetRecommendedConfigurationInfo( 0, pKeyValues );	

		// Read individual values from keyvalues which came from dxsupport.cfg database
		int nSkipLevels = pKeyValues->GetInt( "ConVar.mat_picmip", 0 );
		int nAnisotropicLevel = pKeyValues->GetInt( "ConVar.mat_forceaniso", 1 );
		int nForceTrilinear = pKeyValues->GetInt( "ConVar.mat_trilinear", 0 );
		int nAASamples = pKeyValues->GetInt( "ConVar.mat_antialias", 0 );
		int nAAQuality = pKeyValues->GetInt( "ConVar.mat_aaquality", 0 );
		int nRenderToTextureShadows = pKeyValues->GetInt( "ConVar.r_shadowrendertotexture", 0 );
		int nShadowDepthTextureShadows = pKeyValues->GetInt( "ConVar.r_flashlightdepthtexture", 0 );
#ifndef _X360
		int nWaterUseRealtimeReflection = pKeyValues->GetInt( "ConVar.r_waterforceexpensive", 0 );
#endif
		int nWaterUseEntityReflection = pKeyValues->GetInt( "ConVar.r_waterforcereflectentities", 0 );
		int nMatVSync = pKeyValues->GetInt( "ConVar.mat_vsync", 1 );
		int nRootLOD = pKeyValues->GetInt( "ConVar.r_rootlod", 0 );
		int nReduceFillRate = pKeyValues->GetInt( "ConVar.mat_reducefillrate", 0 );
		int nDXLevel = pKeyValues->GetInt( "ConVar.mat_dxlevel", 0 );
		int nColorCorrection = pKeyValues->GetInt( "ConVar.mat_colorcorrection", 0 );
		int nMotionBlur = pKeyValues->GetInt( "ConVar.mat_motion_blur_enabled", 0 );
		// It doesn't make sense to retrieve this convar from dxsupport, because we'll then have materialsystem setting this config at loadtime. (Also, it only has very minimal support for CPU related configuration.)
		//int nMulticore = pKeyValues->GetInt( "ConVar.mat_queue_mode", 0 );
		int nMulticore = GetCPUInformation()->m_nPhysicalProcessors >= 2;
		
		// Only recommend a dxlevel if there is more than one available
		if ( m_pDXLevel->GetItemCount() > 1 )
		{
			for (int i = 0; i < m_pDXLevel->GetItemCount(); i++)
			{
				KeyValues *kv = m_pDXLevel->GetItemUserData(i);
				if (kv->GetInt("dxlevel") == pKeyValues->GetInt("ConVar.mat_dxlevel"))
				{
					SetComboItemAsRecommended( m_pDXLevel, i );
					break;
				}
			}
		}
	
		SetComboItemAsRecommended( m_pModelDetail, 2 - nRootLOD );
		SetComboItemAsRecommended( m_pTextureDetail, 2 - nSkipLevels );

		switch ( nAnisotropicLevel )
		{
		case 2:
			SetComboItemAsRecommended( m_pFilteringMode, 2 );
			break;
		case 4:
			SetComboItemAsRecommended( m_pFilteringMode, 3 );
			break;
		case 8:
			SetComboItemAsRecommended( m_pFilteringMode, 4 );
			break;
		case 16:
			SetComboItemAsRecommended( m_pFilteringMode, 5 );
			break;
		case 0:
		default:
			if ( nForceTrilinear != 0 )
			{
				SetComboItemAsRecommended( m_pFilteringMode, 1 );
			}
			else
			{
				SetComboItemAsRecommended( m_pFilteringMode, 0 );
			}
			break;
		}

		// Map desired mode to list item number
		int nMSAAMode = FindMSAAMode( nAASamples, nAAQuality );
		SetComboItemAsRecommended( m_pAntialiasingMode, nMSAAMode );

		if ( nShadowDepthTextureShadows )
			SetComboItemAsRecommended( m_pShadowDetail, 2 );	// Shadow depth mapping (in addition to RTT shadows)
		else if ( nRenderToTextureShadows )
			SetComboItemAsRecommended( m_pShadowDetail, 1 );	// RTT shadows
		else
			SetComboItemAsRecommended( m_pShadowDetail, 0 );	// Blobbies

		SetComboItemAsRecommended( m_pShaderDetail, nReduceFillRate ? 0 : 1 );
		
#ifndef _X360
		if ( nWaterUseRealtimeReflection )
#endif
		{
			if ( nWaterUseEntityReflection )
			{
				SetComboItemAsRecommended( m_pWaterDetail, 2 );
			}
			else
			{
				SetComboItemAsRecommended( m_pWaterDetail, 1 );
			}
		}
#ifndef _X360
		else
		{
			SetComboItemAsRecommended( m_pWaterDetail, 0 );
		}
#endif

		SetComboItemAsRecommended( m_pVSync, nMatVSync != 0 );

		SetComboItemAsRecommended( m_pMulticore, nMulticore != 0 );

		SetComboItemAsRecommended( m_pHDR, nDXLevel >= 90 ? 2 : 0 );

		SetComboItemAsRecommended( m_pColorCorrection, nColorCorrection );

		SetComboItemAsRecommended( m_pMotionBlur, nMotionBlur );

		pKeyValues->deleteThis();
	}

	void ApplyChangesToConVar( const char *pConVarName, int value )
	{
		Assert( cvar->FindVar( pConVarName ) );
		char szCmd[256];
		Q_snprintf( szCmd, sizeof(szCmd), "%s %d\n", pConVarName, value );
		engine->ClientCmd_Unrestricted( szCmd );
	}

	virtual void ApplyChanges()
	{
		if ( !m_bUseChanges )
			return;
		
		KeyValues *pActiveItem = m_pDXLevel->GetActiveItemUserData();
		if ( pActiveItem )
		{
			ApplyChangesToConVar( "mat_dxlevel", pActiveItem->GetInt( "dxlevel" ) );
		}
		
		ApplyChangesToConVar( "r_rootlod", 2 - m_pModelDetail->GetActiveItem());
		ApplyChangesToConVar( "mat_picmip", 2 - m_pTextureDetail->GetActiveItem());

		// reset everything tied to the filtering mode, then the switch sets the appropriate one
		ApplyChangesToConVar( "mat_trilinear", false );
		ApplyChangesToConVar( "mat_forceaniso", 1 );
		switch ( m_pFilteringMode->GetActiveItem() )
		{
		case 0:
			break;
		case 1:
			ApplyChangesToConVar( "mat_trilinear", true );
			break;
		case 2:
			ApplyChangesToConVar( "mat_forceaniso", 2 );
			break;
		case 3:
			ApplyChangesToConVar( "mat_forceaniso", 4 );
			break;
		case 4:
			ApplyChangesToConVar( "mat_forceaniso", 8 );
			break;
		case 5:
			ApplyChangesToConVar( "mat_forceaniso", 16 );
			break;
		default:
			// Trilinear.
			ApplyChangesToConVar( "mat_forceaniso", 1 );
			break;
		}

		// Set the AA convars according to the menu item chosen
		int nActiveAAItem = m_pAntialiasingMode->GetActiveItem();
		ApplyChangesToConVar( "mat_antialias", m_nAAModes[nActiveAAItem].m_nNumSamples );
		ApplyChangesToConVar( "mat_aaquality", m_nAAModes[nActiveAAItem].m_nQualityLevel );

		if( m_pHDR->IsEnabled() )
		{
			ConVarRef mat_hdr_level("mat_hdr_level");
			Assert( mat_hdr_level.IsValid() );
			mat_hdr_level.SetValue(m_pHDR->GetActiveItem());
		}

		if ( m_pShadowDetail->GetActiveItem() == 0 )						// Blobby shadows
		{
			ApplyChangesToConVar( "r_shadowrendertotexture", 0 );			// Turn off RTT shadows
			ApplyChangesToConVar( "r_flashlightdepthtexture", 0 );			// Turn off shadow depth textures
		}
		else if ( m_pShadowDetail->GetActiveItem() == 1 )					// RTT shadows only
		{
			ApplyChangesToConVar( "r_shadowrendertotexture", 1 );			// Turn on RTT shadows
			ApplyChangesToConVar( "r_flashlightdepthtexture", 0 );			// Turn off shadow depth textures
		}
		else if ( m_pShadowDetail->GetActiveItem() == 2 )					// Shadow depth textures
		{
			ApplyChangesToConVar( "r_shadowrendertotexture", 1 );			// Turn on RTT shadows
			ApplyChangesToConVar( "r_flashlightdepthtexture", 1 );			// Turn on shadow depth textures
		}

		ApplyChangesToConVar( "mat_reducefillrate", ( m_pShaderDetail->GetActiveItem() > 0 ) ? 0 : 1 );

		switch ( m_pWaterDetail->GetActiveItem() )
		{
		default:
		case 0:
#ifndef _X360
			ApplyChangesToConVar( "r_waterforceexpensive", false );
#endif
			ApplyChangesToConVar( "r_waterforcereflectentities", false );
			break;
		case 1:
#ifndef _X360
			ApplyChangesToConVar( "r_waterforceexpensive", true );
#endif
			ApplyChangesToConVar( "r_waterforcereflectentities", false );
			break;
		case 2:
#ifndef _X360
			ApplyChangesToConVar( "r_waterforceexpensive", true );
#endif
			ApplyChangesToConVar( "r_waterforcereflectentities", true );
			break;
		}

		ApplyChangesToConVar( "mat_vsync", m_pVSync->GetActiveItem() );	 

		int iMC = m_pMulticore->GetActiveItem();
		ApplyChangesToConVar( "mat_queue_mode", (iMC == 0) ? 0 : -1 );	 

		ApplyChangesToConVar( "mat_colorcorrection", m_pColorCorrection->GetActiveItem() );

		ApplyChangesToConVar( "mat_motion_blur_enabled", m_pMotionBlur->GetActiveItem() );
		
		CCvarSlider *pFOV = (CCvarSlider *)FindChildByName( "FOVSlider" );
		if ( pFOV ) 
		{
			pFOV->ApplyChanges();
		}
	}

	virtual void OnResetData()
	{
		ConVarRef mat_dxlevel( "mat_dxlevel" );
		ConVarRef r_rootlod( "r_rootlod" );
		ConVarRef mat_picmip( "mat_picmip" );
		ConVarRef mat_trilinear( "mat_trilinear" );
		ConVarRef mat_forceaniso( "mat_forceaniso" );
		ConVarRef mat_antialias( "mat_antialias" );
		ConVarRef mat_aaquality( "mat_aaquality" );
		ConVarRef mat_vsync( "mat_vsync" );
		ConVarRef mat_queue_mode( "mat_queue_mode" );
		ConVarRef r_flashlightdepthtexture( "r_flashlightdepthtexture" );
#ifndef _X360
		ConVarRef r_waterforceexpensive( "r_waterforceexpensive" );
#endif
		ConVarRef r_waterforcereflectentities( "r_waterforcereflectentities" );
		ConVarRef mat_reducefillrate("mat_reducefillrate" );
		ConVarRef mat_hdr_level( "mat_hdr_level" );
		ConVarRef mat_colorcorrection( "mat_colorcorrection" );
		ConVarRef mat_motion_blur_enabled( "mat_motion_blur_enabled" );
		ConVarRef r_shadowrendertotexture( "r_shadowrendertotexture" );

		ResetDXLevelCombo();

		m_pModelDetail->ActivateItem( 2 - clamp(r_rootlod.GetInt(), 0, 2) );
		m_pTextureDetail->ActivateItem( 2 - clamp(mat_picmip.GetInt(), -1, 2) );

		if ( r_flashlightdepthtexture.GetBool() )		// If we're doing flashlight shadow depth texturing...
		{
			r_shadowrendertotexture.SetValue( 1 );		// ...be sure render to texture shadows are also on
			m_pShadowDetail->ActivateItem( 2 );
		}
		else if ( r_shadowrendertotexture.GetBool() )	// RTT shadows, but not shadow depth texturing
		{
			m_pShadowDetail->ActivateItem( 1 );
		}
		else	// Lowest shadow quality
		{
			m_pShadowDetail->ActivateItem( 0 );
		}

		m_pShaderDetail->ActivateItem( mat_reducefillrate.GetBool() ? 0 : 1 );
		m_pHDR->ActivateItem(clamp(mat_hdr_level.GetInt(), 0, 2));

		switch (mat_forceaniso.GetInt())
		{
		case 2:
			m_pFilteringMode->ActivateItem( 2 );
			break;
		case 4:
			m_pFilteringMode->ActivateItem( 3 );
			break;
		case 8:
			m_pFilteringMode->ActivateItem( 4 );
			break;
		case 16:
			m_pFilteringMode->ActivateItem( 5 );
			break;
		case 0:
		default:
			if (mat_trilinear.GetBool())
			{
				m_pFilteringMode->ActivateItem( 1 );
			}
			else
			{
				m_pFilteringMode->ActivateItem( 0 );
			}
			break;
		}

		// Map convar to item on AA drop-down
		int nAASamples = mat_antialias.GetInt();
		int nAAQuality = mat_aaquality.GetInt();
		int nMSAAMode = FindMSAAMode( nAASamples, nAAQuality );
		m_pAntialiasingMode->ActivateItem( nMSAAMode );
	
		m_pAntialiasingMode->SetEnabled( m_nNumAAModes > 1 );

#ifndef _X360
		if ( r_waterforceexpensive.GetBool() )
#endif
		{
			if ( r_waterforcereflectentities.GetBool() )
			{
				m_pWaterDetail->ActivateItem( 2 );
			}
			else
			{
				m_pWaterDetail->ActivateItem( 1 );
			}
		}
#ifndef _X360
		else
		{
			m_pWaterDetail->ActivateItem( 0 );
		}
#endif

		m_pVSync->ActivateItem( mat_vsync.GetInt() );

		int iMC = mat_queue_mode.GetInt();

		// We (Rick!) have now switched -2 to mean enabled. So this comment has been rendered obsolete:
		//  -- For testing, we have -2, the legacy default setting as meaning multicore is disabled.
		//  -- After that, we'll switch -2 to mean it's enabled.
		m_pMulticore->ActivateItem( (iMC == 0) ? 0 : 1 );

		m_pColorCorrection->ActivateItem( mat_colorcorrection.GetInt() );

		m_pMotionBlur->ActivateItem( mat_motion_blur_enabled.GetInt() );

		// get current hardware dx support level
		char dxVer[64];
		GetNameForDXLevel( mat_dxlevel.GetInt(), dxVer, sizeof( dxVer ) );
		SetControlString("dxlabel", dxVer);

		// get installed version
		char szVersion[64];
		szVersion[0] = 0;
		system()->GetRegistryString( "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\DirectX\\Version", szVersion, sizeof(szVersion) );
		int os = 0, majorVersion = 0, minorVersion = 0, subVersion = 0;
		sscanf(szVersion, "%d.%d.%d.%d", &os, &majorVersion, &minorVersion, &subVersion);
		Q_snprintf(dxVer, sizeof(dxVer), "DirectX v%d.%d", majorVersion, minorVersion);
		SetControlString("dxinstalledlabel", dxVer);
	}

	virtual void OnCommand( const char *command )
	{
		if ( !stricmp(command, "OK") )
		{
			if ( RequiresRestart() )
			{
				// Bring up the confirmation dialog
				QueryBox *box = new QueryBox("#GameUI_SettingRequiresDisconnect_Title", "#GameUI_SettingRequiresDisconnect_Info");
				box->AddActionSignalTarget( this );
				box->SetOKCommand(new KeyValues("OK_Confirmed"));
				box->SetCancelCommand(new KeyValues("ResetDXLevelCombo"));
				box->DoModal();
				box->MoveToFront();
				return;
			}

			m_bUseChanges = true;
			Close();
		}
		else
		{
			BaseClass::OnCommand( command );
		}
	}

	void OnKeyCodeTyped(KeyCode code)
	{
		// force ourselves to be closed if the escape key it pressed
		if (code == KEY_ESCAPE)
		{
			Close();
		}
		else
		{
			BaseClass::OnKeyCodeTyped(code);
		}
	}

	bool RequiresRestart()
	{
		if ( GameUI().IsInLevel() )
		{
			if ( GameUI().IsInBackgroundLevel() )
				return false;
			if ( !GameUI().IsInMultiplayer() )
				return false;

			ConVarRef mat_dxlevel( "mat_dxlevel" );
			KeyValues *pUserData = m_pDXLevel->GetActiveItemUserData();
			Assert( pUserData );
			if ( pUserData && mat_dxlevel.GetInt() != pUserData->GetInt("dxlevel") )
			{
				return true;
			}

			// HDR changed?
			if ( m_pHDR->IsEnabled() )
			{
				ConVarRef mat_hdr_level("mat_hdr_level");
				Assert( mat_hdr_level.IsValid() );
				if ( mat_hdr_level.GetInt() != m_pHDR->GetActiveItem() )
					return true;
			}
		}
		return false;
	}

private:
	bool m_bUseChanges;
	vgui::ComboBox *m_pModelDetail, *m_pTextureDetail, *m_pAntialiasingMode, *m_pFilteringMode;
	vgui::ComboBox *m_pShadowDetail, *m_pHDR, *m_pWaterDetail, *m_pVSync, *m_pMulticore, *m_pShaderDetail;
	vgui::ComboBox *m_pColorCorrection;
	vgui::ComboBox *m_pMotionBlur;
	vgui::ComboBox *m_pDXLevel;

	int m_nNumAAModes;
	AAMode_t m_nAAModes[16];
};

#if defined( USE_SDL )

//-----------------------------------------------------------------------------
// Purpose: Get display index we will go fullscreen on.
//-----------------------------------------------------------------------------
static int getSDLDisplayIndex()
{
	static ConVarRef sdl_displayindex( "sdl_displayindex" );

	Assert( sdl_displayindex.IsValid() );
	return sdl_displayindex.IsValid() ? sdl_displayindex.GetInt() : 0;
}

//-----------------------------------------------------------------------------
// Purpose: Get display index we are currently fullscreen on. (or -1 if none).
//-----------------------------------------------------------------------------
static int getSDLDisplayIndexFullscreen()
{
	static ConVarRef sdl_displayindex_fullscreen( "sdl_displayindex_fullscreen" );

	Assert( sdl_displayindex_fullscreen.IsValid() );
	return sdl_displayindex_fullscreen.IsValid() ? sdl_displayindex_fullscreen.GetInt() : -1;
}

#endif // USE_SDL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
COptionsSubVideo::COptionsSubVideo(vgui::Panel *parent) : PropertyPage(parent, NULL)
{
	m_bRequireRestart = false;

	m_bDisplayedVRModeMessage = false;

	m_pGammaButton = new Button( this, "GammaButton", "#GameUI_AdjustGamma" );
	m_pGammaButton->SetCommand(new KeyValues("OpenGammaDialog"));
	m_pMode = new ComboBox(this, "Resolution", 8, false);
	m_pAspectRatio = new ComboBox( this, "AspectRatio", 6, false );
	m_pVRMode = new ComboBox( this, "VRMode", 2, false );
	m_pAdvanced = new Button( this, "AdvancedButton", "#GameUI_AdvancedEllipsis" );
	m_pAdvanced->SetCommand(new KeyValues("OpenAdvanced"));
	m_pBenchmark = new Button( this, "BenchmarkButton", "#GameUI_LaunchBenchmark" );
	m_pBenchmark->SetCommand(new KeyValues("LaunchBenchmark"));
	m_pThirdPartyCredits = new URLButton(this, "ThirdPartyVideoCredits", "#GameUI_ThirdPartyTechCredits");
	m_pThirdPartyCredits->SetCommand(new KeyValues("OpenThirdPartyVideoCreditsDialog"));
	m_pHDContent = new CheckButton( this, "HDContentButton", "#GameUI_HDContent" );

	char pszAspectName[3][64];
	const wchar_t *unicodeText = g_pVGuiLocalize->Find("#GameUI_AspectNormal");
	g_pVGuiLocalize->ConvertUnicodeToANSI(unicodeText, pszAspectName[0], 32);
	unicodeText = g_pVGuiLocalize->Find("#GameUI_AspectWide16x9");
	g_pVGuiLocalize->ConvertUnicodeToANSI(unicodeText, pszAspectName[1], 32);
	unicodeText = g_pVGuiLocalize->Find("#GameUI_AspectWide16x10");
	g_pVGuiLocalize->ConvertUnicodeToANSI(unicodeText, pszAspectName[2], 32);

	int iNormalItemID = m_pAspectRatio->AddItem( pszAspectName[0], NULL );
	int i16x9ItemID = m_pAspectRatio->AddItem( pszAspectName[1], NULL );
	int i16x10ItemID = m_pAspectRatio->AddItem( pszAspectName[2], NULL );

	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

	int iAspectMode = GetScreenAspectMode( config.m_VideoMode.m_Width, config.m_VideoMode.m_Height );
	switch ( iAspectMode )
	{
	default:
	case 0:
		m_pAspectRatio->ActivateItem( iNormalItemID );
		break;
	case 1:
		m_pAspectRatio->ActivateItem( i16x9ItemID );
		break;
	case 2:
		m_pAspectRatio->ActivateItem( i16x10ItemID );
		break;
	}

	char pszVRModeName[2][64];
	unicodeText = g_pVGuiLocalize->Find("#GameUI_Disabled");
	g_pVGuiLocalize->ConvertUnicodeToANSI(unicodeText, pszVRModeName[0], 32);
	unicodeText = g_pVGuiLocalize->Find("#GameUI_Enabled");
	g_pVGuiLocalize->ConvertUnicodeToANSI(unicodeText, pszVRModeName[1], 32);

	m_pVRMode->AddItem( pszVRModeName[0], NULL );
	m_pVRMode->AddItem( pszVRModeName[1], NULL );

	// Multimonitor under Direct3D requires you to destroy and recreate the device, 
	// which is an operation we don't support as it currently stands. The user can 
	// pass -adapter N to use a different device.
#if defined( USE_SDL ) && defined( DX_TO_GL_ABSTRACTION )
	int numVideoDisplays = SDL_GetNumVideoDisplays();

	m_pWindowed = new vgui::ComboBox( this, "DisplayModeCombo", 5 + numVideoDisplays, false );

	if ( numVideoDisplays <= 1 )
	{
		m_pWindowed->AddItem( "#GameUI_Fullscreen", NULL );
		m_pWindowed->AddItem( "#GameUI_Windowed", NULL );
	}
	else
	{
		// Add something like this:
		//   Full Screen (0)
		//   Full Screen (1)
		//   Windowed
		wchar_t *fullscreenText = g_pVGuiLocalize->Find( "#GameUI_Fullscreen" );

		for ( int i = 0; i < numVideoDisplays; i++ )
		{
			wchar_t ItemText[ 256 ];

			V_swprintf_safe( ItemText, L"%ls (%d)", fullscreenText, i );
			m_pWindowed->AddItem( ItemText, NULL );
		}

		m_pWindowed->AddItem( "#GameUI_Windowed", NULL );
	}

#else
	m_pWindowed = new vgui::ComboBox( this, "DisplayModeCombo", 6, false );

	m_pWindowed->AddItem( "#GameUI_Fullscreen", NULL );
	m_pWindowed->AddItem( "#GameUI_Windowed", NULL );
#endif

	LoadControlSettings("Resource\\OptionsSubVideo.res");

	// Moved down here so we can set the Drop down's
	// menu state after the default (disabled) value is loaded
	PrepareResolutionList();

	// only show the benchmark button if they have the benchmark map
	if ( !g_pFullFileSystem->FileExists("maps/test_hardware.bsp") )
	{
		m_pBenchmark->SetVisible( false );
	}

	if ( ModInfo().HasHDContent() )
	{
		m_pHDContent->SetVisible( true );
	}
	
	// if VR mode isn't available, disable the dropdown
	if( !g_pSourceVR )
	{
		// if sourcevr.dll is missing entirely that means VR mode is not
		// supported in this game. Hide the mode dropdown and its label 
		m_pVRMode->SetVisible( false );

		Panel *label = FindChildByName( "VRModeLabel" );
		if( label )
			label->SetVisible( false );
	}
	else if( !g_pSourceVR->IsHmdConnected() )
	{
		m_pVRMode->ActivateItem( 0 );
		m_pVRMode->SetEnabled( false );
		m_pVRMode->GetTooltip()->SetText( "#GameUI_NoVRTooltip" );
		EnableOrDisableWindowedForVR();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Generates resolution list
//-----------------------------------------------------------------------------
void COptionsSubVideo::PrepareResolutionList()
{
	// get the currently selected resolution
	char sz[256];
	m_pMode->GetText(sz, 256);
	int currentWidth = 0, currentHeight = 0;
	sscanf( sz, "%i x %i", &currentWidth, &currentHeight );

	// Clean up before filling the info again.
	m_pMode->DeleteAllItems();
	m_pAspectRatio->SetItemEnabled(1, false);
	m_pAspectRatio->SetItemEnabled(2, false);

	// get full video mode list
	vmode_t *plist = NULL;
	int count = 0;
	gameuifuncs->GetVideoModes( &plist, &count );

	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

	// Windowed is the last item in the combobox.
	bool bWindowed = ( m_pWindowed->GetActiveItem() >= ( m_pWindowed->GetItemCount() - 1 ) );
	int desktopWidth, desktopHeight;
	gameuifuncs->GetDesktopResolution( desktopWidth, desktopHeight );

#if defined( USE_SDL )
	bool bFullScreenWithMultipleDisplays = ( !bWindowed && ( SDL_GetNumVideoDisplays() > 1 ) );
	if ( bFullScreenWithMultipleDisplays )
	{
		SDL_Rect rect;
#if defined( DX_TO_GL_ABSTRACTION )
		int displayIndex = m_pWindowed->GetActiveItem();
#else
		int displayIndex = materials->GetCurrentAdapter();
#endif

		if ( !SDL_GetDisplayBounds( displayIndex, &rect ) )
		{
			desktopWidth = rect.w;
			desktopHeight = rect.h;
		}
	}

	// If we are switching to fullscreen, and this isn't the mode we're currently in, then
	//	fake things out so the native fullscreen resolution is selected. Stuck this in
	//	because I assume most people will go fullscreen at native resolution, and it's sometimes
	//	difficult to find the native resolution with all the aspect ratio options.
	bool bNewFullscreenDisplay = ( !bWindowed && ( getSDLDisplayIndexFullscreen() != m_pWindowed->GetActiveItem() ) );
	if ( bNewFullscreenDisplay )
	{
		currentWidth = desktopWidth;
		currentHeight = desktopHeight;
	}
#endif

	// iterate all the video modes adding them to the dropdown
	bool bFoundWidescreen = false;
	int selectedItemID = -1;
	for (int i = 0; i < count; i++, plist++)
	{
#if !defined( USE_SDL )
		// don't show modes bigger than the desktop for windowed mode
		if ( bWindowed )
#endif
		{
			if ( plist->width > desktopWidth || plist->height > desktopHeight )
			{
				// Filter out sizes larger than our desktop.
				continue;
			}
		}

		GetResolutionName( plist, sz, sizeof( sz ), desktopWidth, desktopHeight );

		int itemID = -1;
		int iAspectMode = GetScreenAspectMode( plist->width, plist->height );
		if ( iAspectMode > 0 )
		{
			m_pAspectRatio->SetItemEnabled( iAspectMode, true );
			bFoundWidescreen = true;
		}

		// filter the list for those matching the current aspect
		if ( iAspectMode == m_pAspectRatio->GetActiveItem() )
		{
			itemID = m_pMode->AddItem( sz, NULL);
		}

		// try and find the best match for the resolution to be selected
		if ( plist->width == currentWidth && plist->height == currentHeight )
		{
			selectedItemID = itemID;
		}
		else if ( selectedItemID == -1 && plist->width == config.m_VideoMode.m_Width && plist->height == config.m_VideoMode.m_Height )
		{
			selectedItemID = itemID;
		}
	}

	// disable ratio selection if we can't display widescreen.
	m_pAspectRatio->SetEnabled( bFoundWidescreen );

	m_nSelectedMode = selectedItemID;

	if ( selectedItemID != -1 )
	{
		m_pMode->ActivateItem( selectedItemID );
	}
	else
	{
		int Width = config.m_VideoMode.m_Width;
		int Height = config.m_VideoMode.m_Height;

#if defined( USE_SDL )
		// If we are switching to a new display, or the size is greater than the desktop, then
		//	display the desktop width and height.
		if ( bNewFullscreenDisplay || ( Width > desktopWidth ) || ( Height > desktopHeight ) )
		{
			Width = desktopWidth;
			Height = desktopHeight;
		}
#endif

		Q_snprintf( sz, ARRAYSIZE( sz ), "%d x %d", Width, Height );
		m_pMode->SetText( sz );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COptionsSubVideo::~COptionsSubVideo()
{
	if (m_hOptionsSubVideoAdvancedDlg.Get())
	{
		m_hOptionsSubVideoAdvancedDlg->MarkForDeletion();
	}
}


FILE *FOpenGameHDFile( const char *pchMode )
{
	const char *pGameDir = engine->GetGameDirectory();
	char szModSteamInfPath[ 1024 ];
	V_ComposeFileName( pGameDir, "game_hd.txt", szModSteamInfPath, sizeof( szModSteamInfPath ) );

	FILE *fp = fopen( szModSteamInfPath, pchMode );
	return fp;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool COptionsSubVideo::BUseHDContent()
{
	FILE *fp = FOpenGameHDFile( "rb" );
	if ( fp )
	{
		fclose(fp);
		return true;
	}
	return false;

}


//-----------------------------------------------------------------------------
// Purpose: hint the engine to load HD content if possible, logic must match with engine/common.cpp BLoadHDContent
//-----------------------------------------------------------------------------
void COptionsSubVideo::SetUseHDContent( bool bUse )
{
	if ( bUse )
	{
		FILE *fp = FOpenGameHDFile( "wb+" );
		if ( fp )
		{
			fprintf( fp, "If this file exists on disk HD content will be loaded.\n" );
			fclose( fp );
		}
	}
	else
	{
		const char *pGameDir = engine->GetGameDirectory();
		char szModSteamInfPath[ 1024 ];
		V_ComposeFileName( pGameDir, "game_hd.txt", szModSteamInfPath, sizeof( szModSteamInfPath ) );
		_unlink( szModSteamInfPath );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnResetData()
{
	m_bRequireRestart = false;

	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

    // reset UI elements
#if defined( USE_SDL ) && defined( DX_TO_GL_ABSTRACTION )
	int ItemIndex;

	if ( config.Windowed() )
	{
		// Last item in the combobox is Windowed.
		ItemIndex = ( m_pWindowed->GetItemCount() - 1 );
	}
	else
	{
		// Check which fullscreen displayindex is currently selected, and pick it.
		ItemIndex = getSDLDisplayIndex();

		if ( ( ItemIndex < 0 ) || ItemIndex >= ( m_pWindowed->GetItemCount() - 1 ) )
		{
			Assert( 0 );
			ItemIndex = 0;
		}
	}

    m_pWindowed->ActivateItem( ItemIndex );
#else
    m_pWindowed->ActivateItem( config.Windowed() ? 1 : 0 );
#endif

	// reset gamma control
	m_pGammaButton->SetEnabled( !config.Windowed() );

	m_pHDContent->SetSelected( BUseHDContent() );

    SetCurrentResolutionComboItem();

	bool bVREnabled = config.m_nVRModeAdapter != -1;
	m_pVRMode->ActivateItem( bVREnabled ? 1 : 0 );
	EnableOrDisableWindowedForVR();

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVideo::SetCurrentResolutionComboItem()
{
	vmode_t *plist = NULL;
	int count = 0;
	gameuifuncs->GetVideoModes( &plist, &count );

	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

    int resolution = -1;
    for ( int i = 0; i < count; i++, plist++ )
	{
		if ( plist->width == config.m_VideoMode.m_Width && 
			 plist->height == config.m_VideoMode.m_Height )
		{
            resolution = i;
			break;
		}
	}

    if (resolution != -1)
	{
		char sz[256];
		int desktopWidth, desktopHeight;

		gameuifuncs->GetDesktopResolution( desktopWidth, desktopHeight );

#if defined( USE_SDL )
		SDL_Rect rect;
#if defined( DX_TO_GL_ABSTRACTION )
		int displayIndex = getSDLDisplayIndex();
#else
		int displayIndex = materials->GetCurrentAdapter();
#endif

		if ( !SDL_GetDisplayBounds( displayIndex, &rect ) )
		{
			desktopWidth = rect.w;
			desktopHeight = rect.h;
		}
#endif

		GetResolutionName( plist, sz, sizeof(sz), desktopWidth, desktopHeight );
        m_pMode->SetText(sz);
	}
}

//-----------------------------------------------------------------------------
// Purpose: restarts the game
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnApplyChanges()
{
	if ( RequiresRestart() )
	{
		INetChannelInfo *nci = engine->GetNetChannelInfo();
		if ( nci )
		{
			// Only retry if we're not running the server
			const char *pAddr = nci->GetAddress();
			if ( pAddr )
			{
				if ( Q_strncmp(pAddr,"127.0.0.1",9) && Q_strncmp(pAddr,"localhost",9) )
				{
					engine->ClientCmd_Unrestricted( "retry\n" );
				}
				else
				{
					engine->ClientCmd_Unrestricted( "disconnect\n" );
				}
			}
		}
	}

	// apply advanced options
	if (m_hOptionsSubVideoAdvancedDlg.Get())
	{
		m_hOptionsSubVideoAdvancedDlg->ApplyChanges();
	}

	// resolution
	char sz[256];
	if ( m_nSelectedMode == -1 )
	{
		m_pMode->GetText( sz, 256 );
	}
	else
	{
		m_pMode->GetItemText( m_nSelectedMode, sz, 256 );
	}

	int width = 0, height = 0;
	sscanf( sz, "%i x %i", &width, &height );

	// windowed
	bool bConfigChanged = false;
	bool windowed = ( m_pWindowed->GetActiveItem() == ( m_pWindowed->GetItemCount() - 1 ) ) ? true : false;
	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

	bool bVRMode = m_pVRMode->GetActiveItem() != 0;
	if( ( -1 != config.m_nVRModeAdapter ) != bVRMode )
	{
		// let engine fill in mat_vrmode_adapter 
		char szCmd[256];
		Q_snprintf( szCmd, sizeof(szCmd), "mat_enable_vrmode %d\n", bVRMode ? 1 : 0 );
		engine->ClientCmd_Unrestricted( szCmd );

		// force windowed. VR mode ignores this flag and desktop mode needs to be in a window always
		windowed = bVRMode;
	}


	// make sure there is a change
	if ( config.m_VideoMode.m_Width != width
		|| config.m_VideoMode.m_Height != height
		|| config.Windowed() != windowed )
	{
		bConfigChanged = true;
	}

#if defined( USE_SDL )
	if ( !windowed )
	{
		SDL_Rect rect;
		int displayIndexTarget = m_pWindowed->GetActiveItem();
		int displayIndexCurrent = getSDLDisplayIndexFullscreen();

		// Handle going fullscreen from display X to display Y.
		if ( displayIndexCurrent != displayIndexTarget )
		{
			static ConVarRef sdl_displayindex( "sdl_displayindex" );

			if ( sdl_displayindex.IsValid() )
			{
				// Set the displayindex we want to go fullscreen on now.
				sdl_displayindex.SetValue( displayIndexTarget );
				bConfigChanged = true;
			}
		}

		if ( !SDL_GetDisplayBounds( displayIndexTarget, &rect ) )
		{
			// If we are going non-native fullscreen, tweak the resolution to have the same aspect ratio as the display.
			if ( ( width != rect.w ) || ( height != rect.h ) )
			{
				// TODO: We may want a convar to allow folks to mess with their aspect ratio?
				height = ( width * rect.h ) / rect.w;
				bConfigChanged = true;
			}
		}
	}
#endif // USE_SDL

	if ( bConfigChanged )
	{
		// set mode
		char szCmd[ 256 ];
		Q_snprintf( szCmd, sizeof( szCmd ), "mat_setvideomode %i %i %i\n", width, height, windowed ? 1 : 0 );
		engine->ClientCmd_Unrestricted( szCmd );
	}

	if ( ModInfo().HasHDContent() )
	{
		if ( BUseHDContent() != m_pHDContent->IsSelected() )
		{
			SetUseHDContent( m_pHDContent->IsSelected() );
			// Bring up the confirmation dialog
			MessageBox *box = new MessageBox("#GameUI_OptionsRestartRequired_Title", "#GameUI_HDRestartRequired_Info");
			box->DoModal();
			box->MoveToFront();
		}
	}

	// apply changes
	engine->ClientCmd_Unrestricted( "mat_savechanges\n" );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVideo::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( m_pGammaButton )
	{
		const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
		m_pGammaButton->SetEnabled( !config.Windowed() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: enables apply button on data changing
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnTextChanged(Panel *pPanel, const char *pszText)
{
	if (pPanel == m_pMode)
    {
		const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

		m_nSelectedMode = m_pMode->GetActiveItem();

		int w = 0, h = 0;
		sscanf(pszText, "%i x %i", &w, &h);
        if ( config.m_VideoMode.m_Width != w || config.m_VideoMode.m_Height != h )
        {
            OnDataChanged();
        }
    }
    else if (pPanel == m_pAspectRatio)
    {
		PrepareResolutionList();
    }
	else if (pPanel == m_pWindowed)
	{
		PrepareResolutionList();
		OnDataChanged();
	}
	else if ( pPanel == m_pVRMode )
	{
		if ( !m_bDisplayedVRModeMessage )
		{
			bool bVRNowEnabled = m_pVRMode->GetActiveItem() == 1;
			bool bVRWasEnabled = materials->GetCurrentConfigForVideoCard().m_nVRModeAdapter != -1;
			if( bVRWasEnabled != bVRNowEnabled )
			{
				m_bDisplayedVRModeMessage = true;
				MessageBox *box = new MessageBox( "#GameUI_VRMode", "#GameUI_VRModeRelaunchMsg", this );
				box->MoveToFront();
				box->DoModal();
			}
		}

		EnableOrDisableWindowedForVR();
	}
}


//-----------------------------------------------------------------------------
// Purpose: enables windowed combo box
//-----------------------------------------------------------------------------
void		COptionsSubVideo::EnableOrDisableWindowedForVR()
{
	bool bCanBeEnabled = g_pSourceVR && g_pSourceVR->IsHmdConnected();
	bool bVRNowEnabled = m_pVRMode->GetActiveItem() == 1;
	bool bVRWasEnabled = materials->GetCurrentConfigForVideoCard().m_nVRModeAdapter != -1;
	if( bCanBeEnabled && ( bVRNowEnabled || bVRWasEnabled ) )
	{
		m_pWindowed->SetEnabled( false );
		m_pWindowed->ActivateItem( m_pWindowed->GetItemCount() - 1 );
		m_pWindowed->GetTooltip()->SetText( "#GameUI_WindowedTooltip" );
	}
	else
	{
		m_pWindowed->SetEnabled( true );
	}

}


//-----------------------------------------------------------------------------
// Purpose: enables apply button
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnDataChanged()
{
	PostActionSignal(new KeyValues("ApplyButtonEnable"));

}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if the changes requires a restart to take effect
//-----------------------------------------------------------------------------
bool COptionsSubVideo::RequiresRestart()
{
	if ( m_hOptionsSubVideoAdvancedDlg.Get() 
		&& m_hOptionsSubVideoAdvancedDlg->RequiresRestart() )
	{
		return true;
	}

	// make sure there is a change
	return m_bRequireRestart;
}

//-----------------------------------------------------------------------------
// Purpose: Opens advanced video mode options dialog
//-----------------------------------------------------------------------------
void COptionsSubVideo::OpenAdvanced()
{
	if ( !m_hOptionsSubVideoAdvancedDlg.Get() )
	{
		m_hOptionsSubVideoAdvancedDlg = new COptionsSubVideoAdvancedDlg( BasePanel()->FindChildByName( "OptionsDialog" ) ); // we'll parent this to the OptionsDialog directly
	}

	m_hOptionsSubVideoAdvancedDlg->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Opens gamma-adjusting dialog
//-----------------------------------------------------------------------------
void COptionsSubVideo::OpenGammaDialog()
{
	if ( !m_hGammaDialog.Get() )
	{
		m_hGammaDialog = new CGammaDialog( GetVParent() );
	}

	m_hGammaDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Opens benchmark dialog
//-----------------------------------------------------------------------------
void COptionsSubVideo::LaunchBenchmark()
{
	BasePanel()->OnOpenBenchmarkDialog();
}

//-----------------------------------------------------------------------------
// Purpose: third-party audio credits dialog
//-----------------------------------------------------------------------------
class COptionsSubVideoThirdPartyCreditsDlg : public vgui::Frame
{
   DECLARE_CLASS_SIMPLE( COptionsSubVideoThirdPartyCreditsDlg, vgui::Frame );
public:
   COptionsSubVideoThirdPartyCreditsDlg( vgui::VPANEL hParent ) : BaseClass( NULL, NULL )
   {
      // parent is ignored, since we want look like we're steal focus from the parent (we'll become modal below)

      SetTitle("#GameUI_ThirdPartyVideo_Title", true);
      SetSize( 500, 200 );
      LoadControlSettings( "resource/OptionsSubVideoThirdPartyDlg.res" );
      MoveToCenterOfScreen();
      SetSizeable( false );
      SetDeleteSelfOnClose( true );
   }

   virtual void Activate()
   {
      BaseClass::Activate();

      input()->SetAppModalSurface(GetVPanel());
   }

   void OnKeyCodeTyped(KeyCode code)
   {
      // force ourselves to be closed if the escape key it pressed
      if (code == KEY_ESCAPE)
      {
         Close();
      }
      else
      {
         BaseClass::OnKeyCodeTyped(code);
      }
   }
};


//-----------------------------------------------------------------------------
// Purpose: Open third party audio credits dialog
//-----------------------------------------------------------------------------
void COptionsSubVideo::OpenThirdPartyVideoCreditsDialog()
{
   if (!m_OptionsSubVideoThirdPartyCreditsDlg.Get())
   {
      m_OptionsSubVideoThirdPartyCreditsDlg = new COptionsSubVideoThirdPartyCreditsDlg(GetVParent());
   }
   m_OptionsSubVideoThirdPartyCreditsDlg->Activate();
}
