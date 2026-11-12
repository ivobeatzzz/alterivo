# chainIVO — Manual de usuario

chainIVO procesa voces secundarias: ad-libs, coros, dobles, segundas voces, armonías, respuestas,
intros/outros, chops, susurros, gritos, voces habladas, texturas y efectos. Escucha la pista,
la compara con la voz principal y el beat (si se los das) y monta la cadena. Todo se procesa en
tu ordenador: el audio nunca sale de él.

Parte de una voz cruda y ya afinada. chainIVO **no reafina ni cambia formantes**: el único efecto
de tono es "Pitch" del motor CREATIVE, que añade una capa aparte y sólo si tú lo enciendes.

## 0. Dónde se pone
- **En la pista entera** de los ad-libs, de los coros, de los dobles... (lo normal). Procesa todo lo que suene
  en esa pista durante la canción; los silencios no cuentan para el análisis.
- **En una pista con un momento suelto**: si una frase concreta la pasas a otra pista, pon ahí otro chainIVO
  con otro preset (por ejemplo un efecto) y sólo esa frase sonará distinta. Con una frase corta (1,5 s de voz o
  más y luego silencio) AUTO decide con lo que hay; si después llega más voz y no has tocado nada, afina una vez más.
- **Varias pistas a la vez**: cada instancia se coordina con las demás (punto 7).
- No es para el master ni para el beat.

## 1. Empezar en 30 segundos (AUTO)
1. Inserta chainIVO en la pista de la voz secundaria.
2. Dale al play. "TU PISTA" muestra el análisis; con unos 8 s de voz decide qué tipo de voz es.
3. AUTO monta la cadena y explica cada decisión en **POR QUÉ** (pulsa cada ficha de LA CADENA).
4. Si quieres, elige tú el **tipo de voz** (chips) y el **estilo** (Rage, Plugg, reggaetón, rap…).
5. Ajusta **INTENSITY**: 0 % es la voz tal cual (nulo exacto), 100 % el tratamiento completo.

AUTO sólo monta la cadena una vez. Al volver a abrir un proyecto nunca cambia tu mezcla.
"↻ ANALIZAR" vuelve a empezar el análisis.

## 2. Presets
Pulsa el nombre del preset para abrir el buscador: texto libre (nombre, estilo, carácter: "oscuro",
"delay", "rage"…), categorías, favoritos (estrella) y presets de usuario. Las flechas ‹ › pasan de uno
en uno. Hay 124 presets IVO (31 ad-libs, 30 coros, 20 dobles/segundas, 20 armonías, 23 efectos).
Un preset IVO es una intención: al cargarlo se adapta a tu grabación (graves, eses, nivel, choque
con la principal…). "Guardar como…" guarda una copia exacta de tu cadena; los presets IVO nunca
se modifican. Tus presets están en Documentos/IVO Beatz/chainIVO/Presets y se pueden importar y exportar.

## 3. PRO
Los mismos motores con todos los ajustes. Pulsa una ficha para editarla.
- El **punto** de la ficha enciende o apaga el motor.
- **‹ ›** (al pasar el ratón por la ficha) cambian el orden de la cadena.
- **LOCK**: AUTO no tocará ese motor.
- **SOLO**: escuchas sólo ese motor (escucha por módulo).
- **AUTO** (botón lima): nueva propuesta para todo lo que no esté bloqueado.
- Doble clic en un mando lo devuelve a su valor inicial. Deja el ratón encima para ver qué hace.

## 4. Los motores
- **EQ**: paso alto (12/24 dB), graves, medios, presencia (dinámica si quieres) y aire.
  Con la voz principal conectada, la presencia dinámica baja sólo cuando la principal canta.
- **COMP**: FET (rápido, con mordida), Opto (suave), VCA (firme), Vari-Mu (redondo), Clean (transparente).
- **HARMONICS**: Tube, Tape, Transformer, Exciter, con sobremuestreo para que no ensucie.
- **VOCAL**: de-esser (sólo las eses), dureza (2.5-5 kHz dinámico) y respiraciones.
- **SPACE**: reverbs Plate, Hall, Room, Chamber, Ambience o IR (convolución con tu propia respuesta
  de impulso); delays Digital, Analog, Tape y Ping-Pong al tempo; ducking; anchura; doblador.
  Automatizaciones musicales: **THROW** manda la última palabra de cada frase al delay (remate) y
  **SWELL** hace crecer la cola al acabar cada frase. En la pantalla de SPACE ves cuántos finales de
  frase ha detectado.
- **CREATIVE** (nunca se enciende solo): Stutter, Gate y Reverse al tempo; Pitch (capa con otra
  afinación, ±24 st) y Granular (nube de granos).
  **POR PALABRA**: el efecto que elijas (Eco, Stutter, Filtro teléfono, Distorsión o Pitch) suena sólo en
  las palabras que elijas: todas, 1 de cada 2, 1 de cada 4, las más fuertes o la primera de cada frase.
  También **por sección**: en el último compás de cada 4 o de cada 8 (los remates entre secciones).
  En la pantalla de CREATIVE se ven las palabras detectadas (gris) y las que llevan efecto (lima).

## 5. La voz principal (sidechain)
chainIVO nunca modifica la voz principal: sólo la escucha.
- **Ableton Live**: en el dispositivo, despliega el sidechain y en "Audio From" elige la pista de la voz principal.
- **FL Studio**: en el mezclador, envía la pista de la voz principal a la de chainIVO con "Sidechain to this track";
  en la ventana del plugin (engranaje → Processing) asigna esa entrada al bus lateral.
- **Logic Pro**: arriba a la derecha del plugin, "Side Chain" → la pista de la voz principal.
- **Cubase / Studio One / REAPER / Bitwig**: activa la entrada lateral del plugin y envíale la voz principal.
Sin voz principal también funciona, pero no puede distinguir doble, armonía o respuesta.

## 6. BEAT · EXPORTAR (abajo en TU PISTA)
- **Cargar el beat**: AUTO deja la voz donde el instrumental tiene hueco. Sólo se guarda su espectro.
- **Procesar un archivo**: crea "nombre (chainIVO).wav" (24 bits o 32 bits float) junto al original,
  con la cola completa y alineado. El original no se toca.
- **Reverb IR**: carga una respuesta de impulso (WAV) para la reverb de convolución.
- **Perfil personal** (apagado de serie): si lo enciendes, chainIVO aprende lo que sueles cambiar después de
  AUTO (por ejemplo, más reverb en los ad-libs) y lo tiene en cuenta en las siguientes propuestas.
  "Ver lo aprendido" lo enseña y "Reiniciar el perfil" lo borra. Nunca toca los presets IVO.
- **Tema oscuro**: cambia la piel (se guarda en tu ordenador).

## 7. Varias capas con chainIVO
Si pones chainIVO en varias voces del mismo proyecto, las instancias se ven entre sí: cada capa busca
su hueco de presencia y usa un pre-delay distinto para que las colas no se fundan. "TU PISTA" dice
cuántas capas hay en el grupo.

## 8. Abajo: globales
INPUT, INTENSITY, DRY/WET, OUTPUT · QUALITY (ECO, NORMAL, HIGH, ULTRA: cambia el sobremuestreo, el
filtro y la reverb; ECO gasta menos CPU) · GAIN MATCH (iguala volumen para comparar de forma justa) ·
CLIP SAFE (protección suave por encima de -1 dBFS) · BYPASS · A/B · deshacer/rehacer · medidores
IN/OUT · CPU, latencia, Gain Match y correlación estéreo (1 = mono, cerca de 0 = muy abierto,
negativo = problema de fase).

## 9. Protección
Si al plugin le llega audio roto (valores no válidos) lo limpia antes de procesar; si algún motor fallara,
se reinicia solo y ese bloque sale sin procesar. Se anota en Documentos/IVO Beatz/chainIVO/diagnostico.txt
y abajo verás "recuperado".

## 10. Problemas comunes
- No aparece en el DAW: vuelve a escanear plugins. En Mac, si macOS lo bloquea: Ajustes → Privacidad
  y seguridad → "Abrir igualmente".
- "Esperando audio…": la pista no suena o está en silencio.
- No detecta la voz principal: revisa el enrutado del sidechain (punto 5).
- La IR no suena: comprueba que el tipo de reverb sea "IR" y que haya una IR cargada.

## 11. Límites conocidos
Ver ARQUITECTURA.md (presets pendientes de escuchar con voces reales, clasificación por reglas,
sin reconocimiento automático de género, sin sistema de licencias).
