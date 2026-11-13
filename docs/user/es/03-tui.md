# TUI LOFITERM

[🇫🇷 Français](../fr/03-interface-tui.md) · [🇬🇧 English](../en/03-tui.md) · 🇪🇸 Español

La TUI ncursesw es una interfaz de pantalla completa para terminales interactivos. Usa el mismo agente, transporte y políticas de herramientas que la CLI clásica.

## Seis vistas

| Tecla | Vista | Contenido |
|---|---|---|
| F1 | Conversación | Historial, streaming y compositor. |
| F2 | Herramientas | Herramientas declaradas y actividad realmente observada. |
| F3 | Actividad | Contadores HTTP/SSE, duraciones, fragmentos de texto y detalles de herramientas conservados. |
| F4 | Diagnóstico | Errores y diagnósticos observados. |
| F5 | Configuración | Valores disponibles en modo de solo lectura; secretos ocultos. |
| F6 | Ayuda | Primeros pasos, atajos, Acerca de y límites. |

Las vistas muestran información disponible y no inventan progreso. F3 aclara que los fragmentos de texto no son tokens y muestra `PARTIAL` cuando faltan detalles de eventos.

## Teclado y ratón

- F1–F6 seleccionan vistas.
- Tab cambia el foco. Escape cierra una sección abierta o vuelve a Conversación.
- Las flechas mueven el cursor del compositor o la selección del menú según el foco.
- Enter inserta una línea cuando el compositor tiene el foco; F9 envía una vez desde Conversación.
- Page Up/Page Down desplazan la vista actual. F10 solicita salir.
- `M` alterna Markdown/texto plano cuando el contenido de Conversación tiene el foco.
- Un clic en una pestaña selecciona su vista; un clic en el compositor le da el foco; las secciones F5/F6 se abren o seleccionan con clic. La rueda desplaza el contenido activo.

La selección nativa de texto depende del terminal; Ghostty puede requerir arrastrar con Mayús. En `WRITE APPROVAL`, el desafío se introduce en un campo separado del compositor. Ningún clic o atajo de navegación concede aprobación de escritura.

## Markdown, código y Unicode

Las respuestas usan un subconjunto seguro de Markdown (títulos, listas, citas, énfasis, código y enlaces como texto). Los enlaces no se abren, no se interpreta HTML y no se ejecutan secuencias ANSI del modelo. `M` alterna el formato y el texto plano. Los bloques de código cerrados pueden tener color para C++, Python, Bash, JSON, YAML y diff; los bloques incompletos permanecen provisionales. El aspecto depende del terminal, la fuente y la configuración regional. No se afirma conformidad Unicode universal.

## Terminales pequeños y salida

La distribución admite dimensiones compactas e informa cuando la terminal es demasiado pequeña para enviar. Aumenta el tamaño antes de F9 si se indica. F10 solicita cerrar; un turno de red o herramienta activo puede terminar antes de salir. La sesión ncurses restaura los modos de terminal, sujeto al comportamiento de este.

## Captura de referencia

![Vista Conversación capturada del binario real en un PTY de tmux](../../assets/screenshots/tui-f1-pty-capture.svg)

Esta captura sin datos se generó a partir de la cuadrícula ncurses emitida por el binario en el SHA documentado en el README. No es una captura de Ghostty ni una calificación física. Las otras vistas están en el [README](../../../README.es.md#vista-previa-de-la-tui).

Consulta [primeros pasos](01-primeros-pasos.md), [herramientas](04-herramientas-workspace.md) y [seguridad](05-seguridad-y-limites.md).
