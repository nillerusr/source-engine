#include "kv_editor_base_panel.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CKV_Editor_Base_Panel::CKV_Editor_Base_Panel( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_pKey = NULL;
	m_pKeyParent = NULL;
	m_flSortOrder = 0;
	m_pFileSpecNode = NULL;
	m_pEditor = NULL;
	m_bAllowDeletion = true;
}

void CKV_Editor_Base_Panel::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CKV_Editor_Base_Panel::SetKey( KeyValues *pKey )
{
	if ( pKey == m_pKey )
		return;

	m_pKey = pKey;
	UpdatePanel();
}

void CKV_Editor_Base_Panel::SetKeyParent( KeyValues *pKey )
{
	if ( pKey == m_pKeyParent )
		return;

	m_pKeyParent = pKey;
}

void CKV_Editor_Base_Panel::SetFileSpecNode( KeyValues *pKey )
{
	m_pFileSpecNode = pKey;

	ApplySettings( pKey );
}