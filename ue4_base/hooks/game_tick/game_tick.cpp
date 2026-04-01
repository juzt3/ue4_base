#include <includes.h>
#include "game_tick.h"
#include "../hooks.h"
#include "../../ipc/command_queue.h"
#include "../../ipc/protocol.h"
#include <fstream>
#include <ctime>

// Logger simple a archivo
static void ipc_log(const std::string& msg) {
    std::ofstream log("ipc_debug.log", std::ios::app);
    if (log.is_open()) {
        // Timestamp
        time_t now = time(0);
        tm* ltm = localtime(&now);
        log << "[" << ltm->tm_hour << ":" << ltm->tm_min << ":" << ltm->tm_sec << "] ";
        log << msg << std::endl;
        log.flush();
        log.close();
    }
}

// Función para obtener el PlayerController actual
static ue4::game_framework::a_player_controller* get_local_player_controller() {
    const auto world = *reinterpret_cast<decltype(ue4::engine::world)*>(ue4::engine::world);
	if (!world) nullptr;

	const auto local_player = world->owning_game_instance->local_players[0];
	if (!local_player) return nullptr;

	const auto player_controller = local_player->player_controller;
	if (!player_controller) return nullptr;

	return local_player->player_controller;
}

// Función interna sin objetos C++ para usar con SEH
static void call_move_to_location(ue4::game_framework::a_player_controller* controller, 
                                   const ue4::math::vector& from, 
                                   const ue4::math::vector& to, 
                                   bool is_by_mouse) {
    controller->move_to_location(from, to, is_by_mouse);
}

// Ejecutar comando MoveToLocation
static ipc::ResultCode execute_move_to_location(const ipc::MoveToLocationPayload& payload) {
    std::cout << "[IPC] Ejecutando move_to_location..." << std::endl;
    
    auto* controller = get_local_player_controller();
    if (!controller) {
        std::cout << "[IPC] Error: No hay controller" << std::endl;
        return ipc::ResultCode::ERROR_NOT_IN_GAME;
    }

    // Validar puntero
    if (IsBadReadPtr(controller, sizeof(void*))) {
        std::cout << "[IPC] Error: Controller pointer inválido" << std::endl;
        return ipc::ResultCode::ERROR_NOT_IN_GAME;
    }

    // Verificar acknowledged_pawn
    if (!controller->acknowledged_pawn) {
        std::cout << "[IPC] Error: No hay pawn" << std::endl;
        return ipc::ResultCode::ERROR_NOT_IN_GAME;
    }

    ue4::math::vector from{ payload.from.x, payload.from.y, payload.from.z };
    ue4::math::vector to{ payload.to.x, payload.to.y, payload.to.z };
    bool is_by_mouse = (payload.is_by_mouse != 0);

    std::cout << "[IPC] Moviendo desde (" << from.x << ", " << from.y << ", " << from.z << ") ";
    std::cout << "hacia (" << to.x << ", " << to.y << ", " << to.z << ")" << std::endl;

    call_move_to_location(controller, from, to, is_by_mouse);
    return ipc::ResultCode::SUCCESS;
    
}

// Función interna sin objetos C++ para usar con SEH
static void call_use_command(ue4::game_framework::a_player_controller* controller,
                              const ue4::containers::f_string& cmd) {
    controller->use_command(cmd);
}

// Ejecutar comando UseCommand
static ipc::ResultCode execute_use_command(const ipc::UseCommandPayload& payload) {
    std::cout << "[IPC] Ejecutando use_command..." << std::endl;
    
    // Validar el payload primero
    if (payload.command_length == 0 || payload.command_length >= ipc::MAX_COMMAND_LENGTH) {
        std::cout << "[IPC] Error: Command length inválido: " << payload.command_length << std::endl;
        return ipc::ResultCode::ERROR_INVALID_PARAM;
    }

    // Verificar que el buffer del comando sea válido
    if (IsBadReadPtr(payload.command, payload.command_length * sizeof(wchar_t))) {
        std::cout << "[IPC] Error: Command buffer inválido" << std::endl;
        return ipc::ResultCode::ERROR_INVALID_PARAM;
    }

    // Asegurar que el comando esté null-terminated
    if (payload.command[payload.command_length] != L'\0') {
        std::cout << "[IPC] Error: Command no está null-terminated" << std::endl;
        return ipc::ResultCode::ERROR_INVALID_PARAM;
    }
    
    auto* controller = get_local_player_controller();
    if (!controller) {
        std::cout << "[IPC] Error: No hay controller" << std::endl;
        return ipc::ResultCode::ERROR_NOT_IN_GAME;
    }

    // Validar puntero
    if (IsBadReadPtr(controller, sizeof(void*))) {
        std::cout << "[IPC] Error: Controller pointer inválido" << std::endl;
        return ipc::ResultCode::ERROR_NOT_IN_GAME;
    }

    // Crear f_string desde el comando recibido
    std::wcout << L"[IPC] Comando (raw): " << payload.command << std::endl;
    std::wstring cmd_str(payload.command, payload.command_length);
    std::wcout << L"[IPC] Comando (wstring): " << cmd_str << std::endl;
    
    ue4::containers::f_string cmd_fstring(cmd_str.c_str());

    call_use_command(controller, cmd_fstring);
    std::cout << "[IPC] use_command completado" << std::endl;
    return ipc::ResultCode::SUCCESS;
}

// Ejecutar comando GetPlayerPos
static ipc::ResultCode execute_get_player_pos(ipc::PlayerPositionPayload& out_payload) {
    auto* controller = get_local_player_controller();
    if (!controller) {
        out_payload.valid = false;
        return ipc::ResultCode::ERROR_NOT_IN_GAME;
    }

    // Validar controller
    if (IsBadReadPtr(controller, sizeof(void*))) {
        out_payload.valid = false;
        return ipc::ResultCode::ERROR_NOT_IN_GAME;
    }

    if (!controller->acknowledged_pawn) {
        out_payload.valid = false;
        return ipc::ResultCode::ERROR_NOT_IN_GAME;
    }

    // Validar pawn
    if (IsBadReadPtr(controller->acknowledged_pawn, sizeof(void*))) {
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

    if (commands.size() > 0)
        std::cout << "[IPC] Prcesando " << commands.size() << " comandos" << std::endl;

    for (const auto& cmd : commands) {
        std::cout << "[IPC] Procesando comando tipo: " << static_cast<int>(cmd.message.header.type) << std::endl;
        
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

        // Encolar respuesta para que el hilo del servidor la envíe
        ipc::CommandQueue::instance().enqueue_response(response, cmd.response_pipe);
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
