#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Fabrique les trois sons de l'échiquier : coup, prise, échec.

Synthétisés ici plutôt qu'empruntés : aucun fichier tiers, aucune licence à suivre.
Un coup est un choc de bois — deux résonances graves vite amorties et un souffle très
bref à l'attaque ; la prise est plus sèche et doublée ; l'échec y ajoute une note tenue.
Le tirage est fixé : relancer le script redonne les mêmes fichiers, à l'octet près.
"""
import math
import random
import struct
import wave
from pathlib import Path

RATE = 44100


def knock(length, tones, noise, decay, delay=0.0):
    rng = random.Random(64)
    samples = []
    for i in range(int(RATE * length)):
        t = i / RATE - delay
        if t < 0:
            samples.append(0.0)
            continue
        body = sum(level * math.sin(2 * math.pi * pitch * t) * math.exp(-t * damping)
                   for pitch, level, damping in tones)
        breath = noise * rng.uniform(-1, 1) * math.exp(-t * 900)
        samples.append((body + breath) * math.exp(-t * decay) * min(1.0, t * 2000))
    return samples


def mix(*layers):
    size = max(len(layer) for layer in layers)
    return [sum(layer[i] for layer in layers if i < len(layer)) for i in range(size)]


def write(name, samples):
    peak = max(abs(s) for s in samples)
    data = b"".join(struct.pack("<h", int(s / peak * 0.8 * 32767)) for s in samples)
    with wave.open(str(Path(__file__).resolve().parents[1] / "assets" / "sounds" / name), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(data)


wood = [(185, 1.0, 38), (410, 0.55, 60), (960, 0.18, 110)]
dry = [(240, 1.0, 50), (620, 0.6, 80), (1500, 0.25, 140)]
write("move.wav", knock(0.09, wood, 0.5, 22))
write("capture.wav", mix(knock(0.12, dry, 0.9, 26), knock(0.12, wood, 0.4, 24, delay=0.028)))
write("check.wav", mix(knock(0.2, wood, 0.5, 22), knock(0.2, [(880, 0.35, 14), (1320, 0.15, 18)], 0.0, 6, delay=0.01)))
