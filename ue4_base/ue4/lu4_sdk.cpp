#include <includes.h>
#include "lu4_sdk.h"
#include <fstream>
#include <iostream>

// Estructura FText interna para extraer el string
namespace {
    // FTextData contiene el string en offset 0x28
    struct FTextData {
        uint8_t Pad_0[0x28];
        ue4::containers::f_string TextSource;
    };

    // FText tiene un puntero a FTextData en offset 0x00
    struct FText_Internal {
        FTextData* TextData;
        uint8_t Pad_8[0x10];

        std::string to_string() const {
            if (!TextData) return "";
            return TextData->TextSource.to_string();
        }
    };
}

std::string SDK::FGameSkill_Data::get_skill_name() const {
    // El nombre está en offset 0x00 como FText (0x18 bytes)
    const auto* text = reinterpret_cast<const FText_Internal*>(SkillName_FText);
    return text->to_string();
}

std::string ue4::sdk::get_skill_name_by_id(int32_t skill_id) {
    // 1. Buscar el objeto FL_Data_C
    static ue4::core_object::u_object* fl_data_c = nullptr;
    static ue4::core_object::u_object* get_skill_data_fn = nullptr;

    if (!fl_data_c) {
        // Recorrer todos los objetos para encontrar FL_Data_C
        auto* objects = ue4::core_object::objects;
        if (!objects) return "";

        for (std::uint32_t i = 0; i < objects->num_elements; i++) {
            auto* obj = objects->get(i);
            if (!obj) continue;

            std::string name = obj->get_name();
            if (name.find("FL_Data_C") != std::string::npos) {
                fl_data_c = obj;
                break;
            }
        }

        if (!fl_data_c) return "";
    }

    // 2. Buscar la función GetSkillDataByID
    if (!get_skill_data_fn) {
        // Usar find en la clase del objeto
        auto* obj_class = fl_data_c->_class;
        if (!obj_class) return "";

        // Buscar la función en la clase
        // Necesitamos buscar "Function FL_Data.FL_Data_C.GetSkillDataByID"
        for (std::uint32_t i = 0; i < ue4::core_object::objects->num_elements; i++) {
            auto* obj = ue4::core_object::objects->get(i);
            if (!obj) continue;

            std::string full_name = obj->get_full_name();
            if (full_name.find("Function FL_Data.FL_Data_C.GetSkillDataByID") != std::string::npos) {
                get_skill_data_fn = obj;
                break;
            }
        }

        if (!get_skill_data_fn) return "";
    }

    // 3. Preparar parámetros
    SDK::FL_Data_C_GetSkillDataByID params{};
    params.ID = skill_id;
    
    // Obtener el World Context - necesitamos el GameInstance o similar
    // Por ahora usamos el CDO (Class Default Object) como contexto
    params.__WorldContext = fl_data_c;

    // 4. Llamar ProcessEvent
    process_event(fl_data_c, get_skill_data_fn, &params);

    // 5. Extraer el nombre de la skill
    return params.SkillData.get_skill_name();
}

void ue4::sdk::dump_skills_to_file(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[dump_skills] Error: No se pudo abrir el archivo " << filename << std::endl;
        return;
    }

    std::cout << "[dump_skills] Iniciando dump de skills 0-20002..." << std::endl;

    const int32_t max_skill_id = 391;
    int found_count = 0;

    for (int32_t i = 1; i <= max_skill_id; i++) {
        std::string skill_name = get_skill_name_by_id(i);
        
        // Solo guardar si tiene nombre (no vacío)
        if (!skill_name.empty()) {
            if (skill_name.compare("Unk") == 0)
                continue;
            file << i << " " << skill_name << std::endl;
            found_count++;
        }

        // Progreso cada 1000 skills
        if (i % 1000 == 0) {
            std::cout << "[dump_skills] Progreso: " << i << "/" << max_skill_id 
                      << " (encontradas: " << found_count << ")" << std::endl;
        }
    }

    file.close();
    std::cout << "[dump_skills] Completado! Total de skills encontradas: " << found_count << std::endl;
    std::cout << "[dump_skills] Archivo guardado: " << filename << std::endl;
}
