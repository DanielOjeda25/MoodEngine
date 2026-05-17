#!/usr/bin/env bash
# sizeometer.sh — reporta los archivos .cpp/.h mas grandes del repo.
#
# Establecido en AUDIT-1 (2026-05-17). Regla vigente segun memory rule
# `feedback_file_size_limit`: soft 500 LOC / hard 800 LOC.
#
# Excluye: build*/, _deps/, third-party/, .git/, archivos generados.
# Cuenta lineas totales (no LOC "real" excluyendo comments/blanks) -- la
# convencion del repo trata el conteo crudo como el limite operativo.
#
# Salida: top N archivos ordenados desc por LOC, con flag HARD (>800),
# SOFT (501-800), o OK. Exit code 0 siempre (no falla el build); el dev
# inspecciona el reporte.
#
# Uso:
#   tools/sizeometer.sh [N]    # default N=20
#
# Patron de invocacion en AUDIT-N: tools/sizeometer.sh 30 > docs/audits/AUDIT_N_before.txt

set -euo pipefail

N="${1:-20}"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

cd "$REPO_ROOT"

# Buscar .cpp y .h en src/ y tests/ (codigo nuestro, NO third-party ni
# generated). Excluir build*/, _deps/, .git/ explicitamente.
mapfile -t files < <(find src tests \
    \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) \
    -type f \
    -not -path "*/build*/*" \
    -not -path "*/_deps/*" \
    2>/dev/null | sort)

if [[ ${#files[@]} -eq 0 ]]; then
    echo "sizeometer: ningun archivo encontrado (estas en el repo root?)" >&2
    exit 1
fi

# Generar lineas tabulares: "<loc> <flag> <path>"
declare -a rows=()
hardcount=0
softcount=0
total=0
for f in "${files[@]}"; do
    loc=$(wc -l < "$f" | tr -d ' ')
    total=$((total + loc))
    if [[ "$loc" -gt 800 ]]; then
        flag="HARD"
        hardcount=$((hardcount + 1))
    elif [[ "$loc" -gt 500 ]]; then
        flag="SOFT"
        softcount=$((softcount + 1))
    else
        flag="OK  "
    fi
    rows+=("$(printf '%6d %s %s' "$loc" "$flag" "$f")")
done

# Ordenar desc por LOC (primera columna).
printf '%s\n' "${rows[@]}" | sort -rn -k1 | head -n "$N"

echo
echo "---"
echo "TOTAL: ${#files[@]} archivos, $total LOC."
echo "HARD cap (>800): $hardcount archivos."
echo "SOFT cap (501-800): $softcount archivos."
