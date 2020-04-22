//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "client_pch.h"

#include "ivideomode.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include <vgui_controls/Panel.h>
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "client.h"
#include "gl_matsysiface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef VPROF_ENABLED

static ConVar vprof_graph	   ( "vprof_graph","0", 0, "Draw the vprof graph." );
static ConVar vprof_graphwidth ( "vprof_graphwidth", "512", FCVAR_ARCHIVE );
static ConVar vprof_graphheight( "vprof_graphheight", "256", FCVAR_ARCHIVE );

#define	TIMINGS		256		// Number of values to track (must be power of 2) b/c of masking

#define GRAPH_RED	(0.9f * 255)
#define GRAPH_GREEN (0.9f * 255)
#define GRAPH_BLUE	(0.7f * 255)

#define LERP_HEIGHT 24


//-----------------------------------------------------------------------------
// Purpose: Displays the netgraph 
//-----------------------------------------------------------------------------
class CVProfGraphPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
private:

	vgui::HFont			m_hFont;

public:
						CVProfGraphPanel( vgui::VPANEL parent );
	virtual				~CVProfGraphPanel( void );

	virtual void		ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void		Paint();
	virtual void		OnTick( void );

	virtual bool		ShouldDraw( void );


	struct CLineSegment
	{
		int			x1, y1, x2, y2;
		byte		color[4];
	};

	inline void			DrawLine( vrect_t *rect, unsigned char *color, unsigned char alpha );

	void				DrawLineSegments();

	void				GraphGetXY( vrect_t *rect, int width, int *x, int *y );

private:

	void				PaintLineArt( int x, int y, int w );

	CMaterialReference	m_WhiteMaterial;

	// VProf interface:
	float m_Samples[ TIMINGS ][3];
	CVProfNode*  m_Components;

	int   m_CurrentSample;

	void GetNextSample();

public:
	static CVProfNode*  m_CurrentNode;
};

CVProfNode* CVProfGraphPanel::m_CurrentNode = NULL;


void IN_VProfPrevSibling(void)
{
	CVProfNode* n = CVProfGraphPanel::m_CurrentNode->GetPrevSibling();
	if( n )
		CVProfGraphPanel::m_CurrentNode = n;
}

void IN_VProfNextSibling(void)
{
	CVProfNode* n = CVProfGraphPanel::m_CurrentNode->GetSibling();
	if( n )
		CVProfGraphPanel::m_CurrentNode = n;

}

void IN_VProfParent(void)
{
	CVProfNode* n = CVProfGraphPanel::m_CurrentNode->GetParent();
	if( n )
		CVProfGraphPanel::m_CurrentNode = n;

}

void IN_VProfChild(void)
{
	CVProfNode* n = CVProfGraphPanel::m_CurrentNode->GetChild();
	if( n )
	{
		// Find the largest child:
		CVProfGraphPanel::m_CurrentNode = n; 

		for( ; n; n = n->GetSibling() )
		{
			if( n->GetPrevTime() > CVProfGraphPanel::m_CurrentNode->GetPrevTime() )
				CVProfGraphPanel::m_CurrentNode = n;
		}
	}
}

static ConCommand vprof_siblingprev	("vprof_prevsibling", IN_VProfPrevSibling);
static ConCommand vprof_siblingnext	("vprof_nextsibling", IN_VProfNextSibling);
static ConCommand vprof_parent		("vprof_parent",	  IN_VProfParent);
static ConCommand vprof_child		("vprof_child",		  IN_VProfChild);

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
CVProfGraphPanel::CVProfGraphPanel( vgui::VPANEL parent ) : BaseClass( NULL, "CVProfGraphPanel" )
{
	SetParent( parent ); 
	SetSize( videomode->GetModeStereoWidth(), videomode->GetModeStereoHeight() );
	SetPos( 0, 0 );
	SetVisible( false );
	SetCursor( null );

	m_hFont = 0;

	SetFgColor( Color( 0, 0, 0, 255 ) );
	SetPaintBackgroundEnabled( false );

	memset( m_Samples, 0, sizeof( m_Samples ) );
	m_CurrentSample = 0;
	m_CurrentNode = g_VProfCurrentProfile.GetRoot();

	// Move down to an interesting node ( the render / sound / etc level)
	if( m_CurrentNode->GetChild() )
	{
		m_CurrentNode = m_CurrentNode->GetChild();

		if( m_CurrentNode->GetChild() )
		{
			m_CurrentNode = m_CurrentNode->GetChild();
		}
	}

	vgui::ivgui()->AddTickSignal( GetVPanel() );

	m_WhiteMaterial.Init( "vgui/white", TEXTURE_GROUP_OTHER );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVProfGraphPanel::~CVProfGraphPanel( void )
{
}

void CVProfGraphPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hFont = pScheme->GetFont( "DefaultVerySmall" );
	Assert( m_hFont );
}


//-----------------------------------------------------------------------------
// Purpose: Figure out x and y position for graph based on vprof_graphpos
//   value.
// Input  : *rect - 
//			width - 
//			*x - 
//			*y - 
//-----------------------------------------------------------------------------
void CVProfGraphPanel::GraphGetXY( vrect_t *rect, int width, int *x, int *y )
{
	*x = rect->x + rect->width - 5 - width;
	*y = rect->y+rect->height - LERP_HEIGHT - 5;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVProfGraphPanel::OnTick( void )
{
	SetVisible( ShouldDraw() );
	
}

bool CVProfGraphPanel::ShouldDraw( void )
{
	return vprof_graph.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVProfGraphPanel::Paint() 
{
	int			x, y, w;
	vrect_t		vrect;

	if ( ( ShouldDraw() ) == false )
		return;
	
	// Get screen rectangle
	vrect.x		 = 0;
	vrect.y		 = 0;
	vrect.width	 = videomode->GetModeStereoWidth();
	vrect.height = videomode->GetModeStereoHeight();

	// Determine graph width
	w = vprof_graphwidth.GetInt();
	if ( vrect.width < w + 10 )
	{
		w = vrect.width - 10;
	}

	// Get the graph's location:
	GraphGetXY( &vrect, w, &x, &y );

	PaintLineArt( x, y, w );

	// Draw the text overlays:

	// Print it out
	y -= vprof_graphheight.GetInt();

	double RootTime =  g_VProfCurrentProfile.GetRoot()->GetPrevTime();

	char sz[256];
	if ( ( g_ClientGlobalVariables.absoluteframetime) > 0.f )
	{
		Q_snprintf( sz, sizeof( sz ), "%s - %0.1f%%%%", m_CurrentNode->GetName(), ( m_CurrentNode->GetPrevTime() /  RootTime ) * 100.f);
		g_pMatSystemSurface->DrawColoredText( m_hFont, x, y, GRAPH_RED, GRAPH_GREEN, GRAPH_BLUE, 255, "%s", sz );
	}

	byte color[3][3] = 
	{
		{ 255, 0, 0 },
		{ 0, 0, 255 },
		{ 255, 255, 255 },
	};

	const char *pTitles[3];
	pTitles[0] = m_CurrentNode->GetName();
	pTitles[1] = "Parent";
	pTitles[2] = "Total";

	// Draw the legend:
	x += w / 2;
	for( int i = 3; --i >= 0; )
	{
		Q_snprintf( sz, sizeof( sz ), "%07.3f ms (%s)", m_Samples[m_CurrentSample][i], pTitles[i] );
		y -= 10;
		g_pMatSystemSurface->DrawColoredText( m_hFont, x, y, color[i][0], color[i][1], color[i][2], 180, "%s", sz );
	}
}


// VProf interface:
void CVProfGraphPanel::GetNextSample()
{
	// Increment to the next sample:
	m_CurrentSample = ( m_CurrentSample + 1 ) % TIMINGS; 
	m_Samples[m_CurrentSample][0] = m_CurrentNode->GetPrevTime();
	m_Samples[m_CurrentSample][1] = m_CurrentNode->GetParent() ? m_CurrentNode->GetParent()->GetPrevTime() : m_CurrentNode->GetPrevTime();
	m_Samples[m_CurrentSample][2] = g_VProfCurrentProfile.GetRoot()->GetPrevTime();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVProfGraphPanel::PaintLineArt( int x, int y, int w ) 
{
	int nPanelHeight = vprof_graphheight.GetFloat() - LERP_HEIGHT - 2;
	int h, a;

	// Update the sample graph:
	GetNextSample();

	CMatRenderContextPtr pRenderContext( materials );

	IMesh* m_pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, m_WhiteMaterial );
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( m_pMesh, MATERIAL_LINES, 3 * w + 4 );

	// Draw lines at 20, 30, 60 hz, and baseline
	int i;
	for ( i = 0; i < 4; ++i )
	{
		int nLineY = y - (nPanelHeight / 3) * i;

		if ( i == 0 )
		{
			meshBuilder.Color4ub( 255, 255, 255, 255 );
		}
		else
		{
			meshBuilder.Color4ub( 128, 128, 128, 255 );
		}

		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Position3f( x, nLineY, 0 );
		meshBuilder.AdvanceVertex();

		if ( i == 0 )
		{
			meshBuilder.Color4ub( 255, 255, 255, 255 );
		}
		else
		{
			meshBuilder.Color4ub( 128, 128, 128, 255 );
		}

		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Position3f( x + w, nLineY, 0 );
		meshBuilder.AdvanceVertex();
	}

	byte color[3][4] = 
	{
		{ 255, 0, 0, 255 },
		{ 0, 0, 255, 255 },
		{ 255, 255, 255, 255 },
	};

	// 0.05f = 1/20 = 20Hz
	float flMsToPixel = nPanelHeight / 50.0f;
	float flDxDSample = (w <= TIMINGS) ? 1.0f : (float)w / (float)TIMINGS;

	for( i = 3; --i >= 0; )
	{
		int sample = m_CurrentSample;
		for (a=w; a >= 0; a-- )
		{
			h = (int)(m_Samples[sample][i] * flMsToPixel + 0.5f);

			// Clamp the height: (though it shouldn't need it)
			if ( h > nPanelHeight )
			{
				h = nPanelHeight;
			}

			int px = (int)(x + (w - a - 1) * flDxDSample + 0.5f);
			int py = y - h;

			meshBuilder.Color4ubv( color[i] );
			meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
			meshBuilder.Position3f( px, py, 0 );
			meshBuilder.AdvanceVertex();

			if ( ( a != w ) && ( a != 0 ) )
			{
				meshBuilder.Color4ubv( color[i] );
				meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
				meshBuilder.Position3f( px, py, 0 );
				meshBuilder.AdvanceVertex();
			}

			// Move on to the next sample:
			sample--;
			if ( sample < 0 ) 
			{
				sample = TIMINGS - 1;
			}
		}
	}

	meshBuilder.End();
	m_pMesh->Draw();
}


#endif // VPROF_ENABLED


//-----------------------------------------------------------------------------
// Creates/destroys the vprof graph panel
//-----------------------------------------------------------------------------

#ifdef VPROF_ENABLED
static CVProfGraphPanel *s_pVProfGraphPanel = NULL;
#endif

void CreateVProfGraphPanel( vgui::Panel *pParent )
{
#ifdef VPROF_ENABLED
	s_pVProfGraphPanel = new CVProfGraphPanel( pParent->GetVPanel() );
#endif
}

void DestroyVProfGraphPanel()
{
#ifdef VPROF_ENABLED
	if ( s_pVProfGraphPanel )
	{
		s_pVProfGraphPanel->SetParent( (vgui::Panel *)NULL );
		delete s_pVProfGraphPanel;
		s_pVProfGraphPanel = NULL;
	}
#endif
}



