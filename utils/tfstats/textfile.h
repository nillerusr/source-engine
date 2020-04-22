//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Interface of CTextFile. 
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef TEXTFILE_H
#define TEXTFILE_H
#ifdef WIN32
#pragma once
#endif
#include <stdio.h>
#include <string.h>
#include <string>
#define BUF_SIZE 5000

//------------------------------------------------------------------------------------------------------
// Purpose:  CTextFile represents a configuration file.  The default delimiters 
// are { } = \n \r \t ; " and space. Also C++ style comments are ignored by default.
// CTextFile only supports reading, a 5000 character word /line buffer
// and only supports one pushback
//------------------------------------------------------------------------------------------------------
class CTextFile
{
private:
	std::string filename;
	char wordBuf[BUF_SIZE];
	char peekBuf[BUF_SIZE];
	bool fWordPushed;
	bool noComments;
	FILE* theFile;

	char* delims;
	char* stringDelims;
	char* normalDelims;
	char* blockDelims;

	char getNextNonWSChar();
	bool isDelim(char c);
	
	const char* getToken(char* outputBuf);
public: 
	explicit CTextFile(const char* filename,bool eliminateComments=true){init(filename,eliminateComments);}
	explicit CTextFile(const string& filename,bool eliminateComments=true){init(filename.c_str(),eliminateComments);}
	void init(const char* filename,bool eliminateComments=true);
	std::string& fileName(){return filename;}
	
	void discardBlock();
	const char* readString();
	const char* readString(char* buf);
	int readInt();
	unsigned long readULong();
	const char* getToken();
	const char* getLine();
	bool isValid(){return theFile!=NULL;}
	char* currWord(){return wordBuf;}
	void  pushBack(const char* pushTok);
	char* peekNext();
	char* peekNextString();
	bool eof();
	void reset();
	bool discard(char* test);
	void discard(){getToken();}
	
	~CTextFile();
};

#endif // TEXTFILE_H
