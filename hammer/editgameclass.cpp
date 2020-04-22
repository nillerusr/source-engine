//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a class that encapsulates much of the functionality
//			of entities. CMapWorld and CMapEntity are both derived from this
//			class.
//
//			CEditGameClass-derived objects have the following properties:
//
//			Key/value pairs - A list of string pairs that hold data properties
//				of the object. Each property has a unique name.
//
//			Connections - A list of outputs in this object that are connected to
//				inputs in another entity.
//
//=============================================================================//

#include "stdafx.h"
#include "ChunkFile.h"
#include "fgdlib/GameData.h"
#include "GameConfig.h"
#include "EditGameClass.h"
#include "MapEntity.h"
#include "mathlib/Mathlib.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//
// An empty string returned by GetComments when we have no comments set.
//
char *CEditGameClass::g_pszEmpty = "";


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//-----------------------------------------------------------------------------
CEditGameClass::CEditGameClass(void)
{
	m_pClass = NULL;
	m_szClass[0] = '\0';
	m_pszComments = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees memory.
//-----------------------------------------------------------------------------
CEditGameClass::~CEditGameClass(void)
{
	delete m_pszComments;

	Connections_RemoveAll();
	Upstream_RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pConnection - 
//-----------------------------------------------------------------------------
void CEditGameClass::Connections_Add(CEntityConnection *pConnection)
{
#if	defined(_DEBUG) && 0
		LPCTSTR	pszTargetName = GetKeyValue("targetname");
		if ( pszTargetName && !strcmp(pszTargetName, "zapperpod7_rotator") )
		{
			// Set breakpoint here for debugging this entity's visiblity
			int foo = 0;
		}
#endif

	if ( m_Connections.Find(pConnection) == -1 )
		m_Connections.AddToTail(pConnection);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pConnection - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEditGameClass::Connections_Remove(CEntityConnection *pConnection)
{
	int nIndex = m_Connections.Find(pConnection);
	if (nIndex != -1)
	{
		m_Connections.Remove(nIndex);
		return(true);
	}

	return(false);
}


//-----------------------------------------------------------------------------
// NOTE: unlike Connections_Remove, this actually frees each connection!!
//-----------------------------------------------------------------------------
void CEditGameClass::Connections_RemoveAll()
{
	//
	// Remove all our connections from their targets' upstream lists.
	//	
	int nConnectionsCount = m_Connections.Count();
	for (int nConnection = 0; nConnection < nConnectionsCount; nConnection++)
	{
		CEntityConnection *pConnection = m_Connections.Element( nConnection );

#if defined( ENTITY_MAINTAIN_UPSTREAM_LISTS )
		CMapEntityList *pTargetList = pConnection->GetTargetEntityList();
		if ( pTargetList )
		{
			FOR_EACH_OBJ( *pTargetList, pos )
			{
				CMapEntity *pEntity = pTargetList->Element( pos );
				pEntity->Upstream_Remove( pConnection );
			}
		}
#endif 
		
		delete pConnection;
	}

	m_Connections.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEditGameClass::Connections_FixBad(bool bRelink)
{
	int nConnectionsCount = m_Connections.Count();
	for (int nConnections = 0; nConnections < nConnectionsCount; nConnections++)
	{
		CEntityConnection *pConnection = m_Connections.Element(nConnections);
		CMapEntityList *pTargetEntities = pConnection->GetTargetEntityList();
		int nEntityCount = pTargetEntities->Count();

		for ( int nEntities = 0; nEntities < nEntityCount; nEntities++ )
		{
			CMapEntity *pEntity = pTargetEntities->Element(nEntities);
			pEntity->Upstream_Remove( pConnection );
		}

		if ( bRelink )
			pConnection->LinkTargetEntities();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pConnection - 
//-----------------------------------------------------------------------------
void CEditGameClass::Upstream_Add(CEntityConnection *pConnection)
{
#if	defined(_DEBUG) && 0
		LPCTSTR	pszTargetName = GetKeyValue("targetname");
		if ( pszTargetName && !strcmp(pszTargetName, "zapperpod7_rotator") )
		{
			// Set breakpoint here for debugging this entity's visiblity
			int foo = 0;
		}
#endif

#if defined( ENTITY_MAINTAIN_UPSTREAM_LISTS )
	if ( m_Upstream.Find(pConnection) == -1 )
		m_Upstream.AddToTail(pConnection);
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pConnection - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEditGameClass::Upstream_Remove(CEntityConnection *pConnection)
{
	int nIndex = m_Upstream.Find(pConnection);
	if (nIndex != -1)
	{
		m_Upstream.Remove(nIndex);
		return(true);
	}

	return(false);
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEditGameClass::Upstream_RemoveAll(void)
{
#if defined( ENTITY_MAINTAIN_UPSTREAM_LISTS )
	//
	// Remove all our connections from their targets' upstream lists.
	//	
	int nUpstreamCount = m_Upstream.Count();
	for (int nConnection = 0; nConnection < nUpstreamCount; nConnection++)
	{
		CEntityConnection *pConnection = m_Upstream.Element( nConnection );

		CMapEntityList *pSourceList = pConnection->GetSourceEntityList();
		if ( pSourceList )
		{
			FOR_EACH_OBJ( *pSourceList, pos )
			{
				CMapEntity *pEntity = pSourceList->Element( pos );
				pEntity->Connection_Remove( pConnection );
			}
		}
	}
#endif 

	m_Upstream.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEditGameClass::Upstream_FixBad()
{
#if	defined(_DEBUG) && 0
		LPCTSTR	pszTargetName = GetKeyValue("targetname");
		if ( pszTargetName && !strcmp(pszTargetName, "cave_guard_seq1") )
		{
			// Set breakpoint here for debugging this entity
			int foo = 0;
		}
#endif

	int nUpstreamCount = m_Upstream.Count();
	for (int nUpstream = 0; nUpstream < nUpstreamCount; nUpstream++)
	{
		CEntityConnection *pUpstream = m_Upstream.Element(nUpstream);
	    pUpstream->LinkTargetEntities();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszClass - 
//			bLoading - 
//-----------------------------------------------------------------------------
void CEditGameClass::SetClass(LPCTSTR pszClass, bool bLoading)
{
	extern GameData *pGD;
	strcpy(m_szClass, pszClass);

	StripEdgeWhiteSpace(m_szClass);

	if (pGD)
	{
		m_pClass = pGD->ClassForName(m_szClass);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Copies the data from a given CEditGameClass object into this one.
// Input  : pFrom - Object to copy.
// Output : Returns a pointer to this.
//-----------------------------------------------------------------------------
CEditGameClass *CEditGameClass::CopyFrom(CEditGameClass *pFrom)
{
	m_pClass = pFrom->m_pClass;
	strcpy( m_szClass, pFrom->m_szClass );

	//
	// Copy all the keys.
	//
	m_KeyValues.RemoveAll();
	for ( int i=pFrom->GetFirstKeyValue(); i != pFrom->GetInvalidKeyValue(); i=pFrom->GetNextKeyValue( i ) )
	{
		m_KeyValues.SetValue(pFrom->GetKey(i), pFrom->GetKeyValue(i));
	}

	//
	// Copy all the connections objects
	//
	Connections_RemoveAll();
	int nConnCount = pFrom->Connections_GetCount();
	for (int i = 0; i < nConnCount; i++)
	{
		CEntityConnection *pConn = pFrom->Connections_Get(i);
		CEntityConnection *pNewConn = new CEntityConnection( *pConn );
		Connections_Add(pNewConn);
	}

	//
	// Copy the comments.
	//
	SetComments(pFrom->GetComments());

	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: Applies the default keys for this object's game class. Called when
//			the entity's class is changed.
//-----------------------------------------------------------------------------
void CEditGameClass::GetDefaultKeys( void )
{
	if ( m_pClass != NULL )
	{
		//
		// For each variable from the base class...
		//
		int nVariableCount = m_pClass->GetVariableCount();
		for ( int i = 0; i < nVariableCount; i++ )
		{
			GDinputvariable *pVar = m_pClass->GetVariableAt(i);
			Assert(pVar != NULL);

			if (pVar != NULL)
			{
				int iIndex;
				LPCTSTR p = m_KeyValues.GetValue(pVar->GetName(), &iIndex);

				//
				// If the variable is not present in this object, set the default value.
				//
				if (p == NULL) 
				{
					MDkeyvalue tmpkv;
					pVar->ResetDefaults();
					pVar->ToKeyValue(&tmpkv);

					//
					// Only set the key value if it is non-zero.
					//
					if ((tmpkv.szKey[0] != 0) && (tmpkv.szValue[0] != 0) && (stricmp(tmpkv.szValue, "0")))
					{
						SetKeyValue(tmpkv.szKey, tmpkv.szValue);
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns this object's angles as a vector of the form:
//			[0] PITCH [1] YAW [2] ROLL
//-----------------------------------------------------------------------------
void CEditGameClass::GetAngles(QAngle &vecAngles)
{
	vecAngles = vec3_angle;
	const char *pszAngles = GetKeyValue("angles");
	if (pszAngles != NULL)
	{
		sscanf(pszAngles, "%f %f %f", &vecAngles[PITCH], &vecAngles[YAW], &vecAngles[ROLL]);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets a new yaw, overriding any existing angles. Uses special values
//			for yaw to indicate pointing straight up or straight down.
//
//			This method of representing orientation is obsolete; this code is
//			only for importing old RMF or MAP files.
//
// Input  : a - Value for angle.
//-----------------------------------------------------------------------------
void CEditGameClass::ImportAngle(int nAngle)
{
	QAngle vecAngles;
	vecAngles.Init();

	if (nAngle == -1) // UP
	{
		vecAngles[PITCH] = -90;
	}
	else if (nAngle == -2) // DOWN
	{
		vecAngles[PITCH] = 90;
	}
	else
	{
		vecAngles[YAW] = nAngle;
	}

	SetAngles(vecAngles);
}


//-----------------------------------------------------------------------------
// Purpose: Sets this object's angles as a vector of the form:
//			[0] PITCH [1] YAW [2] ROLL
//-----------------------------------------------------------------------------
void CEditGameClass::SetAngles(const QAngle &vecAngles)
{
	char szAngles[80];	
	sprintf(szAngles, "%g %g %g", (double)vecAngles[PITCH], (double)vecAngles[YAW], (double)vecAngles[ROLL]);
	SetKeyValue("angles", szAngles);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CEditGameClass::SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo)
{
	ChunkFileResult_t eResult = pFile->WriteKeyValue("classname", m_szClass);

	if (eResult != ChunkFile_Ok)
	{
		return(eResult);
	}

	//
	// Determine whether we have a game data class. This will help us decide which keys
	// to write.
	//
	GDclass *pGameDataClass = NULL;
	if (pGD != NULL)
	{
		pGameDataClass = pGD->ClassForName(m_szClass);
	}

	//
	// Consider all the keyvalues in this object for serialization.
	//
	for ( int z=m_KeyValues.GetFirst(); z != m_KeyValues.GetInvalidIndex(); z=m_KeyValues.GetNext( z ) )
	{
		MDkeyvalue &KeyValue = m_KeyValues.GetKeyValue(z);

		//
		// Don't write keys that were already written above.
		//
		bool bAlreadyWritten = false;
		if (!stricmp(KeyValue.szKey, "classname"))
		{
			bAlreadyWritten = true;
		}

		//
		// If the variable wasn't already written above.
		//
		if (!bAlreadyWritten)
		{
			//
			// Write it to the MAP file.
			//
			eResult = pFile->WriteKeyValue(KeyValue.szKey, KeyValue.szValue);
			if (eResult != ChunkFile_Ok)
			{
				return(eResult);
			}
		}
	}

	//
	// If we have a game data class, for each keyvalue in the class definition, write out all keys
	// that are not present in the object and whose defaults are nonzero in the class definition.
	//
	if (pGameDataClass != NULL)
	{
		//
		// For each variable from the base class...
		//
		int nVariableCount = pGameDataClass->GetVariableCount();
		for (int i = 0; i < nVariableCount; i++)
		{
			GDinputvariable *pVar = pGameDataClass->GetVariableAt(i);
			Assert(pVar != NULL);

			if (pVar != NULL)
			{
				int iIndex;
				LPCTSTR p = m_KeyValues.GetValue(pVar->GetName(), &iIndex);

				//
				// If the variable is not present in this object, write out the default value.
				//
				if (p == NULL) 
				{
					MDkeyvalue TempKey;
					pVar->ResetDefaults();
					pVar->ToKeyValue(&TempKey);

					//
					// Only write the key value if it is non-zero.
					//
					if ((TempKey.szKey[0] != 0) && (TempKey.szValue[0] != 0) && (stricmp(TempKey.szValue, "0")))
					{
						eResult = pFile->WriteKeyValue(TempKey.szKey, TempKey.szValue);
						if (eResult != ChunkFile_Ok)
						{
							return(eResult);
						}
					}
				}
			}
		}
	}

	//
	// Save all the connections.
	//
	if ((eResult == ChunkFile_Ok) && (Connections_GetCount() > 0))
	{
		eResult = pFile->BeginChunk("connections");
		if (eResult == ChunkFile_Ok)
		{
			int nConnCount = Connections_GetCount();
			for (int i = 0; i < nConnCount; i++)
			{
				CEntityConnection *pConnection = Connections_Get(i);
				if (pConnection != NULL)
				{
					char szTemp[512];

					sprintf(szTemp, "%s,%s,%s,%g,%d", pConnection->GetTargetName(), pConnection->GetInputName(), pConnection->GetParam(), pConnection->GetDelay(), pConnection->GetTimesToFire());
					eResult = pFile->WriteKeyValue(pConnection->GetOutputName(), szTemp);

					if (eResult != ChunkFile_Ok)
					{
						return(eResult);
					}
				}
			}
		
			eResult = pFile->EndChunk();
		}
	}

	return(eResult);
}

//-----------------------------------------------------------------------------
// Purpose: Builds a connection from a keyvalue pair.
// Input  : szKey - Contains the name of the output.
//			szValue - Contains the target, input, delay, and parameter, comma delimited.
//			pEditGameClass - Entity to receive the connection.
// Output : Returns ChunkFile_Ok if the data was well-formed, ChunkFile_Fail if not.
//-----------------------------------------------------------------------------
ChunkFileResult_t CEditGameClass::LoadKeyCallback(const char *szKey, const char *szValue, CEditGameClass *pEditGameClass)
{
	CEntityConnection *pConnection = new CEntityConnection;

	// Set the "source" name to be the name of the pEditGameClass' targetname
	pConnection->SetSourceName( pEditGameClass->GetKeyValue("targetname") ); // Use the classname if no targetname is defined?
	
	// Set the "output" from the passed in parameter
	pConnection->SetOutputName(szKey);

	char szToken[MAX_PATH];

	//
	// Parse the target name.
	//
	const char *psz = nexttoken(szToken, szValue, ',');
	if (szToken[0] != '\0')
	{
		pConnection->SetTargetName(szToken);
	}

	//
	// Parse the input name.
	//
	psz = nexttoken(szToken, psz, ',');
	if (szToken[0] != '\0')
	{
		pConnection->SetInputName(szToken);
	}

	//
	// Parse the parameter override.
	//
	psz = nexttoken(szToken, psz, ',');
	if (szToken[0] != '\0')
	{
		pConnection->SetParam(szToken);
	}

	//
	// Parse the delay.
	//
	psz = nexttoken(szToken, psz, ',');
	if (szToken[0] != '\0')
	{
		pConnection->SetDelay((float)atof(szToken));
	}

	//
	// Parse the number of times to fire the output.
	//
	nexttoken(szToken, psz, ',');
	if (szToken[0] != '\0')
	{
		pConnection->SetTimesToFire(atoi(szToken));
	}

	pEditGameClass->Connections_Add(pConnection); // Does this belong here or SetSourceName or LinkSourceEntities??

	return(ChunkFile_Ok);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
//			*pEntity - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CEditGameClass::LoadConnectionsCallback(CChunkFile *pFile, CEditGameClass *pEditGameClass)
{
	return(pFile->ReadChunk((KeyHandler_t)LoadKeyCallback, pEditGameClass));
}


//-----------------------------------------------------------------------------
// Purpose: Returns all the spawnflags.
//-----------------------------------------------------------------------------
unsigned long CEditGameClass::GetSpawnFlags(void)
{
	LPCTSTR pszVal = GetKeyValue("spawnflags");
	if (pszVal == NULL)
	{
		return(0);
	}

	unsigned long val = 0;
	sscanf( pszVal, "%lu", &val );
	return val;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if a given spawnflag (or flags) is set, false if not.
//-----------------------------------------------------------------------------
bool CEditGameClass::GetSpawnFlag(unsigned long nFlags)
{
	unsigned long nSpawnFlags = GetSpawnFlags();
	return((nSpawnFlags & nFlags) != 0);
}


//-----------------------------------------------------------------------------
// Purpose: Sets the given spawnflag (or flags) to the given state.
// Input  : nFlag - Flag values  to set or clear (ie 1, 2, 4, 8, 16, etc.)
//			bSet - True to set the flags, false to clear them.
//-----------------------------------------------------------------------------
void CEditGameClass::SetSpawnFlag(unsigned long nFlags, bool bSet)
{
	unsigned long nSpawnFlags = GetSpawnFlags();

	if (bSet)
	{
		nSpawnFlags |= nFlags;
	}
	else
	{
		nSpawnFlags &= ~nFlags;
	}

	SetSpawnFlags(nSpawnFlags);
}


//-----------------------------------------------------------------------------
// Purpose: Sets all the spawnflags at once.
// Input  : nSpawnFlags - New value for spawnflags.
//-----------------------------------------------------------------------------
void CEditGameClass::SetSpawnFlags(unsigned long nSpawnFlags)
{
	char szValue[80];
	V_snprintf( szValue, sizeof( szValue ), "%lu", nSpawnFlags );
	SetKeyValue("spawnflags", nSpawnFlags);
}
