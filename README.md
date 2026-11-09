# alterIVO — by IVO Beatz

Manipulador de voz (pitch, formantes, hard-tune y robot) en VST3 para **Mac y Windows**, y AU en Mac.

| Control | Qué hace |
|---|---|
| **PITCH** | ±12 semitonos. En Robot elige la nota (0 = el Do una octava por encima del Do central). |
| **FORMANT** | ±12. Hacia arriba la voz suena más pequeña/aguda; hacia abajo, más grande/grave. |
| **DRIVE** | Saturación tipo válvula a 4x de sobremuestreo, con el nivel compensado. |
| **MIX** | Mezcla seco/procesado, con el seco alineado. |
| **TRANSPOSE / QUANTIZE / ROBOT** | Intervalo fijo / afinación cromática dura / todo a una sola nota. |
| **LINK** | Los formantes siguen al tono (efecto ardilla o gigante). |
| **PRESETS** | 15 presets de fábrica (Hard Tune, armonías, octavas, robot, ardilla, gigante…), con flechas y menú en la barra de abajo. |
| **MIDI** | Con una nota pulsada: en Transpose/Quantize marca el intervalo respecto a C3; en Robot es la nota de salida. |

El nivel de salida se compensa solo: al subir o bajar el tono, o mover los formantes, la voz mantiene el mismo volumen.

---

## Conseguir los instaladores (.pkg para Mac y .exe para Windows)

GitHub compila el plugin y crea los dos instaladores sin que tengas que instalar nada de programación.

1. Crea una cuenta gratis en github.com y pulsa **New repository** (llámalo `alter-ivo`).
2. En el repositorio vacío, pulsa **uploading an existing file** y arrastra **todo el contenido** de esta carpeta, incluida la carpeta oculta `.github`. En Mac pulsa `Cmd + Shift + .` en Finder para ver las carpetas ocultas. Pulsa **Commit changes**.
3. Ve a la pestaña **Actions**. Verás "Build alterIVO" en marcha; tarda unos 15 minutos.
4. Cuando salga en verde, entra en el run y abajo, en **Artifacts**, descarga **alterIVO-Instaladores**. Es la carpeta para compartir y lleva:
   - `alterIVO-Mac-Instalador.pkg`
   - `alterIVO-Windows-Instalador.exe`
   - `LEEME.txt` (instrucciones de instalación)

**Enlace de descarga fijo:** ve a **Releases → Create a new release**, crea la etiqueta `v1.0.0` y publica. Unos minutos después, los dos instaladores aparecen en esa página para descargarlos desde cualquier ordenador.

## Instalar

**Mac (.pkg)**
1. Doble clic en `alterIVO-Mac-Instalador.pkg`. Como el instalador no está firmado por Apple, macOS lo bloquea la primera vez: ve a **Ajustes del Sistema → Privacidad y seguridad**, baja hasta el aviso de alterIVO y pulsa **Abrir igualmente**.
2. En la pantalla **Tipo de instalación** marca **VST3** y/o **Audio Unit (AU)** y pulsa Instalar. Te pedirá la contraseña del Mac.
3. Se instala en `/Library/Audio/Plug-Ins/VST3` y `/Library/Audio/Plug-Ins/Components`.

**Windows (.exe)**
1. Doble clic en `alterIVO-Windows-Instalador.exe`. Si sale "Windows protegió su PC", pulsa **Más información → Ejecutar de todas formas**.
2. Elige la instalación y la carpeta VST3 (por defecto `C:\Program Files\Common Files\VST3`) y pulsa Instalar.
3. Para desinstalarlo: **Configuración → Aplicaciones → alterIVO**.

Los avisos de seguridad salen porque el plugin no tiene firma de desarrollador. Para quitarlos al venderlo, hace falta la cuenta de Apple Developer (99 $/año) y un certificado de firma de código para Windows.

## Compilar en tu propio ordenador (opcional)

**Mac:** `xcode-select --install`, instala CMake y ejecuta:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 8
bash packaging/mac/build-pkg.sh      # crea dist/alterIVO-Mac-Instalador.pkg
```

**Windows:** instala Visual Studio Community (con *Desarrollo para el escritorio con C++*) e Inno Setup, y en *Developer PowerShell for VS*:
```
cmake -B build -A x64
cmake --build build --config Release
& "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" packaging\windows\installer.iss   # crea dist\alterIVO-Windows-Instalador.exe
```

La primera vez se descarga JUCE (unos 200 MB). Si falla con errores dentro de JUCE (no de `Source/`), cambia en `CMakeLists.txt` la línea `GIT_TAG 8.0.17-security` por `GIT_TAG 9.0.5` y vuelve a intentarlo.

## Usarlo en Ableton

- Activa **Configuración → Plug-Ins → Use VST3 Plug-In System Folders** (y en Mac también *Use Audio Units*) y pulsa **Rescan**. Aparece como **IVO Beatz → alterIVO**.
- **Para controlarlo por MIDI:** crea una pista MIDI y, en *MIDI To*, elige la pista de la voz y luego **alterIVO**. Pon el monitor de la pista MIDI en *In*.
- **Latencia:** unos 43 ms. Ableton la compensa sola en la mezcla.

## Personalizar

- **Colores:** `Source/Theme.h`.
- **Sonido:** `Source/PitchEngine.h` (pitch y formantes) y `Source/TubeDrive.h` (saturación).
- **Pruebas sin JUCE:**
  ```
  g++ -O2 -std=c++17 tests/engine_test.cpp -o engine_test && ./engine_test /tmp && python3 tests/analyze.py /tmp
  ```

## Licencia

JUCE 8 es gratuito con licencia AGPLv3. Si vendes o repartes el plugin sin publicar el código, necesitas la licencia comercial de JUCE.
