# Monta os kits de fábrica "Kit 1..N" a partir dos sons do usuário (autorizados por ele).
# Uso: python3 build_user_kits.py <pasta_com_os_pacotes_extraidos> <saida sounds/nexus>
import sys, os, json, unicodedata, numpy as np, soundfile as sf
SRC, DST = sys.argv[1], sys.argv[2]
idx = {}
for root, _, fs in os.walk(SRC):
    for f in fs:
        k = unicodedata.normalize("NFC", f.replace("#U0303", "̃").replace("#U0301", "́")).upper()
        idx.setdefault(k, os.path.join(root, f))
def F(name): 
    k = unicodedata.normalize("NFC", name).upper()
    if k not in idx: raise SystemExit("nao achei: " + name)
    return idx[k]

S = "BATERIA SERTANEJO BRUNO Samples/"
# kits: nome, eq/reverb, e peças -> lista de arquivos (vários = variações que se alternam)
KITS = [
 ("Kit 1", dict(kick=["BB.wav#S"], snare=["CX.wav#S"], rim=["ARO.wav"], tom1=["T1.wav#S"], tom2=["T2.wav#S"], floor=["T3.wav#S"],
   hat=["CHINBAL FECHADO.wav"], hato=["CHINBAL ABERTO.wav"], hatp=["CHINBAL PISADO.wav"], crash=["CLASH SABIAN.wav"], ride=["CUPULA 1.wav"],
   ride2=["CUPULA 2.wav"], china=["CHINA.wav"], splash=["SPLASH.wav"]), (1.0, 0.0, 1.0, 0.12, 0.45)),
 ("Kit 2", dict(kick=["Bumbo-KICK.wav"], snare=["CX-forró.wav"], snare2=["CX-reverb.wav"], tom1=["TOM-1_RÕTANTAN.wav"], tom2=["TOM-3.wav"],
   hato=["Cimbal-Aberto.wav"], hatp=["Cimbal-Pé-fx.wav"], crash=["Prato-1.wav"]), (1.0, 0.0, 1.5, 0.10, 0.40)),
 ("Kit 3", dict(kick=["bb process - SAMPLE LUCAS_1.wav", "bb sub process - SAMPLE LUCAS_2.wav", "bb sub e in process - SAMPLE LUCAS_3.wav"],
   snare=["CX - SAMPLE LUCAS.wav"], ghost=["CX GHOST - SAMPLE LUCAS.wav"], rim=["RT - SAMPLE LUCAS.wav"], tom1=["T1 - SAMPLE LUCAS.wav"],
   tom2=["T2 - SAMPLE LUCAS.wav"], tom3=["T3 - SAMPLE LUCAS.wav"], floor=["T4 - SAMPLE LUCAS.wav"], floor2=["T5 - SAMPLE LUCAS.wav"]), (1.0, 0.0, 1.0, 0.12, 0.45)),
 ("Kit 4", dict(kick=["BB PROCESS_02.wav", "BB PROCESS_03.1.wav"], snare=["SD UP.wav"], snare2=["SD DN.wav"], fx=["CX FX.wav"], rim=["RT 6.wav", "RT 8.wav"],
   tom1=["T1_03-05.wav"], tom2=["T2_03-09.wav"], tom3=["T3_03-05.wav"], floor=["T4_03-05.wav"], floor2=["T5_03-05.wav"], floor3=["T6_03-05.wav"]), (1.0, 0.0, 1.0, 0.10, 0.40)),
 ("Kit 5", dict(kick=["BB_01.wav"], snare=["CX_01.wav"], tom1=["T1_01.wav"], tom2=["T2_01.wav"], tom3=["T3_01.wav"], floor=["T4_01.wav"]), (2.0, 0.0, 1.0, 0.14, 0.50)),
 ("Kit 6", dict(kick=["BB 1.aif"], kick2=["BB SUB.aif"], snare=["CX.aif#G"], perc=["ESTEIRA.aif"], tom1=["T 8.aif"], tom2=["T 10.aif"], tom3=["T 12.aif"],
   floor=["T 14.aif"], floor2=["T 16.aif"]), (1.0, 0.0, 1.0, 0.10, 0.45)),
 ("Kit 7", dict(kick=["KK-GD.wav"], snare=["SN 57.cm.wav"], tom1=["TOM 1.cm.wav"], tom2=["TOM 2.cm.wav"], tom3=["TOM 3.cm.wav"], floor=["TOM 4.cm.wav"],
   roto=["HOTONTON.cm.wav", "HOTONTON 2.cm.wav"]), (1.0, 0.0, 1.0, 0.16, 0.55)),
 ("Kit 8", dict(kick=["BB.cm.wav"], kick2=["PEDAL L3.wav"], snare=["CAIXA ACÚSTICA.wav"], snare2=["CAIXA ACÚSTICA HM 2.wav"], tom1=["TOM 1 HM.wav"],
   tom2=["TOM 2 HM.wav"], tom3=["TOM 3 HM.wav"], floor=["TOM 4 HM.wav"], roto=["ROTON 01.wav", "ROTON 02.wav"]), (1.0, 0.0, 1.5, 0.20, 0.60)),
 ("Kit 9", dict(kick=["KICK IN - A.N .wav"], snare=["CX JV OK.wav"], snare2=["CX ADM _1.wav"], tom1=["TOM1 E 86.wav"], tom2=["TOM2 E 86.wav"], tom3=["TOM3 E 86.wav"],
   floor=["TOM4 E 86.wav"], floor2=["TOM5 E 86.wav"], floor3=["TOM6 E 86.wav"]), (1.0, 0.0, 1.0, 0.14, 0.50)),
 ("Kit 10", dict(kick=["BUMBO - LUCAS - RB.wav"], snare=["CAIXA - LUCAS - RB.wav"], tom1=["TOM 1 - SAMPLE LUCAS MIX - RB.wav"],
   tom2=["TOM 2 - SAMPLE LUCAS MIX - RB.wav"], floor=["TOM 3 - SAMPLE LUCAS MIX - RB.wav"], tom3=["TOM 16 - 421 .wav"]), (1.0, 0.0, 1.0, 0.12, 0.45)),
 ("Kit 11", dict(kick=["BUMBO PISEIRO.wav"], kick2=["BUMBO - PISEIRO MADIMBAS.wav"], kick3=["BUMBO DJ IVIS - 01.wav"], kick4=["01 BB SAPAO.wav"],
   snare=["CAIXA CACHORRO ORIGINAL.wav"], snare2=["CAIXA EFEITO - LATA.wav"], fx=["CAIXA DE EFEITO - TECLADO.wav"], snare3=["CAIXA HENRY.wav"],
   tom1=["TOM ELETRONICO 1.1-01.wav"], tom2=["TOM ELETRONICO 2.1-01.wav"], tom3=["TOM ELETRONICO 3.1-01.wav"], floor=["TOM ELETRONICO 5.1-01.wav"],
   floor2=["TOM ELETRONICO 6.1-01.wav"], floor3=["TOM ELETRONICO 7.1-01.wav"]), (2.0, -1.0, 2.0, 0.10, 0.40)),
 ("Kit 12", dict(kick=["BB - CAMISETA CD LEO.wav"], kick2=["BUMBO - PROMO GUILHERME DANTAS_2.wav"], kick3=["BB.wav"], kick4=["BD REP_01.wav"], kick5=["01 Bumbo.wav"],
   snare=["CAIXA GUILHERME DANTAS - PROMOCIONAL.wav"], snare2=["CX PEDALADA.wav"], fx=["CX FX - PEDALADA.wav"], tom1=["TOM 1_01.wav"], tom2=["TOM 2_01.wav"],
   tom3=["TOM 3_01.wav"], floor=["TOM 4_01.wav"], floor2=["T 16 - PEDALADA.wav"], tom4=["TOM 1 MAZZEI.wav"], tom5=["TOM 2 MAZZEI.wav"],
   floor3=["TOM 3 MAZZEI.wav"], floor4=["TOM 4 MAZZEI.wav"]), (1.0, 0.0, 1.0, 0.12, 0.45)),
 ("Surdos", dict(surdo1=["22_01.wav"], surdo2=["24_02.wav"], surdo3=["22 Rep_01.wav"], surdo4=["24 Rep_01.wav"], surdo5=["SD 22 SAMPLER .wav"],
   surdo6=["SD 24 SAMPLER.wav"], surdo7=["SDLOW_04.wav"], surdo8=["SDMID_04.wav"], surdo9=["FUNDO  1.wav"], surdo10=["FUNDO2.wav"],
   surdo11=["TIMBA SRD 22"], surdo12=["TIMBA SRD 24"], surdo13=["SRD REP 22"], surdo14=["SRD REP 24"]), None),
]
# nome visível e categoria de cada tipo de peça
PIECE = {"kick": ("Bumbo", "Bumbo"), "snare": ("Caixa", "Caixa"), "ghost": ("Caixa Ghost", "Caixa"), "rim": ("Caixa Aro", "Caixa"), "fx": ("Caixa Efeito", "Caixa"),
         "tom": ("Tom", "Tom"), "floor": ("Surdo", "Tom"), "roto": ("Rototom", "Tom"), "hat": ("Chimbal Fechado", "Chimbal"), "hato": ("Chimbal Aberto", "Chimbal"),
         "hatp": ("Chimbal Pedal", "Chimbal"), "crash": ("Prato Ataque", "Prato"), "ride": ("Condução", "Prato"), "china": ("China", "Prato"),
         "splash": ("Splash", "Prato"), "perc": ("Percussão", "Percussão"), "surdo": ("Surdo de Marcação", "Percussão")}
MAXDUR = {"crash": 4.0, "ride": 4.0, "china": 4.0, "splash": 3.0, "hato": 2.5, "hat": 0.8, "hatp": 0.8}

def piece(key):
    base = key.rstrip("0123456789"); num = key[len(base):]
    nm, cat = PIECE[base]
    return base, (nm + (" " + num if num else "")), cat

def load(name):
    pick = None
    if "#" in name: name, pick = name.split("#")
    # "#S" = do pacote sertanejo, "#G" = da pasta do jingle (nomes repetidos)
    cand = [p for k, p in idx.items() if unicodedata.normalize("NFC", k) == unicodedata.normalize("NFC", name).upper()]
    if pick == "S": cand = [p for p in cand if "SERTANEJO" in p]
    if pick == "G": cand = [p for p in cand if "JINGLE" in p]
    if not cand: raise SystemExit("nao achei: " + name)
    x, fs = sf.read(cand[0], dtype="float32", always_2d=True)
    if x.shape[1] == 1: x = np.repeat(x, 2, 1)
    return x[:, :2], fs

def prep(x, fs, maxdur):
    a = np.abs(x).max(1); pk = a.max()
    if pk < 1e-6: return None
    s0 = max(0, int(np.argmax(a > pk * 0.02)) - int(0.001 * fs))
    x = x[s0:]
    a = np.abs(x).max(1)
    above = np.where(a > pk * 0.0015)[0]            # ~ -56 dB do pico
    end = min(len(x), int(above[-1]) + int(0.02 * fs) if len(above) else len(x), int(maxdur * fs))
    x = x[:end].copy()
    fl = min(len(x), int(0.04 * fs)); x[-fl:] *= np.linspace(1, 0, fl, dtype=np.float32)[:, None]
    return x * (0.89 / pk), fs

os.makedirs(DST, exist_ok=True)
sounds, kits = [], []
for kitname, pieces, mix in KITS:
    ids = {}
    for key, files in pieces.items():
        base, nm, cat = piece(key)
        sid = (kitname.replace(" ", "").lower() + "_" + key)
        takes = []
        for f in files:
            x, fs = load(f)
            r = prep(x, fs, MAXDUR.get(base, 2.5 if base in ("floor", "tom", "surdo") else 1.6))
            if r is not None: takes.append(r)
        os.makedirs(f"{DST}/{sid}", exist_ok=True)
        for i, (t, fs) in enumerate(takes):
            sf.write(f"{DST}/{sid}/v01_rr{i+1}.flac", t, fs, subtype="PCM_16")
        name = f"{nm} · {kitname}" if kitname != "Surdos" else nm
        sounds.append({"id": sid, "name": name, "category": cat, "choke": 1 if base.startswith("hat") else 0, "layers": 1})
        ids[key] = name
    if mix:
        lo, mid, hi, rw, rr = mix
        k1 = KITS[0][1]
        def get(*keys, fallback=None):
            for k in keys:
                if k in ids: return ids[k]
            return fallback
        sert = {k: f"{piece(k)[1]} · Kit 1" for k in k1}
        kits.append({"name": kitname,
            "kick": get("kick"), "snare": get("snare"), "hat": get("hat", fallback=sert["hat"]),
            "tom1": get("tom1"), "tom2": get("tom2", "tom1"), "floor": get("floor", "tom3", "tom2"),
            "crash": get("crash", fallback=sert["crash"]), "ride": get("ride", fallback=sert["ride"]),
            "eqLow": lo, "eqMid": mid, "eqHigh": hi, "revWet": rw, "revRoom": rr, "blend": 0.6})
json.dump({"kit": "Kits Nexus", "license": "Uso autorizado pelos autores para o Nexus Drum Trigger", "sounds": sounds, "kits": kits},
          open(f"{DST}/sons.json", "w"), ensure_ascii=False, indent=1)
print(len(sounds), "sons", len(kits), "kits")
