#pragma once

#include "pch.h"

namespace signature {
	void* resolve_library(const char* name);
	void* resolve_library(u32 address);
	void* resolve_import(void* handle, const char* name);

	u8* find_pattern(const char* module, const char* pattern);
	u8* find_pattern(const char* pattern, uptr start, uptr length);

	template <typename T>
	auto find_pattern(const char* module, const char* pattern, u32 offset) {
		return reinterpret_cast<T>(find_pattern(module, pattern) + offset);
	}
	void* resolve_callgate(void* address);
} // namespace signature
