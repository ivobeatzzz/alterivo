#!/bin/bash
# Crea el instalador de Mac (alterIVO-Mac-Instalador.pkg) a partir de la compilación.
# Uso: bash packaging/mac/build-pkg.sh [versión]
set -euo pipefail

VERSION="${1:-1.0.0}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ART="$ROOT/build/AlterIvo_artefacts/Release"
WORK="$ROOT/build/pkg"
DIST="$ROOT/dist"

rm -rf "$WORK"
mkdir -p "$WORK/vst3" "$WORK/au" "$WORK/out" "$DIST"

cp -R "$ART/VST3/alterIVO.vst3" "$WORK/vst3/"
cp -R "$ART/AU/alterIVO.component" "$WORK/au/"

# Firma ad-hoc (necesaria en Apple Silicon)
codesign --force --deep -s - "$WORK/vst3/alterIVO.vst3"
codesign --force --deep -s - "$WORK/au/alterIVO.component"

chmod +x "$ROOT/packaging/mac/scripts/postinstall"

# Que el instalador no "busque" el plugin en otra carpeta: siempre a la ruta estándar
for kind in vst3 au; do
    pkgbuild --analyze --root "$WORK/$kind" "$WORK/$kind.plist"
    plutil -replace 0.BundleIsRelocatable -bool NO "$WORK/$kind.plist"
done

pkgbuild --root "$WORK/vst3" \
         --component-plist "$WORK/vst3.plist" \
         --install-location "/Library/Audio/Plug-Ins/VST3" \
         --identifier com.ivobeatz.alterivo.vst3 \
         --version "$VERSION" \
         "$WORK/out/alterIVO-VST3.pkg"

pkgbuild --root "$WORK/au" \
         --component-plist "$WORK/au.plist" \
         --install-location "/Library/Audio/Plug-Ins/Components" \
         --identifier com.ivobeatz.alterivo.au \
         --scripts "$ROOT/packaging/mac/scripts" \
         --version "$VERSION" \
         "$WORK/out/alterIVO-AU.pkg"

productbuild --distribution "$ROOT/packaging/mac/distribution.xml" \
             --resources "$ROOT/packaging/mac/resources" \
             --package-path "$WORK/out" \
             "$DIST/alterIVO-Mac-Instalador.pkg"

echo "Instalador creado: $DIST/alterIVO-Mac-Instalador.pkg"
