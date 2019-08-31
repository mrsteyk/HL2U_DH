#include "pch.h"

#include "signature.hh"


using namespace signature;
using namespace hex;

static u8* find_pattern_internal(uptr start, uptr end, const char* pattern) {
	u8* first = 0;

	const char* pat = pattern;

	for (uptr cur = start; cur < end; cur += 1) {
		if (!*pat) return first;
		if (*(u8*)pat == '\?' || *(u8*)cur == byte(pat)) {
			if (first == 0) first = reinterpret_cast<u8*>(cur);
			if (!pat[2]) return first;
			if (*reinterpret_cast<const u16*>(pat) == '\?\?' ||
				*reinterpret_cast<const u8*>(pat) != '\?')
				pat += 3;
			else
				pat += 2;
		}
		else {
			pat = pattern;
			first = 0;
		}
	}
	return nullptr;
}

static std::pair<uptr, uptr> find_module_code_section(const char* module_name) {
	auto module = resolve_library(module_name);

	auto module_addr = reinterpret_cast<uptr>(module);
	auto dos_header = (IMAGE_DOS_HEADER*)module;
	auto nt_header = (IMAGE_NT_HEADERS32*)(((u32)module) + dos_header->e_lfanew);

	return std::make_pair(module_addr + nt_header->OptionalHeader.BaseOfCode,
		module_addr + nt_header->OptionalHeader.SizeOfCode);
	// this means that you need to implement your platform
	assert(0);
}

void* signature::resolve_library(const char* name) {
	// TODO: actually check directories for this dll instead of
	// letting the loader do the work
	char buffer[1024];
	snprintf(buffer, 1023, "%s.dll", name);

	auto handle = GetModuleHandleA(buffer);

	// TODO: this is commented out to mimic linux behaviour
	// TODO: do we really want this??
	//assert(handle);

	return handle;

	assert(0);

	return nullptr;
}
void* signature::resolve_library(u32 address) {
	HMODULE h;
	if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCTSTR)address, &h)) {
		return h;
	}
	return nullptr;
}
void* signature::resolve_import(void* handle, const char* name) {
	return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
}

u8* signature::find_pattern(const char* module_name, const char* pattern) {
	auto code_section = find_module_code_section(module_name);

	return find_pattern_internal(code_section.first, code_section.second, pattern);
}

u8* signature::find_pattern(const char* pattern, uptr start, uptr length) {
	return find_pattern_internal(start, start + length, pattern);
}

void* signature::resolve_callgate(void* address) {
	// TODO: are these the only instructions here?
	assert(reinterpret_cast<i8*>(address)[0] == '\xE8' || reinterpret_cast<i8*>(address)[0] == '\xE9');

	return reinterpret_cast<void*>(*reinterpret_cast<uptr*>(
		(reinterpret_cast<uptr>(address) + 1)) +
		(reinterpret_cast<uptr>(address) + 5));
}
