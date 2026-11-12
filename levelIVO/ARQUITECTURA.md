# levelIVO — Arquitectura (versión 0.1)

Tres hilos que nunca se esperan entre sí:

| Hilo | Qué hace | Archivos |
|---|---|---|
| Audio | Captura la toma con la posición del DAW (cola sin bloqueos) y reproduce el resultado ya calculado según esa posición, sin latencia. En modo tiempo real, nivelador causal con 300 ms de anticipación. | `PluginProcessor.*`, `realtime/RtLeveler.h` |
| Motor (segundo plano) | Detección → limpieza → decisión de nivel. Cada etapa sólo se repite si cambia lo que la afecta. Publica un `Result` inmutable. | `core/Engine.*` |
| Interfaz | Lee el resultado y escribe parámetros y ediciones. | `ui/*` |

## Módulos

- `analysis/` **Detección:** sonoridad ponderada K (BS.1770), bandas, periodicidad tipo YIN y segmentación en frases (pausas de 250 ms o más), palabras (valles de 6 dB o más) y sílabas (núcleos vocálicos), cada corte con su confianza. También ataques, respiraciones, otras voces, golpes, candidatos a clic (residuo de predicción lineal, aislados) y clipping.
- `cleanup/` **Limpieza (motor B):**
  - Clipping: interpolación autorregresiva (LSAR) con coherencia de recorte.
  - Clics: LSAR.
  - Golpes: paso alto de fase cero sólo en el tramo.
  - Ruido: Wiener con estimación «decision-directed», perfil aprendido en pausas y reducción localizada.
  - Sangrado con referencia: retardo por GCC-PHAT, función de transferencia aprendida en pausas, resta compleja más Wiener del resto.
  - Sangrado sin referencia: atenuación en huecos.
  - Respiraciones y otras voces: ganancia.
- `leveling/` **Decisión (motor A):**
  - Tendencia: mediana de palabras en ±2 s.
  - Palabra: desviación frente a la tendencia, con zona muerta calculada sobre la propia toma y multiplicada por la confianza.
  - Sílaba: sólo realce, como mucho 4 dB.
  - Una sola curva: suavizado que termina antes de cada ataque, nunca sube pausas ni respiraciones, protección de picos dentro de la curva y ediciones del usuario (desplazar, bloquear, expresión).
- `render/` Resultado y utilidades (curva por trama a ganancia por muestra, compensación de volumen de cada escucha).
- `presets/` Presets de fábrica y de usuario.

## Pruebas

- `tests/bench.cpp`: errores y defectos conocidos inyectados.
- `tests/plugintest.cpp`: el plugin completo como lo usa un DAW.
- Resultados en `tests/resultados.txt`.
