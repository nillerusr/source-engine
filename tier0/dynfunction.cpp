//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared library loading and symbol lookup.
//
// $NoKeywords: $
//=============================================================================//

#include "pch_tier0.h"
#include "tier0/dynfunction.h"

#if defined(WIN32)
typedef HMODULE LibraryHandle;
#define LoadLibraryHandle(libname) LoadLibrary(libname)
#define CloseLibraryHandle(handle) FreeLibrary(handle)
#define LookupInLibraryHandle(handle, fn) GetProcAddress(handle, fn)
#elif defined(POSIX)
#include <dlfcn.h>
typedef void *LibraryHandle;
#define LoadLibraryHandle(libname) dlopen(libname, RTLD_NOW)
#define CloseLibraryHandle(handle) dlclose(handle)
#define LookupInLibraryHandle(handle, fn) dlsym(handle, fn)
#else
#error Please define your platform.
#endif

#if 1
static inline void dbgdynfn(const char *fmt, ...) {}
#else
#define dbgdynfn printf
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

class CSharedLibraryCache
{
public:
	static CSharedLibraryCache &GetCache()
	{
		static CSharedLibraryCache Singleton;
		return Singleton;
	}

	struct CSharedLibraryItem
	{
		CSharedLibraryItem(LibraryHandle handle, const char *name)
		{
			m_handle = handle;
			m_name = new char[strlen(name) + 1];
			m_next = NULL;
			strcpy(m_name, name);
		}

		~CSharedLibraryItem()
		{
			dbgdynfn("CDynamicFunction: Closing library '%s' (%p)\n", m_name, (void *) m_handle);
			CloseLibraryHandle(m_handle);
			delete[] m_name;
			delete m_next;
		}

		char *m_name;
		CSharedLibraryItem *m_next;
		LibraryHandle m_handle;
	};

	CSharedLibraryCache() : m_pList(NULL) {}
	~CSharedLibraryCache() { CloseAllLibraries(); }

	LibraryHandle GetHandle(const char *name)
	{
		CSharedLibraryItem *item = GetCacheItem(name);
		if (item == NULL)
		{
			LibraryHandle lib = LoadLibraryHandle(name);
			dbgdynfn("CDynamicFunction: Loading library '%s' (%p)\n", name, (void *) lib);
			if (lib == NULL)
				return NULL;

			item = new CSharedLibraryItem(lib, name);
			item->m_next = m_pList;
			m_pList = item;
		}
		return item->m_handle;
	}

	void CloseLibrary(const char *name)
	{
		CSharedLibraryItem *item = GetCacheItem(name);
		if (item)
		{
			assert(item == m_pList);
			m_pList = item->m_next;
			item->m_next = NULL;
			delete item;
		}
	}

	void CloseAllLibraries()
	{
		delete m_pList;
	}

private:
	CSharedLibraryItem *GetCacheItem(const char *name)
	{
		CSharedLibraryItem *prev = NULL;
		CSharedLibraryItem *item = m_pList;
		while (item)
		{
			if (strcmp(item->m_name, name) == 0)
			{
				// move this item to the front of the list, since there will
				//  probably be a big pile of these lookups in a row
				//  and then none ever again.
				if (prev != NULL)
				{
					prev->m_next = item->m_next;
					item->m_next = m_pList;
					m_pList = item;
				}
				return item;
			}

			prev = item;
			item = item->m_next;
		}
		return NULL;  // not found.
	}

	CSharedLibraryItem *m_pList;
};

void *VoidFnPtrLookup_Tier0(const char *libname, const char *fn, void *fallback)
{
	LibraryHandle lib = CSharedLibraryCache::GetCache().GetHandle(libname);
	void *retval = NULL;
	if (lib != NULL)
	{
		retval = LookupInLibraryHandle(lib, fn);
		dbgdynfn("CDynamicFunction: Lookup of '%s' in '%s': %p\n", fn, libname, retval);
	}

	if (retval == NULL)
		retval = fallback;
	return retval;
}

