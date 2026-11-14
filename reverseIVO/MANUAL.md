# reverseIVO — Manual

reverseIVO crea **reverses, risers y transiciones vocales** a partir de tu propia voz. Coge una palabra o una frase, la da la vuelta, la estira al tempo, le da forma (volumen, tono, filtro, espacio, ritmo, carácter) y la coloca para que **termine exactamente en el golpe**: el HIT. El resultado suena sincronizado con el DAW y se puede arrastrar como WAV.

AUTO y MANUAL usan el mismo motor: AUTO regenera solo a cada cambio; MANUAL espera a que pulses **GENERATE**.

## Flujo de trabajo

1. **Inserta reverseIVO** en la pista de la voz (o en una pista nueva con la voz).
2. **Captura:** pulsa el botón rojo y dale al play sobre la frase. Al parar el transporte la captura se cierra sola. También puedes **importar** un WAV/AIFF/MP3/FLAC con el botón de la flecha.
3. **Región automática:** reverseIVO detecta la primera palabra (o frase corta) y la marca en la tira **SRC** de abajo. Arrastra los bordes para elegir otra parte.
4. **HIT:** se coloca donde empieza la frase, ajustado a 1/16 si cae cerca. Muévelo con **‹ ›** (1/16) o pulsa **SET** para ponerlo donde está el cursor del DAW.
5. Elige **duración**, toca **Intensity / Pitch / Character** o carga un **preset**.
6. **GENERATE** (o automático en AUTO). Tarda de 0,2 a 4 segundos según los efectos.
7. **Play en el DAW:** el reverse suena justo antes del HIT y acaba en el golpe, sin latencia. Con el botón ▶ lo escuchas desde el propio plugin.
8. **DRAG TO DAW** o **exportar** (WAV 24 bits con un marcador en el HIT).

También puedes dispararlo con **MIDI**: cualquier nota lo reproduce desde el principio, con la velocidad como volumen.

## CREATE

- **Onda grande:** el resultado. La parte izquierda es la subida (reverse), la línea vertical es el HIT y a la derecha la cola. Los puntos son los tiempos del compás.
- **Tira SRC:** el audio fuente con la región elegida.
- **Smart Sync:** la duración sigue al tempo y al compás del proyecto. Si cambias el BPM, se regenera solo.
- **Duración:** 1/4, 1/2, 1 BAR, 2 BARS, 4 BARS o FREE (segundos libres).
- **Intensity:** cuánto empuja la subida (compresión hacia arriba, reverb previa, brillo).
- **Pitch:** transposición general en semitonos.
- **Character:** de limpio a sucio (saturación, armónicos, textura).
- **▶ Preview**, **GENERATE**, **↻ Reset** (vuelve a los valores de fábrica).

## CUSTOMIZE

Cada pestaña dibuja su curva sobre la onda; **arrastra los puntos** de inicio, final y curvatura.

- **Volume:** volumen inicial y final, curva, fade-in y **Transient** (cuánto se conserva el ataque del final). **Ending:** Soft, Natural o Hard.
- **Pitch:** tono inicial y final y su curva (el clásico riser que sube de tono), más **Micro Pitch** (ligera desafinación que da anchura).
- **Filter:** paso alto y paso bajo con su evolución, resonancia y curva.
- **Layers:** capa principal (nivel, anchura) y capa de textura (nivel, tono, anchura y *smear*, que la convierte en un pad).

## FX

- **Space:** *Reverse Wash* (la reverb que se oye ANTES de la voz, el sonido típico del reverse), Reverb (tamaño, decay, pre-delay, mezcla, damping) y Delay a tempo (con ping-pong).
- **Rhythm:** Tremolo, **Progressive Cuts** (cortes que se aceleran hacia el HIT, de 1/4 a 1/16) y Stutter.
- **Character:** Drive, Bitcrush, Noise (ruido que sigue a la voz), Harmonics, Sub Boost y Air.
- **Stereo:** anchura, auto-pan y su velocidad.
- **Movement:** cuánto crece cada efecto a lo largo de la subida (reverb, delay, drive, pan, filtro, anchura, tremolo).
- **Smart:** Smart Ending (corta la cola al llegar al golpe), Pre-Impact (pequeño vacío antes del HIT para que pegue más), Tail, Out Fade y Clip Safe (nunca pasa de −0,3 dBFS). **Playback:** Output, Through (deja pasar el audio de la pista), **Ducking** (baja la pista justo después del HIT) y Release.

## Presets

**Clean:** Clean Vocal Reverse, Soft Reverse, Natural Reverse.
**Underground:** Crank Riser, Dark Vocal Rise, Aggressive Reverse, Distorted Vocal Rise.
**Atmospheric:** Dreamy Reverse, Wide Reverse, Ambient Rise.
**Transitions:** Long Vocal Transition, Short Impact, Long Build, Rhythmic Build, Final Transition.

Los tuyos se guardan con **Guardar** en el menú de presets (Música/IVO Beatz/reverseIVO/Presets).

## Consejos

- Para un riser de 2 o 4 compases, usa una frase entera, no sólo una palabra.
- Si el final suena blando, sube **Transient** o pon **Ending: Hard**.
- Si se come la mezcla, baja **Reverse Wash** y la mezcla de la reverb, o usa el paso alto.
- **Ducking** en 30–50 % hace hueco al golpe sin tocar la pista original.
- El proyecto guarda la captura, la región, el HIT y todos los ajustes.
