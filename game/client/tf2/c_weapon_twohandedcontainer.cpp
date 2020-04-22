//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's Support "weapon"
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "weapon_twohandedcontainer.h"

//-----------------------------------------------------------------------------
// Purpose: Draws little viewmodel attachments
//-----------------------------------------------------------------------------
void C_WeaponTwoHandedContainer::ViewModelDrawn( C_BaseViewModel *pBaseViewModel )
{
	BaseClass::ViewModelDrawn( pBaseViewModel );

	if (pBaseViewModel->ViewModelIndex() != 0)
	{
		if ( m_hRightWeapon )
		{
			m_hRightWeapon->ViewModelDrawn(pBaseViewModel);
		}
	}
	else
	{
		if ( m_hLeftWeapon )
		{
			m_hLeftWeapon->ViewModelDrawn(pBaseViewModel);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_WeaponTwoHandedContainer::OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, 
											  const QAngle& angles, int event, const char *options )
{
	bool bRight = m_hRightWeapon->OnFireEvent( pViewModel, origin, angles, event, options );
	bool bLeft = m_hLeftWeapon->OnFireEvent( pViewModel, origin, angles, event, options );

	return ( bRight || bLeft );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_WeaponTwoHandedContainer::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// Did we just receive a new weapon?
	bool rightchanged = m_hOldRightWeapon.Get() != m_hRightWeapon.Get() ? true : false;
	bool leftchanged = m_hOldLeftWeapon.Get() != m_hLeftWeapon.Get() ? true : false;

	if ( rightchanged || leftchanged )
	{
		/*
		// Tell weapons that they're being holstered
		if ( m_hRightWeapon != NULL && rightchanged )
		{
			m_hRightWeapon->Holster( NULL );
		}
		if ( m_hLeftWeapon != NULL && leftchanged )
		{
			m_hLeftWeapon->Holster( NULL );
		}
		*/

		HookWeaponEntities();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_WeaponTwoHandedContainer::HookWeaponEntities( void )
{
	m_hOldRightWeapon	= m_hRightWeapon;
	m_hOldLeftWeapon	= m_hLeftWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: Never draw the container
//-----------------------------------------------------------------------------
bool C_WeaponTwoHandedContainer::ShouldDraw( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: This weapon is the active weapon, and it should now draw anything
//			it wants to. This gets called every frame.
//-----------------------------------------------------------------------------
void C_WeaponTwoHandedContainer::Redraw()
{
	BaseClass::Redraw();

	if ( m_hRightWeapon.Get() )
	{
		m_hRightWeapon->Redraw();
	}
	if ( m_hLeftWeapon.Get() )
	{
		m_hLeftWeapon->Redraw();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw both weapon's ammo's
//-----------------------------------------------------------------------------
void C_WeaponTwoHandedContainer::DrawAmmo()
{
	// Just tell our active weapon to draw it's ammo, since our offhand doesn't use it
	if ( m_hLeftWeapon )
	{
		m_hLeftWeapon->DrawAmmo();
	}
	if ( m_hRightWeapon.Get() )
	{
		m_hRightWeapon->DrawAmmo();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Let each weapon handle input
//-----------------------------------------------------------------------------
void C_WeaponTwoHandedContainer::HandleInput( void )
{
	BaseClass::HandleInput();

	if ( m_hRightWeapon )
	{
		m_hRightWeapon->HandleInput();
	}
	if ( m_hLeftWeapon )
	{
		m_hLeftWeapon->HandleInput();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Let each weapon handle input
//-----------------------------------------------------------------------------
void C_WeaponTwoHandedContainer::OverrideMouseInput( float *x, float *y )
{
	BaseClass::OverrideMouseInput( x,y );

	if ( m_hRightWeapon )
	{
		m_hRightWeapon->OverrideMouseInput( x,y );
	}
	if ( m_hLeftWeapon )
	{
		m_hLeftWeapon->OverrideMouseInput( x,y );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_WeaponTwoHandedContainer::VisibleInWeaponSelection( void )
{
	return false;
}
