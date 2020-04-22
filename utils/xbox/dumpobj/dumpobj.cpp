//========= Copyright Valve Corporation, All rights reserved. ============//
/*****************************************************************************
	DUMPOBJ.CPP

*****************************************************************************/
#include "..\toollib\toollib.h"

typedef struct section_s
{
	int			size;
	char*		name;
	section_s*	nextPtr;
} section_t;

typedef struct obj_s
{
	char*		filename;
	section_t*	sectionPtr;
	obj_s*		nextPtr;
} obj_t;

obj_t*	g_obj_head;
char	g_sectionName[128];
bool	g_bVerbose;

typedef struct
{
	char*	filename;
	int		size;
} entry_t;

/***********************************************************
	_SortCallback

***********************************************************/
int _SortCallback(const void *a, const void *b) 
{
	entry_t*	aPtr;
	entry_t*	bPtr;

	aPtr = (entry_t*)a;
	bPtr = (entry_t*)b;

	return (aPtr->size-bPtr->size);
}

/***********************************************************
	FreeInfo

***********************************************************/
void FreeInfo()
{
	obj_t*		objPtr;
	obj_t*		nextObjPtr;
	section_t*	sectionPtr;
	section_t*	nextSectionPtr;

	objPtr = g_obj_head;
	while (objPtr)
	{
		nextObjPtr = objPtr->nextPtr;
		sectionPtr = objPtr->sectionPtr;
		while (sectionPtr)
		{
			nextSectionPtr = sectionPtr->nextPtr;
			
			TL_Free(sectionPtr->name);
			TL_Free(sectionPtr);

			sectionPtr = nextSectionPtr;
		}

		TL_Free(objPtr->filename);
		TL_Free(objPtr);

		objPtr = nextObjPtr;
	}
}

/***********************************************************
	AnalyzeData

***********************************************************/
void AnalyzeData()
{
	obj_t*		objPtr;
	section_t*	sectionPtr;
	char*		sections[128];
	int			totals[128];
	int			numSections;
	int			i;
	int			index;
	int			numEntries;
	entry_t*	entries;
	int			total;

	printf("\n");

	memset(totals, 0, sizeof(totals));

	// determine unique sections
	numSections = 0;
	objPtr = g_obj_head;
	while (objPtr)
	{
		sectionPtr = objPtr->sectionPtr;
		while (sectionPtr)
		{
			for (i=0; i<numSections; i++)
			{
				if (!stricmp(sections[i],sectionPtr->name))
					break;
			}
			if (i >= numSections)
			{
				// add it
				sections[numSections] = sectionPtr->name;
				numSections++;
				assert(numSections < 128);
			
				index = numSections-1;
			}
			else
				index = i;

			totals[index] += sectionPtr->size;

			sectionPtr = sectionPtr->nextPtr;
		}
		objPtr = objPtr->nextPtr;
	}

	if (g_sectionName[0])
	{
		// dump only matching section
		printf("Section: %s\n", g_sectionName);
		printf("-------------------------\n");

		// tally entries
		numEntries = 0;
		objPtr = g_obj_head;
		while (objPtr)
		{
			sectionPtr = objPtr->sectionPtr;
			while (sectionPtr)
			{
				if (!stricmp(g_sectionName, sectionPtr->name))
					numEntries++;
				sectionPtr = sectionPtr->nextPtr;
			}
			objPtr = objPtr->nextPtr;
		}

		// fill entries
		if (numEntries)
		{
			entries    = (entry_t*)TL_Malloc(numEntries*sizeof(entry_t));
			numEntries = 0;
			objPtr      = g_obj_head;
			while (objPtr)
			{
				sectionPtr = objPtr->sectionPtr;
				while (sectionPtr)
				{
					if (!stricmp(g_sectionName, sectionPtr->name))
					{
						entries[numEntries].size     = sectionPtr->size;
						entries[numEntries].filename = objPtr->filename;
						numEntries++;
					}
					sectionPtr = sectionPtr->nextPtr;
				}
				objPtr = objPtr->nextPtr;
			}

			// sort
			qsort(entries, numEntries, sizeof(entry_t), _SortCallback);

			// display results
			int total = 0;
			for (i=0; i<numEntries; i++)
			{
				printf("%8d %s\n", entries[i].size, entries[i].filename);
				total += entries[i].size;
			}
			printf("-------------------------\n");
			printf("%8d Total\n", total);

			TL_Free(entries);
		}
	}
	else
	{
		// dump all sections
		printf("Sections:\n");
		printf("---------\n");
		total = 0;
		for (i=0; i<numSections; i++)
		{
			printf("%8d %s\n", totals[i], sections[i]);
			total += totals[i];
		}
		printf("---------\n");
		printf("%8d\n", total);
	}
}


/***********************************************************
	LocalExec

***********************************************************/
int LocalExec(const char* batfilename, const char* arg1)
{
	intptr_t	pid;
	int			errcode;
	const char*	args[8]; 

	args[0] = batfilename;
	args[1] = arg1;
	args[2] = NULL;
	
	pid = _spawnvp(P_NOWAIT, batfilename, args);
	_cwait(&errcode, pid, 0);

	return (errcode);
}

/*****************************************************************************
	GetInfo

*****************************************************************************/
void GetInfo(char* objFilename, char* batFilename, char* logFilename)
{
	char		buff[TL_MAXPATH];
	int			size;
	char		name[128];
	char*		ptr;
	int			errcode;
	char*		token;
	obj_t*		objPtr;
	section_t*	sectionPtr;

	if (g_bVerbose)
		printf("%s\n", objFilename);

	ptr = objFilename;
	if (!strnicmp(objFilename,".\\",2))
	{
		ptr += 2;

		getcwd(buff, sizeof(buff));
		TL_AddSeperatorToPath(buff, buff);

		strcat(buff, ptr);	
	}
	else
		strcpy(buff, objFilename);

	// exec the batch file with the obj file
	errcode = LocalExec(batFilename, buff);
	if (errcode)
	{
		printf("Failed on %s\n", buff);
		return;
	}

	// add new object node
	objPtr           = (obj_t*)TL_Malloc(sizeof(obj_t));
	objPtr->filename = (char*)TL_Malloc((int)strlen(objFilename)+1);
	strcpy(objPtr->filename, objFilename);

	// link it in
	objPtr->nextPtr = g_obj_head;
	g_obj_head      = objPtr;
	
	// read the results
	TL_LoadScriptFile(logFilename);

	while (1)
	{
		token = TL_GetToken(true);
		if (!token || !token[0])
			break;

		if (!stricmp(token, "summary"))
			break;
		else
			TL_SkipRestOfLine();
	}

	while (1)
	{
		token = TL_GetToken(true);
		if (!token || !token[0])
			break;
		sscanf(token, "%x", &size);
		
		token = TL_GetToken(true);
		if (!token || !token[0])
			break;
		strcpy(name, token);

		// add new section node
		sectionPtr       = (section_t*)TL_Malloc(sizeof(section_t));
		sectionPtr->name = (char*)TL_Malloc((int)strlen(name)+1);
		strcpy(sectionPtr->name, name);
		sectionPtr->size = size;

		// link it in
		sectionPtr->nextPtr = objPtr->sectionPtr;
		objPtr->sectionPtr  = sectionPtr;
	}

	TL_FreeScriptFile();
}

/*****************************************************************************
	Usage

*****************************************************************************/
void Usage(void)
{
	printf("usage: dumpobj <*.obj> [-s section] [-r] [-v]\n");
	exit(-1);
}

/*****************************************************************************
	main

*****************************************************************************/
int main(int argc, char* argv[])
{
	tlfile_t**	filelist;
	int			filecount;
	int			i;
	int			recurse;
	int			section;
	FILE*		fp;
	char*		vcvarsPath;
	char		batFilename[TL_MAXPATH];
	char		txtFilename[TL_MAXPATH];

	batFilename[0] = '\0';
	txtFilename[0] = '\0';

	TL_Setup("DUMPOBJ",argc,argv);

	// find critical path to vcvars32.bat
	vcvarsPath = "c:\\Program Files\\Microsoft Visual Studio .Net 2003\\Vc7\\bin\\vcvars32.bat";
	if (!TL_Exists(vcvarsPath))
		TL_Error("Cannot find: %s\n", vcvarsPath);

	if (argc < 2)
		Usage();

	recurse   = TL_CheckParm("r");
	filecount = TL_FindFiles2(argv[1], recurse != 0, &filelist);

	g_bVerbose = TL_CheckParm("v") != 0;

	section = TL_CheckParm("s");
	if (section && section+1 < argc)
		strcpy(g_sectionName, argv[section+1]);
	else
		g_sectionName[0] = '\0';

	strcpy(batFilename,"c:\\");
	TL_TempFilename(batFilename);
	TL_ReplaceDosExtension(batFilename, ".bat");
	
	strcpy(txtFilename,"c:\\");
	TL_TempFilename(txtFilename);
	TL_ReplaceDosExtension(txtFilename, ".txt");

	if (filecount)
	{
		// create bat file
		fp = fopen(batFilename, "wt+");
		if (!fp)
			TL_Error("Could not open bat file '%s'", batFilename);

		fprintf(fp, "@echo off\n");
		fprintf(fp, "@call \"%s\" > nul: \n", vcvarsPath); 
		fprintf(fp, "@dumpbin %%1 > %s\n", txtFilename);
		fclose(fp);
	}

	for (i=0; i<filecount; i++)
		GetInfo(filelist[i]->filename, batFilename, txtFilename);

	if (filecount)
		AnalyzeData();

	if (filecount)
	{
		FreeInfo();
		unlink(batFilename);
		unlink(txtFilename);
	}

	TL_FreeFileList(filecount,filelist);

	TL_End(false);

	return (0);	
}