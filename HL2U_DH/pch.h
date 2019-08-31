// pch.h: это предварительно скомпилированный заголовочный файл.
// Перечисленные ниже файлы компилируются только один раз, что ускоряет последующие сборки.
// Это также влияет на работу IntelliSense, включая многие функции просмотра и завершения кода.
// Однако изменение любого из приведенных здесь файлов между операциями сборки приведет к повторной компиляции всех(!) этих файлов.
// Не добавляйте сюда файлы, которые планируете часто изменять, так как в этом случае выигрыша в производительности не будет.

#pragma once

// Добавьте сюда заголовочные файлы для предварительной компиляции
//#include "framework.h"

#include <cassert>

#include <cstdint>

using u8   = uint8_t;
using u16  = uint16_t;
using u32  = uint32_t;
using u64  = uint64_t;
using uptr = uintptr_t;

using i8   = int8_t;
using i16  = int16_t;
using i32  = int32_t;
using i64  = int64_t;
using iptr = intptr_t;

namespace hex {
	static constexpr auto in_range(char x, char a, char b) {
		return (x >= a && x <= b);
	}

	static constexpr auto get_bits(char x) {
		if (in_range((x & (~0x20)), 'A', 'F')) {
			return (x & (~0x20)) - 'A' + 0xa;
		}
		else if (in_range(x, '0', '9')) {
			return x - '0';
		}
		else {
			return 0;
		}
	}

	static constexpr auto byte(const char* x) {
		return get_bits(x[0]) << 4 | get_bits(x[1]);
	}
	static constexpr auto word(const char* x) {
		return byte(x) << 8 | byte(x + 2);
	}
	static constexpr auto dword(const char* x) {
		return word(x) << 16 | word(x + 4);
	}
} // namespace hex

#include "vfunc.hh"
#include "hooks.hh"
#include "signature.hh"

#include "sdk.hh"