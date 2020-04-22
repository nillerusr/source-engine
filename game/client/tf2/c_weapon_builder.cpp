//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's CWeaponBuilder class
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "in_buttons.h"
#include "clientmode_tfnormal.h"
#include "engine/IEngineSound.h"
#include "c_weapon_builder.h"
#include "c_weapon__stubs.h"
#include "iinput.h"
#include "ObjectControlPanel.h"
#include <vgui/IVGui.h>

#define BUILD_ICON_SCALE			0.75

#define BUILD_ICON_BOTTOM_OFFSET	YRES(160)

//-----------------------------------------------------------------------------
// Purpose: Draw a material on a quad
//-----------------------------------------------------------------------------
void DrawQuadMaterial( IMaterial *pMaterial, int iX, int iY, int iWidth, int iHeight, 
	unsigned char r = 255, unsigned char g = 255, unsigned char b = 255, unsigned char a = 255, 
	float flTextureLeft = 0.0, float flTextureRight = 1.0, bool bRotated = false )
{
	IMesh* pMesh = materials->GetDynamicMesh( true, NULL, NULL, pMaterial );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Color4ub( r, g, b, a );
	if ( bRotated )
	{
		meshBuilder.TexCoord2f( 0,flTextureLeft,1 );
	}
	else
	{
		meshBuilder.TexCoord2f( 0,flTextureLeft,0 );
	}
	meshBuilder.Position3f( iX,iY,0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( r, g, b, a );
	if ( bRotated )
	{
		meshBuilder.TexCoord2f( 0,flTextureLeft,0 );
	}
	else
	{
		meshBuilder.TexCoord2f( 0,flTextureRight,0 );
	}
	meshBuilder.Position3f( iX+iWidth, iY, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( r, g, b, a );
	if ( bRotated )
	{
		meshBuilder.TexCoord2f( 0,flTextureRight,0 );
	}
	else
	{
		meshBuilder.TexCoord2f( 0,flTextureRight,1 );
	}
	meshBuilder.Position3f( iX+iWidth, iY+iHeight, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( r, g, b, a );
	if ( bRotated )
	{
		meshBuilder.TexCoord2f( 0,flTextureRight,1 );
	}
	else
	{
		meshBuilder.TexCoord2f( 0,flTextureLeft,1 );
	}
	meshBuilder.Position3f( iX, iY+iHeight, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

STUB_WEAPON_CLASS_IMPLEMENT( weapon_builder, C_WeaponBuilder );

IMPLEMENT_CLIENTCLASS_DT(C_WeaponBuilder, DT_WeaponBuilder, CWeaponBuilder)
	RecvPropInt( RECVINFO(m_iBuildState) ),
	RecvPropInt( RECVINFO(m_iCurrentObject) ),
	RecvPropInt( RECVINFO(m_iCurrentObjectState) ),
	RecvPropEHandle( RECVINFO(m_hObjectBeingBuilt) ),
	RecvPropTime( RECVINFO(m_flStartTime) ),
	RecvPropTime( RECVINFO(m_flTotalTime) ),
	RecvPropArray
	( 
		RecvPropInt( RECVINFO(m_bObjectValidity[0])), m_bObjectValidity
	),
	RecvPropArray
	( 
		RecvPropInt( RECVINFO(m_bObjectBuildability[0])), m_bObjectBuildability
	),
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_WeaponBuilder::C_WeaponBuilder()
{
	m_iBuildState = 0;
	m_iCurrentObject = BUILDER_INVALID_OBJECT;
	m_iCurrentObjectState = 0;
	m_flStartTime = 0;
	m_flTotalTime = 0;

	m_pIconFireToSelect.Init( "Hud/build/firetobuild", TEXTURE_GROUP_VGUI );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_WeaponBuilder::~C_WeaponBuilder()
{
}

//-----------------------------------------------------------------------------
// A couple helper methods for drawing builder status 
//-----------------------------------------------------------------------------
static void DrawTextIcon( IMaterial* pMaterial, int parentWidth, int parentHeight, float r = 1.0f, float g = 1.0f, float b = 1.0f )
{
	if ( !pMaterial )
		return;

	// We're in build selection mode, so draw the current build icon
	int iWidth = pMaterial->GetMappingWidth();
	int iHeight = pMaterial->GetMappingHeight();
	int iX = (parentWidth - iWidth) / 2;
	int iY = (parentHeight - 216);
	DrawQuadMaterial( pMaterial, iX, iY, iWidth, iHeight, r * 255, g * 255, b * 255 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *C_WeaponBuilder::GetCurrentSelectionObjectName( void )
{
	if ( m_iCurrentObject == -1 || (m_iBuildState == BS_SELECTING) )
		return "";

	return GetObjectInfo( m_iCurrentObject )->m_pBuilderWeaponName;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_WeaponBuilder::Redraw()
{
	BaseClass::Redraw();

	// Don't draw if we're hiding the weapons, or the player's dead
	if ( gHUD.IsHidden( HIDEHUD_WEAPONSELECTION | HIDEHUD_PLAYERDEAD ) )
		return;
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	vgui::Panel *pParent = GetClientModeNormal()->GetViewport();
	int parentWidth, parentHeight;
	pParent->GetSize(parentWidth, parentHeight);

	// If we're in placement mode, draw the placement icon
	switch( m_iBuildState )
	{
	case BS_PLACING:
	case BS_PLACING_INVALID:
		break;

	default:
		{
			if( !inv_demo.GetInt() )
			{
				DrawTextIcon( m_pIconFireToSelect, parentWidth, parentHeight );
			}
			break;
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_WeaponBuilder::IsPlacingObject( void )
{
	if ( m_iBuildState == BS_PLACING || m_iBuildState == BS_PLACING_INVALID )
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_WeaponBuilder::IsBuildingObject( void )
{
	if ( m_iBuildState == BS_BUILDING )
		return true;
	return false;
}

#include "vgui_bitmapimage.h"
#include "vgui_bitmappanel.h"


//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CHumanPDAPanel : public CVGuiScreenPanel
{
	DECLARE_CLASS( CHumanPDAPanel, CVGuiScreenPanel );

public:
	CHumanPDAPanel( vgui::Panel *parent, const char *panelName );
	~CHumanPDAPanel();
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();

private:
	C_BaseCombatWeapon *GetOwningWeapon();

	vgui::Label *m_pObjectName;
	vgui::Label *m_pObjectCost;
	vgui::Label *m_pObjectOnTeamCount;
	vgui::Label *m_pObjectPlacementDetails;

	CBitmapPanel *m_pBitmapPanel;

	BitmapImage *m_pObjectImage;

	int				m_nLastObjectID;
	int				m_nLastObjectCount;
	int				m_nLastObjectCost;

};


DECLARE_VGUI_SCREEN_FACTORY( CHumanPDAPanel, "human_pda" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CHumanPDAPanel::CHumanPDAPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CHumanPDAPanel", vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/PDAControlPanelScheme.res", "TFBase" ) ) 
{
	m_pObjectImage = NULL;

	m_pObjectName = new vgui::Label( this, "ObjectName", "" );
	m_pObjectCost = new vgui::Label( this, "ObjectCost", "" );
	m_pObjectOnTeamCount = new vgui::Label( this, "ObjectOnTeamCount", "" );
	m_pObjectPlacementDetails = new vgui::Label( this, "ObjectPlacementDetails", "" );

	m_pBitmapPanel = new CBitmapPanel( this, "ObjectImage" );
	m_pObjectImage = new BitmapImage();
	m_pObjectImage->UsePanelRenderSize( m_pBitmapPanel->GetVPanel() );
	m_pBitmapPanel->SetImage( m_pObjectImage );

	m_nLastObjectID = -1;
	m_nLastObjectCount = -1;
	m_nLastObjectCost = -1;
}

CHumanPDAPanel::~CHumanPDAPanel()
{
	if ( m_pObjectImage )
	{
		delete m_pObjectImage;
	}
}


//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CHumanPDAPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{

	// Make sure we get ticked...
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Returns the object it's attached to 
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHumanPDAPanel::GetOwningWeapon()
{
	C_BaseEntity *pScreenEnt = GetEntity();
	if (!pScreenEnt)
		return NULL;

	C_BaseEntity *pOwner = pScreenEnt->GetOwnerEntity();
	if (!pOwner)
		return NULL;

	C_BaseViewModel *pViewModel = dynamic_cast< C_BaseViewModel * >( pOwner );
	if ( !pViewModel )
		return NULL;

	return pViewModel->GetOwningWeapon();
}

//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CHumanPDAPanel::OnTick()
{
	BaseClass::OnTick();

	SetVisible( true );

	char buf[256];

	C_BaseCombatWeapon *weapon = GetOwningWeapon();
	if ( !weapon )
		return;

	C_WeaponBuilder *builder = dynamic_cast< C_WeaponBuilder * >( weapon );
	if ( !builder )
		return;

	CBaseTFPlayer *pOwner = ToBaseTFPlayer( builder->GetOwner() );
	if ( !pOwner )
		return;

	// FIXME: Check build state??

	int objectType = builder->m_iCurrentObject;
	CObjectInfo const *info = GetObjectInfo( objectType );
	if ( !info )
		return;

	int numOwned = pOwner->GetNumObjects(objectType);
	int iCost = CalculateObjectCost( objectType, numOwned, pOwner->GetTeamNumber() );

	if ( m_nLastObjectID == objectType &&
		m_nLastObjectCount == numOwned &&
		m_nLastObjectCost == iCost)
		return;

	m_nLastObjectID = objectType;
	m_nLastObjectCount = numOwned;
	m_nLastObjectCost = iCost;

	Q_snprintf( buf, sizeof( buf ), "hud/menu/%s", info->m_pClassName );
	m_pObjectImage->SetImageFile( buf );
	m_pObjectImage->SetColor( GetFgColor() );
	
	Q_snprintf( buf, sizeof( buf ), "%s", info->m_pStatusName );
	m_pObjectName->SetText( buf );

	Q_snprintf( buf, sizeof( buf ), "Cost:  %i", iCost );
	m_pObjectCost->SetText( buf );

	Q_snprintf( buf, sizeof( buf ), "You own:  %i", numOwned );
	m_pObjectOnTeamCount->SetText( buf );

	Q_snprintf( buf, sizeof( buf ), "%s", info->m_pBuilderPlacementString ? info->m_pBuilderPlacementString : "" );
	m_pObjectPlacementDetails->SetText( buf );
	//m_pObjectPlacementDetails->SizeToContents();
}
