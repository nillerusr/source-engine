//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ITREEITEM_H
#define ITREEITEM_H
#ifdef _WIN32
#pragma once
#endif

class CWorkspace;
class CProject;
class CScene;
class CVCDFile;
class CSoundEntry;
class CWaveFile;
class mxTreeView;

class ITreeItem
{
public:
	ITreeItem()
	{
		m_bExpanded = false;
		m_nOrdinal = -1;
	}

	virtual char const *GetName() const = 0;

	ITreeItem *GetParentItem();

	virtual CWorkspace	*GetWorkspace() = 0;
	virtual CProject	*GetProject() = 0;
	virtual CScene		*GetScene() = 0;
	virtual CVCDFile	*GetVCDFile() = 0;
	virtual CSoundEntry	*GetSoundEntry() = 0;
	virtual CWaveFile	*GetWaveFile() = 0;

	virtual int			GetIconIndex() const = 0;

	bool IsExpanded() const
	{
		return m_bExpanded;
	}

	void SetExpanded( bool exp )
	{
		m_bExpanded = exp;
	}

	mxTreeViewItem *FindItem( mxTreeView *tree, mxTreeViewItem *parent, bool recurse = false );

	virtual void Checkout( bool updatestateicons = true ) = 0;
	virtual void Checkin( bool updatestateicons = true ) = 0;

	virtual void MoveChildUp( ITreeItem *child ) = 0;
	virtual void MoveChildDown( ITreeItem *child ) = 0;

	virtual bool	IsFirstChild()
	{
		if ( !GetParentItem() )
			return false;

		return GetParentItem()->IsChildFirst( this );
	}

	virtual bool	IsLastChild()
	{
		if ( !GetParentItem() )
			return false;

		return GetParentItem()->IsChildLast( this );
	}

	virtual bool		IsChildFirst( ITreeItem *child ) = 0;
	virtual bool		IsChildLast( ITreeItem *child ) = 0;

	void				SetOrdinal( int ordinal ) { m_nOrdinal = ordinal; }
	int					GetOrdinal( void ) const { return m_nOrdinal; }

	virtual void		SetDirty( bool dirty ) = 0;
private:
	bool				m_bExpanded;
	int					m_nOrdinal;
};

#endif // ITREEITEM_H
