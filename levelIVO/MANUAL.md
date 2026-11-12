# levelIVO — Manual

levelIVO deja una toma de voz **pareja y limpia antes de mezclar**. Hace lo que harías a mano con el clip gain palabra por palabra y con un editor de restauración, pero automático, con explicación de cada decisión y editable.

No pone EQ, compresión de color ni efectos (eso es chainIVO) y no afina ni cambia el tono (eso es alterIVO).

## Flujo de trabajo

1. **Inserta levelIVO el primero** en la pista de la voz.
2. **Beat al sidechain (opcional):** en Ableton, en la sección de sidechain de levelIVO elige la pista del beat; en FL Studio, enruta el beat a la entrada lateral del slot. Con el beat como referencia, el sangrado de cascos se resta de verdad.
3. **CAPTURAR** y dale al play. Puedes capturar la canción entera o sólo partes (varias pasadas se juntan).
4. **Para el transporte:** el análisis empieza solo. La barra de estado muestra el progreso.
5. **Revisa o aplica.** Con *REVISAR ANTES* suena el original hasta que pulses **APLICAR**; con *AUTO APPLY* se aplica en cuanto termina.
6. **Reproduce:** levelIVO sigue la posición del DAW y hace sonar el resultado ya calculado, **sin latencia**.
7. **Exportar WAV** (24 bits o 32 bits flotante): el archivo empieza donde empezó la captura, para arrastrarlo al proyecto en su sitio.

Si mueves o cambias el clip después de capturar, levelIVO lo detecta, deja sonar el audio sin procesar y te avisa para que vuelvas a capturar.

## La pantalla

- **Forma de onda gris:** la toma original. **Clara:** el resultado.
- **Curva verde lima:** la ganancia real que se aplica (lo que ves es exactamente lo que suena).
- **Carriles de arriba:** frases, palabras y sílabas detectadas. Las palabras más apagadas tienen menos confianza; las rayadas son de baja confianza y se corrigen menos.
- **Turquesa:** respiraciones (con lo que se bajan). **Ámbar:** golpes, clics reparados y, abajo, cuánto ruido o sangrado se quita. **Rojo:** clipping. **Marcas blancas abajo:** ataques y consonantes protegidos.

### Edición

- Clic en un carril (frase, palabra o sílaba) o en la onda para **seleccionar**; Mayús o Cmd/Ctrl para seleccionar varias.
- **Arrastra arriba o abajo** dentro de la selección para corregir la curva (Mayús = ajuste fino).
- El **inspector** explica la decisión: nivel medido, corrección, motivo y confianza. Botones:
  - **BLOQUEAR** fija la ganancia actual de ese tramo.
  - **EXPRESIÓN** marca un grito o un susurro intencionado: no se corrige frente a su frase.
  - **RESET** quita tus ediciones en ese tramo.
- Doble clic en la selección la restablece; doble clic en un hueco ajusta el zoom a la toma entera.
- Rueda = zoom, Mayús + rueda o desplazamiento lateral = moverse, la barra de abajo = vista general.
- Cmd/Ctrl+Z deshacer, Cmd/Ctrl+Mayús+Z rehacer, barra espaciadora = escuchar desde el plugin.

## Controles

### Global
- **Natural / Balanced / Controlled / Manual:** cuánto se iguala. Al tocar un control de nivelación pasa a Manual.
- **Offline / Realtime:** Offline analiza la toma entera (recomendado). Realtime es un modo sencillo con 300 ms de latencia, sin segmentación por palabras ni reparación de clipping.
- **Presets:** Natural, Balanced, Controlled, Rap, Trap, Reggaeton, Ad-Libs, Background, Clean, Noisy y los tuyos (Guardar como…).
- **A / B:** dos juegos de ajustes para comparar.
- **Escuchar ORIGINAL / NIVEL / LIMPIEZA / FINAL**, con **volumen igualado**: así ninguna gana por sonar más fuerte.

### Nivelación
- **Frase:** deriva lenta (distancia al micro, cansancio).
- **Palabra:** palabras que se pierden o sobresalen dentro de su frase.
- **Sílaba:** sílabas que se caen. Sólo sube, como mucho 4 dB.
- **Intensidad**, **Sensibilidad** (baja = sólo errores claros), **Suavizado**.
- **Seguir al beat:** con el beat en el sidechain, la voz acompaña un poco a la energía de la base.
- **Máx. subida / bajada**, **Picos** (nunca pasa de −1 dBFS) y **Consonantes** (los cambios terminan antes de cada ataque).
- **Objetivo:** además de nivelar, lleva la voz a un nivel fijo en LUFS.

### Limpieza (va antes de la nivelación)
- **Ruido de fondo:** aprende el ruido en los silencios. Reduce todo lo indicado entre frases y mucho menos durante la voz, para no apagarla.
- **Sangrado:** con el beat en el sidechain lo resta con su fase y su retardo. Sin beat, sólo lo baja donde no cantas, con margen para no cortar colas. La cifra *coh.* indica cuánto se parece la referencia al sangrado: si es baja, se aplica menos.
- **Otras voces:** baja frases mucho más flojas o con otro tono. Viene apagado porque un susurro tuyo podría confundirse.
- **Golpes al micro:** paso alto sólo en ese instante.
- **Ruidos de boca:** reconstruye las muestras del clic.
- **Respiraciones:** se bajan, nunca se borran.
- **Clipping:** se detecta siempre; en REPARAR se reconstruyen los picos recortados cortos. Los largos se marcan como no reparables.

### Salida
- **Output**, medidores IN / OUT / GAIN (ganancia que se está aplicando), **Bypass**.
- **AUTOMATIZACIÓN:** escribe la curva en el parámetro *Ride Gain*. Arma la automatización de levelIVO en tu DAW, dale al play y la curva queda en el DAW para retocarla allí.

## Archivos
- Capturas: `Música/IVO Beatz/levelIVO/Capturas` (se pueden borrar al terminar el tema; sin ellas, el proyecto pedirá volver a capturar).
- Presets de usuario: `Música/IVO Beatz/levelIVO/Presets` (archivos `.levelivo`).

## Límites conocidos (versión 0.1)
- Sin el beat en el sidechain no se puede quitar el sangrado que suena mientras cantas.
- Las otras voces sólo se bajan cuando no se solapan con la tuya.
- La nivelación aún no se ha afinado con tomas cantadas reales con su automatización hecha a mano: cuando me pases unas cuantas, se ajustan los modos.
