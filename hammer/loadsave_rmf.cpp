//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include <io.h>
#include "hammer.h"
#include "MapEntity.h"
#include "MapFace.h"
#include "MapSolid.h"
#include "MapStudioModel.h"
#include "MapWorld.h"
#include "GlobalFunctions.h"
#include "VisGroup.h"
#include "MapDoc.h"
#include "MapDisp.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static CMapWorld *pLoadingWorld;
static float fThisVersion;
static BOOL bCorrupt;

class COldVisGroup
{
public:

	char m_szName[128];
	color32 m_rgbColor;

	DWORD m_dwID;
	bool m_bVisible;
};


float GetFileVersion() { return fThisVersion; }


static void WriteString(std::fstream& file, LPCTSTR pszString)
{
	BYTE cLen = strlen(pszString)+1;
	file.write((char*)&cLen, 1);
	file.write(pszString, cLen);
}

static void ReadString(std::fstream& file, char * pszString)
{
	BYTE cLen;
	file.read((char *)&cLen, 1);
	file.read(pszString, cLen);
}


//-----------------------------------------------------------------------------
// Purpose: Loads a solid face from RMF.
//-----------------------------------------------------------------------------
int CMapFace::SerializeRMF(std::fstream& file, BOOL fIsStoring)
{
	int iSize;

	if (fIsStoring)
	{
		//
		// After 3.3 the alignment of vec4_t's changed. We never save the new format,
		// since RMF is no longer being revved.
		//
		TEXTURE_33 OldTex33;
		memset(&OldTex33, 0, sizeof(OldTex33));

		memcpy(OldTex33.texture, texture.texture, sizeof(OldTex33.texture));

		OldTex33.UAxis[0] = texture.UAxis[0];
		OldTex33.UAxis[1] = texture.UAxis[1];
		OldTex33.UAxis[2] = texture.UAxis[2];
		OldTex33.UAxis[3] = texture.UAxis[3];

		OldTex33.VAxis[0] = texture.VAxis[0];
		OldTex33.VAxis[1] = texture.VAxis[1];
		OldTex33.VAxis[2] = texture.VAxis[2];
		OldTex33.VAxis[3] = texture.VAxis[3];

		OldTex33.rotate = texture.rotate;

		OldTex33.scale[0] = texture.scale[0];
		OldTex33.scale[1] = texture.scale[1];

		OldTex33.smooth = texture.smooth;
		OldTex33.material = texture.material;
		OldTex33.q2surface = texture.q2surface;
		OldTex33.q2contents = texture.q2contents;
		OldTex33.nLightmapScale = texture.nLightmapScale;

		file.write((char *)&OldTex33, sizeof(OldTex33));

		iSize = nPoints;
		file.write((char *)&iSize, sizeof(int));

		//
		// Save face points. We don't serialize the Vectors directly because the memory
		// layout changed with SSE optimizations.
		//
		float SavePoints[256][3];
		for (int i = 0; i < iSize; i++)
		{
			SavePoints[i][0] = Points[i].x;
			SavePoints[i][1] = Points[i].y;
			SavePoints[i][2] = Points[i].z;
		}

		file.write((char *)SavePoints, nPoints * 3 * sizeof(float));

		//
		// Save plane points. We don't serialize the Vectors directly because the memory
		// layout changed with SSE optimizations.
		//
		for (int i = 0; i < 3; i++)
		{
			SavePoints[i][0] = plane.planepts[i].x;
			SavePoints[i][1] = plane.planepts[i].y;
			SavePoints[i][2] = plane.planepts[i].z;
		}

		file.write((char *)SavePoints, 3 * 3 * sizeof(float));
	}
	else
	{
		// Pre-2.2 used a different texture structure format.
		TEXTURE_21 OldTex;
		memset(&OldTex, 0, sizeof(OldTex));

		if (fThisVersion < 0.9f)
		{
			// Read the name
			file.read(OldTex.texture, 16);

			// Ensure name is ASCIIZ
			OldTex.texture[16] = 0;

			// Read the rest - skip the name
			file.read((char *)&OldTex.rotate, sizeof(OldTex.rotate) + sizeof(OldTex.shift) + sizeof(OldTex.scale));
		}
		else if (fThisVersion < 1.2f)
		{
			// Didn't have smooth/material groups:
			file.read((char *)&OldTex, 40);
			file.read((char *)&OldTex, sizeof(OldTex.texture) - (MAX_PATH) + sizeof(OldTex.rotate) + sizeof(OldTex.shift) + sizeof(OldTex.scale));
		}
		else if (fThisVersion < 1.7f)
		{
			// No quake2 fields yet and smaller texture size.
			file.read((char *)&OldTex, 40);
			file.read((char *)&OldTex.rotate, sizeof(OldTex) - (sizeof(int) * 3) - MAX_PATH);
		}
		else if (fThisVersion < 1.8f)
		{
			// Texture name field changed from 40 to MAX_PATH in size.
			file.read((char *)&OldTex, 40);
			file.read((char *)&OldTex.rotate, sizeof(OldTex) - MAX_PATH);
		}
		else if (fThisVersion < 2.2f)
		{
			file.read((char *)&OldTex, sizeof(OldTex));
		}
		else
		{
			//
			// After 3.3 the alignment of vec4_t's changed. We never save the new format,
			// since RMF is no longer being revved.
			//
			TEXTURE_33 OldTex33;
			memset(&OldTex33, 0, sizeof(OldTex33));

			file.read((char *)&OldTex33, sizeof(OldTex33));

			memcpy(texture.texture, OldTex33.texture, sizeof(texture.texture));

			texture.UAxis[0] = OldTex33.UAxis[0];
			texture.UAxis[1] = OldTex33.UAxis[1];
			texture.UAxis[2] = OldTex33.UAxis[2];
			texture.UAxis[3] = OldTex33.UAxis[3];

			texture.VAxis[0] = OldTex33.VAxis[0];
			texture.VAxis[1] = OldTex33.VAxis[1];
			texture.VAxis[2] = OldTex33.VAxis[2];
			texture.VAxis[3] = OldTex33.VAxis[3];

			texture.rotate = OldTex33.rotate;

			texture.scale[0] = OldTex33.scale[0];
			texture.scale[1] = OldTex33.scale[1];

			texture.smooth = OldTex33.smooth;
			texture.material = OldTex33.material;
			texture.q2surface = OldTex33.q2surface;
			texture.q2contents = OldTex33.q2contents;
			texture.nLightmapScale = OldTex33.nLightmapScale;

			if (texture.nLightmapScale == 0)
			{
				texture.nLightmapScale = g_pGameConfig->GetDefaultLightmapScale();
			}
		}

		// If reading from a pre-2.2 RMF file, copy the texture info from the old format.
		if (fThisVersion < 2.2f)
		{
			memcpy(texture.texture, OldTex.texture, sizeof(texture.texture));
			memcpy(texture.scale, OldTex.scale, sizeof(texture.scale));
			texture.rotate = OldTex.rotate;
			texture.smooth = OldTex.smooth;
			texture.material = OldTex.material;
			texture.q2surface = OldTex.q2surface;
			texture.q2contents = OldTex.q2contents;
			texture.UAxis[3] = OldTex.shift[0];
			texture.VAxis[3] = OldTex.shift[1];
		}

		if (fThisVersion < 1.8f)
		{
			texture.texture[40] = 0;
		}

		//
		// Reverse forward slashes if we are not using materials.
		//
		if (g_pGameConfig->GetTextureFormat() != tfVMT)
		{
			for (int i = strlen(texture.texture) - 1; i >= 0; i--)
			{
				if (texture.texture[i] == '/')
				{
					texture.texture[i] = '\\';
				}
			}
		}

		if (texture.texture[1] == ':')
		{
			char szBuf[MAX_PATH];
			char *psz;
			strcpy(szBuf, texture.texture);
			psz = strstr(szBuf, "textures\\");
			if (psz)
			{
				memset(texture.texture, 0, sizeof(texture.texture));
				psz += strlen("textures\\");
				strcpy(texture.texture, psz);
			}
		}
		
		if (fThisVersion < 0.6f)
		{
			float light;
			file.read((char*) &light, sizeof(light));
		}

		//
		// Load the points into an array of float[3]'s and transfer them into
		// an array of Vectors which will be used for face creation. We can't
		// load directly into the Vectors because the memory layout changed
		// when SSE optimizations were added.
		//
		float LoadPoints[256][3];

		file.read((char *)&iSize, sizeof(int));
		file.read((char *)&LoadPoints, iSize * 3 * sizeof(float));

		Vector CreatePoints[256];
		for (int i = 0; i < iSize; i++)
		{
			CreatePoints[i].x = LoadPoints[i][0];
			CreatePoints[i].y = LoadPoints[i][1];
			CreatePoints[i].z = LoadPoints[i][2];

			//
			// Negate Z for older RMF files.
			//
			if (fThisVersion < 0.5f)
			{
				CreatePoints[i].z = -CreatePoints[i].z;
			}
		}

        if (fThisVersion < 2.2f)
        {
            CreateFace(CreatePoints, iSize);
        }

		//
		// Load the plane points. We don't really need them, but they can fix the face if, somehow, it
		// was saved without any points. RMF could have been smaller if we only saved these plane points.
		//
		if (fThisVersion >= 0.7f)
		{
			//
			// Load the points into an array of float[3]'s and transfer them into
			// the array of Vectors. We can't load directly into the Vectors because the memory
			// layout changed when SSE optimizations were added.
			//
			float LoadPlanePoints[3][3];
			file.read((char *)LoadPlanePoints, sizeof(LoadPlanePoints));
			
			for (int i = 0; i < 3; i++)
			{
				plane.planepts[i].x = LoadPlanePoints[i][0];
				plane.planepts[i].y = LoadPlanePoints[i][1];
				plane.planepts[i].z = LoadPlanePoints[i][2];
			}

			CalcPlane();

			// If reading from an older RMF file, set up the texture axes Quake-style.
			if (fThisVersion < 2.2f)
			{
				InitializeTextureAxes(TEXTURE_ALIGN_QUAKE, INIT_TEXTURE_AXES | INIT_TEXTURE_FORCE);
			}
		}

        if( fThisVersion < 2.2f )
        {
            SetTexture(texture.texture);
        }

        //
        // version 3.4 -- added displacement info to faces
        //
        if( ( fThisVersion >= 3.4f ) && ( fThisVersion <= 3.6f ) )
        {
			bool bHasMapDisp;

            if( fThisVersion >= 3.5f )
            {
                int nLoadHasMapDisp;

                // check displacement mapping flag
                file.read( ( char* )&nLoadHasMapDisp, sizeof( int ) );
				bHasMapDisp = nLoadHasMapDisp != 0;
			}                
            else
            {
                // check displacement mapping flag
                file.read( ( char* )&bHasMapDisp, sizeof( bool ) );
            }

            if( bHasMapDisp )
            {
				EditDispHandle_t handle = EditDispMgr()->Create();
				SetDisp( handle );

				CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );
				pDisp->SetParent( this );
				pDisp->SerializedLoadRMF( file, this, fThisVersion );
            }
        }

        if (fThisVersion >= 2.2f)
        {
            CreateFace(CreatePoints, iSize); 
            SetTexture(texture.texture);
        }
	}

	if (file.bad())
	{
		return(-1);
	}

	return(0);
}


int MDkeyvalue::SerializeRMF(std::fstream& file, BOOL fIsStoring)
{
	// load/save a keyvalue
	if( fIsStoring )
	{
		WriteString(file, szKey);
		WriteString(file, szValue);
	}
	else
	{
		ReadString(file, szKey);
		ReadString(file, szValue);
	}

	if( file.bad() )
		return -1;
	return 0;
}


int CMapSolid::SerializeRMF(std::fstream& file, BOOL fIsStoring)
{
	int iRvl, iSize;

	// load/save children
	CMapClass::SerializeRMF(file, fIsStoring);

	// load/save a brush
	if(fIsStoring)
	{
		// serialize the Faces
		iSize = Faces.GetCount();
		file.write((char*) &iSize, sizeof(int));
		for(int i = 0; i < iSize; i++)
		{
			iRvl = Faces[i].SerializeRMF(file, fIsStoring);
			if(iRvl < 0)
				return iRvl;
		}
	}
	else
	{
		// There once was a bug that caused black solids. Fix it here.
		if ((r == 0) && (g == 0) || (b == 0))
		{
			PickRandomColor();
		}

		// read Faces
		file.read((char*) &iSize, sizeof(int));
		Faces.SetCount(iSize);
	
		for(int i = 0; i < iSize; i++)
		{
			// extract face
			iRvl = Faces[i].SerializeRMF(file, fIsStoring);
			if (iRvl < 0)
			{
				return(iRvl);
			}

			Faces[i].SetRenderColor(r, g, b);
			Faces[i].SetParent(this);
		}

		CalcBounds();

		//
		// Set solid type based on texture name.
		//
		m_eSolidType = HL1SolidTypeFromTextureName(Faces[0].texture.texture);
	}

	if (file.bad())
	{
		return -1;
	}
	
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			fIsStoring - 
// Output : int
//-----------------------------------------------------------------------------
int CEditGameClass::SerializeRMF(std::fstream& file, BOOL fIsStoring)
{
	int iSize, iRvl;
	int iAngle = 0;

	if (fIsStoring)
	{
		// save data
		WriteString(file, GetClassName());
		file.write((char*) &iAngle, sizeof(iAngle));

		int nSpawnFlags = GetSpawnFlags();
		file.write((char *)&nSpawnFlags, sizeof(nSpawnFlags));

		//
		// Write the number of keyvalues.
		//
		iSize = 0;
		for ( int z=m_KeyValues.GetFirst(); z != m_KeyValues.GetInvalidIndex(); z=m_KeyValues.GetNext( z ) )
		{
			iSize++;
		}
		file.write((char*) &iSize, sizeof(int));

		//
		// Write the keyvalues.
		//
		for ( int z=m_KeyValues.GetFirst(); z != m_KeyValues.GetInvalidIndex(); z=m_KeyValues.GetNext( z ) )
		{
			MDkeyvalue KeyValue = m_KeyValues.GetKeyValue(z);

			iRvl = KeyValue.SerializeRMF(file, fIsStoring);
			if (iRvl < 0)
			{
				return iRvl;
			}
		}

		//
		// Save dummy timeline info.
		//
		BOOL bTimeline = FALSE;
		int nTime = 0;
		file.write((char*) &bTimeline, sizeof bTimeline);
		file.write((char*) &nTime, sizeof nTime);
		file.write((char*) &nTime, sizeof nTime);
	}
	else
	{
		char buf[128];
		ReadString(file, buf);
		file.read((char*) &iAngle, sizeof(iAngle));

		int nSpawnFlags;
		file.read((char *)&nSpawnFlags, sizeof(nSpawnFlags));

		Assert(buf[0]);

		CEditGameClass::SetClass(buf, true);

		//
		// Read the keyvalues.
		//
		file.read((char *) &iSize, sizeof(int));
		for (int i = 0; i < iSize; i++ )
		{
			MDkeyvalue KeyValue;
			iRvl = KeyValue.SerializeRMF(file, fIsStoring);
			if (iRvl < 0)
			{
				return iRvl;
			}
			m_KeyValues.SetValue(KeyValue.szKey, KeyValue.szValue);
		}

		SetSpawnFlags(nSpawnFlags);
		m_KeyValues.SetValue("classname", buf);

		// backwards compatibility for old iAngle
		if (iAngle)
		{
			ImportAngle(iAngle);
		}

		//
		// Dummy timeline information - unused.
		//
		if (fThisVersion >= 1.5f)
		{
			BOOL bTimeline;
			int nTime;

			file.read((char*) &bTimeline, sizeof bTimeline);
			file.read((char*) &nTime, sizeof nTime);
			file.read((char*) &nTime, sizeof nTime);
		}
	}

	return file.bad() ? -1 : 0;
}


int CMapClass::SerializeRMF(std::fstream& file, BOOL fIsStoring)
{
	int iSize, iRvl;

	if(fIsStoring)
	{
		// write type
		WriteString(file, GetType());

		//
		// Write the visgroup ID (zero if none).
		//
		DWORD dwID = 0;
		
		/*if (m_pVisGroup)
		{
			// visgroupfixme: how to handle saving RMF? save the first group??
			dwID = m_pVisGroup->GetID();
		}*/ 

		file.write((char *)&dwID, sizeof(dwID));

		//
		// Write the object color.
		//
		file.write((char *)&r, sizeof(BYTE));
		file.write((char *)&g, sizeof(BYTE));
		file.write((char *)&b, sizeof(BYTE));

		//
		// Save children.
		//
		int nChildCount = 0;

		FOR_EACH_OBJ( m_Children, pos )
		{
			CMapClass *pChild = m_Children.Element(pos);
			if (pChild->ShouldSerialize())
			{
				nChildCount++;
			}
		}

		file.write((char *)&nChildCount, sizeof(int));

		FOR_EACH_OBJ( m_Children, pos )
		{
			CMapClass *pChild = m_Children.Element(pos);
			if (pChild->ShouldSerialize())
			{
				iRvl = pChild->SerializeRMF(file, fIsStoring);
				if (iRvl < 0)
				{
					return iRvl;
				}
			}
		}
	}
	else
	{
		// read our stuff
		if(fThisVersion < 1.0f)
		{
			// kill group information .. unfortunate
			file.read((char*) &iSize, sizeof(int));
			file.seekg(iSize, std::ios::cur);
		}
		else
		{
			// just read the visgroup ID but ignore it
			DWORD dwGroupID;
			file.read((char*) &dwGroupID, sizeof(DWORD));
		}

		//
		// Read the object color.
		//
		file.read((char *)&r, sizeof(BYTE));
		file.read((char *)&g, sizeof(BYTE));
		file.read((char *)&b, sizeof(BYTE));

		// load children
		file.read((char*) &iSize, sizeof(int));
		for(int i = 0; i < iSize; i++)
		{
			char buf[128];
			ReadString(file, buf);
			CMapClass *pChild = CMapClassManager::CreateObject(buf);
			if(!pChild)
			{
				bCorrupt = TRUE;
				return -1;
			}
			iRvl = pChild->SerializeRMF(file, fIsStoring);
			if(iRvl < 0)
				return iRvl;
			AddChild(pChild);
		}
	}

	return file.bad() ? -1 : 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			fIsStoring - 
// Output : 
//-----------------------------------------------------------------------------
int CMapEntity::SerializeRMF(std::fstream &file, BOOL fIsStoring)
{
	int iSize;
	Vector Origin;

	//
	// Read/write base class.
	//
	CMapClass::SerializeRMF(file, fIsStoring);
	CEditGameClass::SerializeRMF(file, fIsStoring);
	
	if (fIsStoring)
	{
		// Write flags
		file.write((char*) &flags, sizeof(flags));

		// Write origin
		GetOrigin(Origin);
		file.write((char *)Origin.Base(), 3 * sizeof(float));

		// Save padding for unused "complex" field 
		iSize = 0;
		file.write((char*) &iSize, sizeof(int));
	}
	else
	{
		// Read flags
		file.read((char *)&flags, sizeof(flags));

		// Read origin
		file.read((char *)Origin.Base(), 3 * sizeof(float));
		SetOrigin(Origin);

		if (IsClass())
		{
			// Known class. Determine flags based on the class.
			flags = IsSolidClass() ? (flags & ~flagPlaceholder) : (flags | flagPlaceholder);
		}
		else
		{
			// Unknown class. Determine flags by looking for children (only solid ents have children at this point).
			flags = (m_Children.Count() > 0) ? (flags & ~flagPlaceholder) : (flags | flagPlaceholder);
		}

		if (!(IsPlaceholder()))
		{
			CMapPoint::SetOrigin(Vector(0, 0, 0));
		}

		GetOrigin(Origin);

		// import for previous to 0.5
		if (fThisVersion < 0.5f)
		{
			Origin.z = -Origin.z;
		}

		// load unused "complex" field
		file.read((char *)&iSize, sizeof(int));

		SetOrigin(Origin);

		//
		// HACK: Set our class to NULL so that it is properly set from our "classname"
		// key in PostloadWorld.
		//
		m_szClass[0] = '\0';

		CalcBounds(TRUE);
	}

	if (file.bad())
	{
		return -1;
	}
	
	return 0;
}


int CMapWorld::SerializeRMF(std::fstream &file, BOOL fIsStoring)
{
    float fVersion = 3.7f;
	float fLastCompat = 0.3f;
	
	int nSolids = 0;
	int iSize;

	pLoadingWorld = this;
	bCorrupt = FALSE;

	// load/save a world
	if(fIsStoring)	
	{
		// write version
		file.write((char*) &fVersion, sizeof(fVersion));

		file.write("RMF", 3);

		// we don't save vis groups
		iSize = 0; 
		file.write((char*) &iSize, sizeof(int));

		// save children & local data
		if(CMapClass::SerializeRMF(file, fIsStoring) == -1)
			goto FatalError;

		// save ceditgameclass
		if(CEditGameClass::SerializeRMF(file, fIsStoring) == -1)
			goto FatalError;

		// save paths
		iSize = m_Paths.Count();
		file.write((char*) &iSize, sizeof(iSize));

		FOR_EACH_OBJ( m_Paths, pos )
		{
			CMapPath *pPath = m_Paths.Element(pos);
			pPath->SerializeRMF(file, TRUE);
		}

		if(file.bad())
			goto FatalError;
	}
	else
	{
		// read & check version
		file.read((char*) &fThisVersion, sizeof(fThisVersion));
		if(fThisVersion < fLastCompat || fThisVersion > fVersion)
		{
			CString str;
			str.Format("Oops! SerializeRMF() v%1.1f tried to load a file v%1.1f. Aborting.",
				fVersion, fThisVersion);
			AfxMessageBox(str);
			return -1;
		}

		char buf[128];

		if(fThisVersion >= 0.8f)
		{
			file.read(buf, 3);
			if(strncmp(buf, "RMF", 3))
			{
				AfxMessageBox("Invalid file type.");
				return -1;
			}
		}

		// load groups
		if (fThisVersion >= 1.0f)
		{
			file.read((char*) &iSize, sizeof(int));

			for ( int i = 0; i < iSize; i++)
			{
				// just skip vis groups
				COldVisGroup oldVisGroup;
				file.read((char*) &oldVisGroup, sizeof(COldVisGroup));
			}
		}

		m_Render2DBox.ResetBounds();

		// make sure it's a CMapWorld
		ReadString(file, buf);
		if(strcmp(buf, GetType()))
		{
			AfxMessageBox("Invalid file type.");
			return -1;
		}

		// load children & local data
		if(CMapClass::SerializeRMF(file, fIsStoring) == -1)
			goto FatalError;

		// load ceditgameclass & CMapClass
		if(CEditGameClass::SerializeRMF(file, fIsStoring) == -1)
			goto FatalError;

		if(fThisVersion < 1.0f)
		{
			const int old_group_bytes = 134;
			file.read((char*) &iSize, sizeof(int));
			file.seekg(old_group_bytes * iSize, std::ios::cur);
		}

		// load paths
		if(fThisVersion >= 1.1f)
		{
			file.read((char*) &iSize, sizeof iSize);
			for(int i = 0; i < iSize; i++)
			{
				CMapPath *pPath = new CMapPath;
				pPath->SerializeRMF(file, FALSE);
				if(pPath->GetNodeCount() == 0)
				{
					delete pPath;
					continue;	// no add dead paths
				}
				m_Paths.AddToTail(pPath);
			}
		}

		// read camera
		if(fThisVersion < 1.4f)
		{
			float unused[3];
			file.read((char*) unused, sizeof(float)*3);
			file.read((char*) unused, sizeof(float)*3);
		}

		if(file.bad())
			goto FatalError;

		PostloadWorld();
		
		if (g_pGameConfig->GetTextureFormat() == tfVMT)
		{
			// do batch search and replace of textures from trans.txt if it exists.
			char translationFilename[MAX_PATH];
			Q_snprintf( translationFilename, sizeof( translationFilename ), "materials/trans.txt" );
			FileHandle_t searchReplaceFP = fopen( translationFilename, "r" );
			if( searchReplaceFP )
			{
				CMapDoc::GetActiveMapDoc()->BatchReplaceTextures( searchReplaceFP );
				g_pFileSystem->Close( searchReplaceFP );
			}
		}
	}

	return nSolids;

FatalError:
	CString str;
	if(bCorrupt)
	{
		// file-is-corrupt error
		str.Format("The file is corrupt.");
		AfxMessageBox(str);
		
		return -1;
	}

	// OS error.
	str.Format("The OS reported an error %s the file: %s",
		fIsStoring ? "saving" : "loading", strerror(errno));
	AfxMessageBox(str);

	return -1;
}
