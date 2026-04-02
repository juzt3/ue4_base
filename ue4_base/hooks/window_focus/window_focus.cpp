#include <includes.h>
#include "window_focus.h"
#include "../hooks.h"

// Contador de llamadas para debug
inline int call_count = 0;

// Hook para deteccion de foco
// NOTA: Configurar manualmente el indice en window_focus.h
// Usar utils::print_assembly() y utils::find_bool_function_in_vtable() para encontrar la funcion correcta
bool __stdcall hooks::window_focus::hook(ue4::core_object::u_object* viewport_client) {
    // Llamar a la funcion original
    bool result = hooks::window_focus::original(viewport_client);
    
    call_count++;
    
    // Log cada 60 llamadas
    if (call_count % 60 == 0) {
        std::cout << "[window_focus] result=" << result 
                  << " count=" << call_count << std::endl;
    }
    
    // Detectar cambios
    if (result != hooks::window_focus::has_focus) {
        hooks::window_focus::has_focus = result;
        
        std::cout << "[window_focus] Cambio! focus=" 
                  << (result ? "TIENE" : "PERDIO") << std::endl;
        
        if (on_focus_changed != nullptr) {
            on_focus_changed(result);
        }
    }
    
    return result;
}
