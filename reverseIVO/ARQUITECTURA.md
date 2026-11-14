# reverseIVO — Arquitectura (versión 1.0)

Tres hilos que nunca se esperan entre sí:

| Hilo | Qué hace | Archivos |
|---|---|---|
| Audio | Escribe la captura en una cola sin bloqueos (con la posición PPQ de cada bloque) y reproduce el resultado ya calculado según la posición del DAW, de modo que el HIT cae en la muestra exacta. Preview, disparo MIDI, through, ducking y rampas de 3 ms. Nunca reserva ni libera memoria. | `PluginProcessor.*` |
| Motor (segundo plano) | Renderiza el reverse completo. Siempre gana el último trabajo: si llega un ajuste nuevo, el anterior se cancela. Publica un `Result` inmutable (`shared_ptr`); los antiguos se liberan en el hilo de mensajes. | `core/Engine.*`, `render/Renderer.*` |
| Interfaz | Lee el resultado (onda, curvas) y escribe parámetros. | `ui/*` |

## Cadena de render (`render/Renderer.cpp`)

1. Región de la fuente → recorte con fundidos cortos.
2. **Wash previo:** reverb FDN de 8 líneas (Hadamard, amortiguación, modulación) aplicada a la voz ANTES de girarla, para que la cola se convierta en la subida.
3. Giro (reverse) y compresión hacia arriba del oleaje (hasta +32 dB según Intensity).
4. **Ajuste al tiempo:** phase vocoder con bloqueo de fase por picos, rotación estéreo coherente desde el canal medio y reinicio de fase en transitorios; modo *smear* para la capa de textura.
5. **Evolución de tono** manteniendo la duración exacta: se estira a Σ(ratio) y se lee con varispeed (sinc con ventana Kaiser).
6. Capa principal + capa de textura (tono, anchura, smear).
7. Curvas de volumen y filtro (SVF TPT paso alto / paso bajo con resonancia), micro pitch.
8. Ritmo a tempo: tremolo, cortes progresivos, stutter (sin clic en la primera repetición).
9. Carácter: saturación con sobremuestreo 4x polifásico, bitcrush, ruido rosa que sigue a la voz, armónicos, sub y air (shelves).
10. Espacio: reverb, delay a tempo (ping-pong), anchura M/S y auto-pan de potencia constante, todo con su *movement*.
11. Final: transitorio del golpe, Smart Ending, Pre-Impact, cola, fade de salida, normalización a −2 dBFS y limitador suave con sobremuestreo (Clip Safe, −0,3 dBFS).

## Otros módulos

- `analysis/Onset.h`: detección de la primera palabra (envolvente de 5 ms, umbral relativo al pico, final en el valle).
- `core/Params.*`: parámetros, textos de ayuda y lectura a `RenderSettings`. Los de reproducción (Output, Through, Ducking, Release, AUTO/MANUAL) no provocan render.
- `presets/`: 15 presets de fábrica en 4 categorías y presets de usuario en XML.
- `dsp/`: FFT, filtros, sinc, sobremuestreo, reverb, delay, phase vocoder.

## Pruebas

- `tests/RenderTest.cpp`: el motor (duración exacta, HIT, clics, picos, cada efecto, cancelación).
- `tests/PluginTest.cpp`: el plugin completo como lo usa un DAW (captura, sincronía exacta, MIDI, preview, guardar y recargar, Smart Sync con cambio de tempo y 3/4, AUTO/MANUAL, exportar, todos los presets y capturas de la interfaz).
- Resultados en `tests/resultados.txt`.
