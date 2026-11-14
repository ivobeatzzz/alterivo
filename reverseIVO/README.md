# reverseIVO — by IVO Beatz

Reverses, risers y transiciones vocales que terminan **exactamente en el golpe**, sincronizados con el DAW. VST3 para **Mac y Windows** y AU en Mac.

| Sección | Qué hace |
|---|---|
| **CREATE** | Captura o importa la voz, detecta la primera palabra, coloca el HIT y genera el reverse a 1/4, 1/2, 1, 2, 4 compases o libre. Intensity, Pitch y Character. |
| **CUSTOMIZE** | Curvas arrastrables de volumen, tono y filtro, final Soft/Natural/Hard, transitorio y motor de capas (principal + textura). |
| **FX** | Reverse Wash, reverb, delay a tempo, tremolo, cortes progresivos, stutter, saturación, bitcrush, ruido, armónicos, sub, air, estéreo, movimiento de efectos y funciones Smart (Pre-Impact, Smart Ending, Clip Safe, Ducking). |
| **AUTO / MANUAL** | Mismo motor: AUTO regenera solo a cada cambio. |
| **SMART SYNC** | La duración sigue al tempo y al compás; si cambia el BPM se regenera. |
| **PRESETS** | 15 de fábrica (Clean, Underground, Atmospheric, Transitions) y los tuyos. |
| **EXPORT** | WAV 24 bits con marcador en el HIT, o arrastrar directamente al DAW. |
| **MIDI** | Cualquier nota dispara el reverse (velocidad = volumen). |

Sin latencia: el resultado se calcula en segundo plano y suena alineado con la posición del proyecto.

---

## Conseguir los instaladores

1. Sube la carpeta `reverseIVO` y el archivo `.github/workflows/reverseivo.yml` a tu repositorio de GitHub (el mismo de los demás plugins IVO).
2. Pestaña **Actions → Build reverseIVO → Run workflow**. Tarda unos 15–20 minutos.
3. Al terminar en verde, en **Artifacts** descarga **reverseIVO-Instaladores**:
   - `reverseIVO-Mac-Instalador.pkg` (VST3 y/o AU)
   - `reverseIVO-Windows-Instalador.exe` y `reverseIVO-Windows-VST3.zip`
   - `LEEME.txt` y `MANUAL.md`

Los instaladores también quedan en las ramas `reverseivo-instalador-mac` y `reverseivo-instalador-windows`.

## Compilar a mano

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Pruebas: `-DREVERSEIVO_TESTS=ON` compila `ReverseIvoPluginTest`; el motor se prueba con `tests/RenderTest.cpp` (instrucciones dentro del archivo). Resultados en `tests/resultados.txt`.

Más: [MANUAL.md](MANUAL.md) · [ARQUITECTURA.md](ARQUITECTURA.md)
