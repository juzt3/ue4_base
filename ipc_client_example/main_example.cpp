// Ejemplo de uso del cliente IPC
// Compilar como programa independiente (no como DLL)
//
// Para compilar con MSVC:
// cl main_example.cpp ipc_client.cpp /EHsc /Fe:ipc_client_example.exe
//
// O con g++:
// g++ -std=c++17 main_example.cpp ipc_client.cpp -o ipc_client_example.exe

#include "ipc_client.h"
#include <iostream>
#include <string>

void print_usage() {
    std::cout << "Uso: ipc_client_example.exe <comando> [argumentos]" << std::endl;
    std::cout << std::endl;
    std::cout << "Comandos disponibles:" << std::endl;
    std::cout << "  ping                          - Verificar conexión con el servidor" << std::endl;
    std::cout << "  move <x> <y> <z>              - Mover a coordenadas (desde posición actual)" << std::endl;
    std::cout << "  move_from <fx> <fy> <fz> <tx> <ty> <tz>  - Mover desde/hacia coordenadas" << std::endl;
    std::cout << "  cmd <comando>                 - Ejecutar comando (ej: /skill 123)" << std::endl;
    std::cout << "  pos                           - Obtener posición actual del jugador" << std::endl;
    std::cout << "  interactive                   - Modo interactivo" << std::endl;
    std::cout << std::endl;
    std::cout << "Ejemplos:" << std::endl;
    std::cout << "  ipc_client_example.exe ping" << std::endl;
    std::cout << "  ipc_client_example.exe move 1000 2000 0" << std::endl;
    std::cout << "  ipc_client_example.exe cmd \"/target Goblin\"" << std::endl;
    std::cout << "  ipc_client_example.exe cmd \"/use_skill 123\"" << std::endl;
}

bool cmd_ping(ipc::IPCClient& client) {
    std::cout << "Verificando conexión..." << std::endl;
    
    if (ipc::SimpleIPCClient::try_connect(client)) {
        std::cout << "¡Conexión exitosa!" << std::endl;
        return true;
    } else {
        std::cout << "Error: No se pudo conectar al servidor." << std::endl;
        std::cout << "Asegúrate de que el DLL esté inyectado en el juego." << std::endl;
        return false;
    }
}

bool cmd_move(ipc::IPCClient& client, float fx, float fy, float fz, float tx, float ty, float tz, bool by_mouse = false) {
    if (!client.is_connected() && !client.connect()) {
        std::cout << "Error: No se pudo conectar al servidor." << std::endl;
        return false;
    }

    std::cout << "Moviendo desde (" << fx << ", " << fy << ", " << fz << ") ";
    std::cout << "hacia (" << tx << ", " << ty << ", " << tz << ")" << std::endl;

    ipc::ResultCode result = client.move_to_location(
        ipc::Vector3(fx, fy, fz),
        ipc::Vector3(tx, ty, tz),
        by_mouse
    );

    std::cout << "Resultado: " << ipc::result_to_string(result) << std::endl;
    return result == ipc::ResultCode::SUCCESS;
}

bool cmd_use_command(ipc::IPCClient& client, const std::wstring& command) {
    if (!client.is_connected() && !client.connect()) {
        std::cout << "Error: No se pudo conectar al servidor." << std::endl;
        return false;
    }

    std::wcout << L"Ejecutando comando: " << command << std::endl;

    ipc::ResultCode result = client.use_command(command);

    std::cout << "Resultado: " << ipc::result_to_string(result) << std::endl;
    return result == ipc::ResultCode::SUCCESS;
}

bool cmd_get_pos(ipc::IPCClient& client) {
    if (!client.is_connected() && !client.connect()) {
        std::cout << "Error: No se pudo conectar al servidor." << std::endl;
        return false;
    }

    auto pos = client.get_player_position();
    
    if (pos.has_value() && pos->valid) {
        std::cout << "Posición del jugador:" << std::endl;
        std::cout << "  X: " << pos->position.x << std::endl;
        std::cout << "  Y: " << pos->position.y << std::endl;
        std::cout << "  Z: " << pos->position.z << std::endl;
        std::cout << "Rotación:" << std::endl;
        std::cout << "  X: " << pos->rotation.x << std::endl;
        std::cout << "  Y: " << pos->rotation.y << std::endl;
        std::cout << "  Z: " << pos->rotation.z << std::endl;
        return true;
    } else {
        std::cout << "Error: No se pudo obtener la posición (¿no estás en juego?)" << std::endl;
        return false;
    }
}

void interactive_mode(ipc::IPCClient& client) {
    std::cout << "Modo interactivo. Escribe 'help' para ver comandos, 'exit' para salir." << std::endl;
    
    std::string line;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);

        if (line == "exit" || line == "quit") {
            break;
        }

        if (line == "help") {
            std::cout << "Comandos:" << std::endl;
            std::cout << "  ping                    - Verificar conexión" << std::endl;
            std::cout << "  pos                     - Obtener posición" << std::endl;
            std::cout << "  move <x> <y> <z>        - Mover a coordenadas" << std::endl;
            std::cout << "  cmd <comando>           - Ejecutar comando" << std::endl;
            std::cout << "  exit/quit               - Salir" << std::endl;
            continue;
        }

        if (line == "ping") {
            cmd_ping(client);
            continue;
        }

        if (line == "pos") {
            cmd_get_pos(client);
            continue;
        }

        if (line.rfind("move ", 0) == 0) {
            float x, y, z;
            if (sscanf_s(line.c_str(), "move %f %f %f", &x, &y, &z) == 3) {
                // Usar posición actual como origen (0,0,0 por simplicidad)
                cmd_move(client, 0, 0, 0, x, y, z);
            } else {
                std::cout << "Error: Formato incorrecto. Usa: move <x> <y> <z>" << std::endl;
            }
            continue;
        }

        if (line.rfind("cmd ", 0) == 0) {
            std::wstring cmd(line.begin() + 4, line.end());
            cmd_use_command(client, cmd);
            continue;
        }

        if (!line.empty()) {
            std::cout << "Comando desconocido. Escribe 'help' para ver la ayuda." << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << "=================================" << std::endl;
    std::cout << "IPC Client Example for ue4_base" << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << std::endl;

    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];
    ipc::IPCClient client;

    if (command == "ping") {
        return cmd_ping(client) ? 0 : 1;
    }

    if (command == "move") {
        if (argc != 5) {
            std::cout << "Error: Se requieren 3 coordenadas (x y z)" << std::endl;
            return 1;
        }
        float x = std::stof(argv[2]);
        float y = std::stof(argv[3]);
        float z = std::stof(argv[4]);
        return cmd_move(client, 0, 0, 0, x, y, z) ? 0 : 1;
    }

    if (command == "move_from") {
        if (argc != 8) {
            std::cout << "Error: Se requieren 6 coordenadas (fx fy fz tx ty tz)" << std::endl;
            return 1;
        }
        float fx = std::stof(argv[2]);
        float fy = std::stof(argv[3]);
        float fz = std::stof(argv[4]);
        float tx = std::stof(argv[5]);
        float ty = std::stof(argv[6]);
        float tz = std::stof(argv[7]);
        return cmd_move(client, fx, fy, fz, tx, ty, tz) ? 0 : 1;
    }

    if (command == "cmd") {
        if (argc < 3) {
            std::cout << "Error: Se requiere un comando" << std::endl;
            return 1;
        }
        // Concatenar todos los argumentos restantes
        std::wstring full_cmd;
        for (int i = 2; i < argc; i++) {
            if (i > 2) full_cmd += L" ";
            std::wstring arg(argv[i], argv[i] + strlen(argv[i]));
            full_cmd += arg;
        }
        return cmd_use_command(client, full_cmd) ? 0 : 1;
    }

    if (command == "pos") {
        return cmd_get_pos(client) ? 0 : 1;
    }

    if (command == "interactive") {
        interactive_mode(client);
        return 0;
    }

    std::cout << "Comando desconocido: " << command << std::endl;
    print_usage();
    return 1;
}
