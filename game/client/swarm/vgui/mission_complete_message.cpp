#include "cbase.h"
#include "mission_complete_message.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/TextEntry.h"
#include <vgui/isurface.h>
#include "asw_scalable_text.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

CMission_Complete_Message::CMission_Complete_Message( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_flMessageBackgroundStartTime = 0.0f;	
	m_flMessageBackgroundFadeDuration = 0.0f;
}

CMission_Complete_Message::~CMission_Complete_Message()
{

}

void CMission_Complete_Message::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	//LoadControlSettings( "resource/ui/mission_complete_message.res" );
}

void CMission_Complete_Message::PerformLayout()
{
	BaseClass::PerformLayout();

	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
}

void CMission_Complete_Message::OnThink()
{
	BaseClass::OnThink();
}

void CMission_Complete_Message::Paint()
{
	BaseClass::Paint();

	PaintMessageBackground();
	PaintLetters();
}

void CMission_Complete_Message::PaintMessageBackground()
{
	float flLerpAmount = ( gpGlobals->curtime - m_flMessageBackgroundStartTime ) / ( m_flMessageBackgroundFadeDuration );
	flLerpAmount = clamp<float>( flLerpAmount, 0.0f, 1.0f );

	if ( m_bSuccess )
	{
		surface()->DrawSetColor(Color(35,41,57,flLerpAmount * 128.0f));
		surface()->DrawSetColor(Color(0,0,0,flLerpAmount * 128.0f));
	}
	else
	{
		surface()->DrawSetColor(Color(64,0,0,flLerpAmount * 192.0f));
	}
	int top = ScreenHeight() * 0.22f;
	int bottom = ScreenHeight() * 0.62f;
	surface()->DrawFilledRect( 0, top, ScreenWidth() + 1, bottom );

	if ( !m_bSuccess )
	{
		// fail hint bg
		surface()->DrawSetColor(Color(64,0,0,flLerpAmount * 192.0f));
		top = ScreenHeight() * 0.72f;
		bottom = ScreenHeight() * 0.82f;
		surface()->DrawFilledRect( 0, top, ScreenWidth() + 1, bottom );
	}
}

void CMission_Complete_Message::StartMessage( bool bSuccess )
{
	m_bSuccess = bSuccess;
	int row_middle_x = ScreenWidth() * 0.5f;
	int row_middle_y = ScreenHeight() * 0.35f;

	m_aAnimatingLetters.PurgeAndDeleteElements();

	m_flMessageBackgroundStartTime = gpGlobals->curtime + 0.4f;
	m_flMessageBackgroundFadeDuration = 0.6f;

	// MISSION
	float flStartTime = gpGlobals->curtime + 0.2f;
	float flLetterTimeInterval = 0.075f;
	if ( m_bSuccess && ASWGameRules() && ASWGameRules()->IsCampaignGame() && ASWGameRules()->CampaignMissionsLeft() <= 1 )
	{
		float flShiftRight = 0.2f;
		AddLetter( 'C', row_middle_x, row_middle_y, -3.56 + flShiftRight, flStartTime ); flStartTime += flLetterTimeInterval;
		AddLetter( 'A', row_middle_x, row_middle_y, -2.6 + flShiftRight, flStartTime ); flStartTime += flLetterTimeInterval;
		AddLetter( 'M', row_middle_x, row_middle_y, -1.49 + flShiftRight, flStartTime ); flStartTime += flLetterTimeInterval;
		AddLetter( 'P', row_middle_x, row_middle_y, -0.43 + flShiftRight, flStartTime ); flStartTime += flLetterTimeInterval; 
		AddLetter( 'A', row_middle_x, row_middle_y, 0.50 + flShiftRight, flStartTime ); flStartTime += flLetterTimeInterval; 
		AddLetter( 'I', row_middle_x, row_middle_y, 1.27 + flShiftRight, flStartTime ); flStartTime += flLetterTimeInterval; 
		AddLetter( 'G', row_middle_x, row_middle_y, 2.00 + flShiftRight, flStartTime ); flStartTime += flLetterTimeInterval; 
		AddLetter( 'N', row_middle_x, row_middle_y, 3.00 + flShiftRight, flStartTime ); flStartTime += flLetterTimeInterval; 
	}
	else
	{
		AddLetter( 'M', row_middle_x, row_middle_y, -2.44, flStartTime ); flStartTime += flLetterTimeInterval;
		AddLetter( 'I', row_middle_x, row_middle_y, -1.6, flStartTime ); flStartTime += flLetterTimeInterval;
		AddLetter( 'S', row_middle_x, row_middle_y, -0.9, flStartTime ); flStartTime += flLetterTimeInterval;
		AddLetter( 'S', row_middle_x, row_middle_y, 0, flStartTime ); flStartTime += flLetterTimeInterval; 
		AddLetter( 'I', row_middle_x, row_middle_y, 0.71, flStartTime ); flStartTime += flLetterTimeInterval; 
		AddLetter( 'O', row_middle_x, row_middle_y, 1.47, flStartTime ); flStartTime += flLetterTimeInterval; 
		AddLetter( 'N', row_middle_x, row_middle_y, 2.5, flStartTime ); flStartTime += flLetterTimeInterval; 
	}

	flStartTime += 0.2f;

	// COMPLETE
	row_middle_y = ScreenHeight() * 0.50f;

	if ( m_bSuccess )
	{
		row_middle_x += 18.0f * ( ScreenHeight() / 1050.0f );
		AddLetter( 'C', row_middle_x, row_middle_y, -3.56, flStartTime ); flStartTime += flLetterTimeInterval;
		AddLetter( 'O', row_middle_x, row_middle_y, -2.6, flStartTime ); flStartTime += flLetterTimeInterval;
		AddLetter( 'M', row_middle_x, row_middle_y, -1.49, flStartTime ); flStartTime += flLetterTimeInterval;
		AddLetter( 'P', row_middle_x, row_middle_y, -0.43, flStartTime ); flStartTime += flLetterTimeInterval;
		AddLetter( 'L', row_middle_x, row_middle_y, 0.45, flStartTime ); flStartTime += flLetterTimeInterval; 
		AddLetter( 'E', row_middle_x, row_middle_y, 1.32, flStartTime ); flStartTime += flLetterTimeInterval; 
		AddLetter( 'T', row_middle_x, row_middle_y, 2.25, flStartTime ); flStartTime += flLetterTimeInterval; 
		AddLetter( 'E', row_middle_x, row_middle_y, 3.16, flStartTime ); flStartTime += flLetterTimeInterval; 
	}
	else
	{
		row_middle_x += 20.0f * ( ScreenHeight() / 1050.0f );
		AddLetter( 'F', row_middle_x, row_middle_y, -2.1, flStartTime ); flStartTime += flLetterTimeInterval;
		AddLetter( 'A', row_middle_x, row_middle_y, -1.2, flStartTime ); flStartTime += flLetterTimeInterval;
		AddLetter( 'I', row_middle_x, row_middle_y, -0.43, flStartTime ); flStartTime += flLetterTimeInterval;
		AddLetter( 'L', row_middle_x, row_middle_y, 0.22, flStartTime ); flStartTime += flLetterTimeInterval; 
		AddLetter( 'E', row_middle_x, row_middle_y, 1.08, flStartTime ); flStartTime += flLetterTimeInterval; 
		AddLetter( 'D', row_middle_x, row_middle_y, 2.01, flStartTime ); flStartTime += flLetterTimeInterval; 
	}
}

void CMission_Complete_Message::AddLetter( char letter, int x, int y, float letter_offset, float flStartTime )
{
	const int letter_end_spacing = YRES( 50 );
	const int letter_start_spacing = YRES( 100 );
	const int letter_start_width = 300;
	const int letter_start_height = letter_start_width;
	const int letter_end_width = 80;
	const int letter_end_height = letter_end_width;
	const float flAnimTime = 0.35f;

	CAnimating_Letter *pLetter = new CAnimating_Letter;
	pLetter->m_chLetter = letter;
	pLetter->m_flStartTime = flStartTime;
	pLetter->m_flEndTime = flStartTime + flAnimTime;

	pLetter->m_nStartX = x + letter_offset * letter_start_spacing;
	pLetter->m_nStartY = y;
	pLetter->m_nEndX = x + letter_offset * letter_end_spacing;
	pLetter->m_nEndY = y;
	pLetter->m_nStartWidth = letter_start_width;
	pLetter->m_nStartHeight = letter_start_height;
	pLetter->m_nEndWidth = letter_end_width;
	pLetter->m_nEndHeight = letter_end_height;

	m_aAnimatingLetters.AddToTail( pLetter );
}

void CMission_Complete_Message::PaintLetters()
{
	if ( !ASWScalableText() )
		return;

	int nCount = m_aAnimatingLetters.Count();
	for ( int i = 0; i < nCount; i++ )
	{
		PaintLetter( m_aAnimatingLetters[i], true );
	}
	for ( int i = 0; i < nCount; i++ )
	{
		PaintLetter( m_aAnimatingLetters[i], false );
	}
}

void CMission_Complete_Message::PaintLetter( CAnimating_Letter *pLetter, bool bGlow )
{
	float flLerpAmount = ( gpGlobals->curtime - pLetter->m_flStartTime ) / ( pLetter->m_flEndTime - pLetter->m_flStartTime );
	flLerpAmount = clamp<float>( flLerpAmount, 0.0f, 1.0f );
	flLerpAmount *= flLerpAmount;
	flLerpAmount *= flLerpAmount;

	float flX = (float) pLetter->m_nStartX + ( (float) pLetter->m_nEndX - (float) pLetter->m_nStartX ) * flLerpAmount;
	float flY = (float) pLetter->m_nStartY + ( (float) pLetter->m_nEndY - (float) pLetter->m_nStartY ) * flLerpAmount;
	float flWidth = (float) YRES( pLetter->m_nStartWidth ) + ( (float) YRES( pLetter->m_nEndWidth ) - (float) YRES( pLetter->m_nStartWidth ) ) * flLerpAmount;
	float flHeight = (float) YRES( pLetter->m_nStartHeight ) + ( (float) YRES( pLetter->m_nEndHeight ) - (float) YRES( pLetter->m_nStartHeight ) ) * flLerpAmount;
	flWidth *= 0.5f;
	flHeight *= 0.5f;

	if ( bGlow )
	{
		if ( m_bSuccess )
		{
			surface()->DrawSetColor(Color(35,214,250,flLerpAmount * 112.0f));
		}
		else
		{
			surface()->DrawSetColor(Color(250,0,0,flLerpAmount * 192.0f));
		}
	}
	else
	{
		surface()->DrawSetColor(Color(255,255,255,flLerpAmount * 255.0f));
	}
	ASWScalableText()->DrawSetLetterTexture( pLetter->m_chLetter, bGlow );

	Vertex_t points[4] = 
	{ 
		Vertex_t( Vector2D(flX - flWidth, flY - flHeight), Vector2D(0,0) ), 
		Vertex_t( Vector2D(flX + flWidth, flY - flHeight), Vector2D(1,0) ), 
		Vertex_t( Vector2D(flX + flWidth, flY + flHeight), Vector2D(1,1) ), 
		Vertex_t( Vector2D(flX - flWidth, flY + flHeight), Vector2D(0,1) ) 
	}; 
	surface()->DrawTexturedPolygon( 4, points );
}