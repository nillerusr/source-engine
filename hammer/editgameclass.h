//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef EDITGAMECLASS_H
#define EDITGAMECLASS_H
#pragma once

#pragma warning(push, 1)
#pragma warning(disable:4701 4702 4530)
#include <fstream>
#pragma warning(pop)
#include "BlockArray.h"
#include "fgdlib/fgdlib.h"
#include "fgdlib/WCKeyValues.h"
#include "EntityConnection.h"


#define MAX_CLASS_NAME_LEN		64


class CChunkFile;
class CMapClass;
class CSaveInfo;


enum ChunkFileResult_t;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEditGameClass
{
	public:

		CEditGameClass(void);
		~CEditGameClass(void);

		inline bool IsClass(const char *pszClass = NULL);
		inline GDclass *GetClass(void) { return(m_pClass); }
		inline void SetClass(GDclass *pClass) { m_pClass = pClass; }
		inline const char* GetClassName(void) { return(m_szClass); }
		inline bool IsKeyFrameClass(void) { return((m_pClass != NULL) && (m_pClass->IsKeyFrameClass())); }
		inline bool IsMoveClass(void) { return((m_pClass != NULL) && (m_pClass->IsMoveClass())); }
		inline bool IsPointClass(void) { return((m_pClass != NULL) && (m_pClass->IsPointClass())); }
		inline bool IsNPCClass(void) { return((m_pClass != NULL) && (m_pClass->IsNPCClass())); }
		inline bool IsFilterClass(void) { return((m_pClass != NULL) && (m_pClass->IsFilterClass())); }
		inline bool IsSolidClass(void) { return((m_pClass != NULL) && (m_pClass->IsSolidClass())); }
		inline bool IsNodeClass(void) { return((m_pClass != NULL) && (m_pClass->IsNodeClass())); }
		static inline bool IsNodeClass(const char *pszClassName) { return GDclass::IsNodeClass(pszClassName); }

		//
		// Interface to key/value information:
		//
		virtual void SetKeyValue(LPCTSTR pszKey, LPCTSTR pszValue) { m_KeyValues.SetValue(pszKey, pszValue); }
		virtual void DeleteKeyValue(LPCTSTR pszKey) { m_KeyValues.RemoveKey(pszKey); }

		inline void RemoveKey(int nIndex) { m_KeyValues.RemoveKeyAt(nIndex); }
		inline void SetKeyValue(LPCTSTR pszKey, int iValue) { m_KeyValues.SetValue(pszKey, iValue); }
		inline LPCTSTR GetKey(int nIndex) { return(m_KeyValues.GetKey(nIndex)); }
		inline LPCTSTR GetKeyValue(int nIndex) { return(m_KeyValues.GetValue(nIndex)); }
		inline LPCTSTR GetKeyValue(LPCTSTR pszKey, int *piIndex = NULL) { return(m_KeyValues.GetValue(pszKey, piIndex)); }
		
		// Iterate the list of keyvalues.
		inline int GetFirstKeyValue() const			{ return m_KeyValues.GetFirst(); }
		inline int GetNextKeyValue( int i ) const	{ return m_KeyValues.GetNext( i ); }
		static inline int GetInvalidKeyValue()		{ return WCKeyValues::GetInvalidIndex(); }

		//
		// Interface to spawnflags.
		//
		bool GetSpawnFlag(unsigned long nFlag);
		unsigned long GetSpawnFlags(void);
		void SetSpawnFlag(unsigned long nFlag, bool bSet);
		void SetSpawnFlags(unsigned long nFlags);

		//
		// Interface to entity connections.
		//
		void Connections_Add(CEntityConnection *pConnection);
		inline int Connections_GetCount(void);
		inline CEntityConnection *Connections_Get(int nIndex);
		bool Connections_Remove(CEntityConnection *pConnection);
		void Connections_RemoveAll(void);
		void Connections_FixBad(bool bRelink = true);

		//
		// Interface to entity connections.
		//
		void Upstream_Add(CEntityConnection *pConnection);
		inline int Upstream_GetCount(void);
		inline CEntityConnection *Upstream_Get(int nIndex);
		bool Upstream_Remove(CEntityConnection *pConnection);
		void Upstream_RemoveAll(void);
		void Upstream_FixBad();

		//
		// Interface to comments.
		//
		inline const char *GetComments(void);
		inline void SetComments(const char *pszComments);

		//
		// Serialization functions.
		//
		static ChunkFileResult_t LoadConnectionsCallback(CChunkFile *pFile, CEditGameClass *pEditGameClass);
		static ChunkFileResult_t LoadKeyCallback(const char *szKey, const char *szValue, CEditGameClass *pEditGameClass);

		ChunkFileResult_t SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo);

		int SerializeRMF(std::fstream&, BOOL);
		int SerializeMAP(std::fstream&, BOOL);

		virtual void SetClass(LPCTSTR pszClassname, bool bLoading = false);
		CEditGameClass *CopyFrom(CEditGameClass *pFrom);
		void GetDefaultKeys( void );

		virtual void SetAngles(const QAngle &vecAngles);
		virtual void GetAngles(QAngle &vecAngles);

		// Import the old-style yaw only representation of orientation.
		void ImportAngle(int nAngle);

	protected:

		WCKeyValues m_KeyValues;
		GDclass *m_pClass;
		char m_szClass[MAX_CLASS_NAME_LEN];
		char *m_pszComments;		// Comments text, dynamically allocated.

		static char *g_pszEmpty;

		CEntityConnectionList m_Connections;
		CEntityConnectionList m_Upstream;
};


//-----------------------------------------------------------------------------
// Purpose: Returns the number of input/output connections that this object has.
//-----------------------------------------------------------------------------
int CEditGameClass::Connections_GetCount(void)
{
	return m_Connections.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the number of input/output connections that this object has.
//-----------------------------------------------------------------------------
CEntityConnection *CEditGameClass::Connections_Get(int nIndex)
{
	return m_Connections.Element(nIndex);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the number of input/output connections that this object has.
//-----------------------------------------------------------------------------
int CEditGameClass::Upstream_GetCount(void)
{
	return m_Upstream.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the number of input/output connections that this object has.
//-----------------------------------------------------------------------------
CEntityConnection *CEditGameClass::Upstream_Get(int nIndex)
{
	return m_Upstream.Element(nIndex);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the comments text, NULL if none have been set.
//-----------------------------------------------------------------------------
const char *CEditGameClass::GetComments(void)
{
	if (m_pszComments == NULL)
	{
		return(g_pszEmpty);
	}

	return(m_pszComments);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : NULL - 
// Output : inline bool
//-----------------------------------------------------------------------------
inline bool CEditGameClass::IsClass(const char *pszClass)
{
	if (pszClass == NULL)
	{
		return(m_pClass != NULL);
	}
	return((m_pClass != NULL) && (!stricmp(pszClass, m_szClass)));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszComments - 
//-----------------------------------------------------------------------------
void CEditGameClass::SetComments(const char *pszComments)
{
	delete m_pszComments;

	if (pszComments != NULL)
	{
		int nLen = strlen(pszComments);
		if (nLen == 0)
		{
			m_pszComments = NULL;
		}
		else
		{
			m_pszComments = new char [nLen + 1];
			strcpy(m_pszComments, pszComments);
		}
	}
	else
	{
		m_pszComments = NULL;
	}
}


#endif // EDITGAMECLASS_H
