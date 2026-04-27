#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────
# sort_file.sh — FASE 1 del pipeline
#
# Proposito:
#   Leer un archivo de texto, tokenizarlo en palabras, pasar a minusculas,
#   y ordenar lexicograficamente. Cada palabra queda en una linea.
#
# Uso:
#   ./sort_file.sh <archivo_entrada> <archivo_salida>
#
# Ejemplo:
#   ./sort_file.sh arch1.txt sorted1.txt
#
# Por que en archivo y no por stdout:
#   El shell padre (nuestro shell de flujos) NO implementa pipes entre nodos
#   del DAG. La comunicacion entre etapas es vía archivos en disco. Por eso
#   el script recibe la ruta de salida como argumento y escribe ahi.
# ─────────────────────────────────────────────────────────────────────────

set -euo pipefail

if [ $# -ne 2 ]; then
    echo "[sort] uso: $0 <entrada> <salida>" >&2
    exit 2
fi

entrada="$1"
salida="$2"

if [ ! -r "$entrada" ]; then
    echo "[sort] no se puede leer: $entrada" >&2
    exit 3
fi

# tr -cs 'clase' '\n'  : reemplaza cualquier caracter FUERA de la clase por
#                       newline, colapsando secuencias. Esto tokeniza.
# tr '[:upper:]' '[:lower:]' : pasa a minusculas (normalizacion).
# sort                 : ordena lexicograficamente. (NO usamos sort -u porque
#                       queremos preservar las frecuencias para la fase 3.)
tr -cs 'A-Za-z0-9áéíóúñÁÉÍÓÚÑ' '\n' < "$entrada" \
    | tr '[:upper:]' '[:lower:]' \
    | grep -v '^$' \
    | sort > "$salida"

count=$(wc -l < "$salida")
echo "[sort] $entrada -> $salida ($count palabras)"
