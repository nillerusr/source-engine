//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Implementation of CTextFile. See TextFile.h for details
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "TFStatsApplication.h"
#include "util.h"
#include "TextFile.h"

//------------------------------------------------------------------------------------------------------
// Function:	CTextFile::init
// Purpose:	initializes a CTextFile object
// Input:	filename - name of the file that this object will represent
//				eliminateComments - if true, C++ style comments will not be handed
//				back as tokens to the user of this object
// Output:	
//------------------------------------------------------------------------------------------------------
void CTextFile::init(const char* filename,bool eliminateComments)
{
	this->filename=filename;
	//only support one push back
	fWordPushed=false;
	noComments=eliminateComments;
	memset(wordBuf,0,BUF_SIZE);
	theFile=fopen(filename,"rt");

	
	//three different delim sets
	//use this when reading strings
	stringDelims="\"";
	//use this when reading normal file stuff
	normalDelims=" \t{}=\n\r;\"";
	//use this when you want to discard a block
	blockDelims="}";
	//set to default
	delims=normalDelims;
}

//------------------------------------------------------------------------------------------------------
// Function:	CTextFile::isDelim
// Purpose:	returns true if the given char is in the current delimiter set
// Input:	c - the character to test
// Output:	Returns true on success, false on failure.
//------------------------------------------------------------------------------------------------------
bool CTextFile::isDelim(char c)
{
	for (int i=0;delims[i] != 0; i++)
	{
		if (c==delims[i])
			return true;
	}
	return false;
}

//------------------------------------------------------------------------------------------------------
// Function:	CTextFile::discardBlock
// Purpose:	advances the file ptr to the character after the next }
//------------------------------------------------------------------------------------------------------
void CTextFile::discardBlock()
{
	delims=blockDelims;
	getToken();
	delims=normalDelims;
	discard("}");
}

//------------------------------------------------------------------------------------------------------
// Function:	CTextFile::getToken
// Purpose:	gets and returns the next token in the file
// Output:	const char*
//------------------------------------------------------------------------------------------------------
const char* CTextFile::getToken()
{
	return getToken(wordBuf);
}

//------------------------------------------------------------------------------------------------------
// Function:	CTextFile::getToken
// Purpose:	gets and returns the next token in the file, with an arbitrary buffer
// Input:	outputBuf - the buffer to put the token in
// Output:	const char*
//------------------------------------------------------------------------------------------------------
const char* CTextFile::getToken(char* outputBuf)
{
	if (fWordPushed)
	{
		fWordPushed=false;
		strcpy(outputBuf,wordBuf);
		return wordBuf;
	}
	readToken:
	char c=getNextNonWSChar();
	if (!theFile || feof(theFile)) return NULL;
	if (isDelim(c) && !(isspace(c)))
	{
		outputBuf[0]=c;
		outputBuf[1]=0;
		return outputBuf;
	}

	int write=0;
	while (!isDelim(c))
	{
		if (c=='\\')
		{
			c=fgetc(theFile);
			if (!theFile&& feof(theFile))
				break;
		}
		
		//these are both in the normal delimiter set, so this case won't happen unless we're reading a string
		//which is exactly the behaviour we want
		if (c=='\n' || c=='\r')
		{
			outputBuf[write]=0;
			g_pApp->fatalError("new line in string constant (or unterminated string constant):\n\"%s\"",outputBuf);
		}
		outputBuf[write++]=c;
		c=fgetc(theFile);
		
		if (feof(theFile))
			break;
	}
	
	if (theFile && !feof(theFile))
		fseek(theFile,-1,SEEK_CUR); //seek to before the delimiter

	outputBuf[write]=0;

	if (outputBuf[0] == '/' && outputBuf[1] == '/')
	{
		while (fgetc(theFile)!='\n');
		goto readToken;
	}
	
	return outputBuf;
}

//------------------------------------------------------------------------------------------------------
// Function:	CTextFile::getNextNonWSChar
// Purpose:	gets and returns  the next non whitespace character
// Output:	char
//------------------------------------------------------------------------------------------------------
char CTextFile::getNextNonWSChar()
{
	char c;
top:
	c=' ';
	while (theFile && !feof(theFile) && isspace(c))
		c=fgetc(theFile);
	
	if (!theFile) return 0;
	if (feof(theFile)) 	return 0;
	
	if (!noComments)
		return c;

	//check for comments
	if (c=='/')
	{
		char c2=fgetc(theFile);
		if (c2=='/') //found comment?  
		{
			//discard and start over
			getLine();
			goto top;
		}
		else
		{
			fseek(theFile,-1,SEEK_CUR);
			return c;
		}
	}
	else 
	{
		return c;
	}

}

//------------------------------------------------------------------------------------------------------
// Function:	CTextFile::readString
// Purpose:	reads and returns the contents of the file up to the next "
// Output:	const char*
//------------------------------------------------------------------------------------------------------
const char* CTextFile::readString()
{
	if (fWordPushed)
	{
		fWordPushed=false;
		return wordBuf;
	}
	char c=getNextNonWSChar();
	
	if (c=='\"')
	{
		delims=stringDelims;
		getToken();		//handles escaping stuff
		delims=normalDelims;
		if (wordBuf[0]=='\"' && wordBuf[1]==0)
			wordBuf[0]=0;
		else
			discard("\"");
	}
	else
	{
		fseek(theFile,-1,SEEK_CUR);
		getToken();
	}


	return wordBuf;
}

//------------------------------------------------------------------------------------------------------
// Function:	CTextFile::readString
// Purpose:	reads and returns the contents of the file up to the next "
// Output:	const char*
//------------------------------------------------------------------------------------------------------
const char* CTextFile::readString(char* buf)
{
	if (fWordPushed)
	{
		fWordPushed=false;
		strcpy(buf,wordBuf);
		return buf;
	}
	char c=getNextNonWSChar();
	if (eof()) return NULL;
	if (c=='\"')
	{
		delims=stringDelims;
		getToken(buf);		//handles escaping stuff
		delims=normalDelims;
		if (buf[0]=='\"' && buf[1]==0)
			buf[0]=0;
		else
			discard("\"");
	}
	else
	{
		fseek(theFile,-1,SEEK_CUR);
		getToken(buf);
	}


	return buf;
}

//------------------------------------------------------------------------------------------------------
// Function:	CTextFile::getLine
// Purpose:	reads and returns the contents of the file up to the next \r or \n 
// Output:	const char*
//------------------------------------------------------------------------------------------------------
const char* CTextFile::getLine()
{
	fgets(wordBuf,BUF_SIZE,theFile);
	return wordBuf;
}

//------------------------------------------------------------------------------------------------------
// Function:	CTextFile::discard
// Purpose:	discards the next token, also checks to see if it is the same as the token
//		the user of the object was expecting to discard. 
// Input:	test - the token that is expected to be discarded.
// Output:	Returns true on success, false on failure.
//------------------------------------------------------------------------------------------------------
bool CTextFile::discard(char* test)
{
	if (!theFile || feof(theFile))
		return true;

	char wordBuf2[BUF_SIZE];
	getToken(wordBuf2);

	int result = stricmp(wordBuf2,test);
	if (result !=0)
		g_pApp->fatalError("While parsing %s, expecting \"%s\", got \"%s\"",filename.c_str(),test,wordBuf2);
	
	return true;
}

//------------------------------------------------------------------------------------------------------
// Function:	CTextFile::peekNext
// Purpose:	peeks at the next token.  gets then immediately pushes it back
//		destroys any pushed back words from before.
// Output:	char*
//------------------------------------------------------------------------------------------------------
char* CTextFile::peekNext()
{
	fpos_t startpos;
	fgetpos(theFile,&startpos);

	const char* c=getToken(peekBuf);
	if (!c)
		return NULL;
	//pushBack(c);
	fseek(theFile,startpos,SEEK_SET);
	return peekBuf;
}

//------------------------------------------------------------------------------------------------------
// Function:	CTextFile::peekNextString
// Purpose:	peeks at the next string.  gets then immediately pushes it back
//		destroys any pushed back words from before.
// Output:	char*
//------------------------------------------------------------------------------------------------------
char* CTextFile::peekNextString()
{
	if (!theFile)
		return NULL;
	fpos_t startpos;
	fgetpos(theFile,&startpos);
	

	const char* c=readString(peekBuf);
	if (!c)
		return NULL;
	//pushBack(c);
	fseek(theFile,startpos,SEEK_SET);
	return peekBuf;
}

//------------------------------------------------------------------------------------------------------
// Function:	CTextFile::pushBack
// Purpose: "pushes" a token back into the stream.  Only can have one pushed back
//		token at a time. getting a token or peeking will destroy the pushed back token
// Input:	pushTok - the token to push back.
//------------------------------------------------------------------------------------------------------
void CTextFile::pushBack(const char* pushTok)
{
	if (pushTok != wordBuf)
		strncpy(wordBuf,pushTok,BUF_SIZE);
	fWordPushed=true;
}

//------------------------------------------------------------------------------------------------------
// Function:	CTextFile::reset
// Purpose:	resets the file ptr to the beginning of the file
//------------------------------------------------------------------------------------------------------
void CTextFile::reset()
{
	fseek(theFile,0,SEEK_SET);
}

//------------------------------------------------------------------------------------------------------
// Function:	CTextFile::~CTextFile
// Purpose: destructor	
//------------------------------------------------------------------------------------------------------
CTextFile::~CTextFile()
{
	if (theFile)
		fclose(theFile);
#ifndef WIN32
		chmod(filename.c_str(),PERMIT);
#endif

} 



bool CTextFile::eof()
{
	bool retval=false;
	
	//test current pos first
	retval = (!theFile || feof(theFile));
	if (retval)
		return true;

	//now see if only whitespace is left
	
	fpos_t beforecheck;
	fgetpos(theFile,&beforecheck);
	char c=getNextNonWSChar();

	retval = (!theFile || feof(theFile));

	fseek(theFile,beforecheck,SEEK_SET);
	return retval;
}
	

int CTextFile::readInt()
{
	readString();
	return atoi(wordBuf);
}

unsigned long CTextFile::readULong()
{
	readString();
	return strtoul(wordBuf,NULL,10);
}