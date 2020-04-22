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
  *
  *   FILE     fileiter.h
  *   VERSION  2.12
  *
  * this file declares various platform independent file and directory
  * iterators, plus binary file input in the form of class map_file.
  *
  */


#ifndef __FILEITER_H
#define __FILEITER_H

#include <jm/jm_cfg.h>

#if (defined(__WIN32__) || defined(_WIN32) || defined(WIN32)) && !defined(JM_NO_WIN32)

#define FI_W32
#include <windows.h>

JM_NAMESPACE(__JM)

typedef WIN32_FIND_DATA _fi_find_data;
typedef HANDLE _fi_find_handle;

JM_END_NAMESPACE

#define _fi_invalid_handle INVALID_HANDLE_VALUE
#define _fi_dir FILE_ATTRIBUTE_DIRECTORY

#else

#include <stdio.h>
#include <ctype.h>
#ifndef JM_NO_STL
#include <iterator>
#include <list>
#if defined(__SUNPRO_CC) && !defined(JM_NO_NAMESPACES)
using __JM_STD::list;
#endif
#endif
#include <assert.h>
#include <dirent.h>

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

JM_NAMESPACE(__JM)

struct _fi_find_data
{
   unsigned dwFileAttributes;
   char cFileName[MAX_PATH];
};

struct _fi_priv_data;

typedef _fi_priv_data* _fi_find_handle;
#define _fi_invalid_handle NULL
#define _fi_dir 1

_fi_find_handle _fi_FindFirstFile(const char* lpFileName, _fi_find_data* lpFindFileData);
bool _fi_FindNextFile(_fi_find_handle hFindFile,   _fi_find_data* lpFindFileData);
bool _fi_FindClose(_fi_find_handle hFindFile);

JM_END_NAMESPACE

#ifdef FindFirstFile
 #undef FindFirstFile
#endif
#ifdef FindNextFile
 #undef FindNextFile
#endif
#ifdef FindClose
 #undef FindClose
#endif

#define FindFirstFile _fi_FindFirstFile
#define FindNextFile _fi_FindNextFile
#define FindClose _fi_FindClose

#endif

JM_NAMESPACE(__JM)

#ifdef FI_W32 // win32 mapfile

class JM_IX_DECL mapfile
{
   HANDLE hfile;
   HANDLE hmap;
   const char* _first;
   const char* _last;
public:

   typedef const char* iterator;

   mapfile(){ hfile = hmap = 0; _first = _last = 0; }
   mapfile(const char* file){ hfile = hmap = 0; _first = _last = 0; open(file); }
   ~mapfile(){ close(); }
   void open(const char* file);
   void close();
   const char* begin(){ return _first; }
   const char* end(){ return _last; }
   size_t size(){ return _last - _first; }
   bool valid(){ return (hfile != 0) && (hfile != INVALID_HANDLE_VALUE); }
};


#elif !defined(JM_NO_STL)  // use POSIX API to emulate the memory map:

class JM_IX_DECL mapfile_iterator;

class JM_IX_DECL mapfile
{
   typedef char* pointer;
   FILE* hfile;
   long int _size;
   pointer* _first;
   pointer* _last;
   mutable __JM_STD::list<pointer*> condemed;
   enum sizes
   {
      buf_size = 4096
   };
   void lock(pointer* node)const;
   void unlock(pointer* node)const;
public:

   typedef mapfile_iterator iterator;

   mapfile(){ hfile = 0; _size = 0; _first = _last = 0; }
   mapfile(const char* file){ hfile = 0; _size = 0; _first = _last = 0; open(file); }
   ~mapfile(){ close(); }
   void open(const char* file);
   void close();
   iterator begin()const;
   iterator end()const;
   unsigned long size()const{ return _size; }
   bool valid()const{ return hfile != 0; }
   friend class mapfile_iterator;
};

class JM_IX_DECL mapfile_iterator : public JM_RA_ITERATOR(char, long)
{
   typedef mapfile::pointer pointer;
   pointer* node;
   const mapfile* file;
   unsigned long offset;
   long position()const
   {
      return file ? ((node - file->_first) * mapfile::buf_size + offset) : 0;
   }
   void position(long pos)
   {
      if(file)
      {
         node = file->_first + (pos / mapfile::buf_size);
         offset = pos % mapfile::buf_size;
      }
   }
public:
   mapfile_iterator() { node = 0; file = 0; offset = 0; }
   mapfile_iterator(const mapfile* f, long position)
   {
      file = f;
      node = f->_first + position / mapfile::buf_size;
      offset = position % mapfile::buf_size;
      if(file)
         file->lock(node);
   }
   mapfile_iterator(const mapfile_iterator& i)
   {
      file = i.file;
      node = i.node;
      offset = i.offset;
      if(file)
         file->lock(node);
   }
   ~mapfile_iterator()
   {
      if(file && node)
         file->unlock(node);
   }
   mapfile_iterator& operator = (const mapfile_iterator& i);
   char operator* ()const
   {
      assert(node >= file->_first);
      assert(node < file->_last);
      return file ? *(*node + sizeof(int) + offset) : char(0);
   }
   mapfile_iterator& operator++ ();
   mapfile_iterator operator++ (int);
   mapfile_iterator& operator-- ();
   mapfile_iterator operator-- (int);

   mapfile_iterator& operator += (long off)
   {
      position(position() + off);
      return *this;
   }
   mapfile_iterator& operator -= (long off)
   {
      position(position() - off);
      return *this;
   }

   friend inline bool operator==(const mapfile_iterator& i, const mapfile_iterator& j)
   {
      return (i.file == j.file) && (i.node == j.node) && (i.offset == j.offset);
   }
#ifndef JM_NO_NOT_EQUAL
   friend inline bool operator!=(const mapfile_iterator& i, const mapfile_iterator& j)
   {
      return !(i == j);
   }
#endif
   friend inline bool operator<(const mapfile_iterator& i, const mapfile_iterator& j)
   {
      return i.position() < j.position();
   }

   friend mapfile_iterator operator + (const mapfile_iterator& i, long off);
   friend mapfile_iterator operator - (const mapfile_iterator& i, long off);
   friend inline long operator - (const mapfile_iterator& i, const mapfile_iterator& j)
   {
      return i.position() - j.position();
   }
};

#endif

// _fi_sep determines the directory separator, either '\\' or '/'
JM_IX_DECL extern const char* _fi_sep;

struct file_iterator_ref
{
   _fi_find_handle hf;
   _fi_find_data _data;
   long count;
};


class JM_IX_DECL file_iterator : public JM_INPUT_ITERATOR(const char*, __JM_STDC::ptrdiff_t)
{
   char* _root;
   char* _path;
   char* ptr;
   file_iterator_ref* ref;

public:
   file_iterator();
   file_iterator(const char* wild);
   ~file_iterator();
   file_iterator(const file_iterator&);
   file_iterator& operator=(const file_iterator&);
   const char* root() { return _root; }
   const char* path() { return _path; }
   _fi_find_data* data() { return &(ref->_data); }
   void next();
   file_iterator& operator++() { next(); return *this; }
   file_iterator operator++(int);
   const char* operator*() { return path(); }

   friend inline bool operator == (const file_iterator& f1, const file_iterator& f2)
   {
      return ((f1.ref->hf == _fi_invalid_handle) && (f1.ref->hf == _fi_invalid_handle));
   }
#ifndef JM_NO_NOT_EQUAL
   friend inline bool operator != (const file_iterator& f1, const file_iterator& f2)
   {
      return !(f1 == f2);
   }
#endif
};

inline bool operator < (const file_iterator& f1, const file_iterator& f2)
{
   return false;
}


class JM_IX_DECL directory_iterator : public JM_INPUT_ITERATOR(const char*, __JM_STDC::ptrdiff_t)
{
   char* _root;
   char* _path;
   char* ptr;
   file_iterator_ref* ref;

public:
   directory_iterator();
   directory_iterator(const char* wild);
   ~directory_iterator();
   directory_iterator(const directory_iterator& other);
   directory_iterator& operator=(const directory_iterator& other);

   const char* root() { return _root; }
   const char* path() { return _path; }
   _fi_find_data* data() { return &(ref->_data); }
   void next();
   directory_iterator& operator++() { next(); return *this; }
   directory_iterator operator++(int);
   const char* operator*() { return path(); }

   static const char* separator() { return _fi_sep; }

   friend inline bool operator == (const directory_iterator& f1, const directory_iterator& f2)
   {
      return ((f1.ref->hf == _fi_invalid_handle) && (f1.ref->hf == _fi_invalid_handle));
   }

#ifndef JM_NO_NOT_EQUAL
   friend inline bool operator != (const directory_iterator& f1, const directory_iterator& f2)
   {
      return !(f1 == f2);
   }
#endif
};

inline bool operator < (const directory_iterator& f1, const directory_iterator& f2)
{
   return false;
}

JM_END_NAMESPACE

#if !defined(JM_NO_NAMESPACES) && !defined(JM_NO_USING)

using __JM::directory_iterator;
using __JM::file_iterator;
using __JM::mapfile;

#endif


#endif     // __WINITER_H








