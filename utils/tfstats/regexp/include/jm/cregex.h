//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*
 *
 * Copyright (c) 1998-9
 * Dr John Maddock
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Dr John Maddock makes no representations
 * about the suitability of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
 *
 */
 
 /*
  *   FILE     cregex.h
  *   VERSION  2.12
  */

#ifndef CREGEX_H
#define CREGEX_H

#include <jm/jm_cfg.h>

/* include these defs only for POSIX compatablity */

typedef int regoff_t;

typedef struct
{
   unsigned int re_magic;
   unsigned int re_nsub;      /* number of parenthesized subexpressions */
   const char* re_endp;       /* end pointer for REG_PEND */
   void* guts;             /* none of your business :-) */
   unsigned int eflags;       /* none of your business :-) */
} regex_tA;

#ifndef JM_NO_WCSTRING
typedef struct
{
   unsigned int re_magic;
   unsigned int re_nsub;      /* number of parenthesized subexpressions */
   const wchar_t* re_endp;       /* end pointer for REG_PEND */
   void* guts;             /* none of your business :-) */
   unsigned int eflags;       /* none of your business :-) */
} regex_tW;
#endif

typedef struct
{
   regoff_t rm_so;      /* start of match */
   regoff_t rm_eo;      /* end of match */
} regmatch_t;

/* regcomp() flags */
#define  REG_BASIC      0000
#define  REG_EXTENDED   0001
#define  REG_ICASE      0002
#define  REG_NOSUB      0004
#define  REG_NEWLINE    0010
#define  REG_NOSPEC     0020
#define  REG_PEND       0040
#define  REG_DUMP       0200
#define  REG_NOCOLLATE  0400

#define  REG_ASSERT  15
#define  REG_INVARG  16
#define  REG_ATOI 255   /* convert name to number (!) */
#define  REG_ITOA 0400  /* convert number to name (!) */

/* regexec() flags */
#define  REG_NOTBOL     00001
#define  REG_NOTEOL     00002
#define  REG_STARTEND   00004

#ifdef __cplusplus
extern "C" {
#endif

JM_IX_DECL int RE_CCALL regcompA(regex_tA*, const char*, int);
JM_IX_DECL unsigned int RE_CCALL regerrorA(int, const regex_tA*, char*, unsigned int);
JM_IX_DECL int RE_CCALL regexecA(const regex_tA*, const char*, unsigned int, regmatch_t*, int);
JM_IX_DECL void RE_CCALL regfreeA(regex_tA*);

#ifndef JM_NO_WCSTRING
JM_IX_DECL int RE_CCALL regcompW(regex_tW*, const wchar_t*, int);
JM_IX_DECL unsigned int RE_CCALL regerrorW(int, const regex_tW*, wchar_t*, unsigned int);
JM_IX_DECL int RE_CCALL regexecW(const regex_tW*, const wchar_t*, unsigned int, regmatch_t*, int);
JM_IX_DECL void RE_CCALL regfreeW(regex_tW*);
#endif

#ifdef UNICODE
#define regcomp regcompW
#define regerror regerrorW
#define regexec regexecW
#define regfree regfreeW
#define regex_t regex_tW
#else
#define regcomp regcompA
#define regerror regerrorA
#define regexec regexecA
#define regfree regfreeA
#define regex_t regex_tA
#endif


#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
JM_NAMESPACE(__JM)
#endif

/* regerror() flags */
typedef enum
{
  REG_NOERROR = 0,   /* Success.  */
  REG_NOMATCH = 1,      /* Didn't find a match (for regexec).  */

  /* POSIX regcomp return error codes.  (In the order listed in the
     standard.)  */
  REG_BADPAT = 2,    /* Invalid pattern.  */
  REG_ECOLLATE = 3,  /* Undefined collating element.  */
  REG_ECTYPE = 4,    /* Invalid character class name.  */
  REG_EESCAPE = 5,   /* Trailing backslash.  */
  REG_ESUBREG = 6,   /* Invalid back reference.  */
  REG_EBRACK = 7,    /* Unmatched left bracket.  */
  REG_EPAREN = 8,    /* Parenthesis imbalance.  */
  REG_EBRACE = 9,    /* Unmatched \{.  */
  REG_BADBR = 10,    /* Invalid contents of \{\}.  */
  REG_ERANGE = 11,   /* Invalid range end.  */
  REG_ESPACE = 12,   /* Ran out of memory.  */
  REG_BADRPT = 13,   /* No preceding re for repetition op.  */
  REG_EEND = 14,     /* unexpected end of expression */
  REG_ESIZE = 15,    /* expression too big */
  REG_ERPAREN = 16,   /* unmatched right parenthesis */
  REG_EMPTY = 17,    /* empty expression */
  REG_E_MEMORY = 18, /* out of memory */
  REG_E_UNKNOWN = 19 /* unknown error */
} reg_errcode_t;

enum match_flags
{
   match_default = 0,
   match_not_bol = 1,                                // first is not start of line
   match_not_eol = match_not_bol << 1,               // last is not end of line
   match_not_bob = match_not_eol << 1,               // first is not start of buffer
   match_not_eob = match_not_bob << 1,               // last is not end of buffer
   match_not_bow = match_not_eob << 1,               // first is not start of word
   match_not_eow = match_not_bow << 1,               // last is not end of word
   match_not_dot_newline = match_not_eow << 1,       // \n is not matched by '.'
   match_not_dot_null = match_not_dot_newline << 1,  // '\0' is not matched by '.'
   match_prev_avail = match_not_dot_null << 1,       // *--first is a valid expression
   match_init = match_prev_avail << 1,               // internal use
   match_any = match_init << 1,                      // don't care what we match
   match_not_null = match_any << 1,                  // string can't be null
   match_continuous = match_not_null << 1,           // each grep match must continue from
                                                     // uninterupted from the previous one
   match_stop = match_continuous << 1                // stop after first match (grep)
};



#ifdef __cplusplus
JM_END_NAMESPACE
#endif

//
// C++ high level wrapper goes here:
//
#if defined(__cplusplus) && !defined(JM_NO_STRING_H)
#include <string>
#include <vector>
JM_NAMESPACE(__JM)

class RegExData;
class RegEx;
struct pred1;
struct pred2;
struct pred3;
struct pred4;

typedef bool (*GrepCallback)(const RegEx& expression);
typedef bool (*GrepFileCallback)(const char* file, const RegEx& expression);
typedef bool (*FindFilesCallback)(const char* file);

class JM_IX_DECL RegEx
{
private:
   RegExData* pdata;
public:
   RegEx();
   RegEx(const RegEx& o);
   ~RegEx();
   RegEx(const char* c, bool icase = false);
   RegEx(const __JM_STD::string& s, bool icase = false);
   RegEx& operator=(const RegEx& o);
   RegEx& operator=(const char* p);
   RegEx& operator=(const __JM_STD::string& s){ return this->operator=(s.c_str()); }
   unsigned int SetExpression(const char* p, bool icase = false);
   unsigned int SetExpression(const __JM_STD::string& s, bool icase = false){ return SetExpression(s.c_str(), icase); }
   __JM_STD::string Expression()const;
   //
   // now matching operators:
   //
   bool Match(const char* p, unsigned int flags = match_default);
   bool Match(const __JM_STD::string& s, unsigned int flags = match_default) { return Match(s.c_str(), flags); }
   bool Search(const char* p, unsigned int flags = match_default);
   bool Search(const __JM_STD::string& s, unsigned int flags = match_default) { return Search(s.c_str(), flags); }
   unsigned int Grep(GrepCallback cb, const char* p, unsigned int flags = match_default);
   unsigned int Grep(GrepCallback cb, const __JM_STD::string& s, unsigned int flags = match_default) { return Grep(cb, s.c_str(), flags); }
   unsigned int Grep(__JM_STD::vector<__JM_STD::string>& v, const char* p, unsigned int flags = match_default);
   unsigned int Grep(__JM_STD::vector<__JM_STD::string>& v, const __JM_STD::string& s, unsigned int flags = match_default) { return Grep(v, s.c_str(), flags); }
   unsigned int Grep(__JM_STD::vector<unsigned int>& v, const char* p, unsigned int flags = match_default);
   unsigned int Grep(__JM_STD::vector<unsigned int>& v, const __JM_STD::string& s, unsigned int flags = match_default) { return Grep(v, s.c_str(), flags); }
   unsigned int GrepFiles(GrepFileCallback cb, const char* files, bool recurse = false, unsigned int flags = match_default);
   unsigned int GrepFiles(GrepFileCallback cb, const __JM_STD::string& files, bool recurse = false, unsigned int flags = match_default) { return GrepFiles(cb, files.c_str(), recurse, flags); }
   unsigned int FindFiles(FindFilesCallback cb, const char* files, bool recurse = false, unsigned int flags = match_default);
   unsigned int FindFiles(FindFilesCallback cb, const __JM_STD::string& files, bool recurse = false, unsigned int flags = match_default) { return FindFiles(cb, files.c_str(), recurse, flags); }
   //
   // now operators for returning what matched in more detail:
   //
   unsigned int Position(int i = 0)const;
   unsigned int Length(int i = 0)const;
   unsigned int Line()const;
   unsigned int Marks()const;
   __JM_STD::string What(int i = 0)const;
   __JM_STD::string operator[](int i)const { return What(i); }

   friend struct pred1;
   friend struct pred2;
   friend struct pred3;
   friend struct pred4;
};


JM_END_NAMESPACE

#if !defined(JM_NO_NAMESPACES) && !defined(JM_NO_USING) && defined(__cplusplus)

using __JM::RegEx;
using __JM::GrepCallback;
using __JM::GrepFileCallback;
using __JM::FindFilesCallback;

#endif

#endif // __cplusplus

#if !defined(JM_NO_NAMESPACES) && !defined(JM_NO_USING) && defined(__cplusplus)

using __JM::match_flags;
using __JM::reg_errcode_t;

using __JM::REG_NOERROR;
using __JM::REG_NOMATCH;
using __JM::REG_BADPAT;
using __JM::REG_ECOLLATE;
using __JM::REG_ECTYPE;
using __JM::REG_EESCAPE;
using __JM::REG_ESUBREG;
using __JM::REG_EBRACK;
using __JM::REG_EPAREN;
using __JM::REG_EBRACE;
using __JM::REG_BADBR;
using __JM::REG_ERANGE;
using __JM::REG_ESPACE;
using __JM::REG_BADRPT;
using __JM::REG_EEND;
using __JM::REG_ESIZE;
using __JM::REG_ERPAREN;
using __JM::REG_EMPTY;
using __JM::REG_E_MEMORY;
using __JM::REG_E_UNKNOWN;
using __JM::match_default;
using __JM::match_not_bol;
using __JM::match_not_eol;
using __JM::match_not_bob;
using __JM::match_not_eob;
using __JM::match_not_bow;
using __JM::match_not_eow;
using __JM::match_not_dot_newline;
using __JM::match_not_dot_null;
using __JM::match_prev_avail;
using __JM::match_init;
using __JM::match_any;
using __JM::match_not_null;
using __JM::match_continuous;
using __JM::match_stop;

#endif


#endif





