#pragma once

namespace ue4::core_object {
    class u_object;
}

namespace hooks::game_tick {
    // Indice de AActor::Tick en la vtable (UE4 4.26)
    // AActor::Tick es index 0x40 tipicamente
    static constexpr auto index = 0x40;
    
    using fn = void(__thiscall*)(ue4::core_object::u_object*, float);
    inline fn original;
    
    void __stdcall hook(ue4::core_object::u_object* world, float delta_seconds);
    
    // Flag para ejecutar el dump una sola vez
    inline bool should_dump_skills = false;
}
