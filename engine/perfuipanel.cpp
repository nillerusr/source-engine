//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "client_pch.h"

#include <vgui_controls/Frame.h>
#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui_controls/BuildGroup.h>
#include <vgui_controls/Tooltip.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/FileOpenDialog.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/TextEntry.h>
#include <vgui/IInput.h>
#include "engineperftools.h"
#include "vgui_baseui_interface.h"
#include "ivideomode.h"
#include "gl_cvars.h"

#include "utlsymbol.h"
#include "utldict.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "icliententitylist.h"
#include "icliententity.h"
#include "ModelInfo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
extern ConVar r_staticpropinfo;
extern ConVar r_DrawPortals;
extern ConVar r_visocclusion;
extern ConVar r_occlusion;
extern ConVar r_occluderminarea;
extern ConVar r_occludeemaxarea;
extern ConVar mat_wireframe;
extern ConVar sv_cheats;

// If you add a tool, add it to the string list and instance the panel in the constructor below
enum PerformanceTool_t
{
	PERF_TOOL_NONE = 0,
	PERF_TOOL_PROP_FADES,
	PERF_TOOL_AREA_PORTALS,
	PERF_TOOL_OCCLUSION,

	PERF_TOOL_COUNT,

	DEFAULT_PERF_TOOL = PERF_TOOL_NONE,
};

static const char *s_pPerfToolNames[PERF_TOOL_COUNT] = 
{
	"No Tool Active",
	"Prop Fade Distance Tool",
	"Area Portal Tool",
	"Occlusion Tool",
};


//-----------------------------------------------------------------------------
// Base class for all perf tool panels 
//-----------------------------------------------------------------------------
class CPerfUIChildPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CPerfUIChildPanel, vgui::EditablePanel );

public:
	CPerfUIChildPanel( vgui::Panel *parent, const char *pName )  : BaseClass( parent, pName ) 
	{
		SetVisible( false );
	}
	virtual void Activate() {}
	virtual void Deactivate() {}
};


//-----------------------------------------------------------------------------
//
// The prop fade distance helper tool 
//
//-----------------------------------------------------------------------------
class CPropFadeUIPanel : public CPerfUIChildPanel
{
	DECLARE_CLASS_SIMPLE( CPropFadeUIPanel, CPerfUIChildPanel );

public:
	CPropFadeUIPanel( vgui::Panel *parent );
	~CPropFadeUIPanel() {}
	void Activate();
	void Deactivate();

protected:
	enum
	{
		VISUALIZE_NONE = 0,
		VISUALIZE_FADE_DISTANCE,
		VISUALIZE_FADE_SCREEN_WIDTH,

		VISUALIZE_TYPE_COUNT
	};

	void OnVisualizationSelected();
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );

	vgui::ComboBox *m_pVisualization;
	vgui::TextEntry *m_pMinScreenArea;
	vgui::TextEntry *m_pMaxScreenArea;

	static const char *s_pFadeVisualizeLabel[VISUALIZE_TYPE_COUNT]; 
};

const char *CPropFadeUIPanel::s_pFadeVisualizeLabel[CPropFadeUIPanel::VISUALIZE_TYPE_COUNT] = 
{
	"No visualization",
	"Show Fade Distance",
	"Show Fade Screen Width"
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPropFadeUIPanel::CPropFadeUIPanel( vgui::Panel *parent ) : BaseClass( parent, "PropFadeUIPanel")
{
	m_pVisualization = new ComboBox(this, "VisualizeMode", VISUALIZE_TYPE_COUNT, false);
	int i;
	for ( i = 0; i < ARRAYSIZE(s_pFadeVisualizeLabel); i++ )
	{
		m_pVisualization->AddItem( s_pFadeVisualizeLabel[i], NULL );
	}
	m_pVisualization->AddActionSignalTarget( this );
	m_pVisualization->ActivateItem( 0 );

	m_pMinScreenArea = new vgui::TextEntry( this, "MinFadeSize" );
	m_pMaxScreenArea = new vgui::TextEntry( this, "MaxFadeSize" );

	LoadControlSettings("Resource\\PerfPropFadeUIPanel.res");
}


//-----------------------------------------------------------------------------
// Visualization changed 
//-----------------------------------------------------------------------------
void CPropFadeUIPanel::OnTextChanged( KeyValues *data )
{
	Panel *pPanel = reinterpret_cast<vgui::Panel *>( data->GetPtr("panel") );
	vgui::ComboBox *pBox = dynamic_cast<vgui::ComboBox *>( pPanel );

	if( pBox == m_pVisualization ) 
	{
		OnVisualizationSelected();
		return;
	}

	vgui::TextEntry *pText = dynamic_cast<vgui::TextEntry *>( pPanel );
	if (( pText == m_pMinScreenArea ) || ( pText == m_pMaxScreenArea )) 
	{
		char buf[256];
		float flMinArea, flMaxArea;

		m_pMinScreenArea->GetText( buf, 256 );
		int nReadMin = sscanf( buf, "%f", &flMinArea );

		m_pMaxScreenArea->GetText( buf, 256 );
		int nReadMax = sscanf( buf, "%f", &flMaxArea );

		if ( nReadMin && nReadMax )
		{
			modelinfoclient->SetLevelScreenFadeRange( flMinArea, flMaxArea );
		}
	}
}


//-----------------------------------------------------------------------------
// Activate, deactivate: 
//-----------------------------------------------------------------------------
void CPropFadeUIPanel::OnVisualizationSelected()
{
	int tool = m_pVisualization->GetActiveItem();
	switch( tool )
	{
	case VISUALIZE_NONE:
		r_staticpropinfo.SetValue( 0 );
		break;

	case VISUALIZE_FADE_DISTANCE:
		r_staticpropinfo.SetValue( 3 );
		break;

	case VISUALIZE_FADE_SCREEN_WIDTH:
		r_staticpropinfo.SetValue( 4 );
		break;
	}
}


//-----------------------------------------------------------------------------
// Activate, deactivate: 
//-----------------------------------------------------------------------------
void CPropFadeUIPanel::Activate()
{
	float flMinArea, flMaxArea;
	modelinfoclient->GetLevelScreenFadeRange( &flMinArea, &flMaxArea );

	char buf[256];
	Q_snprintf( buf, 256, "%.2f", flMinArea );
	m_pMinScreenArea->SetText( buf );
	Q_snprintf( buf, 256, "%.2f", flMaxArea );
	m_pMaxScreenArea->SetText( buf );

	OnVisualizationSelected();
}

void CPropFadeUIPanel::Deactivate()
{
	r_staticpropinfo.SetValue( 0 );
}

	
//-----------------------------------------------------------------------------
// The areaportals helper tool 
//-----------------------------------------------------------------------------
class CAreaPortalsUIPanel : public CPerfUIChildPanel
{
	DECLARE_CLASS_SIMPLE( CAreaPortalsUIPanel, CPerfUIChildPanel );

public:
	CAreaPortalsUIPanel( vgui::Panel *parent );
	~CAreaPortalsUIPanel() {}
	void Activate();
	void Deactivate();
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAreaPortalsUIPanel::CAreaPortalsUIPanel( vgui::Panel *parent ) : BaseClass( parent, "AreaPortalUIPanel")
{
}


//-----------------------------------------------------------------------------
// Activate, deactivate: 
//-----------------------------------------------------------------------------
void CAreaPortalsUIPanel::Activate()
{
	r_DrawPortals.SetValue( 1 );
	mat_wireframe.SetValue( 3 );
}

void CAreaPortalsUIPanel::Deactivate()
{
	r_DrawPortals.SetValue( 0 );
	mat_wireframe.SetValue( 0 );
}


//-----------------------------------------------------------------------------
// The occlusion helper tool 
//-----------------------------------------------------------------------------
class COcclusionUIPanel : public CPerfUIChildPanel
{
	DECLARE_CLASS_SIMPLE( COcclusionUIPanel, CPerfUIChildPanel );

public:
	COcclusionUIPanel( vgui::Panel *parent );
	~COcclusionUIPanel() {}
	void Activate();
	void Deactivate();

protected:
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );
	MESSAGE_FUNC_PTR( OnCheckButtonChecked, "CheckButtonChecked", panel );

private:
	enum
	{
		VISUALIZE_NONE = 0,
		VISUALIZE_ON,

		VISUALIZE_TYPE_COUNT
	};

	void OnVisualizationSelected();
	void OnDeactivateOcclusion();

	vgui::ComboBox *m_pVisualization;
	vgui::TextEntry *m_pMinOccluderArea;
	vgui::TextEntry *m_pMaxOccludeeArea;
	vgui::CheckButton *m_pDeactivateOcclusion;

	static const char *s_pOccVisualizeLabel[VISUALIZE_TYPE_COUNT]; 
};


const char *COcclusionUIPanel::s_pOccVisualizeLabel[COcclusionUIPanel::VISUALIZE_TYPE_COUNT] = 
{
	"No visualization",
	"View occluders",
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COcclusionUIPanel::COcclusionUIPanel( vgui::Panel *parent ) : BaseClass( parent, "AreaPortalUIPanel")
{
	m_pVisualization = new ComboBox(this, "VisualizeMode", VISUALIZE_TYPE_COUNT, false);
	int i;
	for ( i = 0; i < VISUALIZE_TYPE_COUNT; i++ )
	{
		m_pVisualization->AddItem( s_pOccVisualizeLabel[i], NULL );
	}
	m_pVisualization->AddActionSignalTarget( this );
	m_pVisualization->ActivateItem( 0 );

	m_pMinOccluderArea = new vgui::TextEntry( this, "MinOccluderSize" );
	m_pMaxOccludeeArea = new vgui::TextEntry( this, "MaxOccludeeSize" );

	m_pDeactivateOcclusion = new vgui::CheckButton( this, "DeactivateOcclusion", "" );	
	m_pDeactivateOcclusion->AddActionSignalTarget( this );

	LoadControlSettings("Resource\\PerfOcclusionUIPanel.res");
}


//-----------------------------------------------------------------------------
// Activate, deactivate: 
//-----------------------------------------------------------------------------
void COcclusionUIPanel::Activate()
{
	OnVisualizationSelected();
	OnDeactivateOcclusion();

	char buf[256];
	Q_snprintf( buf, 256, "%.2f", r_occluderminarea.GetFloat() );
	m_pMinOccluderArea->SetText( buf );
	Q_snprintf( buf, 256, "%.2f", r_occludeemaxarea.GetFloat() );
	m_pMaxOccludeeArea->SetText( buf );
}

void COcclusionUIPanel::Deactivate()
{
	r_visocclusion.SetValue( 0 );
	mat_wireframe.SetValue( 0 );
}


//-----------------------------------------------------------------------------
// Visualization changed 
//-----------------------------------------------------------------------------
void COcclusionUIPanel::OnTextChanged( KeyValues *data )
{
	Panel *pPanel = reinterpret_cast<vgui::Panel *>( data->GetPtr("panel") );
	vgui::ComboBox *pBox = dynamic_cast<vgui::ComboBox *>( pPanel );

	if( pBox == m_pVisualization ) 
	{
		OnVisualizationSelected();
		return;
	}

	vgui::TextEntry *pText = dynamic_cast<vgui::TextEntry *>( pPanel );
	if (( pText == m_pMinOccluderArea ) || ( pText == m_pMaxOccludeeArea )) 
	{
		char buf[256];
		float flMinArea, flMaxArea;

		m_pMinOccluderArea->GetText( buf, 256 );
		int nReadMin = sscanf( buf, "%f", &flMinArea );
		if ( nReadMin )
		{
			r_occluderminarea.SetValue( flMinArea );
		}

		m_pMaxOccludeeArea->GetText( buf, 256 );
		int nReadMax = sscanf( buf, "%f", &flMaxArea );
		if ( nReadMax )
		{
			r_occludeemaxarea.SetValue( flMaxArea );
		}
	}
}


//-----------------------------------------------------------------------------
// Activate, deactivate: 
//-----------------------------------------------------------------------------
void COcclusionUIPanel::OnVisualizationSelected()
{
	int tool = m_pVisualization->GetActiveItem();
	switch( tool )
	{
	case VISUALIZE_NONE:
		r_visocclusion.SetValue( 0 );
		mat_wireframe.SetValue( 0 );
		break;

	case VISUALIZE_ON:
		r_visocclusion.SetValue( 1 );
		mat_wireframe.SetValue( 3 );
		break;
	}
}


//-----------------------------------------------------------------------------
// Activate, deactivate: 
//-----------------------------------------------------------------------------
void COcclusionUIPanel::OnDeactivateOcclusion()
{
	r_occlusion.SetValue( m_pDeactivateOcclusion->IsSelected() ? 0 : 1 );
}

void COcclusionUIPanel::OnCheckButtonChecked(Panel *panel)
{
	if ( panel == m_pDeactivateOcclusion )
	{
		OnDeactivateOcclusion();
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPerfUIPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CPerfUIPanel, vgui::Frame );

public:
	CPerfUIPanel( vgui::Panel *parent );
	~CPerfUIPanel();

	// Command issued
	virtual void	OnCommand(const char *command);

	virtual void	Activate();

	void			Init();
	void			Shutdown();

	virtual void	OnKeyCodeTyped(KeyCode code);

	virtual void	OnTick();

protected:
	vgui::ComboBox *m_pPerformanceTool;

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );

private:
	void	PopulateControls();
	void	OnPerfToolSelected();

	PerformanceTool_t m_nPerfTool;
	CPerfUIChildPanel *m_pToolPanel[PERF_TOOL_COUNT];
	CPerfUIChildPanel *m_pCurrentToolPanel;
};


//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
CPerfUIPanel::CPerfUIPanel( vgui::Panel *parent ) : BaseClass( parent, "PerfUIPanel")
{
	SetTitle("Level Performance Tools", true);

	m_pPerformanceTool = new ComboBox(this, "PerformanceTool", 10, false);

	vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );

	LoadControlSettings("Resource\\PerfUIPanel.res");

	// Hidden by default
	SetVisible( false );

	SetSizeable( false );
	SetMoveable( true );

	int w = 250;
	int h = 400;

	int x = videomode->GetModeStereoWidth() - w - 10;
	int y = ( videomode->GetModeStereoHeight() - h ) / 2 + videomode->GetModeStereoHeight() * 0.2;
	SetBounds( x, y, w, h );

	// Create the child tool panels
	m_pToolPanel[PERF_TOOL_NONE] = new CPerfUIChildPanel( this, "PerfNone" );
	m_pToolPanel[PERF_TOOL_PROP_FADES] = new CPropFadeUIPanel( this );
	m_pToolPanel[PERF_TOOL_AREA_PORTALS] = new CAreaPortalsUIPanel( this );
	m_pToolPanel[PERF_TOOL_OCCLUSION] = new COcclusionUIPanel( this );

	for ( int i = 0; i < PERF_TOOL_COUNT; ++i )
	{
		m_pToolPanel[i]->SetBounds( 0, 75, w, h - 75 );
	}

	m_nPerfTool = PERF_TOOL_COUNT;
	m_pCurrentToolPanel = NULL;
	PopulateControls();
}

CPerfUIPanel::~CPerfUIPanel()
{
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
void CPerfUIPanel::Init()
{
	// Center the cursor on the panel
	int x, y, w, h;
	GetBounds( x, y, w, h );
	vgui::input()->SetCursorPos( x + w/2, y + h/2 );
}

void CPerfUIPanel::Shutdown()
{
	if ( m_pCurrentToolPanel )
	{
		m_pCurrentToolPanel->Deactivate();
		m_pCurrentToolPanel->SetVisible( false );
	}
}

void CPerfUIPanel::PopulateControls()
{
	m_pPerformanceTool->DeleteAllItems();
	int i;
	for ( i = 0; i < PERF_TOOL_COUNT; i++ )
	{
		m_pPerformanceTool->AddItem( s_pPerfToolNames[i], NULL );
	}
	m_pPerformanceTool->AddActionSignalTarget( this );
	m_pPerformanceTool->ActivateItem( 0 );
}

//-----------------------------------------------------------------------------
// Don't allow this to be enabled without cheats turned on.
//-----------------------------------------------------------------------------
void CPerfUIPanel::OnTick()
{
	// Go away if we were on and sv_cheats is now on.
 	if ( !CanCheat() )
	{
		Shutdown();
	}

	BaseClass::OnTick();
}

//-----------------------------------------------------------------------------
// A new performance tool was selected 
//-----------------------------------------------------------------------------
void CPerfUIPanel::OnTextChanged( KeyValues *data )
{
	Panel *pPanel = reinterpret_cast<vgui::Panel *>( data->GetPtr("panel") );
	vgui::ComboBox *pBox = dynamic_cast<vgui::ComboBox *>( pPanel );

	if( pBox == m_pPerformanceTool ) // don't change the text in the config setting combo
	{
		OnPerfToolSelected();
	}
}

void CPerfUIPanel::OnPerfToolSelected()
{
	int tool = m_pPerformanceTool->GetActiveItem();
	if ( tool == m_nPerfTool )
		return;

	if ( m_pCurrentToolPanel )
	{
		m_pCurrentToolPanel->Deactivate();
		m_pCurrentToolPanel->SetVisible( false );
	}
	m_nPerfTool = (PerformanceTool_t)tool;
	m_pCurrentToolPanel = m_pToolPanel[tool];
	m_pCurrentToolPanel->SetVisible( true );
	m_pCurrentToolPanel->Activate();
}


//-----------------------------------------------------------------------------
// Purpose: Shows the panel
//-----------------------------------------------------------------------------
void CPerfUIPanel::Activate()
{
	if ( !CanCheat() )
		return;

	Init();
	BaseClass::Activate();
}

void CPerfUIPanel::OnCommand( char const *command )
{
	if ( !Q_strcasecmp( command, "submit" ) )
	{
//		OnSubmit();
	}
	else if ( !Q_strcasecmp( command, "cancel" ) )
	{
//		Close();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPerfUIPanel::OnKeyCodeTyped(KeyCode code)
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


//-----------------------------------------------------------------------------
// Main interface to the performance tools 
//-----------------------------------------------------------------------------
static CPerfUIPanel *g_pPerfUI = NULL;

class CEnginePerfTools : public IEnginePerfTools
{
public:
	virtual void		Init( void );
	virtual void		Shutdown( void );

	virtual void		InstallPerformanceToolsUI( vgui::Panel *parent );
	virtual bool		ShouldPause() const;
};

static CEnginePerfTools g_PerfTools;
IEnginePerfTools *perftools = &g_PerfTools;

void CEnginePerfTools::Init( void )
{
}

void CEnginePerfTools::Shutdown( void )
{
	if ( g_pPerfUI )
	{
		g_pPerfUI->Shutdown();
	}
}

void CEnginePerfTools::InstallPerformanceToolsUI( vgui::Panel *parent )
{
	if ( g_pPerfUI )
		return;

	g_pPerfUI = new CPerfUIPanel( parent );
	Assert( g_pPerfUI );
}

bool CEnginePerfTools::ShouldPause() const
{
	return false;
}

void ShowHidePerfUI()
{
	if ( !g_pPerfUI )
		return;

	bool wasvisible = g_pPerfUI->IsVisible();

	if ( wasvisible )
	{
		// hide
		g_pPerfUI->Close();
	}
	else
	{
		g_pPerfUI->Activate();
	}
}


static ConCommand perfui( "perfui", ShowHidePerfUI, "Show/hide the level performance tools UI.", FCVAR_CHEAT );
