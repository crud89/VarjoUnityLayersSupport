#include "hook.h"

#include <cstdint>
#include <cstring>

namespace {

    IMAGE_NT_HEADERS64* NtHeaders(uint8_t* base) {
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);

        if (dos->e_magic != IMAGE_DOS_SIGNATURE) 
            return nullptr;
    
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);

        return nt->Signature == IMAGE_NT_SIGNATURE ? nt : nullptr;
    }

}

void** FindIatSlot(HMODULE module, const char* dllName, const char* functionName) {
    auto* base = reinterpret_cast<uint8_t*>(module);
    IMAGE_NT_HEADERS64* nt = NtHeaders(base);
    
    if (!nt) 
        return nullptr;

    auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    
    if (directory.VirtualAddress == 0) 
        return nullptr;

    for (auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress); descriptor->Name; ++descriptor) {
        if (_stricmp(reinterpret_cast<const char*>(base + descriptor->Name), dllName) != 0) 
            continue;
        
        if (descriptor->OriginalFirstThunk == 0) 
            continue;

        auto* names = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + descriptor->OriginalFirstThunk);
        auto* slots = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + descriptor->FirstThunk);

        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal)) 
                continue;

            auto* byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            
            if (std::strcmp(reinterpret_cast<const char*>(byName->Name), functionName) == 0)
                return reinterpret_cast<void**>(&slots->u1.Function);
        }
    }

    return nullptr;
}

bool IsDelayLoaded(HMODULE module, const char* dllName) {
    auto* base = reinterpret_cast<uint8_t*>(module);
    IMAGE_NT_HEADERS64* nt = NtHeaders(base);

    if (!nt) 
        return false;

    const IMAGE_DATA_DIRECTORY& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];

    if (directory.VirtualAddress == 0) 
        return false;

    for (auto* descriptor = reinterpret_cast<IMAGE_DELAYLOAD_DESCRIPTOR*>(base + directory.VirtualAddress); descriptor->DllNameRVA; ++descriptor)
        if (_stricmp(reinterpret_cast<const char*>(base + descriptor->DllNameRVA), dllName) == 0) 
            return true;

    return false;
}

bool WriteIatSlot(void** slot, void* value) {
    DWORD oldProtect = 0;
    
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &oldProtect)) 
        return false;
    
    InterlockedExchangePointer(slot, value);
    VirtualProtect(slot, sizeof(void*), oldProtect, &oldProtect);

    return true;
}
