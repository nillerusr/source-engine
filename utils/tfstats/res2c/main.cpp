//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define NUM_PER_LINE 40

extern char* szHeaderFile;

void printUsage()
{
	printf("res2c <res file name> <c file name> <object name>\n");
}

char* id4filename(const char* filename)
{
	static char id[500];
	
	const char* read=filename;
	char *write=id;
	
	for (read;*read;read++)
	{
		//if first char
		if (read==filename)
		{
			if (isalpha(*read) || *read=='_')
				*write++=*read;
		}
		else if (isalnum(*read))
			*write++=*read;
	}
	*write++='s';
	*write++='r';
	*write++='c';
	*write++='\0';

	return id;
}
void main(int argc, const char* argv[])
{
	if (argc < 4)
	{
		printUsage();
		return;
	}
	char cppname[200];
	sprintf(cppname,"%s.cpp",argv[2]);
	char hname[200];
	sprintf(hname,"%s.h",argv[2]);
	
	FILE* f=fopen(argv[1],"rb");
	FILE* cppout=fopen(cppname,"at");
	FILE* hout=fopen(hname,"at");
	FILE* brheader=fopen("BinaryResource.h","wt");
	if (!brheader){printf("couldn't open %s to write\n","BinaryResource.h");exit(-1);}
	if (!f){printf("couldn't read %s\n",argv[1]);exit(-1);}
	if (!cppout){printf("couldn't open %s to write\n",argv[2]);exit(-1);}
	if (!hout){printf("couldn't open %s to write\n",argv[2]);exit(-1);}

	
	fprintf(brheader,szHeaderFile);
	fclose(brheader);
	
	
	fprintf(cppout,"\nunsigned char %s[]={\n",id4filename(argv[1]));
	
	int numLeft4Line=NUM_PER_LINE;
	
	unsigned char c;
	int result=fread(&c,sizeof(unsigned char),1,f);
	
	int numbytes=0;
	while (result)
	{
		//int longc=(*c)&0x000000ff;
		fprintf(cppout,"0x%02.2x,",c);
		numbytes++;
		if(--numLeft4Line==0)
		{
			numLeft4Line=NUM_PER_LINE;
			fprintf(cppout,"\n");
		}
		result=fread(&c,sizeof(unsigned char),1,f);
	}

	fprintf(cppout,"\n};\n\n");
	
	char* coloncolon=strstr(argv[3],"::");
	if (coloncolon!=NULL)
	{
		coloncolon+=2;
		fprintf(hout,"static CBinaryResource %s;\n",coloncolon);
		fprintf(cppout,"CBinaryResource %s(\"%s\",%li,%s);\n\n\n",argv[3],argv[1],numbytes,id4filename(argv[1]));
	}
	else
	{
		fprintf(hout,"//extern CBinaryResource g_%s;\n",argv[3]);
		fprintf(cppout,"CBinaryResource g_%s;\n",argv[3]);
	}
	fclose(cppout);
	fclose(hout);
	fclose(f);
}