#pragma once

#include <windows.h>
#include "ntddk.h"

#include "peconv/peb_lookup.h"
#include <iostream>

class PEBFastLocker {
public:
    PEBFastLocker(PEB &_peb)
        : peb(_peb)
    {
        RtlEnterCriticalSection(peb.FastPebLock);
    }

    ~PEBFastLocker()
    {
        RtlLeaveCriticalSection(peb.FastPebLock);
    }

protected:
    PEB &peb;
};

class PEBLoaderLocker {
public:
    PEBLoaderLocker(PEB &_peb)
        : peb(_peb)
    {
        RtlEnterCriticalSection(peb.LoaderLock);
    }

    ~PEBLoaderLocker()
    {
        RtlLeaveCriticalSection(peb.LoaderLock);
}

protected:
    PEB & peb;
};


//here we don't want to use any functions imported form extenal modules

typedef struct _LDR_MODULE { 
    LIST_ENTRY  InLoadOrderModuleList;//   +0x00 
    LIST_ENTRY  InMemoryOrderModuleList;// +0x08   
    LIST_ENTRY  InInitializationOrderModuleList;// +0x10 
    void*   BaseAddress; // +0x18 
    void*   EntryPoint;  // +0x1c 
    ULONG   SizeOfImage; 
    UNICODE_STRING FullDllName; 
    UNICODE_STRING BaseDllName; 
    ULONG   Flags; 
    SHORT   LoadCount; 
    SHORT   TlsIndex; 
    HANDLE  SectionHandle; 
    ULONG   CheckSum; 
    ULONG   TimeDateStamp; 
} LDR_MODULE, *PLDR_MODULE;

inline PPEB get_peb()
{
#if defined(_WIN64)
    return (PPEB)__readgsqword(0x60);
#else
    return (PPEB)__readfsdword(0x30);
/*
//alternative way to fetch it:
    LPVOID PEB = NULL;
    __asm {
        mov eax, fs:[30h]
        mov PEB, eax
    };
    return (PPEB)PEB;

    or:
    LPVOID PEB = RtlGetCurrentPeb();
*/
#endif
}

inline WCHAR to_lowercase(WCHAR c1)
{
    if (c1 <= L'Z' && c1 >= L'A') {
        c1 = (c1 - L'A') + L'a';
    }
    return c1;
}

bool is_wanted_module(LPWSTR curr_name, LPWSTR wanted_name)
{
    if (wanted_name == NULL || curr_name == NULL) return false;

    WCHAR *curr_end_ptr = curr_name;
    while (*curr_end_ptr != L'\0') {
        curr_end_ptr++;
    }
    if (curr_end_ptr == curr_name) return false;

    WCHAR *wanted_end_ptr = wanted_name;
    while (*wanted_end_ptr != L'\0') {
        wanted_end_ptr++;
    }
    if (wanted_end_ptr == wanted_name) return false;

    while ((curr_end_ptr != curr_name) && (wanted_end_ptr != wanted_name)) {

        if (to_lowercase(*wanted_end_ptr) != to_lowercase(*curr_end_ptr)) {
            return false;
        }
        wanted_end_ptr--;
        curr_end_ptr--;
    }
    return true;
}

HMODULE peconv::get_module_via_peb(IN OPTIONAL LPWSTR module_name)
{
    PPEB peb = get_peb();
    if (!peb) {
        return NULL;
    }
    PEBLoaderLocker locker(*peb);
    LIST_ENTRY head = peb->Ldr->InLoadOrderModuleList;

    const PLDR_MODULE first_module = *((PLDR_MODULE *)(&head));
    PLDR_MODULE curr_module = first_module;
    if (!module_name) {
        return (HMODULE)(curr_module->BaseAddress);
    }
    while (curr_module != NULL && curr_module->BaseAddress != NULL) {
        if (is_wanted_module(curr_module->BaseDllName.Buffer, module_name)) {
            return (HMODULE)(curr_module->BaseAddress);
        }
        curr_module = (PLDR_MODULE)curr_module->InLoadOrderModuleList.Flink;
    }
    return NULL;
}

size_t peconv::get_module_size_via_peb(IN OPTIONAL HMODULE hModule)
{
    PPEB peb = get_peb();
    if (!peb) {
        return NULL;
    }
    PEBLoaderLocker locker(*peb);
    LIST_ENTRY list = peb->Ldr->InLoadOrderModuleList;

    PLDR_MODULE curr_module = *((PLDR_MODULE *)(&list));
    if (!hModule) {
        return (size_t)(curr_module->SizeOfImage);
    }
    while (curr_module != NULL && curr_module->BaseAddress != NULL) {
        if (hModule == (HMODULE)(curr_module->BaseAddress)) {
            return (size_t)(curr_module->SizeOfImage);
        }
        curr_module = (PLDR_MODULE)curr_module->InLoadOrderModuleList.Flink;
    }
    return 0;
}

bool peconv::set_main_module_in_peb(HMODULE module_ptr)
{
    PPEB peb = get_peb();
    if (peb == NULL) {
        return false;
    }
    PEBFastLocker locker(*peb);
    peb->ImageBaseAddress = module_ptr;
    return true;
}

HMODULE peconv::get_main_module_via_peb()
{
    PPEB peb = get_peb();
    if (peb == NULL) {
        return NULL;
    }
    PEBFastLocker locker(*peb);
    return (HMODULE) peb->ImageBaseAddress;
}
