//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include <io.h>
#include "hammer.h"
#include "GlobalFunctions.h"
#include "MapErrorsDlg.h"
#include "Options.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapFace.h"
#include "MapGroup.h"
#include "MapSolid.h"
#include "MapStudioModel.h"
#include "MapWorld.h"
#include "progdlg.h"
#include "TextureSystem.h"
#include "MapDisp.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#pragma optimize("g", off)

#pragma warning(disable: 4748)		// buffer overrung with optimizations off	 - remove if we turn "g" back on

#define TEXTURE_NAME_LEN 128

// All files are opened in binary, and we want to save CR/LF
#define ENDLINE "\r\n"


enum
{
	fileOsError = -1,	// big error!
	fileError = -2,		// problem
	fileOk = -3,		// loaded ok
	fileDone = -4		// got not-my-kind of line
};


BOOL bSaveVisiblesOnly;


static BOOL bErrors;
static int nInvalidSolids;
static CProgressDlg *pProgDlg;

static MAPFORMAT MapFormat;

static BOOL bStuffed;
static char szStuffed[255];

static UINT uMapVersion = 0;


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//-----------------------------------------------------------------------------
static void StuffLine(char * buf)
{
	Assert(!bStuffed);
	V_strcpy_safe(szStuffed, buf);
	bStuffed = TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			buf - 
//-----------------------------------------------------------------------------
static void GetLine(std::fstream& file, char *buf)
{
	if(bStuffed)
	{
		if(buf)
			strcpy(buf, szStuffed);
		bStuffed = FALSE;
		return;
	}

	char szBuf[1024];

	while(1)
	{
		file >> std::ws;
		file.getline(szBuf, 512);
		if(file.eof())
			return;
		if(!strncmp(szBuf, "//", 2))
			continue;
		file >> std::ws;
		if(buf)
		{
//			char *p = strchr(szBuf, '\n');
//			if(p) p[0] = 0;
//			p = strchr(szBuf, '\r');
//			if(p) p[0] = 0;
			strcpy(buf, szBuf);
		}
		return;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//			file - 
//			pIntersecting - 
// Output : int
//-----------------------------------------------------------------------------
static int SaveSolidChildrenOf(CMapClass *pObject, std::fstream& file, BoundBox *pIntersecting = NULL)
{
	CMapWorld *pWorld = (CMapWorld*) CMapClass::GetWorldObject(pObject);

	//
	// If we are only saving visible objects and this object isn't visible, don't save it.
	//
	if (bSaveVisiblesOnly && (pObject != pWorld) && !pObject->IsVisible())
	{
		return fileOk;	// not an error - return ok
	}
	
	//
	// If we are only saving objects within a particular bounding box and this object isn't, don't save it.
	//
	if (pIntersecting && !pObject->IsIntersectingBox(pIntersecting->bmins, pIntersecting->bmaxs))
	{
		return fileOk;
	}

	
	const CMapObjectList *pChildren = pObject->GetChildren();
	FOR_EACH_OBJ( *pChildren, pos )
	{
		int iRvl = -1;
		CMapClass *pChild = pChildren->Element(pos);

		if (!pIntersecting || pChild->IsIntersectingBox(pIntersecting->bmins, pIntersecting->bmaxs))
		{
			if (pChild->IsMapClass(MAPCLASS_TYPE(CMapSolid)))
			{
				if (!bSaveVisiblesOnly || pChild->IsVisible())
				{
					iRvl = pChild->SerializeMAP(file, TRUE);
				}
			}
			else if (pChild->IsMapClass(MAPCLASS_TYPE(CMapGroup)))
			{
				iRvl = SaveSolidChildrenOf(pChild, file, pIntersecting);
			}

			// return error if there is an error
			if (iRvl != -1 && iRvl != fileOk)
			{
				return iRvl;
			}
		}
	}

	return fileOk;	// ok.
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//			file - 
//			pIntersecting - 
// Output : int
//-----------------------------------------------------------------------------
static int SaveEntityChildrenOf(CMapClass *pObject, std::fstream& file, BoundBox *pIntersecting)
{
	CMapWorld *pWorld = (CMapWorld *)CMapClass::GetWorldObject(pObject);

	if (bSaveVisiblesOnly && pObject != pWorld && !pObject->IsVisible())
	{
		return fileOk;	// no error
	}

	if (pIntersecting && !pObject->IsIntersectingBox(pIntersecting->bmins, pIntersecting->bmaxs))
	{
		return fileOk;
	}
	
	
	const CMapObjectList *pChildren = pObject->GetChildren();
	FOR_EACH_OBJ( *pChildren, pos )
	{
		int iRvl = -1;

		CMapClass *pChild = pChildren->Element(pos);

		if (!pIntersecting || pChild->IsIntersectingBox(pIntersecting->bmins, pIntersecting->bmaxs))
		{
			if (pChild->IsMapClass(MAPCLASS_TYPE(CMapEntity)))
			{
				if (!bSaveVisiblesOnly || pChild->IsVisible())
				{
					iRvl = pChild->SerializeMAP(file, TRUE);
				}
			}
			else if (pChild->IsMapClass(MAPCLASS_TYPE(CMapGroup)))
			{
				iRvl = SaveEntityChildrenOf(pChild, file, pIntersecting);
			}

			// return error if there is an error
			if (iRvl != -1 && iRvl != fileOk)
			{
				return iRvl;
			}
		}
	}

	return fileOk;	// ok.
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//			file - 
// Output : int
//-----------------------------------------------------------------------------
static int ReadSolids(CMapClass *pObject, std::fstream& file)
{
	int nSolids = 0;
	char szBuf[128];

	while(1)
	{
		GetLine(file, szBuf);
		if(szBuf[0] != '{')
		{
			StuffLine(szBuf);
			break;
		}

		CMapSolid *pSolid = new CMapSolid;
		int iRvl = pSolid->SerializeMAP(file, FALSE);
		if(iRvl == fileError)
		{
			// delete the solid
			delete pSolid;
			++nInvalidSolids;
		}
		else if(iRvl == fileOsError)
		{
			// big problem
			delete pSolid;
			return fileOsError;
		}
		else
			pObject->AddChild(pSolid);

		++nSolids;
	}

	return nSolids;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
// Output : int
//-----------------------------------------------------------------------------
static int PeekChar(std::fstream& file)
{
	if(bStuffed)	// stuffed.. return first char
		return szStuffed[0];
	char szBuf[1024];
	szBuf[0] = 0;
	// get next line
	GetLine(file, szBuf);

	// still blank? return eof
	if(szBuf[0] == 0)
		return EOF;

	// to get it next call to getline
	StuffLine(szBuf);

	return szBuf[0];
}


//-----------------------------------------------------------------------------
// Purpose: Sets the MAP format for saving.
// Input  : mf - MAP format to use when saving.
//-----------------------------------------------------------------------------
void SetMapFormat(MAPFORMAT mf)
{
	Assert((mf == mfHalfLife) || (mf == mfHalfLife2));
	MapFormat = mf;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			fIsStoring - 
// Output : int
//-----------------------------------------------------------------------------
int CMapClass::SerializeMAP(std::fstream& file, BOOL fIsStoring)
{
	// no info stored in MAPs .. 
	return fileOk;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			fIsStoring - 
// Output : int
//-----------------------------------------------------------------------------
int CMapFace::SerializeMAP(std::fstream& file, BOOL fIsStoring)
{
	char szBuf[512];

	if (fIsStoring)
	{
		char *pszTexture;
		char szTexture[sizeof(texture.texture)+1];
		szTexture[sizeof(texture.texture)] = 0;
		memcpy(szTexture, texture.texture, sizeof texture.texture);
		strlwr(szTexture);

		if (MapFormat == mfQuake2)
		{
			pszTexture = strstr(szTexture, ".");
			if (pszTexture)
			{
				*pszTexture = 0;
			}
			pszTexture = strstr(szTexture, "textures\\");
			if (pszTexture == NULL)
			{
				pszTexture = szTexture;
			}
			else
			{
				pszTexture += strlen("textures\\");
			}
		}
		else
		{
			pszTexture = szTexture;
		}

		strupr(szTexture);


		//
		// Reverse the slashes -- thank you id.
		//
		for (int i = strlen(pszTexture) - 1; i >= 0; i--)
		{
			if (pszTexture[i] == '\\')
				pszTexture[i] = '/';
		}

		//
		// Convert the plane points to integers.
		//
		for (int nPlane = 0; nPlane < 3; nPlane++)
		{
			plane.planepts[nPlane][0] = V_rint(plane.planepts[nPlane][0]);
			plane.planepts[nPlane][1] = V_rint(plane.planepts[nPlane][1]);
			plane.planepts[nPlane][2] = V_rint(plane.planepts[nPlane][2]);
		}

		//
		// Check for duplicate plane points. All three plane points must be unique
		// or it isn't a valid plane. Try to fix it if it isn't valid.
		//
		if (!CheckFace())
		{
			Fix();
		}

		sprintf(szBuf,
			"( %.0f %.0f %.0f ) ( %.0f %.0f %.0f ) ( %.0f %.0f %.0f ) "
			"%s "
			"[ %g %g %g %g ] "
			"[ %g %g %g %g ] "
			"%g %g %g ",

			plane.planepts[0][0], plane.planepts[0][1], plane.planepts[0][2],
			plane.planepts[1][0], plane.planepts[1][1], plane.planepts[1][2],
			plane.planepts[2][0], plane.planepts[2][1], plane.planepts[2][2],
	
			pszTexture,

			(double)texture.UAxis[0], (double)texture.UAxis[1], (double)texture.UAxis[2], (double)texture.UAxis[3],
			(double)texture.VAxis[0], (double)texture.VAxis[1], (double)texture.VAxis[2], (double)texture.VAxis[3],

			(double)texture.rotate, 
			(double)texture.scale[0], 
			(double)texture.scale[1]);

		file << szBuf << ENDLINE;

		return fileOk;
	}
	else
	{
		// load the plane
		GetLine(file, szBuf);

		if(szBuf[0] != '(')
		{
			StuffLine(szBuf);
			return fileDone;
		}

		char szTexName[TEXTURE_NAME_LEN];
		DWORD q2contents;
		DWORD q2surface;
		int nLightmapScale;
		int nDummy;
		int nRead;

        if( uMapVersion >= 340 )
        {
			COMPILE_TIME_ASSERT( ARRAYSIZE(szTexName) == 128 );
			nRead = sscanf(szBuf,
				"( %f %f %f ) ( %f %f %f ) ( %f %f %f ) "
				"%127s "
				"[ %f %f %f %f ] "
				"[ %f %f %f %f ] "
				"%f %f %f "
				"%u %u %u",

				&plane.planepts[0][0], &plane.planepts[0][1], &plane.planepts[0][2],
				&plane.planepts[1][0], &plane.planepts[1][1], &plane.planepts[1][2],
				&plane.planepts[2][0], &plane.planepts[2][1], &plane.planepts[2][2],
		
				szTexName,

				&texture.UAxis[0], &texture.UAxis[1], &texture.UAxis[2], &texture.UAxis[3],
				&texture.VAxis[0], &texture.VAxis[1], &texture.VAxis[2], &texture.VAxis[3],

				&texture.rotate, 
				&texture.scale[0], 
				&texture.scale[1],
				
				&q2contents,
				&q2surface,
				&nLightmapScale);
			// Guarantee null-termination to keep /analyze happy.
			szTexName[ ARRAYSIZE(szTexName) - 1 ] = 0;

			if (nRead < 21)
			{
				bErrors = TRUE;
			}
			else if (nRead == 24)
			{
				// got q2 values - set them here
				texture.q2contents = q2contents;
				texture.q2surface = q2surface;
				texture.nLightmapScale = nLightmapScale;
				if (texture.nLightmapScale == 0)
				{
					texture.nLightmapScale = g_pGameConfig->GetDefaultLightmapScale();
				}
			}

            //
            // very cheesy HACK!!! -- this will be better when we have chunks
            //
			if( uMapVersion <= 350 )
			{
				if( ( file.peek() != '(' ) && ( file.peek() != '}' ) )
				{
					EditDispHandle_t handle = EditDispMgr()->Create();
					SetDisp( handle );

					CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );
					pDisp->SerializedLoadMAP( file, this, uMapVersion );
				}
			}
        }
		else if (uMapVersion >= 220 )
		{
			COMPILE_TIME_ASSERT( ARRAYSIZE(szTexName) == 128 );
			nRead = sscanf(szBuf,
				"( %f %f %f ) ( %f %f %f ) ( %f %f %f ) "
				"%127s "
				"[ %f %f %f %f ] "
				"[ %f %f %f %f ] "
				"%f %f %f "
				"%u %u %u",

				&plane.planepts[0][0], &plane.planepts[0][1], &plane.planepts[0][2],
				&plane.planepts[1][0], &plane.planepts[1][1], &plane.planepts[1][2],
				&plane.planepts[2][0], &plane.planepts[2][1], &plane.planepts[2][2],
		
				szTexName,

				&texture.UAxis[0], &texture.UAxis[1], &texture.UAxis[2], &texture.UAxis[3],
				&texture.VAxis[0], &texture.VAxis[1], &texture.VAxis[2], &texture.VAxis[3],

				&texture.rotate, 
				&texture.scale[0], 
				&texture.scale[1],
				
				&q2contents,
				&q2surface,
				&nDummy);		// Pre-340 didn't have lightmap scale.
			// Guarantee null-termination to keep /analyze happy.
			szTexName[ ARRAYSIZE(szTexName) - 1 ] = 0;

			if (nRead < 21)
			{
				bErrors = TRUE;
			}
			else if (nRead == 24)
			{
				// got q2 values - set them here
				texture.q2contents = q2contents;
				texture.q2surface = q2surface;
			}
		}
		else
		{
			COMPILE_TIME_ASSERT( ARRAYSIZE(szTexName) == 128 );
			nRead = sscanf(szBuf,
				"( %f %f %f ) ( %f %f %f ) ( %f %f %f ) "
				"%127s "
				"%f %f %f "
				"%f %f %u %u %u",

				&plane.planepts[0][0], &plane.planepts[0][1], &plane.planepts[0][2],
				&plane.planepts[1][0], &plane.planepts[1][1], &plane.planepts[1][2],
				&plane.planepts[2][0], &plane.planepts[2][1], &plane.planepts[2][2],
		
				szTexName,

				&texture.UAxis[3],
				&texture.VAxis[3],
				&texture.rotate, 
				&texture.scale[0], 
				&texture.scale[1],
				
				&q2contents,
				&q2surface,
				&nDummy);		// Pre-340 didn't have lightmap scale.
			// Guarantee null-termination to keep /analyze happy.
			szTexName[ ARRAYSIZE(szTexName) - 1 ] = 0;

			if (nRead < 15)
			{
				bErrors = TRUE;
			}
			else if (nRead == 18)
			{
				// got q2 values - set them here
				texture.q2contents = q2contents;
				texture.q2surface = q2surface;
			}
		}

		if (g_pGameConfig->GetTextureFormat() != tfVMT)
		{
			// reverse the slashes -- thank you id
			for (int i = strlen(szTexName) - 1; i >= 0; i--)
			{
				if (szTexName[i] == '/')
					szTexName[i] = '\\';
			}
		}

		SetTexture(szTexName);
	}

	if (file.fail())
	{
		return fileOsError;
	}
	return fileOk;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			fIsStoring - 
// Output : int
//-----------------------------------------------------------------------------
int MDkeyvalue::SerializeMAP(std::fstream& file, BOOL fIsStoring)
{
	// load/save a keyvalue
	char szBuf[1024];

	if(fIsStoring)
	{
		// save a keyvalue
		sprintf( szBuf,
			"\"%s\" \"%s\"",

			Key(), Value() );

		file << szBuf << ENDLINE;
	}
	else
	{
		GetLine(file, szBuf);
		if(szBuf[0] != '\"')
		{
			StuffLine(szBuf);
			return fileDone;
		}
		char *p = strchr(szBuf, '\"');
		p = strchr(p+1, '\"');
		if(!p)
			return fileError;
		p[0] = 0;
		V_strcpy_safe(szKey, szBuf+1);

		// advance to start of value string
		p = strchr(p+1, '\"');
		if(!p)
			return fileError;
		// ocpy in value
		strcpy(szValue, p+1);
		// kill trailing "
		p = strchr(szValue, '\"');
		if(!p)
			return fileError;
		p[0] = 0;
	}

	return file.fail() ? fileOsError : fileOk;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			fIsStoring - 
// Output : int
//-----------------------------------------------------------------------------
int CMapSolid::SerializeMAP(std::fstream& file, BOOL fIsStoring)
{
	CMapClass::SerializeMAP(file, fIsStoring);

	// load/save a brush
	if (fIsStoring)
	{
		// save the brush
		file << "{" << ENDLINE;

		// serialize the Faces
		int nFaces = Faces.GetCount();
		for(int i = 0; i < nFaces; i++)
		{
			if(!Faces[i].Points)
				continue;
			if(Faces[i].SerializeMAP(file, fIsStoring) == fileError)
				return fileError;
		}

		// terminator
		file << "}" << ENDLINE;
	}
	else
	{
		// caller skipped delimiter
		Faces.SetCount(0);

		// read Faces
		for(int i = 0; ; i++)
		{
			// extract plane
			if (Faces[i].SerializeMAP(file, fIsStoring) == fileDone)
			{
				// when fileDone is returned, no face was loaded
				break;
			}

			Faces[i].CalcPlane();
		}

		GetLine(file, NULL);	// ignore line

		if (!file.fail())
		{
			//
			// Create the solid using the planes that were read from the MAP file.
			//
			if (CreateFromPlanes() == FALSE)
			{
				bErrors = TRUE;
				return(fileError);
			}

			//
			// If we are reading from an old map file, the texture axes will need to be set up.
			// Leave the rotation and shifts alone; they were read from the MAP file.
			//
			if (uMapVersion < 220)
			{
				InitializeTextureAxes(TEXTURE_ALIGN_QUAKE, INIT_TEXTURE_AXES | INIT_TEXTURE_FORCE);
			}
		}
		else
		{
			return(fileOsError);
		}

		CalcBounds();

		//
		// Set solid type based on texture name.
		//
		m_eSolidType = HL1SolidTypeFromTextureName(Faces[0].texture.texture);
	}

	return(file.fail() ? fileOsError : fileOk);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			fIsStoring - 
// Output : int
//-----------------------------------------------------------------------------
int CMapEntity::SerializeMAP(std::fstream &file, BOOL fIsStoring)
{
	CMapClass::SerializeMAP(file, fIsStoring);

	// load/save an object
	if (fIsStoring)
	{
		//
		// If it's a solidentity but it doesn't have any solids, 
		// don't save it.
		//
		if (!IsPlaceholder() && !m_Children.Count())
		{
			return(fileOk);
		}

		//
		// Save it.
		//
		file << "{" << ENDLINE;

		//
		// Save keyvalues & other data.
		//
		CEditGameClass::SerializeMAP(file, fIsStoring);

		//
		// If this is a placeholder and either has no class or is not a solid class,
		// save our origin.
		//
		if (IsPlaceholder() && (!IsClass() || !IsSolidClass()))
		{
			MDkeyvalue tmpkv;
			strcpy(tmpkv.szKey, "origin");

			Vector Origin;
			GetOrigin(Origin);
			sprintf(tmpkv.szValue, "%.0f %.0f %.0f", Origin[0], Origin[1], Origin[2]);
			tmpkv.SerializeMAP(file, fIsStoring);
		}

		if (!(IsPlaceholder()))
		{
			SaveSolidChildrenOf(this, file);
		}

		file << "}" << ENDLINE;
	}
	else
	{
		// load keyvalues
		CEditGameClass::SerializeMAP(file, fIsStoring);

		// solids
		if (!ReadSolids(this, file))
		{
			flags |= flagPlaceholder;
		}

		// skip delimiter
		GetLine(file, NULL);
	}

	return file.fail() ? fileOsError : fileOk;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			fIsStoring - 
// Output : int
//-----------------------------------------------------------------------------
int CEditGameClass::SerializeMAP(std::fstream& file, BOOL fIsStoring)
{
	int iRvl;

	if (!fIsStoring)
	{
		// loading
		if (PeekChar(file) == '\"')
		{
			// read kv pairs
			MDkeyvalue newkv;
			while (1)
			{
				if (newkv.SerializeMAP(file, fIsStoring) != fileOk)
				{
					// fileDone means the keyvalue was not loaded
					break;
				}
		
				if (!strcmp(newkv.szKey, "classname"))
				{
					m_KeyValues.SetValue(newkv.szKey, newkv.szValue);
				}
				else if (!strcmp(newkv.szKey, "angle"))
				{
					ImportAngle(atoi(newkv.szValue));
				}
				else if (strcmp(newkv.szKey, "wad"))
				{
					//
					// All other keys are simply added to the keyvalue list.
					//
					m_KeyValues.SetValue(newkv.szKey, newkv.szValue);
				}
			}	
		}
	}
	else
	{
		// save keyvalues
		MDkeyvalue tmpkv;

		if (GetKeyValue("classname") == NULL)
		{
			tmpkv.Set("classname", m_szClass);
			tmpkv.SerializeMAP(file, fIsStoring);
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

			iRvl = KeyValue.SerializeMAP(file, fIsStoring);
			if (iRvl != fileOk)
			{
				return(iRvl);
			}
		}

		//
		// If we have a base class, for each keyvalue in the class definition, write out all keys
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
					MDkeyvalue *pKey;
					LPCTSTR p = m_KeyValues.GetValue(pVar->GetName(), &iIndex);

					//
					// If the variable is not present in this object, write out the default value.
					//
					if (p == NULL) 
					{
						pKey = &tmpkv;
						pVar->ResetDefaults();
						pVar->ToKeyValue(pKey);

						//
						// Only write the key value if it is non-zero.
						//
						if ((pKey->szKey[0] != 0) && (pKey->szValue[0] != 0) && (stricmp(pKey->szValue, "0")))
						{
							iRvl = pKey->SerializeMAP(file, fIsStoring);
							if (iRvl != fileOk)
							{
								return(iRvl);
							}
						}
					}
				}
			}
		}
	}

	return(file.fail() ? fileOsError : fileOk);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			fIsStoring - 
//			pIntersecting - 
// Output : int
//-----------------------------------------------------------------------------
int CMapWorld::SerializeMAP(std::fstream &file, BOOL fIsStoring, BoundBox *pIntersecting)
{
	int iRvl;

	bStuffed = FALSE;
	bErrors = FALSE;
	nInvalidSolids = 0;

	// load/save a world
	if (fIsStoring)	
	{
		file << "{" << ENDLINE;

		// save worldobject
		CEditGameClass::SerializeMAP(file, fIsStoring);

		if (MapFormat != mfQuake2)
		{
			MDkeyvalue tmpkv;

			strcpy(tmpkv.szKey, "mapversion");
			strcpy(tmpkv.szValue, "360");
			tmpkv.SerializeMAP(file, fIsStoring);

			// Save wad file line
			strcpy(tmpkv.szKey, "wad");

			// copy all texfiles into value
			tmpkv.szValue[0] = 0;
			BOOL bFirst = TRUE;
			int nGraphicsFiles = g_Textures.FilesGetCount();
			for (int i = 0; i < nGraphicsFiles; i++)
			{
				char szFile[MAX_PATH];
				GRAPHICSFILESTRUCT gf;

				g_Textures.FilesGetInfo(&gf, i);

				//
				// Don't save WAL files - they're never used.
				//
				if (gf.format != tfWAL)
				{
					//
					// Also make sure this is the right kind of WAD file
					// based on the game we're using.
					//
					if (gf.format == g_pGameConfig->GetTextureFormat())
					{
						//
						// Append this WAD file to the WAD list.
						//
						strcpy(szFile, gf.filename);

						// dvs: Strip off the path. This crashes VIS and QRAD!!
						/*
						char *pszSlash = strrchr(szFile, '\\');
						if (pszSlash == NULL)
						{
							pszSlash = strrchr(szFile, '/');
						}

						if (pszSlash != NULL)
						{
							pszSlash++;
						}
						else
						{
							pszSlash = szFile;
						}
						*/

						char *pszSlash = szFile;

						// Strip off any drive letter.
						if (pszSlash[1] == ':')
						{
							pszSlash += 2;
						}

						// WAD names are semicolon delimited.
						if (!bFirst)
						{
							strcat(tmpkv.szValue, ";");
						}

						strcat(tmpkv.szValue, pszSlash);
						bFirst = FALSE;
					}
				}
			}

			if ( tmpkv.szValue[0] != '\0' )
			{
				tmpkv.SerializeMAP(file, fIsStoring);
			}
		}

		//
		// Save the brushes.
		//
		if (SaveSolidChildrenOf(this, file, pIntersecting) == fileOsError)
		{
			goto FatalError;
		}

		file << "}" << ENDLINE;

		//
		// Save the entities.
		//
		if (SaveEntityChildrenOf(this, file, pIntersecting) == fileOsError)
		{
			goto FatalError;
		}

		//
		// Save paths (if paths are visible).
		//
		FOR_EACH_OBJ( m_Paths, pos )
		{
			CMapPath *pPath = m_Paths.Element(pos);
			pPath->SerializeMAP(file, TRUE, pIntersecting);
		}
	}
	else
	{
		pProgDlg = new CProgressDlg;
		pProgDlg->Create();
		pProgDlg->SetStep(1);
		
		CString caption;
		caption.LoadString(IDS_LOADINGFILE);
		pProgDlg->SetWindowText(caption);

		m_Render2DBox.ResetBounds();

		// load world
		GetLine(file, NULL);	// ignore delimiter
		CEditGameClass::SerializeMAP(file, fIsStoring);

		const char* pszMapVersion;

		pszMapVersion = m_KeyValues.GetValue("mapversion");
		if (pszMapVersion != NULL)
		{
			uMapVersion = atoi(pszMapVersion);
		}
		else
		{
			uMapVersion = 0;
		}

		// read solids
		if (ReadSolids(this, file) == fileOsError)
		{
			goto FatalError;
		}

		// skip end-of-entity marker
		GetLine(file, NULL);

		char szBuf[128];

		// read entities
		while (1)
		{
			GetLine(file, szBuf);
			if (szBuf[0] != '{')
			{
				StuffLine(szBuf);
				break;
			}

			if (PeekChar(file) == EOF)
			{
				break;
			}

			CMapEntity *pEntity;

			pEntity = new CMapEntity;
			iRvl = pEntity->SerializeMAP(file, fIsStoring);
			AddChild(pEntity);
			
			if (iRvl == fileError)
			{
				bErrors = TRUE;
			}
			else if (iRvl == fileOsError)
			{
				goto FatalError;
			}
		}

		if (bErrors)
		{
			if (nInvalidSolids)
			{
				CString str;
				str.Format("For your information, %d solids were not loaded\n"
					"due to errors in the file.", nInvalidSolids); 
				AfxMessageBox(str);
			}
			else if (AfxMessageBox("There was a problem loading the MAP file. Do you\n"
				"want to view the error report?", MB_YESNO) == IDYES)
			{
				CMapErrorsDlg dlg;
				dlg.DoModal();
			}
		}

		PostloadWorld();

		if (g_pGameConfig->GetTextureFormat() == tfVMT)
		{
			// do batch search and replace of textures from trans.txt if it exists.
			char translationFilename[MAX_PATH];
			Q_snprintf( translationFilename, sizeof( translationFilename ), "materials/trans.txt" );
			if( CMapDoc::GetActiveMapDoc() )
			{
				FileHandle_t searchReplaceFP = g_pFileSystem->Open( translationFilename, "r" );
				if( searchReplaceFP )
				{
					CMapDoc::GetActiveMapDoc()->BatchReplaceTextures( searchReplaceFP );
					g_pFileSystem->Close( searchReplaceFP );
				}
			}
		}
	}

	if (pProgDlg)
	{
		pProgDlg->DestroyWindow();
		delete pProgDlg;
		pProgDlg = NULL;
	}

	return (bErrors && fIsStoring) ? -1 : 0;

FatalError:

	// OS error.
	CString str;
	str.Format("The OS reported an error %s the file: %s", fIsStoring ? "saving" : "loading", strerror(errno));
	AfxMessageBox(str);

	if (pProgDlg != NULL)
	{
		pProgDlg->DestroyWindow();
		delete pProgDlg;
		pProgDlg = NULL;
	}

	return -1;
}

