# Gera os sons de fábrica a partir do Big Rusty Drums (Karoryfer Samples, CC0 1.0).
# Mistura os microfones de cada peça em estéreo, corta, normaliza por peça (mantendo
# a diferença de força entre as camadas) e grava FLAC em sounds/rusty/<peca>/vNN_rrN.flac
import sys, os, re, glob, json
import numpy as np, soundfile as sf

SRC = sys.argv[1]; DST = sys.argv[2]
# peça -> (pasta, [(mic, ganho)], duração máx s, fade s, nome, categoria, choke)
P = {
 "bumbo":        ("kick_24/kick",     [("kick",1.0),("oh",0.45)], 1.1, 0.25, "Bumbo Acústico 24\"", "Bumbo", 0),
 "caixa":        ("snare_14/center",  [("top",1.0),("btm",0.35),("oh",0.55)], 1.2, 0.3, "Caixa Acústica 14\"", "Caixa", 0),
 "caixa_aro":    ("snare_14/rimshot", [("top",1.0),("btm",0.3),("oh",0.55)], 1.3, 0.3, "Caixa Rimshot", "Caixa", 0),
 "caixa_lateral":("snare_14/sidestick",[("top",1.0),("oh",0.5)], 0.6, 0.15, "Caixa Aro Lateral", "Caixa", 0),
 "chimbal_f":    ("hihat_14/cl",      [("cl",0.8),("oh",0.7)], 0.7, 0.2, "Chimbal Acústico Fechado", "Chimbal", 1),
 "chimbal_meio": ("hihat_14/ho",      [("cl",0.8),("oh",0.7)], 1.6, 0.4, "Chimbal Acústico Meio-Aberto", "Chimbal", 1),
 "chimbal_a":    ("hihat_14/open",    [("cl",0.8),("oh",0.7)], 2.6, 0.7, "Chimbal Acústico Aberto", "Chimbal", 1),
 "chimbal_pe":   ("hihat_14/chik",    [("cl",0.8),("oh",0.7)], 0.5, 0.15, "Chimbal Pedal", "Chimbal", 1),
 "tom14":        ("tom_14/center",    [("cl",1.0),("oh",0.5)], 1.8, 0.5, "Tom Acústico 14\"", "Tom", 0),
 "tom15":        ("tom_15/center",    [("cl",1.0),("oh",0.5)], 1.9, 0.5, "Tom Acústico 15\"", "Tom", 0),
 "tom18":        ("tom_18/center",    [("cl",1.0),("oh",0.5)], 2.2, 0.6, "Surdo Acústico 18\"", "Tom", 0),
 "tom22":        ("tom_22/center",    [("cl",1.0),("oh",0.5)], 2.4, 0.7, "Surdo Acústico 22\"", "Tom", 0),
 "ataque17":     ("crash_17/cr",      [("cl",0.45),("oh",1.0)], 5.0, 2.0, "Prato Ataque 17\"", "Prato", 0),
 "conducao22":   ("ride_22/rd",       [("cl",0.5),("oh",1.0)], 4.0, 1.5, "Prato Condução 22\"", "Prato", 0),
 "cupula22":     ("ride_22/bl",       [("cl",0.5),("oh",1.0)], 4.0, 1.5, "Condução Cúpula", "Prato", 0),
 "china18":      ("china_18/cn",      [("cl",0.45),("oh",1.0)], 4.0, 1.5, "Prato China 18\"", "Prato", 0),
}
def st(x):
    return np.stack([x, x], 1) if x.ndim == 1 else x
index = []
for key, (folder, mics, dur, fade, name, cat, choke) in P.items():
    first = mics[0][0]
    files = sorted(glob.glob(f"{SRC}/{folder}/{first}/*_rr[12].flac"))
    if not files: print("faltou", key); continue
    takes = {}
    for f in files:
        m = re.search(r"_vl(\d+)_rr(\d+)\.flac$", f); vl, rr = int(m[1]), int(m[2])
        mix = None
        for mic, g in mics:
            fn = f"{SRC}/{folder}/{mic}/" + os.path.basename(f)
            if not os.path.exists(fn): continue
            x, fs = sf.read(fn, dtype="float32"); x = st(x) * g
            if mix is None: mix = x
            else:
                n = max(len(mix), len(x)); a = np.zeros((n, 2), np.float32); a[:len(mix)] += mix; a[:len(x)] += x; mix = a
        # começo: corta silêncio antes do ataque (deixa 1 ms)
        pk = np.abs(mix).max(1); thr = pk.max() * 0.02
        s0 = max(0, int(np.argmax(pk > thr)) - int(0.001 * fs))
        mix = mix[s0:s0 + int(dur * fs)]
        fl = min(len(mix), int(fade * fs)); mix[-fl:] *= np.linspace(1, 0, fl, dtype=np.float32)[:, None] ** 2
        takes[(vl, rr)] = mix
    peak = max(np.abs(t).max() for t in takes.values())
    vls = sorted({k[0] for k in takes})
    os.makedirs(f"{DST}/{key}", exist_ok=True)
    for (vl, rr), t in takes.items():
        sf.write(f"{DST}/{key}/v{vls.index(vl)+1:02d}_rr{rr}.flac", (t * (0.89 / peak)).astype(np.float32), fs, subtype="PCM_16")
    index.append({"id": key, "name": name, "category": cat, "choke": choke, "layers": len(vls)})
    print(key, len(vls), "camadas", len(takes), "arquivos")
json.dump({"kit": "Big Rusty Drums", "author": "Karoryfer Samples", "license": "CC0 1.0", "sounds": index},
          open(f"{DST}/sons.json", "w"), ensure_ascii=False, indent=1)
