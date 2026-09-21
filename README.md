# DSi IA — DS Escolar

Asistente educativo offline para Nintendo DS/DSi.

## Contenido
- `source/main.cpp`: programa principal.
- `data/knowledge.txt`: base de conocimiento escolar de más de 2 MB.
- `Makefile`: compilación con devkitARM/libnds.
- `.github/workflows/build.yml`: compilación automática con GitHub Actions.

## Comandos conservados
`XD`, `42`, `bruh`, `Quien creo esto`, `Said`, `Inteligente`, `Calculadora`, `CALC`, `APRENDER`, `GUARDAR`, `ESTADO MEMORIA`, `BORRAR CONVERSACION`, `BORRAR MEMORIA`, `TRADUCIR`, `AYUDA` y `dsi`.

## Memoria
La conversación y lo aprendido se guardan en `DSiIA.dat` cuando el almacenamiento FAT está disponible.

## Compilación
Requiere devkitPro/devkitARM con libnds y nds-dev. El workflow de GitHub Actions está preparado para producir `DSi-IA.nds` como artefacto.

## Nota
El proyecto fue revisado con una comprobación de sintaxis C++ independiente de libnds. La compilación final del `.nds` debe realizarse con el toolchain de Nintendo DS.
