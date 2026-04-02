# Cliente IPC en Python para ue4_base

Cliente Python para comunicarse con el juego mediante Named Pipes, implementando el mismo protocolo que el cliente C++.

## Características

- ✅ Conexión mediante Named Pipes de Windows
- ✅ Todos los comandos implementados:
  - `ping()` - Verificar conexión
  - `move_to_location()` - Mover personaje
  - `use_command()` - Ejecutar comandos de texto
  - `get_player_pos()` - Obtener posición del jugador
  - `use_skill()` - Usar skills
  - `attack()` - Atacar
  - `select_target()` - Seleccionar objetivo
- ✅ Estructuras de datos con `ctypes` para compatibilidad binaria
- ✅ Context manager (`with` statement)
- ✅ Tipado completo con type hints

## Requisitos

- Python 3.7+
- Windows (usa API de Windows para Named Pipes)

## Uso

### Ejemplo básico

```python
from ipc_client import IPCClient, Vector3, ResultCode

# Crear y conectar cliente
client = IPCClient()
if not client.connect():
    print("No se pudo conectar")
    exit(1)

# Verificar conexión
if client.ping():
    print("Conexión exitosa!")

# Obtener posición del jugador
result, pos = client.get_player_pos()
if result == ResultCode.SUCCESS:
    print(f"Posición: {pos.position}")
    print(f"Rotación: {pos.rotation}")

# Usar una skill
result = client.use_skill(1001)
print(f"Skill resultado: {result}")

# Atacar
result = client.attack(force=True, lock_movement=False)

# Mover personaje
from_pos = Vector3(100.0, 200.0, 300.0)
to_pos = Vector3(150.0, 250.0, 300.0)
result = client.move_to_location(from_pos, to_pos)

# Ejecutar comando
result = client.use_command("teleport")

# Desconectar
client.disconnect()
```

### Uso con context manager

```python
from ipc_client import IPCClient, Vector3, ResultCode

with IPCClient() as client:
    if not client.is_connected():
        print("Error de conexión")
        return

    # Usar el cliente aquí
    result, pos = client.get_player_pos()
    if result == ResultCode.SUCCESS:
        print(f"Posición: {pos.position}")

# Desconexión automática al salir del contexto
```

## API Reference

### Clase `IPCClient`

#### Métodos

| Método | Parámetros | Retorno | Descripción |
|--------|-----------|---------|-------------|
| `connect()` | - | `bool` | Conecta al servidor IPC |
| `disconnect()` | - | - | Desconecta del servidor |
| `is_connected()` | - | `bool` | Verifica si está conectado |
| `ping()` | - | `bool` | Envía ping al servidor |
| `move_to_location(from_vec, to_vec, is_by_mouse)` | `Vector3, Vector3, bool` | `ResultCode` | Mueve el personaje |
| `use_command(command)` | `str` | `ResultCode` | Ejecuta comando de texto |
| `get_player_pos()` | - | `(ResultCode, PlayerPositionPayload)` | Obtiene posición |
| `use_skill(skill_id)` | `int` | `ResultCode` | Usa una skill |
| `attack(force, lock_movement)` | `bool, bool` | `ResultCode` | Ejecuta ataque |
| `select_target(actor_address, force_attack)` | `int, bool` | `ResultCode` | Selecciona target |

### Estructuras

#### `Vector3(x, y, z)`
Representa un vector 3D:
```python
pos = Vector3(100.0, 200.0, 300.0)
print(pos.x, pos.y, pos.z)
```

#### `ResultCode`
Enum con los códigos de resultado:
- `SUCCESS` (0) - Éxito
- `ERROR_INVALID_PARAM` (1) - Parámetro inválido
- `ERROR_NOT_IN_GAME` (2) - No está en juego
- `ERROR_EXECUTION_FAILED` (3) - Fallo en ejecución
- `ERROR_UNKNOWN_COMMAND` (4) - Comando desconocido
- `ERROR_SERVER_BUSY` (5) - Servidor ocupado

#### `PlayerPositionPayload`
Retornado por `get_player_pos()`:
```python
result, pos = client.get_player_pos()
if result == ResultCode.SUCCESS:
    print(f"Pos: {pos.position}")  # Vector3
    print(f"Rot: {pos.rotation}")  # Vector3
    print(f"Válido: {pos.valid}")  # bool
```

## Ejemplo completo

```python
#!/usr/bin/env python3
"""
Ejemplo de uso del cliente IPC
"""

from ipc_client import IPCClient, Vector3, ResultCode, result_to_string

def main():
    client = IPCClient()

    print("Conectando al servidor IPC...")
    if not client.connect():
        print("ERROR: No se pudo conectar")
        return

    try:
        # Ping
        print("\n[1] Ping...")
        if client.ping():
            print("    -> OK")

        # Obtener posición
        print("\n[2] Posición del jugador:")
        result, pos = client.get_player_pos()
        if result == ResultCode.SUCCESS:
            print(f"    -> Posición: ({pos.pos_x:.2f}, {pos.pos_y:.2f}, {pos.pos_z:.2f})")

        # Usar skill
        print("\n[3] Usando skill 1001...")
        result = client.use_skill(1001)
        print(f"    -> {result_to_string(result)}")

        # Atacar
        print("\n[4] Atacando...")
        result = client.attack(force=True, lock_movement=False)
        print(f"    -> {result_to_string(result)}")

        # Mover
        print("\n[5] Moviendo...")
        from_pos = Vector3(0, 0, 0)
        to_pos = Vector3(100, 0, 100)
        result = client.move_to_location(from_pos, to_pos)
        print(f"    -> {result_to_string(result)}")

    except KeyboardInterrupt:
        print("\nInterrumpido")
    finally:
        client.disconnect()
        print("\nDesconectado")

if __name__ == "__main__":
    main()
```

## Ejecutar

```bash
python ipc_client.py
```

## Notas

- El juego debe estar corriendo con el DLL `ue4_base` inyectado
- El servidor IPC se conecta mediante el Named Pipe `\\.\pipe\ue4_base_ipc`
- Las direcciones de actor para `select_target()` deben obtenerse del SDK/dumps del juego
