#pragma once

namespace utils {
	std::uintptr_t pattern_scan(std::uintptr_t address, const char* signature, bool relative = false);
	
	// Imprimir bytes de assembly de una funcion
	void print_assembly(void* func, int bytes = 32);
	
	// Analizar si una funcion parece retornar bool
	bool analyze_function_returns_bool(void* func);
	
	// Escanear vtable buscando candidatos a funciones bool
	// Retorna el indice del primer candidato encontrado, o -1 si no hay ninguno
	int find_bool_function_in_vtable(void** vtable, int start_index = 0x50, int max_index = 0x100);
}

namespace utils::console {
	bool initialize(const std::string& title);
	void release();
}
