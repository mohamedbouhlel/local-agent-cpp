# Manual de usuario — LOFITERM

[🇫🇷 Français](USAGE.md) · [🇬🇧 English](USAGE.en.md) · 🇪🇸 Español

Este manual describe el comportamiento verificado en la rama documental con SHA `39646ffade1c71ece9995fde7770dcf428e45d3a`. Los ensayos manuales y la calificación de otros SHA se conservan por separado y no se presentan como repetidos aquí.

## 1. Elegir una interfaz

Desde una compilación del código fuente, define la ruta del repositorio e inicia desde un workspace de usuario dedicado:

```sh
cd /ruta/a/local-agent-cpp
repo_dir=$PWD
mkdir -p "$HOME/lofiterm-workspace"
cd "$HOME/lofiterm-workspace"
"$repo_dir/build/agent" --ui=classic
"$repo_dir/build/agent" --ui=tui
```

Sin opciones se inicia la interfaz clásica. El ejecutable compilado se llama `agent`; LOFITERM es el nombre público de la interfaz.

Con el paquete local Debian 13, ejecuta `lofiterm --ui=classic` o `lofiterm --ui=tui`. Su lanzador sitúa el proceso en un workspace del usuario bajo XDG; esto es distinto del `workspace/` relativo al directorio de inicio de una compilación desde fuentes. Consulta [instalación Debian 13](docs/INSTALL_DEBIAN13.md).

## 2. Configuración

El ejecutable compilado desde fuentes lee las variables del entorno mediante `std::getenv()`. No lee `.env` directamente.

```sh
repo_dir=$PWD
cp .env.example .env
chmod 600 .env
# Edita .env sin compartir la clave.
set -a; . ./.env; set +a
mkdir -p "$HOME/lofiterm-workspace"
cd "$HOME/lofiterm-workspace"
"$repo_dir/build/agent" --ui=tui
```

En una compilación desde fuentes, el workspace es `workspace/` bajo el directorio de trabajo actual. Inicia desde este directorio de usuario para reutilizar el mismo workspace.

| Variable | Valor predeterminado del ejecutable | Función |
|---|---:|---|
| `LLM_BASE_URL` | `http://localhost:8080/v1/chat/completions` | Endpoint HTTP Chat Completions compatible con OpenAI. |
| `LLM_MODEL` | `local-model` | Identificador enviado al servidor. |
| `LLM_API_KEY` | vacío | Si se define, se envía en el encabezado Bearer. |
| `LLM_CONNECT_TIMEOUT_SECONDS` | `min(LLM_TIMEOUT_SECONDS, 10)`; normalmente 10 | Límite para establecer la conexión. |
| `LLM_IDLE_TIMEOUT_SECONDS` | `LLM_TIMEOUT_SECONDS`; normalmente 60 | Máximo sin encabezados ni bytes de respuesta; los datos recibidos reinician el plazo. |
| `LLM_TOTAL_TIMEOUT_SECONDS` | `300` | Límite duro de duración total de la solicitud. |
| `LLM_TIMEOUT_SECONDS` | `60` | Variable heredada, fallback del tiempo de inactividad del ejecutable. |

Los valores deben ser enteros positivos. Los valores no válidos generan una advertencia en `stderr` y utilizan el fallback del ejecutable. Los tiempos de conexión, inactividad y duración total son independientes. Un flujo activo puede superar 60 segundos, pero sigue limitado por el plazo total.

**Paquete Debian:** la configuración del usuario es un archivo `NAME=VALUE`, sin sintaxis de shell, expansión ni comillas; la ruta predeterminada es `~/.config/lofiterm/config`. El ejemplo instalado incluye `LLM_BASE_URL`, `LLM_MODEL`, `LLM_API_KEY` y `LLM_TIMEOUT_SECONDS`. Las variables del entorno tienen prioridad. Consulta la [guía de configuración](docs/user/es/01-primeros-pasos.md#configuracion-del-servidor-llm).

## 3. Interfaz clásica y REPL

El modo clásico muestra el indicador `>`, envía una línea no vacía al pulsar Enter y presenta la respuesta progresivamente. `exit` exacto termina el REPL; Ctrl-D también finaliza la entrada. Las líneas vacías no generan solicitudes.

Para enviar un bloque:

```text
> :multi
... Resume los archivos de notas.
... Conserva los nombres de archivo en la respuesta.
... :end
```

`:multi` y `:end` deben ocupar toda la línea. El bloque se envía una sola vez; se conservan líneas vacías, espacios y tabulaciones, normalizando CRLF a LF. No se añade un salto de línea final. Un bloque vacío no se envía; un bloque con líneas vacías sí. EOF antes de `:end` abandona el bloque sin enviarlo. Dentro del bloque, `exit` es texto.

## 4. TUI LOFITERM

Inicia la TUI en una terminal interactiva. F1–F6 seleccionan las seis vistas:

| Tecla | Vista | Información |
|---|---|---|
| F1 | Conversación | Historial, streaming y compositor multilínea. |
| F2 | Herramientas | Herramientas declaradas y actividad observada. |
| F3 | Actividad | Mediciones observadas de HTTP, SSE, fragmentos de texto y herramientas del turno actual/anterior. |
| F4 | Diagnóstico | Errores y diagnósticos observados. |
| F5 | Configuración | Valores disponibles en modo de solo lectura; clave API oculta. |
| F6 | Ayuda | Primeros pasos, teclado, límites e información de versión disponible. |

F9 envía una vez desde Conversación; F10 solicita salir. Tab cambia el foco; Escape vuelve a Conversación o cierra una sección; las flechas y Page Up/Page Down navegan según el foco. Enter añade una línea cuando el compositor tiene el foco. `M` alterna Markdown/texto plano cuando el foco está en el contenido de Conversación. Los clics en pestañas/secciones y la rueda funcionan si la terminal genera esos eventos. La selección de texto puede requerir arrastrar con Mayús, según el terminal.

F3 no muestra velocidad de tokens. `Text fragments` cuenta fragmentos delta, no tokens; las tramas SSE y los bytes HTTP son contadores distintos. `PARTIAL` indica que faltan detalles de eventos; no demuestra que la tarea se haya completado correctamente.

## 5. Herramientas del workspace y aprobación

El modelo puede llamar a estas herramientas:

- `list_files`: enumera recursivamente archivos no ocultos; el código actual no limita el número de entradas de salida.
- `read_file`: lee un archivo de texto relativo al workspace, hasta 50 000 bytes; rechaza archivos binarios y con varios enlaces físicos.
- `search_files`: busca una cadena literal y devuelve hasta 50 líneas coincidentes. No hay límite de longitud para una línea individual. Los archivos ocultos, especiales, simbólicos y con varios enlaces físicos se omiten/rechazan según el recorrido.
- `write_file`: crea un archivo inexistente tras una aprobación explícita. El contenido está limitado a 200 000 bytes; no permite sobrescribir.

El modelo llama a las herramientas durante la conversación; no son comandos manuales de la CLI. Las lecturas no requieren aprobación.

Antes de crear, la interfaz muestra el workspace, la ruta relativa, el tamaño, los directorios padre faltantes y todo el contenido en una representación escapada reversible. En la CLI, escribe la línea completa `yes <desafío actual>` y pulsa Enter; cualquier otra línea deniega. En la TUI, introduce el mismo desafío en el diálogo. Cada desafío pertenece a una sola solicitud. Aprueba solo si la ruta y el contenido coinciden con lo que deseas. Denegación, EOF, cancelación, expiración o indisponibilidad no autorizan la escritura. Los directorios padre solo se crean después de aprobar.

## 6. Confinamiento y límites

En Linux, Landlock se activa antes de iniciar la interfaz. Sin soporte del kernel o permisos suficientes, el inicio falla; el programa no cambia a un modo sin confinamiento. El acceso del proceso se limita al workspace y a las rutas de lectura del sistema documentadas en [AGENTS.md](AGENTS.md). Esto no aísla el proceso de otros procesos del mismo usuario. Los cambios simultáneos del workspace por el mismo UID quedan fuera de la garantía de Landlock. No se garantiza la durabilidad de escrituras tras un fallo del sistema (`fsync` no se utiliza).

El contexto conversacional permanece en memoria durante el proceso. Se eliminan turnos completos antiguos cuando se supera el límite de referencia de 20 mensajes, sin resumen; un turno actual grande puede superar ese límite. No hay memoria persistente entre sesiones ni una caché duradera de la aplicación.

## 7. Errores y solución de problemas

- **No conecta:** comprueba `LLM_BASE_URL`, la disponibilidad del servidor y los plazos. El cliente informa del error de transporte; no pegues una clave API en una incidencia.
- **Respuesta truncada o SSE incompleto:** la ausencia de `[DONE]`, JSON SSE malformado o `finish_reason=length` hace fallar el turno. El texto provisional puede permanecer marcado como incompleto; no se acepta como respuesta final validada.
- **Landlock rechazado:** comprueba el kernel y la política del contenedor; la aplicación no continúa sin confinamiento.
- **Resultado inesperado de una herramienta:** comprueba la ruta relativa y el workspace efectivo. Las compilaciones de fuentes usan el directorio de inicio; el paquete usa rutas de datos XDG.
- **Creación denegada:** significa que no se recibió una aprobación válida. No reutilices una confirmación anterior; cada solicitud nueva genera otro desafío.

Consulta [solución de problemas y preguntas frecuentes](docs/user/es/06-tutoriales-solucion-problemas-faq.md) y el [índice documental](docs/README.md) para ver todos los idiomas.
