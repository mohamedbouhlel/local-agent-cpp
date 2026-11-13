# Herramientas del workspace

[🇫🇷 Français](../fr/04-outils-workspace.md) · [🇬🇧 English](../en/04-workspace-tools.md) · 🇪🇸 Español

El agente declara cuatro funciones al servidor LLM. El modelo decide cuándo llamarlas; no son comandos de shell del REPL.

| Herramienta | Función | Límites visibles en el código |
|---|---|---|
| `list_files` | Enumerar recursivamente archivos no ocultos. | No existe un límite codificado de entradas para esta salida. Mantén el workspace acotado. |
| `read_file` | Leer un archivo de texto con ruta relativa. | Hasta 50 000 bytes; se rechazan archivos binarios o con varios enlaces físicos. |
| `search_files` | Buscar una cadena literal en archivos de texto. | Devuelve hasta 50 líneas coincidentes; una línea individual no tiene límite de tamaño. |
| `write_file` | Crear un archivo de texto y directorios padre faltantes. | Contenido limitado a 200 000 bytes; aprobación explícita en cada llamada; se rechazan destinos existentes y `overwrite:true`. |

Las rutas deben ser relativas al workspace. Se rechazan rutas absolutas y componentes `..`. El recorrido omite nombres ocultos y evita enlaces simbólicos; los cambios simultáneos de otro proceso del mismo usuario siguen siendo una limitación (consulta [seguridad](05-seguridad-y-limites.md)).

## Aprobar una creación

La solicitud muestra el workspace efectivo, la ruta relativa, el tamaño en bytes, los directorios padre faltantes y el contenido completo en una forma escapada reversible. En la CLI, escribe exactamente `yes <desafío mostrado>` y pulsa Enter. En la TUI, introduce el mismo desafío completo en el diálogo.

Antes de aprobar:

1. Comprueba el workspace y la ruta destino.
2. Lee el contenido presentado, incluidos los caracteres de control escapados.
3. Aprueba únicamente esa solicitud. Cada llamada obtiene un desafío nuevo.
4. Deniega o cancela si algo no coincide. Sin decisión, EOF o expiración, la operación falla de forma segura.

Los directorios padre solo se crean después de aprobar. La aprobación autoriza una creación; no permite otra ruta, otro contenido ni sobrescritura.

## Ejemplo de solicitud más segura

Pide al modelo un único archivo con una ruta relativa precisa, limita el número de archivos esperados y describe el contenido. Esto facilita revisar el resultado, pero no sustituye la inspección: el modelo puede interpretar una instrucción de forma inesperada. Rechaza cualquier solicitud fuera del alcance descrito.

El historial de actividad de F2 y las métricas F3 no son registros completos garantizados; las pérdidas de eventos pueden dejar detalles parciales. No retienen argumentos o resultados sensibles como historial de herramientas.
