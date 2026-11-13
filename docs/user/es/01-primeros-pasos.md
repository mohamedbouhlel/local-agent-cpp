# Primeros pasos, instalación y configuración

[🇫🇷 Français](../fr/01-demarrage-installation.md) · [🇬🇧 English](../en/01-getting-started.md) · 🇪🇸 Español

## Requisitos

- Linux con soporte Landlock activable; de lo contrario, la aplicación se niega a iniciarse.
- CMake 3.20 o posterior y compilador C++20.
- libcurl y ncursesw para compilar.
- Acceso de red durante la configuración de CMake para descargar `nlohmann/json` 3.11.3 si aún no está disponible.
- Servidor LLM compatible con OpenAI Chat Completions y streaming SSE.

## Compilar desde fuentes

En Debian/Ubuntu, instala las dependencias y compila:

```sh
sudo apt install build-essential cmake libcurl4-openssl-dev libncurses-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

Esto genera `build/agent`; no es una instalación del sistema. No existe un objetivo `make install`.

## Preparar el entorno

El ejecutable compilado solo lee variables del entorno. Para utilizar un archivo local:

```sh
cp .env.example .env
chmod 600 .env
# Edita .env con un editor local.
set -a; . ./.env; set +a
```

No copies la clave API en incidencias, capturas ni historial compartido del shell. `.env` lo carga el shell; el programa no lo analiza.

## Primer inicio sin crear el workspace en el repositorio

En una compilación desde fuentes, `workspace/` se crea respecto al directorio de trabajo actual del proceso. Inicia desde un directorio de usuario dedicado:

```sh
repo_dir=$PWD
mkdir -p "$HOME/lofiterm-run"
cd "$HOME/lofiterm-run"
"$repo_dir/build/agent" --ui=tui
```

Usa `--ui=classic` para la CLI clásica; también es el modo predeterminado. Si hace falta, el agente crea `~/lofiterm-run/workspace`, separado del árbol de fuentes.

## Instalar el paquete local Debian

La receta para un paquete local Debian 13 amd64 está documentada en [`docs/INSTALL_DEBIAN13.md`](../../INSTALL_DEBIAN13.md). El paquete instala `/usr/bin/lofiterm`; se instala con `sudo apt install ./lofiterm_1.0.0-1_amd64.deb`. No es un repositorio APT ni un paquete autorizado para distribución pública: el código fuente no declara licencia de redistribución.

El paquete separa aplicación y datos de usuario: configuración en `${XDG_CONFIG_HOME:-~/.config}/lofiterm/config` y workspace en `${XDG_DATA_HOME:-~/.local/share}/lofiterm/workspace`. `LOFITERM_CONFIG_FILE` y `LOFITERM_DATA_DIR` aceptan rutas absolutas para cambiar esas ubicaciones. El lanzador cambia al directorio de datos antes de iniciar el agente; por tanto, el directorio actual no determina el workspace del paquete.

<a id="configuracion-del-servidor-llm"></a>
## Configuración del servidor LLM

| Variable | Valor predeterminado | Función |
|---|---|---|
| `LLM_BASE_URL` | `http://localhost:8080/v1/chat/completions` | URL completa del endpoint. |
| `LLM_MODEL` | `local-model` | Identificador esperado por el servidor. |
| `LLM_API_KEY` | vacío | Clave Bearer opcional. |
| `LLM_CONNECT_TIMEOUT_SECONDS` | 10 s (o `min(LLM_TIMEOUT_SECONDS,10)`) | Tiempo límite de conexión. |
| `LLM_IDLE_TIMEOUT_SECONDS` | normalmente 60 s | Máximo de inactividad entre datos de respuesta. |
| `LLM_TOTAL_TIMEOUT_SECONDS` | 300 s | Duración total máxima. |
| `LLM_TIMEOUT_SECONDS` | 60 s | Compatibilidad: fallback de inactividad del ejecutable. |

Los tres plazos actuales son independientes y deben ser enteros positivos. El cliente exige un flujo SSE completo según su contrato e informa de los flujos incompletos como errores.

El archivo de configuración del paquete usa la forma simple `NOMBRE=VALOR` y permite cuatro claves: `LLM_BASE_URL`, `LLM_MODEL`, `LLM_API_KEY`, `LLM_TIMEOUT_SECONDS`. El entorno tiene prioridad. El ejemplo se instala en `/usr/share/doc/lofiterm/lofiterm.conf.example`; cópialo con permisos privados, según el manual Debian.

## Comprobar el servidor

Empieza con una pregunta que no pida operaciones sobre archivos. Si falla, comprueba el endpoint, el modelo y la autenticación del servidor. Este repositorio no ha calificado todos los proveedores compatibles con OpenAI; pueden diferir en SSE y llamadas a herramientas.

Continúa con la [interfaz clásica](02-interfaz-clasica.md), la [TUI](03-tui.md) o el [índice documental](../../README.md).
