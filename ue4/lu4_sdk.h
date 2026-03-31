#pragma once

#include <includes.h>

// Estructuras del SDK de Lineage 2 LU4

// Estructura FGameSkill_Data - solo los campos necesarios para extraer el nombre
namespace SDK {
    // Estructura FGameSkill_Data
    // El nombre de la skill está en offset 0x00 como FText (0x18 bytes)
    struct FGameSkill_Data final {
        uint8_t SkillName_FText[0x18];  // FText - primer campo, offset 0x00
        uint8_t Pad_18[0x78];           // Resto de la estructura hasta 0x90 bytes
        
        // Obtener el nombre de la skill como string
        std::string get_skill_name() const;
    };
    static_assert(sizeof(FGameSkill_Data) == 0x90, "FGameSkill_Data size mismatch");

    // Estructura de parámetros para GetSkillDataByID
    struct FL_Data_C_GetSkillDataByID final {
        int32_t ID;
        uint8_t Pad_4[0x4];
        ue4::core_object::u_object* __WorldContext;
        FGameSkill_Data SkillData;
        // padding hasta 0x1E0
        uint8_t Pad_A0[0x140];
    };
    static_assert(sizeof(FL_Data_C_GetSkillDataByID) == 0x1E0, "FL_Data_C_GetSkillDataByID size mismatch");
}

namespace ue4::sdk {
    // Función para obtener el nombre de una skill por su ID
    std::string get_skill_name_by_id(int32_t skill_id);
}
