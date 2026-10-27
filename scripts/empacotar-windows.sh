#!/usr/bin/env bash
# Monta o pacote distribuivel do executavel Windows em dist/Projeto-QR-Code-Windows/,
# com as DLLs do MSYS2 das quais o executavel depende, a fonte e os exemplos.
#
# Uso (no shell MSYS2 UCRT64, a partir da raiz do projeto):
#   bash scripts/empacotar-windows.sh
#
# A variavel MSYS_BIN aponta para o diretorio das DLLs do ambiente usado na
# compilacao (por padrao, o UCRT64 do MSYS2).
set -euo pipefail

MSYS_BIN="${MSYS_BIN:-/c/msys64/ucrt64/bin}"
OBJDUMP="${OBJDUMP:-$MSYS_BIN/objdump}"
EXE="proj2.exe"
SAIDA="dist/Projeto-QR-Code-Windows"

[ -x "$OBJDUMP" ] || { echo "objdump nao encontrado em $OBJDUMP"; exit 1; }
[ -d "$MSYS_BIN" ] || { echo "diretorio de DLLs nao encontrado: $MSYS_BIN"; exit 1; }
[ -f "$EXE" ] || { echo "$EXE nao encontrado: rode 'mingw32-make' antes"; exit 1; }

rm -rf "$SAIDA"
mkdir -p "$SAIDA"

cp "$EXE" "$SAIDA/"
mkdir -p "$SAIDA/assets/fonts"
cp assets/fonts/DejaVuSans.ttf assets/fonts/LICENSE_DEJAVU.txt "$SAIDA/assets/fonts/"
cp -r exemplos "$SAIDA/"
cp README.md LICENSE "$SAIDA/"
[ -f INTEGRANTES.txt ] && cp INTEGRANTES.txt "$SAIDA/"

# Copia, em profundidade, toda DLL importada que exista no diretorio do MSYS2.
# As DLLs do proprio Windows (system32) ficam de fora.
pendentes=("$SAIDA/$EXE")
copiadas=""
while [ ${#pendentes[@]} -gt 0 ]; do
    atual="${pendentes[0]}"
    pendentes=("${pendentes[@]:1}")
    for dll in $("$OBJDUMP" -p "$atual" | awk '/DLL Name:/ { print $3 }'); do
        case " $copiadas " in *" $dll "*) continue ;; esac
        if [ -f "$MSYS_BIN/$dll" ]; then
            cp "$MSYS_BIN/$dll" "$SAIDA/"
            copiadas="$copiadas $dll"
            pendentes+=("$SAIDA/$dll")
        fi
    done
done

echo "Pacote montado em $SAIDA"
echo "DLLs incluidas:$copiadas" | tr ' ' '\n'

# Compacta o pacote para anexar como release do repositorio.
if command -v zip >/dev/null 2>&1; then
    (cd dist && rm -f Projeto-QR-Code-Windows.zip && zip -qr Projeto-QR-Code-Windows.zip Projeto-QR-Code-Windows)
    echo "ZIP gerado em dist/Projeto-QR-Code-Windows.zip"
else
    echo "zip nao encontrado; compacte dist/Projeto-QR-Code-Windows manualmente"
    echo "  (no PowerShell: Compress-Archive -Path dist\Projeto-QR-Code-Windows -DestinationPath dist\Projeto-QR-Code-Windows.zip)"
fi
