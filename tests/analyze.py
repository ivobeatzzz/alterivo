"""Analiza las salidas de engine_test: tono (autocorrelación), formantes y armónicos del drive."""
import sys
import numpy as np

SR = 48000
d = sys.argv[1]


def load(name):
    return np.fromfile(f"{d}/{name}.raw", dtype=np.float32)


def f0_track(x, win=2048, hop=1024, fmin=70, fmax=1000):
    out = []
    for s in range(0, len(x) - win, hop):
        seg = x[s:s + win] * np.hanning(win)
        if np.sqrt(np.mean(seg ** 2)) < 1e-3:
            out.append(0.0)
            continue
        ac = np.fft.irfft(np.abs(np.fft.rfft(seg, 2 * win)) ** 2)[:win]
        lo, hi = int(SR / fmax), int(SR / fmin)
        k = lo + np.argmax(ac[lo:hi])
        if ac[k] / ac[0] < 0.3:
            out.append(0.0)
            continue
        a, b, c = ac[k - 1], ac[k], ac[k + 1]
        k = k + 0.5 * (a - c) / (a - 2 * b + c)
        out.append(SR / k)
    return np.array(out)


def envelope_peak(x, start, length=SR):
    """F1 por LPC (orden 18, señal a 12 kHz): primera resonancia por encima de 300 Hz."""
    seg = x[start:start + length].astype(np.float64)
    seg = seg[::4] * np.hamming(len(seg[::4]))
    fs = SR / 4
    order = 14
    r = np.correlate(seg, seg, 'full')[len(seg) - 1:len(seg) + order]
    from numpy.linalg import solve
    R = np.array([[r[abs(i - j)] for j in range(order)] for i in range(order)])
    a = solve(R + 1e-9 * np.eye(order), r[1:order + 1])
    roots = np.roots(np.concatenate([[1], -a]))
    roots = roots[np.imag(roots) > 0]
    fr = np.sort(np.angle(roots) * fs / (2 * np.pi))
    fr = fr[fr > 300]
    return fr[0] if len(fr) else 0


def _old_envelope_peak(x, start, length=SR):
    """Frecuencia del máximo de la envolvente espectral (cepstrum suavizado) entre 300 y 1500 Hz."""
    seg = x[start:start + length]
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) + 1e-9
    logs = np.log(spec)
    cep = np.fft.irfft(logs)
    lifter = 60
    cep[lifter:-lifter] = 0
    env = np.fft.rfft(cep).real[:len(logs)]
    freqs = np.fft.rfftfreq(len(seg), 1 / SR)
    m = (freqs > 300) & (freqs < 1500)
    return freqs[m][np.argmax(env[m])]


def rms_db(x):
    return 20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12)


vowel = load("in_vowel")
vib = load("in_vibrato")
mid = slice(int(0.1 * SR), int(1.2 * SR))  # zona con voz (antes del hueco de ruido)
print(f"Entrada vocal: f0 = {np.median(f0_track(vowel[mid])):.1f} Hz, F1 ≈ {envelope_peak(vowel, int(0.2*SR)):.0f} Hz, nivel {rms_db(vowel[mid]):.1f} dB")

for name, expect in [("t_p0", 150), ("t_p7", 150 * 2 ** (7 / 12)), ("t_m12", 75), ("t_p12", 300),
                     ("f_p6", 150), ("f_m6", 150), ("link_p7", 150 * 2 ** (7 / 12))]:
    y = load("out_" + name)
    lat = 2 * 2 * 686 + 0  # aprox; se compensa con margen
    seg = y[int(0.2 * SR):int(1.25 * SR)]
    f = f0_track(seg)
    f = f[f > 0]
    fmed = np.median(f) if len(f) else 0
    err_c = 1200 * np.log2(fmed / expect) if fmed else float("nan")
    print(f"{name:8s} f0 = {fmed:7.1f} Hz (esperado {expect:6.1f}, error {err_c:+5.1f} cents)  "
          f"F1 ≈ {envelope_peak(y, int(0.25 * SR)):5.0f} Hz  nivel {rms_db(seg):6.1f} dB")

# Transpose 0: comparación directa con la entrada retrasada
y0 = load("out_t_p0")
best = None
for L in range(2000, 5000):
    a = vowel[int(0.3 * SR):int(1.2 * SR)]
    b = y0[int(0.3 * SR) + L:int(1.2 * SR) + L]
    c = np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b))
    if best is None or c > best[0]:
        best = (c, L)
c, L = best
a = vowel[int(0.3 * SR):int(1.2 * SR)]
b = y0[int(0.3 * SR) + L:int(1.2 * SR) + L]
g = np.dot(a, b) / np.dot(b, b)
res = rms_db(a - g * b) - rms_db(a)
print(f"Transpose 0 vs original: correlación {c:.4f} con retardo {L}, residuo {res:.1f} dB")

for name in ["quant", "robot"]:
    y = load("out_" + name)
    f = f0_track(y[int(0.3 * SR):int(2.8 * SR)], win=2048, hop=512)
    f = f[f > 0]
    fin = f0_track(vib[int(0.3 * SR):int(2.8 * SR)], win=2048, hop=512)
    fin = fin[fin > 0]
    nin = 69 + 12 * np.log2(fin / 440)
    nout = 69 + 12 * np.log2(f / 440)
    dev = 100 * np.abs(nout - np.round(nout))
    notes, counts = np.unique(np.round(nout).astype(int), return_counts=True)
    print(f"{name:8s} entrada notas {nin.min():.2f}..{nin.max():.2f} → salida notas {dict(zip(notes.tolist(), counts.tolist()))}, "
          f"desviación media respecto a la nota {np.median(dev):.1f} cents")

for amt in [30, 60, 100]:
    s = load(f"drive_{amt}")[SR // 4:]
    spec = np.abs(np.fft.rfft(s * np.hanning(len(s))))
    freqs = np.fft.rfftfreq(len(s), 1 / SR)
    h = [spec[np.argmin(np.abs(freqs - 1000 * k))] for k in range(1, 6)]
    thd = np.sqrt(sum(x ** 2 for x in h[1:])) / h[0] * 100
    rel = " ".join(f"H{k+1}:{20*np.log10(h[k]/h[0]):6.1f}" for k in range(1, 5))
    print(f"drive {amt:3d}%  nivel {rms_db(s):6.1f} dB (entrada -21.1)  THD {thd:5.1f}%  {rel} dB")
