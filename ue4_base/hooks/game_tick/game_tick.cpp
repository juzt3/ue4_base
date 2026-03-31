#include <includes.h>
#include "game_tick.h"
#include "../hooks.h"

void __stdcall hooks::game_tick::hook(ue4::core_object::u_object* world, float delta_seconds) {    
    // Ejecutar el dump de skills si se solicito
    if (hooks::game_tick::should_dump_skills) {
        std::cout << "[game_tick] Flag should_dump_skills detectada! Ejecutando dump..." << std::endl;
        
        // Resetear flag para ejecutar solo una vez
        hooks::game_tick::should_dump_skills = false;
        
        // Llamar al dump - estamos en el game thread, es seguro
        ue4::sdk::dump_skills_to_file("skills_dump.txt");
        
        std::cout << "[game_tick] Dump completado!" << std::endl;
    }
    
    // Llamar al original
    hooks::game_tick::original(world, delta_seconds);
}
