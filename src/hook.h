#pragma once

#include <Windows.h>

#include <atomic>
#include <type_traits>

/// <summary>
/// Attempts to find a slot in the import address table (IAT) of a module.
/// </summary>
/// <param name="module">The module on which to lookup the IAT.</param>
/// <param name="dllName">The name of the library.</param>
/// <param name="functionName">The name of the function.</param>
/// <returns>A pointer to the slot in the IAT.</returns>
void** FindIatSlot(HMODULE module, const char* dllName, const char* functionName);

/// <summary>
/// Checks if a module delays importing the IAT.
/// </summary>
/// <param name="module">The module to check.</param>
/// <param name="dllName">The name of the library.</param>
/// <returns>`true` if the library is listed in the module's `IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT` data directory and `false` otherwise.</returns>
bool IsDelayLoaded(HMODULE module, const char* dllName);

/// <summary>
/// Overwrites a slot in the import address table (IAT).
/// </summary>
/// <param name="slot">The slot to overwrite.</param>
/// <param name="value">The address to overwrite the slot with.</param>
/// <returns>`true` if the slot could be overwritten and `false` otherwise.</returns>
bool WriteIatSlot(void** slot, void* value);

/// <summary>
/// Patches a slot in the import address table (IAT).
/// </summary>
/// <typeparam name="Fn">The type of hook function.</typeparam>
/// <param name="slot">The slot to patch.</param>
/// <param name="hook">The function to patch the slot with.</param>
/// <param name="original">A reference to a variable to store the original function in.</param>
/// <returns>`true` if the patch was successfully applied and `false` otherwise.</returns>
template <typename Fn>
bool PatchIatSlot(void** slot, std::type_identity_t<Fn> hook, std::atomic<Fn>& original) {
    // Obtain the address of the hook.
    auto hookAddress = reinterpret_cast<void*>(hook);

    if (*slot == hookAddress) 
        return true;

    // Store the original function address.
    original.store(reinterpret_cast<Fn>(*slot), std::memory_order_release);

    // Write the hook address into the slot.
    return WriteIatSlot(slot, hookAddress);
}
