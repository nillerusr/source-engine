//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"

#include "weapon_csbase.h"
#include "cs_ammodef.h"

#include <vgui/IVGui.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Label.h>
#include <vgui/ILocalize.h>
#include "vgui_controls/BuildGroup.h"
#include "vgui_controls/BitmapImagePanel.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/RichText.h"
#include "vgui_controls/QueryBox.h"
#include "career_box.h"
#include "buypreset_listbox.h"
#include "buypreset_weaponsetlabel.h"
#include "backgroundpanel.h"

#include "cstrike/bot/shared_util.h"

using namespace vgui;

const float horizTitleRatio = 18.0f/68.0f;

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
/*
class PresetNameTextEntry : public TextEntry
{
public:
	PresetNameTextEntry(Panel *parent, CBuyPresetEditMainMenu *menu, const char *name ) : TextEntry( parent, name )
	{
		m_pMenu = menu;
	}

	virtual void FireActionSignal()
	{
		TextEntry::FireActionSignal();
		if ( m_pMenu )
		{
			m_pMenu->SetDirty();
		}
	}
	
private:
	CBuyPresetEditMainMenu *m_pMenu;
};
*/

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
int GetScaledValue( HScheme hScheme, int unscaled )
{
	int val = scheme()->GetProportionalScaledValueEx( hScheme, unscaled );
	return GetAlternateProportionalValueFromScaled( hScheme, val );
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
class PresetBackgroundPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;

public:
	PresetBackgroundPanel( vgui::Panel *parent, const char *panelName ) : BaseClass( parent, panelName )
	{
	};

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		SetBorder( pScheme->GetBorder("ButtonBorder") );
		m_lineColor = pScheme->GetColor( "Border.Bright", Color( 0, 0, 0, 0 ) );
	}

	virtual void ApplySettings( KeyValues *inResourceData )
	{
		BaseClass::ApplySettings( inResourceData );

		m_lines.RemoveAll();
		KeyValues *lines = inResourceData->FindKey( "lines", false );
		if ( lines )
		{
			KeyValues *line = lines->GetFirstValue();
			while ( line )
			{
				const char *str = line->GetString( NULL, "" );
				Vector4D p;
				int numPoints = sscanf( str, "%f %f %f %f", &p[0], &p[1], &p[2], &p[3] );
				if ( numPoints == 4 )
				{
					m_lines.AddToTail( p );
				}
				line = line->GetNextValue();
			}
		}
	}

	virtual void PaintBackground( void )
	{
		BaseClass::PaintBackground();

		vgui::surface()->DrawSetColor( m_lineColor );
		vgui::surface()->DrawSetTextColor( m_lineColor );
		for ( int i=0; i<m_scaledLines.Count(); ++i )
		{
			int x1, x2, y1, y2;

			x1 = m_scaledLines[i][0];
			y1 = m_scaledLines[i][1];
			x2 = m_scaledLines[i][2];
			y2 = m_scaledLines[i][3];

			vgui::surface()->DrawFilledRect( x1, y1, x2, y2 );
		}
	}

	virtual void PerformLayout( void )
	{
		m_scaledLines.RemoveAll();
		for ( int i=0; i<m_lines.Count(); ++i )
		{
			int x1, x2, y1, y2;

			x1 = GetScaledValue( GetScheme(), m_lines[i][0] );
			y1 = GetScaledValue( GetScheme(), m_lines[i][1] );
			x2 = GetScaledValue( GetScheme(), m_lines[i][2] );
			y2 = GetScaledValue( GetScheme(), m_lines[i][3] );

			if ( x1 == x2 )
			{
				++x2;
			}

			if ( y1 == y2 )
			{
				++y2;
			}

			m_scaledLines.AddToTail( Vector4D( x1, y1, x2, y2 ) );
		}
	}

private:
	Color m_lineColor;
	CUtlVector< Vector4D > m_lines;
	CUtlVector< Vector4D > m_scaledLines;
};

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
BuyPresetEditPanel::BuyPresetEditPanel( Panel *parent, const char *panelName, const char *resourceFilename, int fallbackIndex, bool editableName ) : BaseClass( parent, panelName )
{
	SetProportional( parent->IsProportional() );
	if ( IsProportional() )
	{
		m_baseWide = m_baseTall = scheme()->GetProportionalScaledValueEx( GetScheme(), 100 );
	}
	else
	{
		m_baseWide = m_baseTall = 100;
	}
	SetSize( m_baseWide, m_baseTall );

	m_fallbackIndex = fallbackIndex;

	m_pBgPanel = new PresetBackgroundPanel( this, "mainBackground" );

	m_pTitleEntry = NULL;
	m_pTitleLabel = NULL;
	m_pCostLabel = NULL;
	/*
	m_pTitleEntry = new PresetNameTextEntry( this, dynamic_cast<CBuyPresetEditMainMenu *>(parent), "titleEntry" );
	m_pTitleLabel = new Label( this, "title", "" );
	m_pCostLabel = new Label( this, "cost", "" );
	*/

	m_pPrimaryWeapon = new WeaponLabel( this, "primary" );
	m_pSecondaryWeapon = new WeaponLabel( this, "secondary" );

	m_pHEGrenade = new EquipmentLabel( this, "hegrenade" );
	m_pSmokeGrenade = new EquipmentLabel( this, "smokegrenade" );
	m_pFlashbangs = new EquipmentLabel( this, "flashbang" );

	m_pDefuser = new EquipmentLabel( this, "defuser" );
	m_pNightvision = new EquipmentLabel( this, "nightvision" );

	m_pArmor = new EquipmentLabel( this, "armor" );

	if ( resourceFilename )
	{
		LoadControlSettings( resourceFilename );
	}

	int x, y, w, h;
	m_pBgPanel->GetBounds( x, y, w, h );

	m_baseWide = x + w;
	m_baseTall = y + h;
	SetSize( m_baseWide, m_baseTall );
}

//--------------------------------------------------------------------------------------------------------------
BuyPresetEditPanel::~BuyPresetEditPanel()
{
}

//--------------------------------------------------------------------------------------------------------------
void BuyPresetEditPanel::SetWeaponSet( const WeaponSet *pWeaponSet, bool current )
{
	// set to empty state
	Reset();

	// now fill in items
	if ( pWeaponSet )
	{
		if ( m_pTitleLabel )
		{
			m_pTitleLabel->SetText( SharedVarArgs( "#Cstrike_BuyPresetChoice%d", m_fallbackIndex ) );
		}
		if ( m_pTitleEntry )
		{
			m_pTitleEntry->SetText( SharedVarArgs( "#Cstrike_BuyPresetChoice%d", m_fallbackIndex ) );
		}

		if ( m_pCostLabel )
		{
			const int BufLen = 256;
			wchar_t wbuf[BufLen];
			g_pVGuiLocalize->ConstructString( wbuf, sizeof( wbuf ),
				g_pVGuiLocalize->Find( "#Cstrike_BuyPresetPlainCost" ),
				1, NumAsWString( pWeaponSet->FullCost() ) );
			m_pCostLabel->SetText( wbuf );
		}

		m_pPrimaryWeapon->SetWeapon( &pWeaponSet->m_primaryWeapon, true, true );
		m_pSecondaryWeapon->SetWeapon( &pWeaponSet->m_secondaryWeapon, false, true );

		if ( pWeaponSet->m_HEGrenade )
			m_pHEGrenade->SetItem( "gfx/vgui/hegrenade_square", 1 );
		if ( pWeaponSet->m_smokeGrenade )
			m_pSmokeGrenade->SetItem( "gfx/vgui/smokegrenade_square", 1 );
		if ( pWeaponSet->m_flashbangs )
			m_pFlashbangs->SetItem( "gfx/vgui/flashbang_square", pWeaponSet->m_flashbangs );

		if ( pWeaponSet->m_defuser )
			m_pDefuser->SetItem( "gfx/vgui/defuser", 1 );
		if ( pWeaponSet->m_nightvision )
			m_pNightvision->SetItem( "gfx/vgui/nightvision", 1 );

		if ( pWeaponSet->m_armor )
		{
			if ( pWeaponSet->m_helmet )
				m_pArmor->SetItem( "gfx/vgui/kevlar_helmet", 1 );
			else
				m_pArmor->SetItem( "gfx/vgui/kevlar", 1 );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void BuyPresetEditPanel::SetText( const wchar_t *text )
{
	if ( !text )
		text = L"";
	if ( m_pTitleLabel )
	{
		m_pTitleLabel->SetText( text );
	}
	if ( m_pTitleEntry )
	{
		m_pTitleEntry->SetText( text );
	}
	InvalidateLayout();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Handle command callbacks
 */
void BuyPresetEditPanel::OnCommand( const char *command )
{
	if (stricmp(command, "close"))
	{
		PostActionSignal( new KeyValues("Command", "command", SharedVarArgs( "%s %d", command, m_fallbackIndex )) );
	}

	BaseClass::OnCommand(command);
}

//--------------------------------------------------------------------------------------------------------------
void BuyPresetEditPanel::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor( Color( 0, 0, 0, 0 ) );

	IBorder *pBorder = NULL;

	int i;

	for (i = 0; i < GetChildCount(); i++)
	{
		// perform auto-layout on the child panel
		Panel *child = GetChild(i);
		if (!child)
			continue;

		if ( !stricmp( "button", child->GetClassName() ) )
		{
			Button *pButton = dynamic_cast<Button *>(child);
			if ( pButton )
			{
				pButton->SetDefaultBorder( pBorder );
				pButton->SetDepressedBorder( pBorder );
				pButton->SetKeyFocusBorder( pBorder );
			}
		}
	}

	pBorder = pScheme->GetBorder("BuyPresetButtonBorder");

	const int NumButtons = 4;
	const char * buttonNames[4] = { "editPrimary", "editSecondary", "editGrenades", "editEquipment" };
	for ( i=0; i<NumButtons; ++i )
	{
		Panel *pPanel = FindChildByName( buttonNames[i] );
		if ( pPanel )
		{
			pPanel->SetBorder( pBorder );
			if ( !stricmp( "button", pPanel->GetClassName() ) )
			{
				Button *pButton = dynamic_cast<Button *>(pPanel);
				if ( pButton )
				{
					pButton->SetDefaultBorder( pBorder );
					pButton->SetDepressedBorder( pBorder );
					pButton->SetKeyFocusBorder( pBorder );

					Color fgColor, bgColor;
					fgColor = GetSchemeColor("Label.TextDullColor", GetFgColor(), pScheme);
					bgColor = Color( 0, 0, 0, 0 );
					pButton->SetDefaultColor( fgColor, bgColor );
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Overrides EditablePanel's resizing of children to scale them proportionally to the main panel's change.
 */
void BuyPresetEditPanel::OnSizeChanged( int wide, int tall )
{
	if ( !m_baseWide )
		m_baseWide = 1;
	if ( !m_baseTall )
		m_baseTall = 1;

	Panel::OnSizeChanged(wide, tall);
	InvalidateLayout();

	if ( wide == m_baseWide && tall == m_baseTall )
	{
		Repaint();
		return;
	}

	float xScale = wide / (float) m_baseWide;
	float yScale = tall / (float) m_baseTall;

	for (int i = 0; i < GetChildCount(); i++)
	{
		// perform auto-layout on the child panel
		Panel *child = GetChild(i);
		if (!child)
			continue;

		int x, y, w, t;
		child->GetBounds(x, y, w, t);

		int newX = (int) x * xScale;
		int newY = (int) y * yScale;
		int newW = (int) (x+w) * xScale - newX;
		int newT = (int) t * yScale;

		// make sure the child isn't too big...
		if(newX+newW>wide)
		{
			continue;
		}
		
		if(newY+newT>tall)
		{
			continue;
		}

		child->SetBounds(newX, newY, newW, newT);
		child->InvalidateLayout();
	}
	Repaint();

	// update the baselines
	m_baseWide = wide;
	m_baseTall = tall;
}

//--------------------------------------------------------------------------------------------------------------
void BuyPresetEditPanel::Reset()
{
	if ( m_pTitleLabel )
	{
		m_pTitleLabel->SetText( "#Cstrike_BuyPresetNewChoice" );
	}
	if ( m_pTitleEntry )
	{
		m_pTitleEntry->SetText( "#Cstrike_BuyPresetNewChoice" );
	}
	if ( m_pCostLabel )
	{
		m_pCostLabel->SetText( "" );
	}

	BuyPresetWeapon weapon;
	m_pPrimaryWeapon->SetWeapon( &weapon, true, false );
	m_pSecondaryWeapon->SetWeapon( &weapon, false, false );

	m_pHEGrenade->SetItem( NULL, 1 );
	m_pSmokeGrenade->SetItem( NULL, 1 );
	m_pFlashbangs->SetItem( NULL, 1 );

	m_pDefuser->SetItem( NULL, 1 );
	m_pNightvision->SetItem( NULL, 1 );

	m_pArmor->SetItem( NULL, 1 );
}

//--------------------------------------------------------------------------------------------------------------
