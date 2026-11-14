# Tutoriales, solución de problemas y preguntas frecuentes

[🇫🇷 Français](../fr/06-tutoriels-depannage-faq.md) · [🇬🇧 English](../en/06-tutorials-troubleshooting-faq.md) · 🇪🇸 Español

## Tutorial: preguntar sobre un archivo

1. Coloca una copia no sensible del archivo en el workspace efectivo.
2. Pide al modelo que lea esa ruta relativa y resuma un punto concreto.
3. Comprueba la ruta si el modelo llama a `read_file`.
4. Contrasta la respuesta con el archivo original.

El límite de lectura es 50 000 bytes; la herramienta no admite archivos binarios.

## Tutorial: buscar una frase

Coloca los archivos de texto pertinentes en el workspace y pide una búsqueda literal. `search_files` devuelve hasta 50 líneas coincidentes; para un corpus grande, reduce el workspace o concreta la consulta.

## Tutorial: crear un archivo nuevo

Pide un solo archivo con una ruta relativa y describe el contenido deseado. Cuando aparezca la aprobación, compara el destino y cada línea con tu solicitud. Deniega si ya existe o si la pantalla muestra otra cosa. Nunca reutilices un desafío anterior.

## Solución de problemas

| Síntoma | Qué revisar |
|---|---|
| `Could not contact LLM_BASE_URL` o error HTTP | Comprueba URL completa, puerto, servidor, modelo y autenticación. No pegues secretos en registros compartidos. |
| El flujo comienza y luego falla | Revisa los registros del servidor; SSE debe terminar en `[DONE]`, sin errores de protocolo ni truncamiento. |
| `write_file` no disponible en CLI | La aprobación requiere stdin y stderr interactivos. Pipes o ausencia de TTY fallan de forma segura. |
| No se encuentran archivos | Comprueba el workspace efectivo: directorio de inicio en fuentes, ruta XDG en el paquete. |
| La TUI no se ve bien o faltan teclas | Usa un terminal UTF-8 interactivo con ncursesw; prueba una ventana mayor. El ratón depende del terminal. |
| Inicio rechazado | Comprueba Landlock del kernel y restricciones del contenedor. LOFITERM no funciona sin confinamiento. |

## Preguntas frecuentes

**¿LOFITERM es el nombre del ejecutable?** No siempre: el nombre público es LOFITERM, una compilación de fuentes produce `agent` y el paquete local proporciona el comando `lofiterm`.

**¿Las herramientas pueden ejecutar comandos shell?** No se declara ninguna herramienta de shell.

**¿Puedo pedir que sobrescriba un archivo?** No. Se rechazan `overwrite:true` y destinos existentes.

**¿F3 muestra tokens por segundo?** No. Muestra bytes, tramas SSE, fragmentos de texto y duraciones observadas, sin tratarlos como tokens.

**¿Se guardan las conversaciones después de salir?** El historial vive en la memoria del proceso y no se restaura al siguiente inicio. Los archivos creados en el workspace permanecen en disco.

**¿Se puede redistribuir el paquete?**

El código original de LOFITERM se distribuye bajo la licencia 0BSD, que permite su redistribución. El repositorio contiene una receta de paquete Debian 13 amd64 calificada localmente, pero actualmente no publica ninguna release oficial ni un repositorio APT público. Toda redistribución binaria debe cumplir las licencias y los avisos de las dependencias de terceros y someterse a una calificación adecuada.

Abre el [índice documental](../../README.md) para acceder a los otros idiomas y guías.
