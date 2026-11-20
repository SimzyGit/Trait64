#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Fabrique les sons de l'échiquier : coup, prise, échec, roque, promotion et mat,
dans cinq jeux (bois, feutre, clic, marbre, rétro).

Synthétisés ici plutôt qu'empruntés : aucun fichier tiers, aucune licence à suivre.
Le bois est un choc de deux résonances graves vite amorties, avec un souffle très bref
à l'attaque ; le feutre en est la version étouffée ; le clic est sec et aigu ; le marbre
sonne plus long et plus clair ; le rétro est fait d'ondes carrées courtes.
Le tirage est fixé : relancer le script redonne les mêmes fichiers, à l'octet près.
"""
import math
import random
import struct
import wave
from pathlib import Path

RATE = 44100
ROOT = Path(__file__).resolve().parents[1] / "assets" / "sounds"


def knock(length, tones, noise, decay, delay=0.0, square=False):
    rng = random.Random(64)
    samples = []
    for i in range(int(RATE * length)):
        t = i / RATE - delay
        if t < 0:
            samples.append(0.0)
            continue
        if square:
            body = sum(level * (1.0 if math.sin(2 * math.pi * pitch * t) >= 0 else -1.0) * math.exp(-t * damping)
                       for pitch, level, damping in tones)
        else:
            body = sum(level * math.sin(2 * math.pi * pitch * t) * math.exp(-t * damping)
                       for pitch, level, damping in tones)
        breath = noise * rng.uniform(-1, 1) * math.exp(-t * 900)
        samples.append((body + breath) * math.exp(-t * decay) * min(1.0, t * 2000))
    return samples


def mix(*layers):
    size = max(len(layer) for layer in layers)
    return [sum(layer[i] for layer in layers if i < len(layer)) for i in range(size)]


def write(theme, name, samples):
    peak = max(abs(s) for s in samples)
    data = b"".join(struct.pack("<h", int(s / peak * 0.8 * 32767)) for s in samples)
    folder = ROOT / theme
    folder.mkdir(parents=True, exist_ok=True)
    with wave.open(str(folder / name), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(data)


def family(theme, main, sharp, bell, noise, decay, square=False):
    """Six sons d'un même matériau : le coup, puis ses variations."""
    k = lambda length, tones, n, d, delay=0.0: knock(length, tones, n, d, delay, square)
    write(theme, "move.wav", k(0.09, main, noise, decay))
    write(theme, "capture.wav", mix(k(0.12, sharp, noise * 1.8, decay * 1.2), k(0.12, main, noise * 0.8, decay, 0.028)))
    write(theme, "check.wav", mix(k(0.2, main, noise, decay), k(0.2, bell, 0.0, 6, 0.01)))
    # Le roque : deux pièces posées l'une après l'autre.
    write(theme, "castle.wav", mix(k(0.16, main, noise, decay), k(0.16, main, noise * 0.7, decay, 0.075)))
    # La promotion : le coup, puis une note qui monte.
    up = [(pitch * 1.5, level, damping) for pitch, level, damping in bell]
    write(theme, "promote.wav", mix(k(0.24, main, noise, decay), k(0.24, bell, 0.0, 9, 0.04), k(0.24, up, 0.0, 9, 0.11)))
    # Le mat : un accord tenu, plus long que l'échec.
    chord = bell + [(pitch * 1.26, level * 0.8, damping) for pitch, level, damping in bell]
    write(theme, "mate.wav", mix(k(0.45, main, noise, decay), k(0.45, chord, 0.0, 4, 0.02)))


family("bois", [(185, 1.0, 38), (410, 0.55, 60), (960, 0.18, 110)], [(240, 1.0, 50), (620, 0.6, 80), (1500, 0.25, 140)],
       [(880, 0.35, 14), (1320, 0.15, 18)], 0.5, 22)
family("feutre", [(140, 1.0, 55), (300, 0.4, 90)], [(170, 1.0, 70), (380, 0.5, 110)],
       [(660, 0.25, 18), (990, 0.1, 22)], 0.15, 34)
family("clic", [(1800, 1.0, 160), (3200, 0.4, 220)], [(2200, 1.0, 180), (4100, 0.5, 260)],
       [(1760, 0.3, 20), (2640, 0.12, 26)], 0.9, 60)
family("marbre", [(520, 1.0, 18), (1180, 0.6, 26), (2350, 0.25, 40)], [(610, 1.0, 22), (1430, 0.6, 30), (2900, 0.3, 48)],
       [(1046, 0.4, 8), (1568, 0.2, 10)], 0.35, 9)
family("retro", [(330, 0.5, 40)], [(220, 0.5, 45), (440, 0.25, 60)], [(660, 0.3, 12), (990, 0.15, 14)], 0.0, 26,
       square=True)
