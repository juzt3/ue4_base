#include "ipc_client.h"
#include <iostream>
#include <cstring>

namespace ipc {

IPCClient::IPCClient() = default;

IPCClient::~IPCClient() {
    disconnect();
}

bool IPCClient::connect() {
    if (pipe_handle_ != INVALID_HANDLE_VALUE) {
        return true; // Ya conectado
    }

    // Intentar conectar al pipe
    pipe_handle_ = CreateFileW(
        PIPE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,             // Sin compartir
        nullptr,       // Seguridad por defecto
        OPEN_EXISTING, // El pipe ya debe existir
        0,             // Atributos por defecto
        nullptr        // Sin template
    );

    if (pipe_handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    // Establecer modo de mensaje
    DWORD mode = PIPE_READMODE_MESSAGE;
    BOOL success = SetNamedPipeHandleState(
        pipe_handle_,
        &mode,
        nullptr,
        nullptr
    );

    if (!success) {
        disconnect();
        return false;
    }

    return true;
}

void IPCClient::disconnect() {
    if (pipe_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_handle_);
        pipe_handle_ = INVALID_HANDLE_VALUE;
    }
}

bool IPCClient::is_connected() const {
    return pipe_handle_ != INVALID_HANDLE_VALUE;
}

bool IPCClient::ping(int timeout_ms) {
    Message request;
    request.header.type = CommandType::PING;
    request.header.request_id = next_request_id_++;
    request.header.payload_size = 0;

    Message response;
    if (!send_and_receive(request, response, timeout_ms)) {
        return false;
    }

    return response.payload.response.result == ResultCode::SUCCESS;
}

ResultCode IPCClient::move_to_location(const Vector3& from, const Vector3& to, bool is_by_mouse) {
    Message request;
    request.header.type = CommandType::MOVE_TO_LOCATION;
    request.header.request_id = next_request_id_++;
    request.header.payload_size = sizeof(MoveToLocationPayload);

    request.payload.move_to_location.from = from;
    request.payload.move_to_location.to = to;
    request.payload.move_to_location.is_by_mouse = is_by_mouse ? 1 : 0;

    Message response;
    if (!send_and_receive(request, response)) {
        return ResultCode::ERROR_EXECUTION_FAILED;
    }

    return response.payload.response.result;
}

ResultCode IPCClient::use_command(const std::wstring& command) {
    if (command.empty() || command.length() >= MAX_COMMAND_LENGTH) {
        return ResultCode::ERROR_INVALID_PARAM;
    }

    Message request;
    request.header.type = CommandType::USE_COMMAND;
    request.header.request_id = next_request_id_++;
    request.header.payload_size = sizeof(UseCommandPayload);

    request.payload.use_command.command_length = static_cast<uint32_t>(command.length());
    wcsncpy(request.payload.use_command.command, command.c_str(), MAX_COMMAND_LENGTH);
    request.payload.use_command.command[MAX_COMMAND_LENGTH - 1] = L'\0';

    Message response;
    if (!send_and_receive(request, response)) {
        return ResultCode::ERROR_EXECUTION_FAILED;
    }

    return response.payload.response.result;
}

std::optional<PlayerPositionPayload> IPCClient::get_player_position() {
    Message request;
    request.header.type = CommandType::GET_PLAYER_POS;
    request.header.request_id = next_request_id_++;
    request.header.payload_size = 0;

    Message response;
    if (!send_and_receive(request, response)) {
        return std::nullopt;
    }

    if (response.payload.response.result != ResultCode::SUCCESS) {
        return std::nullopt;
    }

    return response.payload.player_pos;
}

bool IPCClient::send_and_receive(const Message& request, Message& response, int timeout_ms) {
    if (!is_connected() && !connect()) {
        return false;
    }

    if (!send_message(request)) {
        return false;
    }

    if (!receive_message(response, timeout_ms)) {
        return false;
    }

    // Verificar que la respuesta corresponde a la solicitud
    if (response.header.request_id != request.header.request_id) {
        return false;
    }

    return true;
}

bool IPCClient::send_message(const Message& msg) {
    DWORD bytes_written = 0;

    // Enviar header
    BOOL success = WriteFile(
        pipe_handle_,
        &msg.header,
        sizeof(MessageHeader),
        &bytes_written,
        nullptr
    );

    if (!success || bytes_written != sizeof(MessageHeader)) {
        return false;
    }

    // Enviar payload si existe
    if (msg.header.payload_size > 0) {
        success = WriteFile(
            pipe_handle_,
            &msg.payload,
            msg.header.payload_size,
            &bytes_written,
            nullptr
        );

        if (!success || bytes_written != msg.header.payload_size) {
            return false;
        }
    }

    FlushFileBuffers(pipe_handle_);
    return true;
}

bool IPCClient::receive_message(Message& msg, int timeout_ms) {
    DWORD bytes_read = 0;
    DWORD total_timeout = static_cast<DWORD>(timeout_ms);
    DWORD start_time = GetTickCount();

    // Leer header
    while (true) {
        BOOL success = ReadFile(
            pipe_handle_,
            &msg.header,
            sizeof(MessageHeader),
            &bytes_read,
            nullptr
        );

        if (success && bytes_read == sizeof(MessageHeader)) {
            break;
        }

        if (GetLastError() == ERROR_MORE_DATA) {
            continue; // Seguir leyendo
        }

        // Verificar timeout
        if (GetTickCount() - start_time > total_timeout) {
            return false;
        }

        if (GetLastError() != ERROR_IO_PENDING) {
            return false;
        }

        Sleep(10);
    }

    // Validar mensaje
    if (!msg.header.is_valid()) {
        return false;
    }

    // Leer payload si existe
    if (msg.header.payload_size > 0) {
        if (msg.header.payload_size > sizeof(msg.payload)) {
            return false;
        }

        while (true) {
            BOOL success = ReadFile(
                pipe_handle_,
                &msg.payload,
                msg.header.payload_size,
                &bytes_read,
                nullptr
            );

            if (success && bytes_read == msg.header.payload_size) {
                break;
            }

            if (GetLastError() == ERROR_MORE_DATA) {
                continue;
            }

            // Verificar timeout
            if (GetTickCount() - start_time > total_timeout) {
                return false;
            }

            if (GetLastError() != ERROR_IO_PENDING) {
                return false;
            }

            Sleep(10);
        }
    }

    return true;
}

// ============ SimpleIPCClient ============

bool SimpleIPCClient::try_connect(IPCClient& client, int max_retries) {
    for (int i = 0; i < max_retries; i++) {
        if (client.connect()) {
            if (client.ping(2000)) {
                return true;
            }
            client.disconnect();
        }
        Sleep(500);
    }
    return false;
}

} // namespace ipc
