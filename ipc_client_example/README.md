# IPC Client para ue4_base

Este sistema permite comunicarse con el DLL inyectado en el juego para ejecutar comandos remotamente.

## Características

- **Named Pipes**: Comunicación rápida y segura entre procesos
- **Comandos disponibles**:
  - `move_to_location` - Mover el personaje a una ubicación
  - `use_command` - Ejecutar comandos del juego (/skill, /target, etc.)
  - `get_player_position` - Obtener posición actual del jugador
  - `ping` - Verificar conexión

## Requisitos

- Windows 10/11
- Visual Studio 2019+ o MinGW-w64
- El DLL `ue4_base.dll` debe estar inyectado en el juego

## Compilación

### Con Visual Studio (MSVC)
```cmd
cd ipc_client_example
cl main_example.cpp ipc_client.cpp /EHsc /std:c++17 /Fe:ipc_client.exe
```

### Con MinGW/g++
```cmd
cd ipc_client_example
g++ -std=c++17 main_example.cpp ipc_client.cpp -o ipc_client.exe
```

## Uso

### Línea de comandos

```cmd
# Verificar conexión
ipc_client.exe ping

# Mover a coordenadas (usando posición actual como origen)
ipc_client.exe move 1000 2000 0

# Mover desde/hacia coordenadas específicas
ipc_client.exe move_from 0 0 0 1000 2000 0

# Ejecutar comando del juego
ipc_client.exe cmd "/target Goblin"
ipc_client.exe cmd "/use_skill 123"

# Obtener posición actual
ipc_client.exe pos

# Modo interactivo
ipc_client.exe interactive
```

### En tu propio programa C++

Incluye los archivos `protocol.h` e `ipc_client.h` en tu proyecto:

```cpp
#include "ipc_client.h"
#include <iostream>

int main() {
    ipc::IPCClient client;
    
    // Conectar
    if (!ipc::SimpleIPCClient::try_connect(client)) {
        std::cout << "No se pudo conectar" << std::endl;
        return 1;
    }
    
    // Mover a ubicación
    ipc::ResultCode result = client.move_to_location(
        ipc::Vector3(0, 0, 0),      // desde
        ipc::Vector3(1000, 2000, 0), // hacia
        false                         // no es por mouse
    );
    
    if (result == ipc::ResultCode::SUCCESS) {
        std::cout << "¡Movimiento exitoso!" << std::endl;
    }
    
    // Ejecutar comando
    result = client.use_command(L"/target Enemy");
    
    // Obtener posición
    auto pos = client.get_player_position();
    if (pos.has_value()) {
        std::cout << "Posición: " << pos->position.x << ", " 
                  << pos->position.y << ", " << pos->position.z << std::endl;
    }
    
    return 0;
}
```

### En Python (usando pywin32)

```python
import win32pipe, win32file, struct
import time

PIPE_NAME = r'\\.\pipe\ue4_base_ipc'

def send_command(cmd_type, payload=b'', timeout=10.0):
    """Envía un comando y espera la respuesta"""
    handle = win32file.CreateFile(
        PIPE_NAME,
        win32file.GENERIC_READ | win32file.GENERIC_WRITE,
        0, None, win32file.OPEN_EXISTING, 0, None
    )
    
    request_id = 1
    
    # Header: magic(4) + version(4) + type(4) + request_id(4) + payload_size(4)
    header = struct.pack('<IIIII', 0x55353449, 1, cmd_type, request_id, len(payload))
    
    # Enviar mensaje
    win32file.WriteFile(handle, header + payload)
    
    # Esperar respuesta con timeout
    start = time.time()
    while time.time() - start < timeout:
        try:
            # Intentar leer respuesta
            hr, data = win32file.ReadFile(handle, 4096)
            if data:
                # Parsear respuesta
                resp_header = struct.unpack('<IIIII', data[:20])
                resp_magic, resp_version, resp_type, resp_id, resp_size = resp_header
                
                if resp_magic == 0x55353449 and resp_id == request_id:
                    if resp_size >= 4:
                        result = struct.unpack('<I', data[20:24])[0]
                        handle.Close()
                        return result  # 0 = SUCCESS
            break
        except:
            time.sleep(0.1)
    
    handle.Close()
    return -1  # Timeout

def move_to_location(from_x, from_y, from_z, to_x, to_y, to_z, is_by_mouse=False):
    """Mover personaje"""
    # Payload: from(12 bytes) + to(12 bytes) + is_by_mouse(4 bytes)
    payload = struct.pack('<ffffffI', from_x, from_y, from_z, to_x, to_y, to_z, int(is_by_mouse))
    return send_command(1, payload)  # 1 = MOVE_TO_LOCATION

def use_command(command):
    """Ejecutar comando"""
    cmd_bytes = command.encode('utf-16-le')
    cmd_len = len(cmd_bytes) // 2
    # Payload: length(4) + command(512 wchar_t = 1024 bytes)
    payload = struct.pack('<I', cmd_len) + cmd_bytes
    payload += b'\x00' * (1024 - len(cmd_bytes))  # Padding
    return send_command(2, payload)  # 2 = USE_COMMAND

def ping():
    """Verificar conexión"""
    return send_command(4)  # 4 = PING

# Ejemplos de uso
print("Ping:", ping())
print("Move:", move_to_location(0, 0, 0, 1000, 2000, 0))
print("Command:", use_command("/target Goblin"))
```

**Nota importante**: Los comandos que requieren ejecución en el hilo del juego (MOVE_TO_LOCATION, USE_COMMAND, GET_PLAYER_POS) tienen un retraso mientras el servidor espera al próximo game tick. El cliente debe esperar la respuesta con un timeout adecuado (recomendado: 5-10 segundos).

## Protocolo

### Named Pipe
- **Nombre**: `\\.\pipe\ue4_base_ipc`
- **Tipo**: Mensaje bidireccional
- **Formato**: Binario estructurado

### Estructura de mensaje

```
Header (20 bytes):
  - magic: 0x55353449 ("UE4I")
  - version: 1
  - type: 1=MOVE_TO_LOCATION, 2=USE_COMMAND, 3=GET_PLAYER_POS, 4=PING
  - request_id: ID único
  - payload_size: tamaño del payload

Payload (variable):
  - MOVE_TO_LOCATION: 28 bytes (3 floats from + 3 floats to + 1 int)
  - USE_COMMAND: 516 bytes (1 int length + 512 wchar_t)
  - GET_PLAYER_POS: 0 bytes
  - PING: 0 bytes
```

## Solución de problemas

### "No se pudo conectar al servidor"
- Asegúrate de que el DLL esté inyectado en el juego
- Verifica que el juego esté corriendo
- Revisa que el Named Pipe exista: `powershell "Get-ChildItem \\.\pipe\ | Where-Object { $_.Name -like '*ue4*' }"`

### "ERROR_NOT_IN_GAME"
- El jugador no está en el mundo del juego
- Intenta de nuevo cuando estés completamente cargado

### Comandos no funcionan
- Algunos comandos requieren estar en combate o tener un objetivo
- Verifica que el comando sea válido para el juego

## Seguridad

- El Named Pipe solo es accesible desde procesos locales (misma máquina)
- No requiere privilegios de administrador
- Las conexiones son aceptadas una a la vez

## Licencia

Este código es parte de ue4_base. Úsalo bajo tu propia responsabilidad.
