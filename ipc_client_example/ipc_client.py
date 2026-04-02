#!/usr/bin/env python3
"""
Cliente IPC para ue4_base - Comunicación con el juego mediante Named Pipes
Basado en el protocolo definido en protocol.h
"""

import ctypes
import struct
import time
from ctypes import wintypes
from enum import IntEnum
from typing import Optional, Tuple

# ============ Constantes ============
PIPE_NAME = r"\\.\pipe\ue4_base_ipc"
PROTOCOL_VERSION = 1
MAX_COMMAND_LENGTH = 512
MAX_MESSAGE_SIZE = 1024 + 24  # payload raw[1024] + header (24 bytes)
MAGIC = 0x55353449  # "UE4I"

# ============ Enums ============
class CommandType(IntEnum):
    INVALID = 0
    MOVE_TO_LOCATION = 1
    USE_COMMAND = 2
    GET_PLAYER_POS = 3
    PING = 4
    RESPONSE = 5
    USE_SKILL = 6
    ATTACK = 7
    SELECT_TARGET = 8
    TARGET_ATTACK = 9

class ResultCode(IntEnum):
    SUCCESS = 0
    ERROR_INVALID_PARAM = 1
    ERROR_NOT_IN_GAME = 2
    ERROR_EXECUTION_FAILED = 3
    ERROR_UNKNOWN_COMMAND = 4
    ERROR_SERVER_BUSY = 5

# ============ Estructuras ============
class Vector3(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("x", ctypes.c_float),
        ("y", ctypes.c_float),
        ("z", ctypes.c_float),
    ]

    def __init__(self, x: float = 0.0, y: float = 0.0, z: float = 0.0):
        super().__init__()
        self.x = x
        self.y = y
        self.z = z

    def __repr__(self):
        return f"Vector3({self.x:.2f}, {self.y:.2f}, {self.z:.2f})"


class MessageHeader(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("magic", ctypes.c_uint32),
        ("version", ctypes.c_uint32),
        ("type", ctypes.c_uint32),  # CommandType como uint32
        ("request_id", ctypes.c_uint32),
        ("payload_size", ctypes.c_uint32),
    ]

    def __init__(self, cmd_type: CommandType = CommandType.INVALID, request_id: int = 0, payload_size: int = 0):
        super().__init__()
        self.magic = MAGIC
        self.version = PROTOCOL_VERSION
        self.type = int(cmd_type)
        self.request_id = request_id
        self.payload_size = payload_size

    def is_valid(self) -> bool:
        return self.magic == MAGIC and self.version == PROTOCOL_VERSION


class MoveToLocationPayload(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("from_x", ctypes.c_float),
        ("from_y", ctypes.c_float),
        ("from_z", ctypes.c_float),
        ("to_x", ctypes.c_float),
        ("to_y", ctypes.c_float),
        ("to_z", ctypes.c_float),
        ("is_by_mouse", ctypes.c_uint32),
    ]

    def __init__(self, from_vec: Vector3, to_vec: Vector3, is_by_mouse: bool = False):
        super().__init__()
        self.from_x = from_vec.x
        self.from_y = from_vec.y
        self.from_z = from_vec.z
        self.to_x = to_vec.x
        self.to_y = to_vec.y
        self.to_z = to_vec.z
        self.is_by_mouse = 1 if is_by_mouse else 0


class UseCommandPayload(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("command_length", ctypes.c_uint32),
        ("command", ctypes.c_wchar * MAX_COMMAND_LENGTH),
    ]

    def __init__(self, command: str):
        super().__init__()
        self.command_length = min(len(command), MAX_COMMAND_LENGTH - 1)
        self.command = command[:MAX_COMMAND_LENGTH - 1]


class ResponsePayload(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("result", ctypes.c_uint32),  # ResultCode como uint32
        ("data_size", ctypes.c_uint32),
    ]


class PlayerPositionPayload(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("pos_x", ctypes.c_float),
        ("pos_y", ctypes.c_float),
        ("pos_z", ctypes.c_float),
        ("rot_x", ctypes.c_float),
        ("rot_y", ctypes.c_float),
        ("rot_z", ctypes.c_float),
        ("valid", ctypes.c_bool),
    ]

    @property
    def position(self) -> Vector3:
        return Vector3(self.pos_x, self.pos_y, self.pos_z)

    @property
    def rotation(self) -> Vector3:
        return Vector3(self.rot_x, self.rot_y, self.rot_z)


class UseSkillPayload(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("skill_id", ctypes.c_int32),
    ]

    def __init__(self, skill_id: int):
        super().__init__()
        self.skill_id = skill_id


class AttackPayload(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("force", ctypes.c_uint32),
        ("lock_movement", ctypes.c_uint32),
    ]

    def __init__(self, force: bool = False, lock_movement: bool = False):
        super().__init__()
        self.force = 1 if force else 0
        self.lock_movement = 1 if lock_movement else 0


class SelectTargetPayload(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("actor_address", ctypes.c_uint64),
        ("force_attack", ctypes.c_uint32),
    ]

    def __init__(self, actor_address: int, force_attack: bool = False):
        super().__init__()
        self.actor_address = actor_address
        self.force_attack = 1 if force_attack else 0


class TargetAttackPayload(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("target_address", ctypes.c_uint64),
        ("loc_x", ctypes.c_float),
        ("loc_y", ctypes.c_float),
        ("loc_z", ctypes.c_float),
        ("lock_movement", ctypes.c_uint32),
    ]

    def __init__(self, target_address: int, location: Vector3, lock_movement: bool = False):
        super().__init__()
        self.target_address = target_address
        self.loc_x = location.x
        self.loc_y = location.y
        self.loc_z = location.z
        self.lock_movement = 1 if lock_movement else 0


# ============ Windows API ============
kernel32 = ctypes.windll.kernel32

GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
PIPE_ACCESS_DUPLEX = 0x00000003

class IPCClient:
    """Cliente IPC para comunicarse con ue4_base mediante Named Pipes"""

    # Constantes de Windows
    INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value

    def __init__(self):
        self.pipe_handle = None
        self.request_counter = 0

    def connect(self) -> bool:
        """Conecta al servidor IPC"""
        # Configurar tipos de la API de Windows
        kernel32.CreateFileW.argtypes = [
            wintypes.LPCWSTR,
            wintypes.DWORD,
            wintypes.DWORD,
            wintypes.LPVOID,
            wintypes.DWORD,
            wintypes.DWORD,
            wintypes.HANDLE
        ]
        kernel32.CreateFileW.restype = wintypes.HANDLE

        handle = kernel32.CreateFileW(
            PIPE_NAME,
            GENERIC_READ | GENERIC_WRITE,
            0,  # No sharing
            None,  # Default security attributes
            OPEN_EXISTING,
            0,  # Default attributes
            None  # No template file
        )

        # Convertir a entero de Python y validar
        handle_value = ctypes.c_void_p(handle).value

        if handle_value is None or handle_value == self.INVALID_HANDLE_VALUE:
            self.pipe_handle = None
            return False

        self.pipe_handle = handle_value
        return True

    def disconnect(self):
        """Desconecta del servidor"""
        if self.pipe_handle is not None:
            kernel32.CloseHandle(ctypes.c_void_p(self.pipe_handle))
            self.pipe_handle = None

    def is_connected(self) -> bool:
        """Verifica si está conectado"""
        return self.pipe_handle is not None

    def _send_request(self, header: MessageHeader, payload: Optional[ctypes.Structure] = None) -> bool:
        """Envía una solicitud al servidor"""
        if self.pipe_handle is None:
            return False

        # Empaquetar header
        header_data = bytes(header)

        # Empaquetar payload si existe
        payload_data = b''
        if payload and header.payload_size > 0:
            payload_data = bytes(payload)[:header.payload_size]

        # Combinar y enviar
        data = header_data + payload_data
        written = wintypes.DWORD(0)

        # Configurar tipos de WriteFile
        kernel32.WriteFile.argtypes = [
            wintypes.HANDLE,
            wintypes.LPCVOID,
            wintypes.DWORD,
            wintypes.LPDWORD,
            wintypes.LPVOID
        ]
        kernel32.WriteFile.restype = wintypes.BOOL

        result = kernel32.WriteFile(
            ctypes.c_void_p(self.pipe_handle),
            data,
            len(data),
            ctypes.byref(written),
            None
        )

        return result != 0

    def _read_response(self, timeout_ms: int = 5000) -> Optional[Tuple[MessageHeader, bytes]]:
        """Lee la respuesta del servidor"""
        if self.pipe_handle is None:
            return None

        # Configurar tipos de ReadFile
        kernel32.ReadFile.argtypes = [
            wintypes.HANDLE,
            wintypes.LPVOID,
            wintypes.DWORD,
            wintypes.LPDWORD,
            wintypes.LPVOID
        ]
        kernel32.ReadFile.restype = wintypes.BOOL

        # Leer header primero
        header_data = ctypes.create_string_buffer(ctypes.sizeof(MessageHeader))
        read = wintypes.DWORD(0)

        result = kernel32.ReadFile(
            ctypes.c_void_p(self.pipe_handle),
            header_data,
            ctypes.sizeof(MessageHeader),
            ctypes.byref(read),
            None
        )

        if result == 0 or read.value != ctypes.sizeof(MessageHeader):
            return None

        header = MessageHeader.from_buffer_copy(header_data)

        if not header.is_valid():
            return None

        # Leer payload si existe
        payload_data = b''
        if header.payload_size > 0:
            payload_buffer = ctypes.create_string_buffer(header.payload_size)
            result = kernel32.ReadFile(
                ctypes.c_void_p(self.pipe_handle),
                payload_buffer,
                header.payload_size,
                ctypes.byref(read),
                None
            )
            if result != 0 and read.value == header.payload_size:
                payload_data = bytes(payload_buffer)

        return header, payload_data

    def _next_request_id(self) -> int:
        """Genera el siguiente ID de solicitud"""
        self.request_counter += 1
        return self.request_counter

    # ============ Comandos Públicos ============

    def ping(self) -> bool:
        """Envía un ping para verificar la conexión"""
        header = MessageHeader(CommandType.PING, self._next_request_id(), 0)

        if not self._send_request(header):
            return False

        response = self._read_response()
        if not response:
            return False

        resp_header, _ = response
        return resp_header.type == int(CommandType.RESPONSE)

    def move_to_location(self, from_vec: Vector3, to_vec: Vector3, is_by_mouse: bool = False) -> ResultCode:
        """
        Mueve el personaje de una ubicación a otra.

        Args:
            from_vec: Posición inicial
            to_vec: Posición destino
            is_by_mouse: Si es True, simula movimiento con mouse

        Returns:
            ResultCode con el resultado de la operación
        """
        payload = MoveToLocationPayload(from_vec, to_vec, is_by_mouse)
        header = MessageHeader(CommandType.MOVE_TO_LOCATION, self._next_request_id(), ctypes.sizeof(payload))

        if not self._send_request(header, payload):
            return ResultCode.ERROR_EXECUTION_FAILED

        response = self._read_response()
        if not response:
            return ResultCode.ERROR_EXECUTION_FAILED

        resp_header, resp_payload = response
        if resp_header.type != int(CommandType.RESPONSE) or len(resp_payload) < ctypes.sizeof(ResponsePayload):
            return ResultCode.ERROR_EXECUTION_FAILED

        result = ResponsePayload.from_buffer_copy(resp_payload)
        return ResultCode(result.result)

    def use_command(self, command: str) -> ResultCode:
        """
        Ejecuta un comando de texto en el juego.

        Args:
            command: Comando a ejecutar (ej: "teleport", "god", etc.)

        Returns:
            ResultCode con el resultado de la operación
        """
        payload = UseCommandPayload(command)
        header = MessageHeader(CommandType.USE_COMMAND, self._next_request_id(), ctypes.sizeof(payload))

        if not self._send_request(header, payload):
            return ResultCode.ERROR_EXECUTION_FAILED

        response = self._read_response()
        if not response:
            return ResultCode.ERROR_EXECUTION_FAILED

        resp_header, resp_payload = response
        if resp_header.type != int(CommandType.RESPONSE) or len(resp_payload) < ctypes.sizeof(ResponsePayload):
            return ResultCode.ERROR_EXECUTION_FAILED

        result = ResponsePayload.from_buffer_copy(resp_payload)
        return ResultCode(result.result)

    def get_player_pos(self) -> Tuple[ResultCode, Optional[PlayerPositionPayload]]:
        """
        Obtiene la posición actual del jugador.

        Returns:
            Tupla con (ResultCode, PlayerPositionPayload o None)
        """
        header = MessageHeader(CommandType.GET_PLAYER_POS, self._next_request_id(), 0)

        if not self._send_request(header):
            return ResultCode.ERROR_EXECUTION_FAILED, None

        response = self._read_response()
        if not response:
            return ResultCode.ERROR_EXECUTION_FAILED, None

        resp_header, resp_payload = response
        if resp_header.type != int(CommandType.RESPONSE) or len(resp_payload) < ctypes.sizeof(ResponsePayload):
            return ResultCode.ERROR_EXECUTION_FAILED, None

        result = ResponsePayload.from_buffer_copy(resp_payload)

        if result.result == int(ResultCode.SUCCESS) and len(resp_payload) >= ctypes.sizeof(PlayerPositionPayload):
            pos_data = PlayerPositionPayload.from_buffer_copy(resp_payload)
            return ResultCode.SUCCESS, pos_data

        return ResultCode(result.result), None

    def use_skill(self, skill_id: int) -> ResultCode:
        """
        Usa una skill por su ID.

        Args:
            skill_id: ID de la skill a usar

        Returns:
            ResultCode con el resultado de la operación
        """
        payload = UseSkillPayload(skill_id)
        header = MessageHeader(CommandType.USE_SKILL, self._next_request_id(), ctypes.sizeof(payload))

        if not self._send_request(header, payload):
            return ResultCode.ERROR_EXECUTION_FAILED

        response = self._read_response()
        if not response:
            return ResultCode.ERROR_EXECUTION_FAILED

        resp_header, resp_payload = response
        if resp_header.type != int(CommandType.RESPONSE) or len(resp_payload) < ctypes.sizeof(ResponsePayload):
            return ResultCode.ERROR_EXECUTION_FAILED

        result = ResponsePayload.from_buffer_copy(resp_payload)
        return ResultCode(result.result)

    def attack(self, force: bool = False, lock_movement: bool = False) -> ResultCode:
        """
        Ejecuta el comando de ataque.

        Args:
            force: Forzar ataque
            lock_movement: Bloquear movimiento durante el ataque

        Returns:
            ResultCode con el resultado de la operación
        """
        payload = AttackPayload(force, lock_movement)
        header = MessageHeader(CommandType.ATTACK, self._next_request_id(), ctypes.sizeof(payload))

        if not self._send_request(header, payload):
            return ResultCode.ERROR_EXECUTION_FAILED

        response = self._read_response()
        if not response:
            return ResultCode.ERROR_EXECUTION_FAILED

        resp_header, resp_payload = response
        if resp_header.type != int(CommandType.RESPONSE) or len(resp_payload) < ctypes.sizeof(ResponsePayload):
            return ResultCode.ERROR_EXECUTION_FAILED

        result = ResponsePayload.from_buffer_copy(resp_payload)
        return ResultCode(result.result)

    def select_target(self, actor_address: int, force_attack: bool = False) -> ResultCode:
        """
        Selecciona un objetivo por su dirección de memoria.

        Args:
            actor_address: Dirección de memoria del actor (obtenida del SDK/dumps)
            force_attack: Forzar ataque al seleccionar

        Returns:
            ResultCode con el resultado de la operación
        """
        payload = SelectTargetPayload(actor_address, force_attack)
        header = MessageHeader(CommandType.SELECT_TARGET, self._next_request_id(), ctypes.sizeof(payload))

        if not self._send_request(header, payload):
            return ResultCode.ERROR_EXECUTION_FAILED

        response = self._read_response()
        if not response:
            return ResultCode.ERROR_EXECUTION_FAILED

        resp_header, resp_payload = response
        if resp_header.type != int(CommandType.RESPONSE) or len(resp_payload) < ctypes.sizeof(ResponsePayload):
            return ResultCode.ERROR_EXECUTION_FAILED

        result = ResponsePayload.from_buffer_copy(resp_payload)
        return ResultCode(result.result)

    def target_attack(self, target_address: int, location: Vector3, lock_movement: bool = False) -> ResultCode:
        """
        Ataca un target específico en una ubicación.

        Args:
            target_address: Dirección de memoria del target (obtenida del SDK/dumps)
            location: Ubicación del ataque (Vector3)
            lock_movement: Bloquear movimiento durante el ataque

        Returns:
            ResultCode con el resultado de la operación
        """
        payload = TargetAttackPayload(target_address, location, lock_movement)
        header = MessageHeader(CommandType.TARGET_ATTACK, self._next_request_id(), ctypes.sizeof(payload))

        if not self._send_request(header, payload):
            return ResultCode.ERROR_EXECUTION_FAILED

        response = self._read_response()
        if not response:
            return ResultCode.ERROR_EXECUTION_FAILED

        resp_header, resp_payload = response
        if resp_header.type != int(CommandType.RESPONSE) or len(resp_payload) < ctypes.sizeof(ResponsePayload):
            return ResultCode.ERROR_EXECUTION_FAILED

        result = ResponsePayload.from_buffer_copy(resp_payload)
        return ResultCode(result.result)

    def __enter__(self):
        """Context manager entry"""
        self.connect()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit"""
        self.disconnect()
        return False


# ============ Funciones de ayuda ============

def result_to_string(code: ResultCode) -> str:
    """Convierte un ResultCode a string legible"""
    names = {
        ResultCode.SUCCESS: "SUCCESS",
        ResultCode.ERROR_INVALID_PARAM: "ERROR_INVALID_PARAM",
        ResultCode.ERROR_NOT_IN_GAME: "ERROR_NOT_IN_GAME",
        ResultCode.ERROR_EXECUTION_FAILED: "ERROR_EXECUTION_FAILED",
        ResultCode.ERROR_UNKNOWN_COMMAND: "ERROR_UNKNOWN_COMMAND",
        ResultCode.ERROR_SERVER_BUSY: "ERROR_SERVER_BUSY",
    }
    return names.get(code, "UNKNOWN")


# ============ Ejemplo de uso ============

if __name__ == "__main__":
    print("=" * 50)
    print("Cliente IPC para ue4_base")
    print("=" * 50)

    # Crear cliente
    client = IPCClient()

    print(f"\nConectando a {PIPE_NAME}...")

    if not client.connect():
        print("ERROR: No se pudo conectar al servidor IPC")
        print("Asegúrate de que el juego esté corriendo con el DLL inyectado")
        exit(1)

    print("Conectado exitosamente!")

    try:
        # Ping
        print("\n[1] Enviando PING...")
        if client.ping():
            print("    -> PONG recibido!")
        else:
            print("    -> No hubo respuesta")

        # Obtener posición del jugador
        print("\n[2] Obteniendo posición del jugador...")
        result, pos = client.get_player_pos()
        print(f"    -> Resultado: {result_to_string(result)}")
        if pos and pos.valid:
            print(f"    -> Posición: {pos.position}")
            print(f"    -> Rotación: {pos.rotation}")

        # Usar una skill (ejemplo: ID 1001)
        print("\n[3] Usando skill ID 1001...")
        result = client.use_skill(1001)
        print(f"    -> Resultado: {result_to_string(result)}")

        # Atacar
        print("\n[4] Ejecutando ataque...")
        result = client.attack(force=True, lock_movement=False)
        print(f"    -> Resultado: {result_to_string(result)}")

        # Mover a ubicación (ejemplo)
        print("\n[5] Moviendo personaje...")
        from_pos = Vector3(100.0, 200.0, 300.0)
        to_pos = Vector3(150.0, 250.0, 300.0)
        result = client.move_to_location(from_pos, to_pos, is_by_mouse=False)
        print(f"    -> Resultado: {result_to_string(result)}")

        # Usar comando
        print("\n[6] Ejecutando comando...")
        result = client.use_command("help")
        print(f"    -> Resultado: {result_to_string(result)}")

        # Seleccionar target (ejemplo con dirección ficticia)
        print("\n[7] Seleccionando target (ejemplo)...")
        # NOTA: Usa una dirección real obtenida del juego
        # result = client.select_target(0x00000000, force_attack=False)
        # print(f"    -> Resultado: {result_to_string(result)}")
        print("    -> (Comentado - requiere dirección real de actor)")

    except KeyboardInterrupt:
        print("\n\nInterrumpido por el usuario")
    finally:
        client.disconnect()
        print("\nDesconectado.")
