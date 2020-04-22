//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

//
// studiomdl.c: generates a studio .mdl file from a .qc script
// sources/<scriptname>.mdl.
//


#pragma warning( disable : 4244 )
#pragma warning( disable : 4237 )
#pragma warning( disable : 4305 )


#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>

#include "cmdlib.h"
#include "scriplib.h"
#include "mathlib/mathlib.h"
#include "studio.h"
#include "studiomdl.h"

// The current version of the SMD file being parsed
// Yes, I know this file is called 'v1support' and there's never actually
// been a v > 1 but now there is
// (actually, there was a while when we were using developing progressive mesh 
// stuff in the middle of HL2 development, but most all that code has long since 
// been deleted)
int g_smdVersion = 1;

int lookup_index( s_source_t *psource, int material, Vector& vertex, Vector& normal, Vector2D texcoord, int iCount, int bones[], float weights[] )
{
	int i, j;

	for (i = 0; i < numvlist; i++) 
	{
		if (v_listdata[i].m == material
			&& DotProduct( g_normal[i], normal ) > normal_blend
			&& VectorCompare( g_vertex[i], vertex )
			&& g_texcoord[i][0] == texcoord[0]
			&& g_texcoord[i][1] == texcoord[1])
		{
			if (g_bone[i].numbones == iCount)
			{
				for (j = 0; j < iCount; j++)
				{
					if (g_bone[i].bone[j] != bones[j] || g_bone[i].weight[j] != weights[j])
						break;
				}
				if (j == iCount)
				{
					v_listdata[i].lastref = numvlist;
					return i;
				}
			}
		}
	}
	if (i >= MAXSTUDIOVERTS) {
		MdlError( "too many indices in source: \"%s\"\n", psource->filename);
	}

	VectorCopy( vertex, g_vertex[i] );
	VectorCopy( normal, g_normal[i] );
	Vector2Copy( texcoord, g_texcoord[i] );

    g_bone[i].numbones = iCount;
	for ( j = 0; j < iCount; j++)
	{
		g_bone[i].bone[j] = bones[j];
		g_bone[i].weight[j] = weights[j];
	}

	v_listdata[i].v = i;
	v_listdata[i].m = material;
	v_listdata[i].n = i;
	v_listdata[i].t = i;

	v_listdata[i].firstref = numvlist;
	v_listdata[i].lastref = numvlist;

	numvlist = i + 1;
	return i;
}


void ParseFaceData( s_source_t *psource, int material, s_face_t *pFace )
{
	int index[3] = {};
	int i, j;
	Vector p;
	Vector normal;
	Vector2D t;
	int		iCount, bones[MAXSTUDIOSRCBONES];
	float   weights[MAXSTUDIOSRCBONES];
	int bone;

	for (j = 0; j < 3; j++) 
	{
		memset( g_szLine, 0, sizeof( g_szLine ) );

		if (!GetLineInput()) 
		{
			MdlError("%s: error on g_szLine %d: %s", g_szFilename, g_iLinecount, g_szLine );
		}

		iCount = 0;

		i = sscanf( g_szLine, "%d %f %f %f %f %f %f %f %f %d %d %f %d %f %d %f %d %f",
			&bone, 
			&p[0], &p[1], &p[2], 
			&normal[0], &normal[1], &normal[2], 
			&t[0], &t[1],
			&iCount,
			&bones[0], &weights[0], &bones[1], &weights[1], &bones[2], &weights[2], &bones[3], &weights[3] );
			
		if (i < 9) 
			continue;

		if (bone < 0 || bone >= psource->numbones) 
		{
			MdlError("bogus bone index\n%d %s :\n%s", g_iLinecount, g_szFilename, g_szLine );
		}

		//Scale face pos
		scale_vertex( p );
		
		// continue parsing more bones.
		// FIXME: don't we have a built in parser that'll do this?
		if (iCount > 4)
		{
			int k;
			int ctr = 0;
			char *token;
			for (k = 0; k < 18; k++)
			{
				while (g_szLine[ctr] == ' ')
				{
					ctr++;
				}
				token = strtok( &g_szLine[ctr], " " );
				ctr += strlen( token ) + 1;
			}
			for (k = 4; k < iCount && k < MAXSTUDIOSRCBONES; k++)
			{
				while (g_szLine[ctr] == ' ')
				{
					ctr++;
				}
				token = strtok( &g_szLine[ctr], " " );
				ctr += strlen( token ) + 1;

				bones[k] = atoi(token);

				token = strtok( &g_szLine[ctr], " " );
				ctr += strlen( token ) + 1;
			
				weights[k] = atof(token);
			}
			// printf("%d ", iCount );

			//printf("\n");
			//exit(1);
		}

		// adjust_vertex( p );
		// scale_vertex( p );

		// move vertex position to object space.
		// VectorSubtract( p, psource->bonefixup[bone].worldorg, tmp );
		// VectorTransform(tmp, psource->bonefixup[bone].im, p );

		// move normal to object space.
		// VectorCopy( normal, tmp );
		// VectorTransform(tmp, psource->bonefixup[bone].im, normal );
		// VectorNormalize( normal );

		// invert v
		t[1] = 1.0 - t[1];

		if (i == 9 || iCount == 0)
		{
			iCount = 1;
			bones[0] = bone;
			weights[0] = 1.0;
		}
		else
		{
			iCount = SortAndBalanceBones( iCount, MAXSTUDIOBONEWEIGHTS, bones, weights );
		}


		index[j] = lookup_index( psource, material, p, normal, t, iCount, bones, weights );
	}

	// pFace->material = material; // BUG
	pFace->a		= index[0];
	pFace->b		= index[2];
	pFace->c		= index[1];
	Assert( ((pFace->a & 0xF0000000) == 0) && ((pFace->b & 0xF0000000) == 0) && 
		((pFace->c & 0xF0000000) == 0) );
}

void Grab_Triangles( s_source_t *psource )
{
	int		i;
	Vector	vmin, vmax;

	vmin[0] = vmin[1] = vmin[2] = 99999;
	vmax[0] = vmax[1] = vmax[2] = -99999;

	g_numfaces = 0;
	numvlist = 0;
 
	//
	// load the base triangles
	//
	int texture;
	int material;
	char texturename[MAX_PATH];

	while (1) 
	{
		if (!GetLineInput()) 
			break;

		// check for end
		if (IsEnd( g_szLine )) 
			break;

		// Look for extra junk that we may want to avoid...
		int nLineLength = strlen( g_szLine );
		if (nLineLength >= sizeof( texturename ))
		{
			MdlWarning("Unexpected data at line %d, (need a texture name) ignoring...\n", g_iLinecount );
			continue;
		}

		// strip off trailing smag
		V_strcpy_safe( texturename, g_szLine );
		for (i = strlen( texturename ) - 1; i >= 0 && ! V_isgraph( texturename[i] ); i--)
		{
		}
		texturename[i + 1] = '\0';

		// funky texture overrides
		for (i = 0; i < numrep; i++)  
		{
			if (sourcetexture[i][0] == '\0') 
			{
				V_strcpy_safe( texturename, defaulttexture[i] );
				break;
			}
			if (stricmp( texturename, sourcetexture[i]) == 0) 
			{
				V_strcpy_safe( texturename, defaulttexture[i] );
				break;
			}
		}

		if (texturename[0] == '\0')
		{
			// weird source problem, skip them
			GetLineInput();
			GetLineInput();
			GetLineInput();
			continue;
		}

		if (stricmp( texturename, "null.bmp") == 0 || stricmp( texturename, "null.tga") == 0 || stricmp( texturename, "debug/debugempty" ) == 0)
		{
			// skip all faces with the null texture on them.
			GetLineInput();
			GetLineInput();
			GetLineInput();
			continue;
		}

		texture = LookupTexture( texturename, ( g_smdVersion > 1 ) );
		psource->texmap[texture] = texture;	// hack, make it 1:1
		material = UseTextureAsMaterial( texture );

		s_face_t f;
		ParseFaceData( psource, material, &f );

		// remove degenerate triangles
		if (f.a == f.b || f.b == f.c || f.a == f.c)
		{
			// printf("Degenerate triangle %d %d %d\n", f.a, f.b, f.c );
			continue;
		}
	
		g_src_uface[g_numfaces] = f;
		g_face[g_numfaces].material = material;
		g_numfaces++;
	}

	BuildIndividualMeshes( psource );
}


int Load_SMD ( s_source_t *psource )
{
	char	cmd[1024];
	int		option;

	// Reset smdVersion
	g_smdVersion = 1;

	if (!OpenGlobalFile( psource->filename ))
		return 0;

	if( !g_quiet )
	{
		printf ("SMD MODEL %s\n", psource->filename);
	}

	g_iLinecount = 0;

	while (GetLineInput()) 
	{
		int numRead = sscanf( g_szLine, "%s %d", cmd, &option );

		// Blank line
		if ((numRead == EOF) || (numRead == 0))
			continue;

		if (stricmp( cmd, "version" ) == 0) 
		{
			if (option < 1 || option > 2) 
			{
				MdlError("bad version\n");
			}
			g_smdVersion = option;
		}
		else if (stricmp( cmd, "nodes" ) == 0) 
		{
			psource->numbones = Grab_Nodes( psource->localBone );
		}
		else if (stricmp( cmd, "skeleton" ) == 0) 
		{
			Grab_Animation( psource, "BindPose" );
		}
		else if (stricmp( cmd, "triangles" ) == 0) 
		{
			Grab_Triangles( psource );
		}
		else if (stricmp( cmd, "vertexanimation" ) == 0) 
		{
			Grab_Vertexanimation( psource, "BindPose" );
		}
		else if ((strncmp( cmd, "//", 2 ) == 0) || (strncmp( cmd, ";", 1 ) == 0) || (strncmp( cmd, "#", 1 ) == 0))
		{
			ProcessSourceComment( psource, cmd );
			continue;
		}
		else 
		{
			MdlWarning("unknown studio command \"%s\"\n", cmd );
		}
	}
	fclose( g_fpInput );

	return 1;
}





















