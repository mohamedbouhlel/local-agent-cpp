# Interfaz clásica y REPL

[🇫🇷 Français](../fr/02-interface-classique.md) · [🇬🇧 English](../en/02-classic-interface.md) · 🇪🇸 Español

La interfaz clásica lee líneas de la terminal, las envía al servidor configurado y escribe la respuesta progresiva en la salida estándar. Los diagnósticos se envían a `stderr`.

## Inicio

Desde el directorio que contiene `build/agent`:

```sh
./build/agent --ui=classic
```

Sin opción se selecciona el modo clásico. Con el paquete local, usa `lofiterm --ui=classic`. Una compilación desde fuentes utiliza un workspace relativo al directorio de trabajo actual; el paquete usa las rutas XDG del [manual de inicio](01-primeros-pasos.md).

## Intercambios de una línea

- Escribe una línea y pulsa Enter para enviarla.
- Las líneas vacías se ignoran.
- La respuesta se muestra a medida que llegan fragmentos.
- Escribe exactamente `exit` y pulsa Enter para salir; Ctrl-D también termina la entrada.
- Los errores HTTP, SSE o del agente se notifican como fallos. Los datos parciales no se presentan como respuesta final validada.

## Mensajes multilínea

Usa los marcadores exactos:

```text
> :multi
... Primera instrucción.
...
... Última línea.
... :end
```

`:multi` y `:end` deben ocupar cada uno toda la línea. El bloque se envía en un solo turno. Se conservan espacios, tabulaciones y líneas vacías; CRLF se normaliza a LF. No se agrega un salto final. Un bloque vacío no se envía. EOF antes de `:end` abandona el bloque sin enviarlo. `exit` dentro de un bloque es texto. Por tanto, no se puede incluir una línea exacta `:end` como contenido.

## Llamadas a herramientas

El modelo puede decidir llamar a herramientas del workspace durante una respuesta. La CLI muestra un indicador y un resultado resumido; no hay comandos de herramientas que se escriban manualmente. `write_file` pausa la respuesta y muestra una solicitud de aprobación. Inspecciona la ruta y todo el contenido antes de introducir el desafío exacto.

## Salidas y scripts

La salida estándar contiene la respuesta visible para el usuario; los errores y diagnósticos van a `stderr`. La CLI no es un formato JSON ni una API estable. Para pruebas automatizadas, usa los componentes C++ y los mocks del repositorio en vez de analizar el texto mostrado.

Consulta también [herramientas](04-herramientas-workspace.md), [seguridad](05-seguridad-y-limites.md) y las [preguntas frecuentes](06-tutoriales-solucion-problemas-faq.md).
