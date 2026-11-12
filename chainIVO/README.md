# chainIVO

Plugin de IVO Beatz para voces secundarias (ad-libs, coros, dobles, armonías, efectos).
VST3 y AU, Mac y Windows. Ver `ARQUITECTURA.md` (decisiones, pruebas y limitaciones) y
`packaging/LEEME.txt` (instalación y uso).

Compilar: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release`
Pruebas: `tests/ChainTest.cpp` (se compila con las mismas fuentes; resultados en `tests/resultados.txt`).
