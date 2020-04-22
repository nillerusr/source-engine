//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef MANIFEST_H
#define MANIFEST_H
#pragma once

#include "KeyValues.h"
#include "utlvector.h"
#include "MapDoc.h"

class BoundBox;
class CSelection;
class CMapWorld;
class CManifestInstance;
typedef struct SManifestLoadPrefs TManifestLoadPrefs;

class CManifestMap
{
public:
 	CManifestMap( void );

	bool IsEditable( void );

	CMapDoc				*m_Map;
	CString				m_RelativeMapFileName, m_AbsoluteMapFileName;
	CString				m_FriendlyName;
	bool				m_bTopLevelMap;
	bool				m_bReadOnly;
	bool				m_bIsVersionControlled;
	bool				m_bCheckedOut;
	bool				m_bDefaultCheckin;
	int					m_InternalID;
	CManifestInstance	*m_Entity;

	// user prefs ( which don't impact the bsp process )
	bool				m_bVisible;
	bool				m_bPrimaryMap;
	bool				m_bProtected;
};


class CManifestInstance : public CMapEntity
{
public:
	DECLARE_MAPCLASS( CManifestInstance, CMapEntity )

	CManifestInstance( void );
	CManifestInstance( CManifestMap *pManifestMap );

	virtual bool		IsEditable( void );
	CManifestMap		*GetManifestMap( void ) { return m_pManifestMap; }

private:
	CManifestMap		*m_pManifestMap;
};


class CManifest : public CMapDoc
{
	friend class CManifestCheckin;

protected:
	DECLARE_DYNCREATE(CManifest)

public:
	CManifest( void );
	~CManifest( void );

	static ChunkFileResult_t LoadKeyInfoCallback( const char *szKey, const char *szValue, CManifest *pDoc );
	static ChunkFileResult_t LoadManifestInfoCallback( CChunkFile *pFile, CManifest *pDoc );
	static ChunkFileResult_t LoadKeyCallback( const char *szKey, const char *szValue, CManifestMap *pManifestMap );
	static ChunkFileResult_t LoadManifestVMFCallback( CChunkFile *pFile, CManifest *pDoc );
	static ChunkFileResult_t LoadManifestMapsCallback( CChunkFile *pFile, CManifest *pDoc );
	static ChunkFileResult_t LoadKeyPrefsCallback( const char *szKey, const char *szValue, TManifestLoadPrefs *pManifestLoadPrefs );
	static ChunkFileResult_t LoadManifestVMFPrefsCallback( CChunkFile *pFile, CManifest *pDoc );
	static ChunkFileResult_t LoadManifestMapsPrefsCallback( CChunkFile *pFile, CManifest *pDoc );
	static ChunkFileResult_t LoadManifestCordoningPrefsCallback( CChunkFile *pFile, CManifest *pDoc );

	bool			Load( const char *pszFileName );
	bool			Save( const char *pszFileName, bool bForce );
	bool			IsValid( void ) { return m_bIsValid; }

	virtual void	Initialize( void );
	virtual void	Update( void );
	virtual void	SetModifiedFlag( BOOL bModified = TRUE );
	virtual bool	IsManifest( void ) { return true; }

	void			GetFullMapPath( const char *pManifestMapFileName, char *pOutputPath );

	void			SetManifestPrefsModifiedFlag( bool bModified = true );
	int				GetNumMaps( void ) { return m_Maps.Count(); }
	CManifestMap	*GetMap( int index ) { return m_Maps[ index ]; }
	CManifestMap	*FindMap( CMapDoc *pMap );
	CManifestMap	*FindMapByID( int InternalID );
	void			SetPrimaryMap( CManifestMap	*pManifestMap );
	CManifestMap	*GetPrimaryMap( void ) { return m_pPrimaryMap; }
	void			SetVisibility( CManifestMap	*pManifestMap, bool bIsVisible );

	void			MoveSelectionToSubmap( CManifestMap *pManifestMap, bool CenterContents );
	CManifestMap	*MoveSelectionToNewSubmap( CString &FriendlyName, CString &FileName, bool CenterContents );
	CManifestMap	*CreateNewMap( const char *AbsoluteFileName, const char *RelativeFileName, bool bSetID );
	CManifestMap	*AddNewSubmap( CString &FriendlyName, CString &FileName );
	bool			AddExistingMap( const char *pszFileName, bool bFromInstance );
	bool			AddExistingMap( void );
	bool			RemoveSubMap( CManifestMap *pManifestMap );

	bool			CheckOut( );
	bool			AddToVersionControl( );
	void			CheckFileStatus( );

	CSelection		*GetSelection( void ) { return m_pSelection; }
	void			ClearSelection( void );

	virtual void	UpdateInstanceMap( CMapDoc *pInstanceMapDoc );
	virtual void	AddObjectToWorld(CMapClass *pObject, CMapClass *pParent = NULL);

	CMapWorld		*GetManifestWorld( void ) { return m_ManifestWorld; }

	bool			m_bReadOnly;
	bool			m_bIsVersionControlled;
	bool			m_bCheckedOut;
	bool			m_bDefaultCheckin;

protected:
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);
	virtual void DeleteContents( void );

private:
	void			AddManifestObjectToWorld( CMapClass *pObject, CMapClass *pParent = NULL );
	void			RemoveManifestObjectFromWorld( CMapClass *pObject, bool bRemoveChildren );
	bool			LoadVMFManifest( const char *pszFileName );
	bool			LoadVMFManifestUserPrefs( const char *pszFileName );
	bool			SaveVMFManifest( const char *pszFileName );
	bool			SaveVMFManifestMaps( const char *pszFileName );
	bool			SaveVMFManifestUserPrefs( const char *pszFileName );

	int								m_NextInternalID;
	bool							m_bIsValid;
	bool							m_bRelocateSave;
	CUtlVector< CManifestMap * >	m_Maps;
	CManifestMap					*m_pPrimaryMap;
	char							m_ManifestDir[ MAX_PATH ];
	CMenu							m_ManifestMenu;
	CMapWorld						*m_ManifestWorld;
	bool							m_bManifestChanged, m_bManifestUserPrefsChanged;
	CHistory						*m_pSaveUndo;
	CHistory						*m_pSaveRedo;

protected:
	//{{AFX_MSG(CMapDoc)
	afx_msg void OnFileSaveAs();
	//}}AFX_MSG

public:
	DECLARE_MESSAGE_MAP()
};

#endif // #define MANIFEST_H