#pragma once

// Este archivo es una copia del protocolo usado por el servidor
// Incluir este archivo en tu proyecto cliente

#include <cstdint>
#include <array>
#include <string>

namespace ipc {

// Nombre del Named Pipe
constexpr const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\ue4_base_ipc";

// Versión del protocolo
constexpr uint32_t PROTOCOL_VERSION = 1;

// Tamaño máximo de comando (use_command)
constexpr size_t MAX_COMMAND_LENGTH = 512;

// Tipos de comandos
enum class CommandType : uint32_t {
    INVALID = 0,
    MOVE_TO_LOCATION = 1,
    USE_COMMAND = 2,
    GET_PLAYER_POS = 3,  // Para debugging/verificación
    PING = 4,            // Verificar conexión
    RESPONSE = 5         // Respuesta del servidor
};

// Códigos de resultado
enum class ResultCode : uint32_t {
    SUCCESS = 0,
    ERROR_INVALID_PARAM = 1,
    ERROR_NOT_IN_GAME = 2,
    ERROR_EXECUTION_FAILED = 3,
    ERROR_UNKNOWN_COMMAND = 4,
    ERROR_SERVER_BUSY = 5
};

// Estructura de vector para IPC
struct Vector3 {
    float x, y, z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
};

// Header de todos los mensajes
struct MessageHeader {
    uint32_t magic;          // 'UE4I' = 0x55353449
    uint32_t version;        // Versión del protocolo
    CommandType type;        // Tipo de comando
    uint32_t request_id;     // ID único para emparejar request/response
    uint32_t payload_size;   // Tamaño del payload después del header

    static constexpr uint32_t MAGIC = 0x55353449; // "UE4I"

    bool is_valid() const {
        return magic == MAGIC && version == PROTOCOL_VERSION;
    }
};

// Payload para MOVE_TO_LOCATION
struct MoveToLocationPayload {
    Vector3 from;
    Vector3 to;
    uint32_t is_by_mouse;  // 0 = false, 1 = true
};

// Payload para USE_COMMAND
struct UseCommandPayload {
    uint32_t command_length;
    wchar_t command[MAX_COMMAND_LENGTH];
};

// Payload de respuesta
struct ResponsePayload {
    ResultCode result;
    uint32_t data_size;
    // Datos opcionales siguen aquí
};

// Payload para GET_PLAYER_POS (respuesta)
struct PlayerPositionPayload {
    Vector3 position;
    Vector3 rotation;
    bool valid;
};

// Estructura completa de mensaje (para alocación)
struct Message {
    MessageHeader header;
    union PayloadUnion {
        MoveToLocationPayload move_to_location;
        UseCommandPayload use_command;
        ResponsePayload response;
        PlayerPositionPayload player_pos;
        uint8_t raw[1024];

        PayloadUnion() {
            memset(this, 0, sizeof(*this));
        }
    } payload;

    Message() : header() {
        header.magic = MessageHeader::MAGIC;
        header.version = PROTOCOL_VERSION;
    }
};

// Tamaño máximo de mensaje
constexpr size_t MAX_MESSAGE_SIZE = sizeof(Message);

// Helper para convertir resultado a string
inline std::string result_to_string(ResultCode code) {
    switch (code) {
    case ResultCode::SUCCESS: return "SUCCESS";
    case ResultCode::ERROR_INVALID_PARAM: return "ERROR_INVALID_PARAM";
    case ResultCode::ERROR_NOT_IN_GAME: return "ERROR_NOT_IN_GAME";
    case ResultCode::ERROR_EXECUTION_FAILED: return "ERROR_EXECUTION_FAILED";
    case ResultCode::ERROR_UNKNOWN_COMMAND: return "ERROR_UNKNOWN_COMMAND";
    case ResultCode::ERROR_SERVER_BUSY: return "ERROR_SERVER_BUSY";
    default: return "UNKNOWN";
    }
}

} // namespace ipc
