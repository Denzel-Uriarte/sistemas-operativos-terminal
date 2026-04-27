#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────
# concatenar.sh — FASE 2 del pipeline
#
# Proposito:
#   Unir varios archivos ya ordenados en un solo archivo. Como cada entrada
#   ya viene ordenada, podriamos usar `sort -m` (merge) para preservar el
#   orden global; pero para word cloud da igual: lo que importa son las
#   frecuencias, no el orden final. Usamos `cat` simple que es mas rapido.
#
# Uso:
#   ./concatenar.sh <salida> <entrada1> <entrada2> ... <entradaN>
#
# Ejemplo:
#   ./concatenar.sh concatenated.txt sorted1.txt sorted2.txt sorted3.txt
#
# Decision de diseno:
#   El archivo de salida va como PRIMER argumento. Asi el script soporta N
#   archivos de entrada variables sin ambiguedad. Esto difiere de `cat` que
#   pone la salida con >, pero aqui no podemos depender de redirecciones
#   porque el JDL no las soporta.
# ─────────────────────────────────────────────────────────────────────────

set -euo pipefail

if [ $# -lt 2 ]; then
    echo "[concat] uso: $0 <salida> <entrada1> [entrada2 ...]" >&2
    exit 2
fi

salida="$1"
shift   # las entradas son todos los args restantes

# Validar que todas las entradas existan antes de empezar
for f in "$@"; do
    if [ ! -r "$f" ]; then
        echo "[concat] no se puede leer: $f" >&2
        exit 3
    fi
done

cat "$@" > "$salida"

count=$(wc -l < "$salida")
echo "[concat] combinados $# archivos en $salida ($count palabras)"
