# Caja Fuerte — Huella Digital + PIN (ESP32)

Sistema de caja fuerte electrónica con doble autenticación: huella digital y PIN numérico. Desarrollado sobre la plataforma ESP32 como proyecto de electrónica embebida.

---

## ¿Qué hace?

- Abre la caja mediante **huella digital** (sensor AS608) o **PIN de 4 dígitos** (teclado 4x4)
- Muestra el estado del sistema en una **pantalla LCD 16x2**
- Controla el cerrojo físico con un **servomotor SG90**
- Soporta hasta **18 usuarios** (IDs 3 al 20), cada uno con huella, PIN o ambos
- Guarda los PIN en la **flash NVS** del ESP32 (persisten entre reinicios)
- Incluye un **modo administrador** para agregar, eliminar y modificar usuarios desde el propio dispositivo

---

## Hardware necesario

| Componente | Cantidad |
|---|---|
| ESP32 DevKit v1 | 1 |
| Sensor de huella AS608 (Adafruit) | 1 |
| Pantalla LCD 16x2 con módulo I2C | 1 |
| Teclado matricial 4x4 | 1 |
| Servomotor SG90 | 1 |
| Fuente 5V / 2A | 1 |

---

## Conexiones principales

| Módulo | Pin módulo | Pin ESP32 |
|---|---|---|
| LCD (I2C) | SDA / SCL | GPIO 21 / 22 |
| Sensor huella | TX / RX | GPIO 16 / 17 |
| Teclado filas | F1–F4 | GPIO 13, 12, 14, 27 |
| Teclado columnas | C1–C4 | GPIO 26, 25, 33, 32 |
| Servo SG90 | PWM | GPIO 4 |

---

## Librerías requeridas

Instalar desde el Library Manager del IDE de Arduino:

- `LiquidCrystal_I2C`
- `Keypad`
- `Adafruit_Fingerprint`
- `ESP32Servo`
- `Preferences` *(incluida en el core ESP32)*

---

## Uso rápido

| Acción | Tecla |
|---|---|
| Autenticar por huella | `*` |
| Autenticar por PIN | `#` |
| Entrar al menú admin | Huella de admin (ID 1 o 2) |
| Salir del menú admin | `#` |

**Dentro del menú admin:**

| Tecla | Función |
|---|---|
| `A` | Agregar usuario |
| `5` | Eliminar usuario |
| `C` | Abrir caja manualmente |
| `D` | Modificar usuario |

---

## Mejoras respecto a la versión original

1. **Bug en `buscarUsuarioPorPIN()`** — el `return id` estaba fuera del `if` de comparación
2. **Constantes con nombre** — reemplaza números mágicos (`3`, `20`, `127`, `1`, `2`)
3. **Código sin uso eliminado** — se sacaron `PIN_MAESTRO`, `struct Usuario` y `usuarios[20]`
4. **Función muerta eliminada** — se removió `eliminarHuella()` que nunca se llamaba
5. **Timeout en sensor** — `esperarDedo()` evita que el sistema quede colgado indefinidamente
6. **`leerHuella()` no bloquea** — vuelve solo a la pantalla principal si no hay dedo
7. **`registrarHuellaEnID()` devuelve `bool`** — informa si la huella se guardó o no
8. **`modificarPIN()` vuelve al menú** — ya no deja la pantalla trabada al cancelar
9. **`capturarID()` valida rango** — evita que `0` pase como ID válido
10. **Comentarios en cada función** — qué hace, parámetros y valor de retorno

---

## Nota de seguridad

Los PIN se guardan en texto plano en Preferences (NVS). Para mayor seguridad se recomienda guardar un hash SHA-256 del PIN en lugar del valor directo.

---

## Autores

Por:  Ulunqui Gabriel, Chocobar Antonio, Cardozo Julieta y Romano Pablo.

Proyecto desarrollado en el Instituto de Educación Superior N° 6001 "Gral. Manuel Belgrano" — Tecnicatura Superior en Análisis de Sistemas y Desarrollo de Software.
