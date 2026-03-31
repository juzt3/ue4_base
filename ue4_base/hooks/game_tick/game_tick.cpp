#include <includes.h>
#include "game_tick.h"
#include "../hooks.h"
#include "../../ipc/command_queue.h"
#include "../../ipc/protocol.h"

// Función para obtener el PlayerController actual
static ue4::game_framework::a_player_controller* get_local_player_controller() {
    ue4::engine::u_world* world = ue4::engine::world;
    if (!world) return nullptr;

    ue4::engine::u_game_instance* game_instance = world->owning_game_instance;
    if (!game_instance) return nullptr;

    if (game_instance->local_players.size() == 0) return nullptr;
    
    ue4::engine::u_player* local_player = game_instance->local_players[0];
    if (!local_player) return nullptr;

    return local_player->player_controller;
}

// Ejecutar comando MoveToLocation
static ipc::ResultCode execute_move_to_location(const ipc::MoveToLocationPayload& payload) {
    auto* controller = get_local_player_controller();
    if (!controller) {
        return ipc::ResultCode::ERROR_NOT_IN_GAME;
    }

    ue4::math::vector from{ payload.from.x, payload.from.y, payload.from.z };
    ue4::math::vector to{ payload.to.x, payload.to.y, payload.to.z };
    bool is_by_mouse = (payload.is_by_mouse != 0);

    controller->move_to_location(from, to, is_by_mouse);
    return ipc::ResultCode::SUCCESS;
}

// Ejecutar comando UseCommand
static ipc::ResultCode execute_use_command(const ipc::UseCommandPayload& payload) {
    auto* controller = get_local_player_controller();
    if (!controller) {
        return ipc::ResultCode::ERROR_NOT_IN_GAME;
    }

    // Convertir wchar_t a f_string (UE4)
    if (payload.command_length == 0 || payload.command_length >= ipc::MAX_COMMAND_LENGTH) {
        return ipc::ResultCode::ERROR_INVALID_PARAM;
    }

    // Crear f_string desde el comando recibido
    std::wstring cmd_str(payload.command, payload.command_length);
    ue4::containers::f_string cmd_fstring(cmd_str.c_str());

    controller->use_command(cmd_fstring);
    return ipc::ResultCode::SUCCESS;
}

// Ejecutar comando GetPlayerPos
static ipc::ResultCode execute_get_player_pos(ipc::PlayerPositionPayload& out_payload) {
    auto* controller = get_local_player_controller();
    if (!controller || !controller->acknowledged_pawn) {
        out_payload.valid = false;
        return ipc::ResultCode::ERROR_NOT_IN_GAME;
    }

    auto location = controller->acknowledged_pawn->get_location();
    auto rotation = controller->acknowledged_pawn->get_rotation();

    out_payload.position.x = location.x;
    out_payload.position.y = location.y;
    out_payload.position.z = location.z;
    out_payload.rotation.x = rotation.x;
    out_payload.rotation.y = rotation.y;
    out_payload.rotation.z = rotation.z;
    out_payload.valid = true;

    return ipc::ResultCode::SUCCESS;
}

void hooks::game_tick::process_ipc_commands() {
    // Obtener todos los comandos pendientes
    auto commands = ipc::CommandQueue::instance().dequeue_all();

    for (const auto& cmd : commands) {
        ipc::Message response;
        response.header.magic = ipc::MessageHeader::MAGIC;
        response.header.version = ipc::PROTOCOL_VERSION;
        response.header.request_id = cmd.message.header.request_id;
        response.header.type = ipc::CommandType::RESPONSE;
        response.header.payload_size = sizeof(ipc::ResponsePayload);

        switch (cmd.message.header.type) {
        case ipc::CommandType::MOVE_TO_LOCATION:
            response.payload.response.result = execute_move_to_location(
                cmd.message.payload.move_to_location);
            break;

        case ipc::CommandType::USE_COMMAND:
            response.payload.response.result = execute_use_command(
                cmd.message.payload.use_command);
            break;

        case ipc::CommandType::GET_PLAYER_POS:
            response.payload.response.result = execute_get_player_pos(
                response.payload.player_pos);
            if (response.payload.response.result == ipc::ResultCode::SUCCESS) {
                response.header.payload_size = sizeof(ipc::PlayerPositionPayload);
            }
            break;

        default:
            response.payload.response.result = ipc::ResultCode::ERROR_UNKNOWN_COMMAND;
            break;
        }

        // Enviar respuesta a través del callback
        if (cmd.response_callback) {
            cmd.response_callback(response);
        }
    }
}

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

    // Procesar comandos IPC pendientes
    process_ipc_commands();
    
    // Llamar al original
    hooks::game_tick::original(world, delta_seconds);
}
