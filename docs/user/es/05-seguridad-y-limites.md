# Seguridad y limitaciones

[🇫🇷 Français](../fr/05-securite-limites.md) · [🇬🇧 English](../en/05-security-and-limits.md) · 🇪🇸 Español

## Confinamiento Linux

Antes del REPL, la aplicación activa Landlock y se niega a arrancar si el kernel o los permisos no lo permiten. En el workspace, el proceso recibe los accesos necesarios para leer, escribir, crear y eliminar según las reglas; se deniega la ejecución. Algunas rutas de certificados y resolución de nombres se pueden leer para TLS/red. Consulta la política normativa de [`AGENTS.md`](../../../AGENTS.md).

Landlock restringe **este proceso**; no lo aísla de otro proceso con el mismo UID. Otro proceso del mismo usuario puede cambiar archivos del workspace simultáneamente. Las protecciones de la aplicación no son una barrera frente a una cuenta local hostil o un proceso paralelo.

## Consentimiento para escribir

Cada creación `write_file` requiere un desafío exacto y específico de la solicitud después de presentar todo el contenido. Denegación, indisponibilidad, expiración, EOF o cierre cancelan la operación. No se permite sobrescribir. En Linux, la publicación usa un archivo temporal anónimo y publicación sin reemplazo cuando es compatible; no cambia silenciosamente a un temporal con nombre menos seguro. No se usa `fsync`, por lo que no se garantiza conservar una creación aprobada tras un fallo o corte eléctrico.

La aprobación no evita una decisión del usuario mal informada. Inspecciona destino, directorios padre y contenido; no apruebes solo porque lo pidió el modelo.

## Datos y privacidad

- La clave API procede del entorno o de la configuración privada del paquete y no se muestra en F5.
- Las métricas conservan contadores e identificadores acotados; no guardan texto de conversaciones, argumentos o resultados de herramientas como historial de métricas.
- El contenido enviado al LLM queda sujeto a las políticas del servidor y de la red. Un modelo local no implica automáticamente que el servidor no registre nada.
- El historial de conversación permanece en la memoria del proceso; los archivos del workspace de una compilación fuente permanecen en disco en la ruta elegida. LOFITERM no ofrece memoria persistente entre sesiones.
- El repositorio no declara licencia de redistribución. El paquete Debian es para instalación/evaluación local, no una autorización de distribución pública.

## Límites operativos establecidos

La lectura de archivos está limitada a 50 000 bytes; la búsqueda devuelve 50 líneas, pero una línea muy larga aún puede generar una salida grande. `list_files` no tiene límite codificado de entradas. El contexto conversacional se limita por número de mensajes y se recorta por turnos completos, no mediante tokenizer ni presupuesto de tokens. Un turno grande puede superar el límite de referencia.

Landlock, la prohibición de sobrescritura y el consentimiento por solicitud reducen el riesgo, pero no demuestran que las respuestas LLM sean correctas, que el servidor sea seguro o que no exista concurrencia de archivos. Consulta los [informes fechados](../../README.md#reports) para ver los límites de calificación y sus SHA.
