#!/usr/bin/env python3
"""Generate KiCad project files for homecadia hb-01 / sb-01 from a netlist spec."""
import json, os, uuid, textwrap

ROOT = os.path.dirname(os.path.abspath(__file__))
DATE = "2026-08-13"

# ---------------------------------------------------------------- design data

HB = dict(
    name="hb-01",
    title="homecadia sensor-01 harness board",
    w=48.0, h=40.0,
    holes=[(3.5, 3.5), (44.5, 3.5), (3.5, 36.5), (44.5, 36.5)],
    comps=[
        # ref, value, footprint, lib, part, lcsc, note
        ("J1", "USB-C 16P", "Connector_USB:USB_C_Receptacle_XKB_U262-16XN-4BVC11",
         "Connector", "USB_C_Receptacle_USB2.0_16P", "C2765186", "power only; D+/D- to test pads"),
        ("R1", "5.1k", "Resistor_SMD:R_0805_2012Metric", "Device", "R", "", "CC1 pulldown"),
        ("R2", "5.1k", "Resistor_SMD:R_0805_2012Metric", "Device", "R", "", "CC2 pulldown"),
        ("D1", "SP0503BAHTG", "Package_TO_SOT_SMD:SOT-143", "Power_Protection",
         "SP0503BAHT", "C3040626", "ESD array on VBUS/D+/D-"),
        ("R3", "1M 1%", "Resistor_SMD:R_0805_2012Metric", "Device", "R", "", "divider high side"),
        ("R4", "1M 1%", "Resistor_SMD:R_0805_2012Metric", "Device", "R", "", "divider low side"),
        ("C1", "100nF", "Capacitor_SMD:C_0805_2012Metric", "Device", "C", "", "ADC hold cap"),
        ("R5", "1k", "Resistor_SMD:R_0805_2012Metric", "Device", "R", "", "LED series"),
        ("D2", "LED 3mm", "LED_THT:LED_D3.0mm", "Device", "LED", "", "or 2-pin header to panel LED"),
        ("SW1", "PEC11R-4220F-S0024", "Rotary_Encoder:RotaryEncoder_Bourns_Vertical_PEC12R-3x17F-Sxxxx",
         "Device", "Rotary_Encoder_Switch", "", "PEC12R fp: terminals verified vs PEC11R datasheet; boss holes 3.1mm oval, unverified"),
        ("J2", "XIAO field 2x7", "Connector_PinHeader_2.54mm:PinHeader_2x07_P2.54mm_Vertical",
         "Connector_Generic", "Conn_02x07_Odd_Even", "", "labelled field to XIAO / driver-board breakout"),
        ("J3", "BAT JST-PH", "Connector_JST:JST_PH_B2B-PH-K_1x02_P2.00mm_Vertical",
         "Connector_Generic", "Conn_01x02", "C131337", "polarity is NOT guaranteed by the key"),
        ("J4", "SHT40 JST-SH", "Connector_JST:JST_SH_BM04B-SRSS-TB_1x04-1MP_P1.00mm_Vertical",
         "Connector_Generic", "Conn_01x04", "", "to sb-01 satellite"),
        ("J5", "5V out", "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical",
         "Connector_Generic", "Conn_01x02", "", "VBUS/GND to XIAO 5V pin"),
        ("TP5", "USB_DP", "TestPoint:TestPoint_Pad_D1.5mm", "Connector", "TestPoint", "", "USB D+ break-out"),
        ("TP6", "USB_DM", "TestPoint:TestPoint_Pad_D1.5mm", "Connector", "TestPoint", "", "USB D- break-out"),
        ("TP1", "VBAT", "TestPoint:TestPoint_Pad_D1.5mm", "Connector", "TestPoint", "", ""),
        ("TP2", "VDIV", "TestPoint:TestPoint_Pad_D1.5mm", "Connector", "TestPoint", "", ""),
        ("TP3", "GND", "TestPoint:TestPoint_Pad_D1.5mm", "Connector", "TestPoint", "", ""),
        ("TP4", "3V3", "TestPoint:TestPoint_Pad_D1.5mm", "Connector", "TestPoint", "", ""),
        ("H1", "M2", "MountingHole:MountingHole_2.2mm_M2", "Mechanical", "MountingHole", "", ""),
        ("H2", "M2", "MountingHole:MountingHole_2.2mm_M2", "Mechanical", "MountingHole", "", ""),
        ("H3", "M2", "MountingHole:MountingHole_2.2mm_M2", "Mechanical", "MountingHole", "", ""),
        ("H4", "M2", "MountingHole:MountingHole_2.2mm_M2", "Mechanical", "MountingHole", "", ""),
    ],
    place={
        "J1":(5.0,16.6,90), "R1":(8.7,13.6,0), "R2":(8.7,18.4,0), "D1":(8.7,23.2,0),
        "J3":(8.9,32.6,0), "R3":(17.3,32.2,0), "R4":(22.7,32.2,0), "C1":(28.1,32.2,0),
        "R5":(33.5,32.2,0), "SW1":(23.25,13.9,0), "J2":(42.4,12.2,0), "J4":(21.6,3.8,0),
        "J5":(32.3,3.8,0), "D2":(11.1,4.0,0),
        "TP1":(15.6,26.6,0), "TP2":(21.0,26.6,0), "TP3":(26.4,26.6,0), "TP4":(31.8,26.6,0), "TP5":(15.6,22.6,0), "TP6":(21.0,22.6,0),
        "H1":(3.5,3.5,0), "H2":(44.5,3.5,0), "H3":(3.5,36.5,0), "H4":(44.5,36.5,0),
    },
    nets={
        "VBUS":   [("J1", "A4"), ("J1", "B4"), ("J1", "A9"), ("J1", "B9"), ("D1", "3"), ("J5", "1")],
        "CC1":    [("J1", "A5"), ("R1", "1")],
        "USB_DP": [("J1", "A6"), ("J1", "B6"), ("D1", "1"), ("TP5", "1")],
        "USB_DM": [("J1", "A7"), ("J1", "B7"), ("TP6", "1")],
        "CC2":    [("J1", "B5"), ("R2", "1")],
        "VBAT":   [("J3", "1"), ("R3", "1"), ("TP1", "1")],
        "VDIV":   [("R3", "2"), ("R4", "1"), ("C1", "1"), ("J2", "13"), ("TP2", "1")],
        "+3V3":   [("J2", "12"), ("J4", "1"), ("TP4", "1")],
        "SDA":    [("J2", "5"), ("J4", "3")],
        "SCL":    [("J2", "6"), ("J4", "2")],
        "ENC_A":  [("J2", "8"), ("SW1", "A")],
        "ENC_B":  [("J2", "10"), ("SW1", "B")],
        "ENC_SW": [("J2", "14"), ("SW1", "S1")],
        "LED_A":  [("J2", "7"), ("R5", "1")],
        "LED_K":  [("R5", "2"), ("D2", "2")],
        "GND":    [("J1", "A1"), ("J1", "B1"), ("J1", "A12"), ("J1", "B12"),
                   ("R1", "2"), ("R2", "2"), ("D1", "2"), ("R4", "2"), ("C1", "2"),
                   ("D2", "1"), ("SW1", "C"), ("SW1", "S2"), ("J2", "1"), ("J3", "2"),
                   ("J4", "4"), ("J5", "2"), ("TP3", "1")],
    },
)

SB = dict(
    name="sb-01",
    title="homecadia SHT40 satellite",
    w=14.0, h=16.0,
    holes=[(7.0, 2.5)],
    comps=[
        ("U1", "SHT40-AD1B-R2",
         "Sensor_Humidity:Sensirion_DFN-4_1.5x1.5mm_P0.8mm_SHT4x_NoCentralPad",
         "Sensor_Humidity", "SHT40-AD1B", "C2909890", "DFN-4 no central pad; needs reflow or hot air"),
        ("C1", "100nF", "Capacitor_SMD:C_0805_2012Metric", "Device", "C", "", "decoupling"),
        ("J1", "JST-SH 4P", "Connector_JST:JST_SH_BM04B-SRSS-TB_1x04-1MP_P1.00mm_Vertical",
         "Connector_Generic", "Conn_01x04", "", "to hb-01 J4"),
        ("H1", "M2", "MountingHole:MountingHole_2.2mm_M2", "Mechanical", "MountingHole", "", ""),
    ],
    place={"U1":(7.0,6.6,0), "C1":(3.0,6.6,0), "J1":(7.0,12.6,0), "H1":(7.0,2.5,0)},
    nets={
        "+3V3": [("U1", "1"), ("C1", "1"), ("J1", "1")],
        "SCL":  [("U1", "2"), ("J1", "2")],
        "SDA":  [("U1", "4"), ("J1", "3")],
        "GND":  [("U1", "3"), ("C1", "2"), ("J1", "4")],
    },
)

# ---------------------------------------------------------------- emitters

def u():
    return str(uuid.uuid4())

def kicad_pro(d):
    return json.dumps({
        "board": {"design_settings": {"defaults": {"board_outline_line_width": 0.1},
                                      "rules": {"min_clearance": 0.127, "min_track_width": 0.25,
                                                "min_via_diameter": 0.6, "min_through_hole_diameter": 0.3}}},
        "meta": {"filename": f"{d['name']}.kicad_pro", "version": 1},
        "net_settings": {"classes": [{"name": "Default", "clearance": 0.127, "track_width": 0.25,
                                      "via_diameter": 0.8, "via_drill": 0.4},
                                     {"name": "Power", "clearance": 0.25, "track_width": 0.6,
                                      "via_diameter": 0.9, "via_drill": 0.5}]},
        "pcbnew": {"last_paths": {}, "page_layout_descr_file": ""},
        "schematic": {"legacy_lib_dir": "", "legacy_lib_list": []},
        "sheets": [], "text_variables": {},
    }, indent=2)

def kicad_pcb(d):
    """Board outline + mounting holes + silkscreen. Footprints are added by
    importing the .net file in Pcbnew."""
    W, H = d["w"], d["h"]
    L = []
    L.append('(kicad_pcb (version 20221018) (generator homecadia_gen)')
    L.append('  (general (thickness 1.6))')
    L.append('  (paper "A4")')
    L.append(f'  (title_block (title "{d["title"]}") (date "{DATE}") (rev "A")')
    L.append('    (comment 1 "Generated from circuit-board-maker/gen spec - UNVERIFIED, never opened in KiCad")')
    L.append('  )')
    L.append('  (layers')
    for i, (nm, ty) in enumerate([("F.Cu", "signal"), ("B.Cu", "signal")]):
        L.append(f'    ({i*31} "{nm}" {ty})')
    for n, (nm, ty) in enumerate([("B.Adhes", "user"), ("F.Adhes", "user"), ("B.Paste", "user"),
                                  ("F.Paste", "user"), ("B.SilkS", "user"), ("F.SilkS", "user"),
                                  ("B.Mask", "user"), ("F.Mask", "user"), ("Dwgs.User", "user"),
                                  ("Cmts.User", "user"), ("Eco1.User", "user"), ("Eco2.User", "user"),
                                  ("Edge.Cuts", "user"), ("Margin", "user"), ("B.CrtYd", "user"),
                                  ("F.CrtYd", "user"), ("B.Fab", "user"), ("F.Fab", "user")], start=32):
        L.append(f'    ({n} "{nm}" {ty})')
    L.append('  )')
    L.append('  (setup (pad_to_mask_clearance 0) (solder_mask_min_width 0.1)\n    (rules (min_clearance 0.127) (min_track_width 0.2) (min_via_diameter 0.6)))')
    L.append('  (net 0 "")')
    for i, n in enumerate(sorted(d["nets"]), start=1):
        L.append(f'  (net {i} "{n}")')
    # outline, 2mm corner radius approximated with straight segments
    pts = [(0, 0), (W, 0), (W, H), (0, H), (0, 0)]
    for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
        L.append(f'  (gr_line (start {x1} {y1}) (end {x2} {y2}) '
                 f'(stroke (width 0.1) (type solid)) (layer "Edge.Cuts") (tstamp {u()}))')
    for (hx, hy) in d["holes"]:
        L.append(f'  (gr_circle (center {hx} {hy}) (end {hx+1.1} {hy}) '
                 f'(stroke (width 0.05) (type solid)) (fill none) (layer "Edge.Cuts") (tstamp {u()}))')
    if W >= 30:
        L.append(f'  (gr_text "{d["title"]}" (at {W/2} {H-3}) (layer "F.SilkS") (tstamp {u()})'
                 f' (effects (font (size 1.2 1.2) (thickness 0.2))))')
        L.append(f'  (gr_text "rev A  {DATE}  UNBUILT" (at {W/2} {H-1.4}) (layer "F.SilkS") (tstamp {u()})'
                 f' (effects (font (size 0.8 0.8) (thickness 0.15))))')
    else:
        L.append(f'  (gr_text "{d["name"]} revA" (at {W/2} {H-1.5}) (layer "F.SilkS") (tstamp {u()})'
                 f' (effects (font (size 0.9 0.9) (thickness 0.15))))')
    L.append(')')
    return "\n".join(L) + "\n"

def kicad_net(d):
    L = ['(export (version "E")', '  (design',
         f'    (source "{d["name"]}.kicad_sch")', f'    (date "{DATE}")',
         '    (tool "homecadia gen.py")', '  )', '  (components']
    for ref, val, fp, lib, part, lcsc, note in d["comps"]:
        L.append(f'    (comp (ref "{ref}")')
        L.append(f'      (value "{val}")')
        L.append(f'      (footprint "{fp}")')
        if lcsc:
            L.append(f'      (property (name "LCSC") (value "{lcsc}"))')
        if note:
            L.append(f'      (property (name "Note") (value "{note}"))')
        L.append(f'      (libsource (lib "{lib}") (part "{part}") (description ""))')
        L.append(f'      (sheetpath (names "/") (tstamps "/"))')
        L.append(f'      (tstamps "{u()}"))')
    L.append('  )')
    L.append('  (nets')
    for i, (name, nodes) in enumerate(sorted(d["nets"].items()), start=1):
        L.append(f'    (net (code "{i}") (name "{name}")')
        for ref, pin in nodes:
            L.append(f'      (node (ref "{ref}") (pin "{pin}") (pintype "passive"))')
        L.append('    )')
    L.append('  )')
    L.append(')')
    return "\n".join(L) + "\n"

def netlist_md(d):
    L = [f"# {d['title']} — netlist\n",
         f"Board `{d['name']}`, {d['w']:.0f} x {d['h']:.0f} mm, 2 layer, 1.6 mm FR4.",
         f"Generated {DATE}. **Never opened in KiCad — see the warning in README.md.**\n",
         "## Components\n",
         "| Ref | Value | Footprint | LCSC | Note |", "|---|---|---|---|---|"]
    for ref, val, fp, lib, part, lcsc, note in d["comps"]:
        code = f"[{lcsc}](https://www.lcsc.com/product-detail/{lcsc}.html)" if lcsc else "—"
        L.append(f"| {ref} | {val} | `{fp}` | {code} | {note or ''} |")
    L += ["\n## Nets\n", "| Net | Nodes |", "|---|---|"]
    for name, nodes in sorted(d["nets"].items()):
        L.append(f"| `{name}` | " + ", ".join(f"{r}.{p}" for r, p in nodes) + " |")
    return "\n".join(L) + "\n"

# ---------------------------------------------------------------- write

for d in (HB, SB):
    dd = os.path.join(ROOT, d["name"])
    os.makedirs(dd, exist_ok=True)
    open(os.path.join(dd, f"{d['name']}.kicad_pro"), "w").write(kicad_pro(d))
    open(os.path.join(dd, f"{d['name']}.kicad_pcb"), "w").write(kicad_pcb(d))
    open(os.path.join(dd, f"{d['name']}.net"), "w").write(kicad_net(d))
    open(os.path.join(dd, "NETLIST.md"), "w").write(netlist_md(d))
    print("wrote", dd, sorted(os.listdir(dd)))
