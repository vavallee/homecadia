#!/usr/bin/env python3
"""Place footprints from the KiCad libraries into the generated .kicad_pcb,
assigning nets from the spec in gen.py. Run gen.py first."""
import os, sys, uuid, importlib.util
from kiutils.board import Board
from kiutils.footprint import Footprint
from kiutils.items.common import Position

FPROOT = "/mnt/c/Program Files/KiCad/10.0/share/kicad/footprints"
HERE = os.path.dirname(os.path.abspath(__file__))

spec = importlib.util.spec_from_file_location("gen", os.path.join(HERE, "gen.py"))
gen = importlib.util.module_from_spec(spec); spec.loader.exec_module(gen)

def load_fp(lib_id):
    lib, name = lib_id.split(":", 1)
    p = f"{FPROOT}/{lib}.pretty/{name}.kicad_mod"
    if not os.path.isfile(p):
        raise FileNotFoundError(lib_id)
    return Footprint.from_file(p)

for d in (gen.HB, gen.SB):
    name = d["name"]
    pcb = os.path.join(HERE, name, f"{name}.kicad_pcb")
    b = Board.from_file(pcb)
    netmap = {}                                     # (ref, pin) -> (code, name)
    for code, (nname, nodes) in enumerate(sorted(d["nets"].items()), start=1):
        for ref, pin in nodes:
            netmap[(ref, str(pin))] = (code, nname)
    placed = missing = 0
    for ref, val, fp_id, lib, part, lcsc, note in d["comps"]:
        if ref not in d["place"]:
            continue
        x, y, rot = d["place"][ref]
        try:
            f = load_fp(fp_id)
        except FileNotFoundError:
            print(f"  {name}: MISSING FOOTPRINT {fp_id} for {ref}"); missing += 1; continue
        f.libraryNickname, f.entryName = fp_id.split(":", 1)
        f.position = Position(X=x, Y=y, angle=rot or None)
        f.layer = "F.Cu"
        f.uuid = str(uuid.uuid4())
        if isinstance(f.properties, dict):
            f.properties["Reference"] = ref
            f.properties["Value"] = val
        f.graphicItems = [g for g in f.graphicItems]
        for gi in f.graphicItems:
            if getattr(gi, "type", None) == "reference": gi.text = ref
            if getattr(gi, "type", None) == "value":     gi.text = val
        for pad in f.pads:
            key = (ref, str(pad.number))
            if key in netmap:
                pad.net = __import__("kiutils.items.common", fromlist=["Net"]).Net(
                    number=netmap[key][0], name=netmap[key][1])
        b.footprints.append(f); placed += 1
    b.to_file(pcb)
    print(f"  {name}: placed {placed} footprints, {missing} missing, {len(b.nets)} nets")
