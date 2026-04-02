#include <includes.h>
#include "utils.h"

std::uintptr_t utils::pattern_scan(const std::uintptr_t address, const char* signature, const bool relative) {
	static auto pattern_to_byte = [](const char* pattern) {
		auto bytes = std::vector<int>{};
		const auto start = const_cast<char*>(pattern);
		const auto end = const_cast<char*>(pattern) + LI_FN(strlen)(pattern);

		for (auto current = start; current < end; ++current) {
			if (*current == '?') {
				++current;
				bytes.push_back(-1);
			} else {
				bytes.push_back(LI_FN(strtoul)(current, &current, 16));
			}
		}
		return bytes;
	};

	const auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(address);
	const auto nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<std::uint8_t*>(address) + dos_header->e_lfanew);

	const auto size_of_image = nt_headers->OptionalHeader.SizeOfImage;
	const auto pattern_bytes = pattern_to_byte(signature);
	const auto scan_bytes = reinterpret_cast<std::uint8_t*>(address);

	const auto s = pattern_bytes.size();
	const auto d = pattern_bytes.data();

	for (auto i = 0ul; i < size_of_image - s; ++i) {
		bool found = true;
		for (auto j = 0ul; j < s; ++j) {
			if (scan_bytes[i + j] != d[j] && d[j] != -1) {
				found = false;
				break;
			}
		}

		if (found) {
			const auto offset = *reinterpret_cast<int*>(&scan_bytes[i] + 3);
			return relative ? reinterpret_cast<std::uintptr_t>(&scan_bytes[i]) + offset + 7 : reinterpret_cast<std::uintptr_t>(&scan_bytes[i]);
		}
	}

	return 0;
}

bool utils::console::initialize(const std::string& title) {
	AllocConsole();

	freopen_s(reinterpret_cast<_iobuf**>(__acrt_iob_func(0)), _("conin$"), _("r"), __acrt_iob_func(0));
	freopen_s(reinterpret_cast<_iobuf**>(__acrt_iob_func(1)), _("conout$"), _("w"), __acrt_iob_func(1));
	freopen_s(reinterpret_cast<_iobuf**>(__acrt_iob_func(2)), _("conout$"), _("w"), __acrt_iob_func(2));


	SetConsoleTitleA(title.c_str());

	return true;
}

void utils::console::release() {
	fclose(__acrt_iob_func(0));
	fclose(__acrt_iob_func(1));
	fclose(__acrt_iob_func(2));

	FreeConsole();
}

// ============================================================================
// Funciones de analisis de assembly
// ============================================================================

void utils::print_assembly(void* func, int bytes) {
	if (!func || IsBadCodePtr(reinterpret_cast<FARPROC>(func))) {
		std::cout << "[print_assembly] Funcion invalida" << std::endl;
		return;
	}
	
	std::cout << "[print_assembly] Assembly en " << func << ":" << std::endl;
	std::cout << std::hex;
	
	unsigned char* ptr = reinterpret_cast<unsigned char*>(func);
	for (int i = 0; i < bytes; i++) {
		if (i % 16 == 0) {
			std::cout << std::endl << "  ";
		}
		std::cout << std::setw(2) << std::setfill('0') << (int)ptr[i] << " ";
	}
	
	std::cout << std::dec << std::endl;
}

bool utils::analyze_function_returns_bool(void* func) {
	if (!func || IsBadCodePtr(reinterpret_cast<FARPROC>(func))) {
		return false;
	}
	
	unsigned char* ptr = reinterpret_cast<unsigned char*>(func);
	bool found_bool_pattern = false;
	
	// Verificar los primeros 64 bytes buscando patrones de retorno bool
	for (int i = 0; i < 64; i++) {
		// Patron: mov al, 0 (B0 00) o mov al, 1 (B0 01)
		if (ptr[i] == 0xB0 && (ptr[i+1] == 0x00 || ptr[i+1] == 0x01)) {
			found_bool_pattern = true;
		}
		// Patron: sete al / setne al / setnz al (0F 90-9F C0)
		else if (ptr[i] == 0x0F && (ptr[i+1] >= 0x90 && ptr[i+1] <= 0x9F) && ptr[i+2] == 0xC0) {
			found_bool_pattern = true;
		}
		// Patron: xor al, al (32 C0)
		else if (ptr[i] == 0x32 && ptr[i+1] == 0xC0) {
			found_bool_pattern = true;
		}
		// Patron: test al, al / ret (84 C0 ... C3)
		// ya capturado por los anteriores
		
		// Verificar retorno
		if (ptr[i] == 0xC3 && found_bool_pattern) {
			return true;
		}
	}
	
	return false;
}

int utils::find_bool_function_in_vtable(void** vtable, int start_index, int max_index) {
	std::cout << "[find_bool_function] Buscando funciones bool en vtable..." << std::endl;
	
	for (int i = start_index; i < max_index && i < 0x200; i++) {
		void* func = vtable[i];
		if (!func) continue;
		
		if (IsBadCodePtr(reinterpret_cast<FARPROC>(func))) continue;
		
		// Verificar si parece retornar bool
		if (analyze_function_returns_bool(func)) {
			std::cout << "  [CANDIDATO] vtable[0x" << std::hex << i << "] = " << func << std::dec;
			std::cout << " (POSIBLE BOOL)" << std::endl;
			return i; // Retornar el primero encontrado
		}
	}
	
	return -1; // No encontrado
}
