//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SCRIPTOBJECT_H
#define SCRIPTOBJECT_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>

class CPanelListPanel;

#define SCRIPT_VERSION 1.0f

typedef void * FileHandle_t;

enum objtype_t
{
	O_BADTYPE,
	O_BOOL,
	O_NUMBER,
	O_LIST,
	O_STRING,
	O_OBSOLETE,
};

typedef struct
{
	objtype_t type;
	char szDescription[32];
} objtypedesc_t;

class CScriptListItem
{
public:
	CScriptListItem();
	CScriptListItem( char const *strItem, char const *strValue );

	char szItemText[128];
	char szValue[256];

	CScriptListItem *pNext;
};

class CScriptObject : public vgui::Panel
{
public:
	void AddItem( CScriptListItem *pItem );
	CScriptObject( void );
	~CScriptObject();

	bool ReadFromBuffer( const char **pBuffer, bool isNewObject );
	void WriteToConfig();
	void WriteToFile( FileHandle_t fp );
	void WriteToScriptFile( FileHandle_t fp );
	void SetCurValue( char const *strValue );

	objtype_t GetType( char *pszType );

	objtype_t type;

	char cvarname[64 ];
	char prompt[ 256 ];

	CScriptListItem *pListItems;

	float fMin, fMax;

	char	defValue[ 128 ];  // Default value string
	float   fdefValue; // Float version of default value.

	char	curValue[ 128 ];
	float   fcurValue;

	bool bSetInfo;  // Prepend "Setinfo" to keyvalue pair in config?
	// Linked list of default list box items.

	CScriptObject *pNext;
};

abstract_class CDescription
{
public:
	CDescription( CPanelListPanel *panel );
	~CDescription();

	bool ReadFromBuffer( const char **pBuffer );
	bool InitFromFile( char *pszFileName );
	void TransferCurrentValues( const char *pszConfigFile );

	void AddObject( CScriptObject *pItem );
	void WriteToConfig();
	void WriteToFile( FileHandle_t fp );
	void WriteToScriptFile( FileHandle_t fp );

	virtual void WriteScriptHeader( FileHandle_t fp ) = 0; // Clients must implement this.
	virtual void WriteFileHeader( FileHandle_t fp ) = 0; // Clients must implement this.

	void setDescription( const char *pszDesc );
	void setHint( const char *pszHint );

	const char *GetDescription( void ) { return m_pszDescriptionType; };
	const char *getHint( void ) { return m_pszHintText; } ;
public:
	CScriptObject *pObjList;

private:
	CScriptObject * FindObject( const char *pszObjectName );

	char *m_pszHintText;
	char *m_pszDescriptionType;

	CPanelListPanel *m_pListPanel;
};

namespace vgui
{
	class Label;
	class Panel;
}

class mpcontrol_t : public vgui::Panel
{
public:
	mpcontrol_t( vgui::Panel *parent, char const *panelName );

	virtual	void	OnSizeChanged( int wide, int tall );

	objtype_t		type;
	vgui::Panel		*pControl;
	vgui::Label		*pPrompt;
	CScriptObject	*pScrObj;

	mpcontrol_t		*next;
};

class CInfoDescription : public CDescription
{
public:
	CInfoDescription( CPanelListPanel *panel );

	void WriteScriptHeader( FileHandle_t fp );
	void WriteFileHeader( FileHandle_t fp ); 
};

void UTIL_StripInvalidCharacters( char *pszInput, int maxlen );

#endif // SCRIPTOBJECT_H
