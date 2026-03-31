#pragma once

#include "protocol.h"
#include <Windows.h>
#include <string>
#include <functional>
#include <optional>

namespace ipc {

// Cliente IPC para conectarse al DLL de ue4_base
class IPCClient {
public:
    IPCClient();
    ~IPCClient();

    // Conectar al servidor
    bool connect();

    // Desconectar
    void disconnect();

    // Verificar si está conectado
    bool is_connected() const;

    // ============ Comandos disponibles ============

    // Ping para verificar conexión
    bool ping(int timeout_ms = 5000);

    // Mover personaje a una ubicación
    // from: posición actual del personaje
    // to: posición destino
    // is_by_mouse: true si es clic con mouse, false si es automático
    ResultCode move_to_location(const Vector3& from, const Vector3& to, bool is_by_mouse = false);

    // Usar un comando (ej: "/skill 123", "/target NpcName", etc.)
    ResultCode use_command(const std::wstring& command);

    // Obtener posición del jugador
    std::optional<PlayerPositionPayload> get_player_position();

private:
    // Enviar mensaje y esperar respuesta
    bool send_and_receive(const Message& request, Message& response, int timeout_ms = 5000);

    // Enviar mensaje
    bool send_message(const Message& msg);

    // Recibir mensaje
    bool receive_message(Message& msg, int timeout_ms);

    HANDLE pipe_handle_ = INVALID_HANDLE_VALUE;
    uint32_t next_request_id_ = 1;
};

// Versión simplificada para uso rápido
class SimpleIPCClient {
public:
    // Intentar conectar, retorna true si exitoso
    static bool try_connect(IPCClient& client, int max_retries = 3);
};

} // namespace ipc
