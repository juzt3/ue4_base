#pragma once

namespace ue4::core_object {
    class u_object;
}

namespace hooks::window_focus {
    // Configuracion manual del hook
    // Modifica este indice segun el resultado de utils::find_bool_function_in_vtable()
    static constexpr auto index = 0x70;  // Cambiar al indice correcto encontrado
    
    // Tipo de la funcion original - ajustar segun lo que retorne la funcion (bool, float, void, etc.)
    using fn = bool(__thiscall*)(ue4::core_object::u_object* viewport_client);
    inline fn original;
    
    // Hook function - ajustar el tipo de retorno segun corresponda
    bool __stdcall hook(ue4::core_object::u_object* viewport_client);
    
    // Variable global para trackear el estado del foco
    inline bool has_focus = true;
    
    // Callback opcional cuando cambia el estado del foco
    inline void (*on_focus_changed)(bool has_focus_now) = nullptr;
}
