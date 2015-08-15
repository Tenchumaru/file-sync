#pragma once

#ifdef _DEBUG
#	ifndef DEBUG
#		define DEBUG
#	endif
#elif defined(DEBUG)
#	error inconsistent DEBUG directives
#elif !defined(NDEBUG)
#	define NDEBUG
#endif
#ifndef STRICT
#	define STRICT
#endif
#include "targetver.h"

// Enable allocation tracking.
#include <cstdlib>
#define _CRT_MAP_ALLOC

// Standard library include directives
#pragma warning(push)
#	pragma warning(disable: 4702)
#include <algorithm>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>
#pragma warning(pop)

#ifdef _UNICODE
typedef std::wifstream tifstream;
typedef std::wistream tistream;
typedef std::wofstream tofstream;
typedef std::wostream tostream;
typedef std::wstreambuf tstreambuf;
typedef std::wstring tstring;
typedef std::wstringstream tstringstream;
#else
typedef std::ifstream tifstream;
typedef std::istream tistream;
typedef std::ofstream tofstream;
typedef std::ostream tostream;
typedef std::streambuf tstreambuf;
typedef std::string tstring;
typedef std::stringstream tstringstream;
#endif

// Windows Header Files:
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <process.h>
#include <tchar.h>
#include <strsafe.h> // SDK; must appear after tchar.h

// CRT debugging
#include <crtdbg.h>
#ifdef _DEBUG
inline void* operator new(size_t size, char const* fileName, int line)
{
	void* p= ::operator new(size, _NORMAL_BLOCK, fileName, line);
	if(p == NULL)
		throw std::bad_alloc();
	return p;
}
inline void* operator new[](size_t size, char const* fileName, int line)
{
	void* p= ::operator new[](size, _NORMAL_BLOCK, fileName, line);
	if(p == NULL)
		throw std::bad_alloc();
	return p;
}
inline void operator delete(void* p, char const* fileName, int line)
{
	::operator delete(p, _NORMAL_BLOCK, fileName, line);
}
inline void __cdecl operator delete[](void* p, char const* fileName, int line)
{
	::operator delete[](p, _NORMAL_BLOCK, fileName, line);
}
#	define DEBUG_NEW new(__FILE__, __LINE__)
#	define new DEBUG_NEW
#	define _VERIFYE _ASSERTE
#else
#	define _VERIFYE(expr) ((void)(expr))
#endif
#define ASSERT _ASSERTE
#define VERIFY _VERIFYE

// See afximpl.h or afxisapi.h.
#ifndef _countof
#	define _countof(array) (sizeof(array) / sizeof((array)[0]))
#endif

// See winnt.h.
#ifndef C_ASSERT
#	define C_ASSERT(expr) typedef char __C_ASSERT__[(expr) ? 1 : -1]
#endif

// Use std::max and std::min instead.
#undef max
#undef min

#if defined(_WIN32)
#	define DwordToPtr(dw) (reinterpret_cast<void*>(static_cast<ULONG_PTR>(dw)))
#	define PtrToDword(p) (static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(p)))
#elif define(_WIN64)
#	error no definition
#else
#	error no support
#endif

// Utility functions
template<typename PtrT>
inline void AlignOn4ByteBoundary(PtrT& p)
{
	ULONG_PTR ul= reinterpret_cast<ULONG_PTR>(p);
	ul= (ul + 3) & ~3;
	p= reinterpret_cast<PtrT>(ul);
}

template<typename T>
T const* GetResource(int id, LPCTSTR type, HMODULE module= GetModuleHandle(NULL))
{
	HRSRC resourceInformation= FindResource(module, MAKEINTRESOURCE(id), type);
	if(resourceInformation == NULL)
		return NULL;
	return static_cast<T const*>(LockResource(LoadResource(module, resourceInformation)));
}

template<typename T>
T const* GetResource(int id, UINT type, HMODULE module= GetModuleHandle(NULL))
{
	return GetResource<T>(id, MAKEINTRESOURCE(type), module);
}

#define DECLARE_CLOSE_POLICY(name_,handle_type_,close_function_) \
	struct name_ { typedef handle_type_ handle_type; \
	void operator()(__in handle_type handle_) throw() { (close_function_)(handle_); } }

// The () operator of close_policy must not throw.
template<typename close_policy>
class auto_handle
{
public:
	typedef typename close_policy::handle_type handle_type;

	auto_handle() throw() : handle() {}
	auto_handle(__in handle_type handle) throw() : handle(handle) {}
	~auto_handle() throw() { close_policy()(handle); }
	operator handle_type() const throw() { return handle; }
	auto_handle& operator=(__in handle_type handle) throw() { close_policy()(this->handle); this->handle= handle; return *this; }
	handle_type operator->() const throw() { return handle; }
	handle_type* operator&() throw() { ASSERT(handle == handle_type()); return &handle; }
	handle_type release() throw() { handle_type rv= handle; handle= handle_type(); return rv; }
	void reset() throw() { close_policy()(handle); handle= handle_type(); }

private:
	handle_type handle;

	auto_handle(auto_handle const&); // undefined
	auto_handle& operator=(auto_handle const&); // undefined
};

// This functor deletes objects.  Do not use this as a close policy for
// auto_handle; use std::auto_ptr.
struct Deleter
{
	template<typename T> void operator()(T* p) const { delete p; }
};

// This functor converts a T const* to a T const&.
struct Dereference
{
	template<typename T>
	T const& operator()(T const* p) const { return *p; }
};

// This functor compares dereferences.
struct DereferenceLess
{
	template<typename PtrT>
	bool operator()(PtrT p1, PtrT p2) const { return *p1 < *p2; }
};

// Extensions to the standard library
namespace xstd
{
	class win32_error : public std::runtime_error
	{
	public:
		win32_error(DWORD error= ::GetLastError());
		DWORD value;
	private:
		static std::string format_error(DWORD error);
	};

	__declspec(noreturn) void throw_last_error(DWORD error= GetLastError()) throw(...);
	void formatv(tstring& s, __format_string LPCTSTR formatString, __in va_list arguments) throw(...);
	void format(tstring& s, __format_string LPCTSTR formatString, ...) throw(...);
#ifdef _UNICODE
	// Set this before using the load functions without specifying the module.
	extern HMODULE defaultResourceModule;

	tstring& load(tstring& s, UINT id, __in HMODULE module= defaultResourceModule) throw(...);
	tstring load(UINT id, __in HMODULE module= defaultResourceModule) throw(...);
	// This method still might throw exceptions such as std::bad_alloc.
	bool try_load(tstring& s, UINT id, __in HMODULE module= defaultResourceModule);
#endif
	tstring& trim(tstring& s);
}

class CCriticalSection
{
public:
	CCriticalSection() throw() { ::InitializeCriticalSection(&cs); }
	~CCriticalSection() throw() { ::DeleteCriticalSection(&cs); }

	void Enter() throw() { ::EnterCriticalSection(&cs); }
	void Leave() throw() { ::LeaveCriticalSection(&cs); }

	class CScope
	{
	public:
		CScope(CCriticalSection& cs) throw() : cs(cs) { cs.Enter(); }
		~CScope() throw() { cs.Leave(); }

	private:
		CCriticalSection& cs;

		CScope(); // undefined
		CScope(CScope const&); // undefined
		CScope& operator=(CScope const&); // undefined
	};

private:
	CRITICAL_SECTION cs;

	CCriticalSection(CCriticalSection const&); // undefined
	CCriticalSection& operator=(CCriticalSection const&); // undefined
};
