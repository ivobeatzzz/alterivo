# chainIVO — arquitectura, decisiones y estado (versión 0.1)

Este documento cumple los puntos 25 a 29 del texto de chainIVO: auditoría, decisiones técnicas,
arquitectura, flujo, AUTO/PRO, referencias, offline/realtime, compatibilidad, pruebas, riesgos y fases.
Todo lo que pone "medido" sale de las pruebas de `tests/` (resultados al final).

## 1. Auditoría de partida

- No había código de chainIVO. La base es la misma que alterIVO (C++17, JUCE 8.0.17, CMake,
  compilación en GitHub Actions para Mac Intel/Apple Silicon y Windows, instaladores gratuitos).
- Se reutiliza de alterIVO sólo lo común de la marca: fuentes (Sora y Manrope, licencia OFL), logo,
  sistema de instaladores y validación (pluginval, auval). El DSP es nuevo.

## 2. Decisiones técnicas (con alternativas)

| Tema | Elegido | Alternativas valoradas | Por qué |
|---|---|---|---|
| Framework | JUCE 8 | iPlug2, DPF, SDKs a mano | Un solo núcleo para VST3/AU/AAX, ya probado en alterIVO en los 7 DAW. iPlug2 es más ligero pero con menos soporte de AU/AAX y de interfaz; DPF no tiene AU/AAX. |
| Licencia JUCE | Starter (gratis) | Indie 40 $/mes, AGPLv3 | Gratis mientras los ingresos de los últimos 12 meses no pasen de 20 000 $ (cuentan todos los ingresos de IVO Beatz, no sólo los plugins). Hay que vigilarlo. |
| VST3 SDK | MIT (desde 3.8) | — | Gratis y compatible con venta. |
| AAX / Pro Tools | Preparado, no se compila | — | Requiere firma PACE (iLok, de pago). Excluido por el requisito de coste cero. |
| Filtros EQ | SVF de Simper (TPT) en float | Biquad RBJ, FIR | Estable al automatizar, buen comportamiento numérico en float, la curva dibujada es la fórmula exacta. FIR añadiría latencia sin mejora audible en una voz secundaria. |
| Precisión | float32 en audio, double en coeficientes de la reverb/medidas | float64 en todo | No hay mejora audible medible en esta cadena; se usa double sólo donde se acumula (sonoridad, osciladores). |
| Sobremuestreo | Sólo en HARMONICS (FIR de media banda, fase lineal, latencia entera) | Todo sobremuestreado, IIR | Es el único motor no lineal fuerte. FIR = mezcla seca/saturada en fase. |
| Reverb | FDN propia (Householder, amortiguación por línea según T60) | Convolución, Freeverb | Control real de tamaño/caída/tono, CPU baja, sin archivos de IR (la convolución queda para una fase futura con IR propias). |
| Inteligencia | Reglas medibles sobre análisis DSP | Modelos ML (Silero VAD MIT, clasificadores) | El texto pide IA sólo si mejora de forma demostrable. Con reglas se explica cada decisión ("POR QUÉ") y no hay dependencias. Un modelo se añadirá sólo si las pruebas con voces reales muestran fallos que lo justifiquen. |
| Privacidad | Todo local | — | El audio nunca sale del ordenador; no hay red. |

## 3. Módulos (punto 24 del texto)

```
Source/
  dsp/DspCore.h              Infraestructura común: SVF, bandas EQ, retardos, envolventes, suavizado, medidores
  engines/EqEngine.h         EQ ENGINE
  engines/CompEngine.h       COMPRESSOR ENGINE (FET, Opto, VCA, Vari-Mu, Clean)
  engines/HarmonicsEngine.h  HARMONICS ENGINE (Tube, Tape, Transformer, Exciter + sobremuestreo)
  engines/VocalEngine.h      VOCAL CONTROL ENGINE (de-ess, dureza, respiraciones)
  engines/SpatialEngine.h    SPATIAL ENGINE (reverb FDN e IR por convolución, delay, ducking, doblador, anchura M/S)
                             + AUTOMATION ENGINE musical: remate (throw) y cola (swell) en cada final de frase
  engines/CreativeEngine.h   CREATIVE ENGINE (stutter, gate, reverse al tempo; pitch creativo; granular)
  analysis/Analyzer.*        ANALYSIS ENGINE (hilo propio, FIFO sin bloqueos)
  (PluginProcessor::exportFile) RENDERING / AUDIO EXPORT ENGINE (WAV 24/32 float, en segundo plano)
  intelligence/Group.h       Coordinación entre instancias (grupo de capas vocales)
  intelligence/Musical.*     MUSICAL INTELLIGENCE + CHAIN BUILDER (tipos de voz, estilos, clasificación, recetas)
  presets/PresetLibrary.*    PRESET ENGINE (presets IVO, usuario, favoritos, importar/exportar)
  core/Params.*              STATE: parámetros versionados (IDs fijos)
  PluginProcessor.*          AUDIO CORE + gestión de ganancia + A/B + estado + capa DAW (JUCE)
  ui/                        USER INTERFACE (Widgets, editor)
```
Pendientes: Diagnostics como módulo propio y Licensing (ver limitaciones).

## 4. Flujo de la señal

```
entrada → INPUT → [6 motores en el orden elegido] → DRY/WET (seca alineada con la latencia)
        → GAIN MATCH (sonoridad K aproximada, ±12 dB) → OUTPUT → BYPASS (con fundido) → CLIP SAFE → salida
sidechain (voz principal) → sólo análisis y llave de la banda dinámica del EQ (nunca se modifica ni se oye)
```
- INTENSITY lleva cada motor de neutro a lo ajustado dentro del propio motor. Medido: con INTENSITY 0 %
  la salida es idéntica a la entrada (diferencia −300 dB, es decir, nula).
- Latencia: sólo la del sobremuestreo (31-65 muestras a 48 kHz, 0.65-1.35 ms), fija para cada calidad,
  comunicada al DAW y mantenida aunque HARMONICS esté apagado.

## 5. AUTO y PRO

- Mismo motor y mismos parámetros. AUTO escribe valores que PRO enseña y deja editar.
- AUTO: cuando hay 6 s de voz analizada (≈8 s = 100 %), clasifica la pista, aplica el preset o la
  propuesta automática adaptada a lo medido y escribe el "POR QUÉ" de cada motor. Lo hace una vez:
  al abrir un proyecto guardado nunca cambia la mezcla.
- El usuario puede: elegir tipo y estilo, cambiar preset, LOCK por motor (AUTO no lo toca), mover el orden,
  encender/apagar motores, deshacer/rehacer y comparar A/B.

## 6. Referencias (voz principal e instrumental)

- Implementado: entrada sidechain "Voz principal" (VST3/AU). Con ella: choque de frecuencias,
  banda dinámica que sólo baja cuando canta la principal, y distinción doble/armonía/respuesta.
- Sin sidechain funciona con lo que tiene y lo dice ("Sin voz principal no distingo...").
- Instrumental de referencia: el usuario carga el archivo del beat (BEAT · EXPORTAR). Se analiza en segundo plano
  y sólo se guarda su espectro (nunca el audio). AUTO pone la presencia donde el beat deja hueco y sube el paso
  alto si el beat tiene graves muy fuertes.
- Instancias coordinadas: las instancias de chainIVO del mismo proyecto comparten (sin tocar el audio) tipo y
  espectro; cada capa busca su hueco de presencia y usa un pre-delay distinto.

## 7. Realtime y offline

- El hilo de audio no reserva memoria, no bloquea, no toca disco ni red. El análisis va en su propio hilo.
- Offline (render) usa exactamente el mismo procesado: el resultado no cambia entre modos.
  Las calidades (ECO/NORMAL/HIGH/ULTRA) son elección del usuario y cambian de verdad el algoritmo
  (factor de sobremuestreo, filtro, nº de líneas de reverb, interpolación).

## 8. Compatibilidad

VST3 (Windows y Mac) y AU (Mac), binario universal. Validación automática en cada compilación:
pluginval nivel 5 (VST3 y AU) y auval. DAW objetivo: Ableton Live, FL Studio, Logic, Cubase,
Studio One, REAPER, Bitwig. La prueba real en cada DAW la tiene que hacer el usuario.

## 9. Resultados de las pruebas (Linux, voz de prueba de 26.4 s, 48 kHz)

| Prueba | Resultado |
|---|---|
| INTENSITY 0 % | salida idéntica a la entrada (−300 dB) |
| Aliasing HARMONICS (seno 7.3 kHz, drive 80 %) | ECO −35.6 dB · NORMAL −55.8 dB · HIGH −55.7 dB · ULTRA −55.7 dB |
| Compresores (umbral −40, 4:1) | rango dinámico 26.5 → FET 15.7 · VCA 18.4 · Clean 17.6 · Opto 21.8 · Vari-Mu 22.2 dB; diferencias entre topologías de −10.6 a −26.9 dB |
| Reverb (decay 2.0 s) | T60 medido: Plate 1.98 · Hall 2.03 · Chamber 1.96 s (Room y Ambience limitadas por diseño a 1.4 y 0.6 s) |
| Mono | anchura 200 % + doblador 60 %: la suma mono pierde sólo 1.8 dB |
| De-esser al 100 % | eses −5.8 dB; vocales −0.02 dB |
| CPU (todo encendido) | ECO 2.3 % · NORMAL 3.7 % · HIGH 4.3 % · ULTRA 6.6 % de un núcleo |
| Estabilidad | 158 s con automatización aleatoria y bloques de 1-1024 muestras: 0 valores inválidos |
| AUTO | doble detectado (66 %), respuesta detectada (100 %); sin principal, voz continua → "segunda voz" (58 %) |
| Estado | guardar/abrir proyecto: 0 parámetros distintos, orden, LOCK, preset y A/B recuperados |
| Exportar WAV | 24 bits o 32 bits float, misma frecuencia y canales, cola completa, alineado con el original (con INTENSITY 0 la diferencia es −300 dB) y el original sin tocar |
| Instrumental | beat sintético con hueco en 4 kHz y graves fuertes: AUTO pone la presencia en 4.0 kHz y sube el paso alto |
| Presets | 122 presets, todos con estilo válido; a igual volumen, la pareja más parecida difiere −19.6 dB (ninguna por debajo de −20 dB) |
| Automatizaciones | 4 de 4 finales de frase detectados; THROW pone el remate en los silencios; SWELL sube la cola +8.4 dB al acabar la frase |
| Pitch creativo | capa −12 st sobre 440 Hz: aparece 220 Hz a −0.6 dB y la voz sigue en 440 Hz |
| Granular | estable (0 valores inválidos), nivel parecido a la voz |
| Reverb IR | la cola sigue la IR cargada (0.8 s) y termina con ella |
| Grupo | dos instancias se ven; la segunda separa su presencia y su pre-delay (20 → 28 ms) |

## 10. Limitaciones conocidas (versión 0.1)

1. 122 presets IVO (30 ad-libs, 30 coros, 20 dobles/segundas, 20 armonías, 22 efectos). La prueba automática
   comprueba que todos suenan distintos entre sí, pero están **sin validar de oído con voces reales**:
   es lo siguiente que hay que hacer con grabaciones del usuario.
2. La clasificación es por reglas: con una sola voz de prueba no se puede afirmar su precisión.
   Hace falta un conjunto de pistas reales etiquetadas.
3. El estilo (género) lo elige el usuario: no hay reconocimiento automático de género.
4. VCA y Clean se parecen más entre sí que el resto (diferencia −26.9 dB con los mismos ajustes).
5. HIGH y ULTRA no reducen más el aliasing que NORMAL en la prueba (−56 dB es el suelo de la medida).
   Se diferencian en filtro, reverb e interpolación.
6. Pendiente: automatizaciones por palabra/frase, reverb de convolución, más efectos creativos (granular, pitch creativo), skin oscura, licencias,
   manual completo y ayuda contextual.
7. No se afirma que chainIVO supere a otras herramientas: no hay comparaciones a ciegas todavía.

## 11. Riesgos

- Licencia JUCE Starter: si los ingresos totales superan 20 000 $/año hay que pasar a Indie.
- Sin firma de código de pago: avisos de seguridad en Mac/Windows (documentado en LEEME).
- Sidechain: cada DAW lo enruta distinto (instrucciones en LEEME).

## 12. Fases

| Fase | Estado |
|---|---|
| 0 Auditoría y viabilidad | hecha |
| 1 Investigación y arquitectura | hecha (este documento) |
| 2 Core y prototipo funcional | hecha y medida (falta validación con voces reales) |
| 3 Calidad de procesamiento | en gran parte (medidas arriba); faltan escuchas comparativas |
| 4 Inteligencia musical | tipos de voz, voz principal, beat, grupo de capas, automatizaciones por frase; faltan secciones y género automático |
| 5 Presets y motor creativo | 122 presets diferenciados (falta escucharlos); 5 efectos creativos |
| 6 Producto completo | exportar WAV, skins, ayuda, manual; faltan licencias (coste) |
| 7 QA y lanzamiento | pendiente (pruebas en cada DAW con el usuario) |
