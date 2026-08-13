#!/usr/bin/env python3
"""Generate board plan + interconnect SVGs for hb-01 / sb-01."""
import os
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "img")
os.makedirs(OUT, exist_ok=True)

CSS = """
  .bg{fill:#fbfbf9}.brd{fill:#dfe9e3;stroke:#2f5d4f;stroke-width:2}
  .cu{fill:#c8dcd2;stroke:#4a7c68;stroke-width:1}
  .pt{fill:#fff;stroke:#38403d;stroke-width:1.2}
  .hot{fill:#f6dcc8;stroke:#b4632a;stroke-width:1.2}
  .cool{fill:#d3e0ef;stroke:#3f6ea6;stroke-width:1.2}
  .hole{fill:#fbfbf9;stroke:#2f5d4f;stroke-width:1.5}
  .t{font-family:ui-monospace,SFMono-Regular,Menlo,monospace;fill:#222826}
  .lbl{font-size:11px}.sm{font-size:9px;fill:#5c6663}.hd{font-size:15px;font-weight:700}
  .cap{font-size:10px;fill:#5c6663}
  .wire{stroke:#4a7c68;stroke-width:1.6;fill:none}
  .wired{stroke:#b4632a;stroke-width:1.6;fill:none;stroke-dasharray:5 3}
  .box{fill:#fff;stroke:#38403d;stroke-width:1.5;rx:4}
  .boxa{fill:#eef4f1;stroke:#2f5d4f;stroke-width:2;rx:4}
"""

def hdr(w, h, title):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
            f'viewBox="0 0 {w} {h}" role="img" aria-label="{title}">'
            f'<style>{CSS}</style><rect class="bg" width="{w}" height="{h}"/>')

def txt(x, y, s, cls="t lbl", anchor="middle"):
    return f'<text class="{cls}" x="{x}" y="{y}" text-anchor="{anchor}">{s}</text>'

def part(x, y, w, h, ref, val, cls="pt"):
    o = [f'<rect class="{cls}" x="{x}" y="{y}" width="{w}" height="{h}" rx="2"/>']
    o.append(txt(x + w / 2, y + h / 2 + 1, ref, "t lbl"))
    if val:
        o.append(txt(x + w / 2, y + h + 11, val, "t sm"))
    return "".join(o)

# ------------------------------------------------------------------ hb-01 plan
S = 8.4                       # px per mm
BW, BH = 48 * S, 40 * S       # 403 x 336
OX, OY = 60, 74
W, H = int(BW + 2 * OX), int(BH + OY + 74)
p = [hdr(W, H, "hb-01 harness board plan view")]
p.append(txt(W / 2, 30, "hb-01 &#8212; harness board", "t hd"))
p.append(txt(W / 2, 50, "48 &#215; 40 mm &#183; 2 layer &#183; 1.6 mm FR4 &#183; all parts hand-solderable", "t cap"))
p.append(f'<rect class="brd" x="{OX}" y="{OY}" width="{BW}" height="{BH}" rx="10"/>')
for hx, hy in [(3.5, 3.5), (44.5, 3.5), (3.5, 36.5), (44.5, 36.5)]:
    p.append(f'<circle class="hole" cx="{OX+hx*S}" cy="{OY+hy*S}" r="{1.1*S}"/>')
    p.append(txt(OX + hx * S, OY + hy * S + 3, "M2", "t sm"))

def mm(x, y, w, h, ref, val, cls="pt"):
    return part(OX + x * S, OY + y * S, w * S, h * S, ref, val, cls)

p.append(mm(0.6, 12, 3.2, 9.2, "J1", "", "hot"))
p.append(txt(OX + 2.2 * S, OY + 22.6 * S, "USB-C 16P", "t sm"))
p.append(txt(OX + 2.2 * S, OY + 23.9 * S, "C2765186", "t sm"))
p.append(mm(6.4, 12.4, 4.6, 2.3, "R1", "5.1k CC1"))
p.append(mm(6.4, 17.2, 4.6, 2.3, "R2", "5.1k CC2"))
p.append(mm(6.4, 22.0, 4.6, 2.3, "D1", "SP0503 ESD"))
p.append(mm(5.6, 30.6, 6.6, 4.0, "J3", "BAT JST-PH", "hot"))
p.append(mm(15.2, 31.0, 4.2, 2.3, "R3", "1M"))
p.append(mm(20.6, 31.0, 4.2, 2.3, "R4", "1M"))
p.append(mm(26.0, 31.0, 4.2, 2.3, "C1", "100nF"))
p.append(mm(31.4, 31.0, 4.2, 2.3, "R5", "1k"))
p.append(f'<circle class="cool" cx="{OX+30.5*S}" cy="{OY+16.4*S}" r="{6.25*S}"/>')
p.append(txt(OX + 30.5 * S, OY + 16.0 * S, "SW1", "t lbl"))
p.append(txt(OX + 30.5 * S, OY + 18.0 * S, "EC11", "t sm"))
p.append(txt(OX + 30.5 * S, OY + 24.1 * S, "PEC11R-4220F", "t sm"))
p.append(mm(40.5, 11.4, 5.2, 15.0, "J2", "", "cool"))
p.append(txt(OX + 43.1 * S, OY + 27.8 * S, "2&#215;7 XIAO field", "t sm"))
p.append(mm(18.5, 2.2, 6.2, 3.2, "J4", "SHT40 JST-SH &#8594; sb-01"))
p.append(mm(30.0, 2.4, 4.6, 2.8, "J5", "5V out"))
p.append(mm(9.5, 2.4, 3.2, 3.2, "D2", "LED"))
for i, (nm, x) in enumerate([("TP1 VBAT", 15.6), ("TP2 VDIV", 21.0), ("TP3 GND", 26.4), ("TP4 3V3", 31.8)]):
    p.append(f'<circle class="pt" cx="{OX+x*S}" cy="{OY+26.6*S}" r="{0.85*S}"/>')
    p.append(txt(OX + x * S, OY + 28.9 * S, nm, "t sm"))
p.append(txt(OX + BW / 2, OY + BH + 26, "orange = carries battery / VBUS  &#183;  blue = connects to the XIAO stack", "t cap"))
p.append(txt(OX + BW / 2, OY + BH + 44, "placement is indicative, not a layout &#8212; nothing here has been routed or DRC&#8217;d", "t cap"))
p.append("</svg>")
open(f"{OUT}/hb-01-plan.svg", "w").write("".join(p))

# ------------------------------------------------------------------ sb-01 plan
S2 = 22
BW2, BH2 = 12 * S2, 12 * S2
OX2, OY2 = 70, 74
W2, H2 = int(BW2 + 2 * OX2), int(BH2 + OY2 + 70)
q = [hdr(W2, H2, "sb-01 SHT40 satellite plan view")]
q.append(txt(W2 / 2, 30, "sb-01 &#8212; SHT40 satellite", "t hd"))
q.append(txt(W2 / 2, 50, "12 &#215; 12 mm &#183; keeps the sensor out of MCU heat", "t cap"))
q.append(f'<rect class="brd" x="{OX2}" y="{OY2}" width="{BW2}" height="{BH2}" rx="8"/>')
q.append(f'<circle class="hole" cx="{OX2+6*S2}" cy="{OY2+2.5*S2}" r="{1.1*S2}"/>')
q.append(txt(OX2 + 6 * S2, OY2 + 2.5 * S2 + 3, "M2", "t sm"))
q.append(part(OX2 + 5.2 * S2, OY2 + 5.2 * S2, 1.6 * S2, 1.6 * S2, "U1", "", "cool"))
q.append(txt(OX2 + 6 * S2, OY2 + 7.6 * S2, "SHT40-AD1B", "t sm"))
q.append(txt(OX2 + 6 * S2, OY2 + 8.5 * S2, "DFN-4 &#183; reflow only", "t sm"))
q.append(part(OX2 + 1.2 * S2, OY2 + 5.6 * S2, 2.2 * S2, 1.2 * S2, "C1", "100nF"))
q.append(part(OX2 + 3.4 * S2, OY2 + 9.6 * S2, 5.2 * S2, 1.8 * S2, "J1", "JST-SH 4P &#8594; hb-01 J4"))
q.append(txt(W2 / 2, OY2 + BH2 + 30, "if reflow isn&#8217;t available, keep the Grove SHT40 module", "t cap"))
q.append(txt(W2 / 2, OY2 + BH2 + 46, "and give it a mounting bracket instead", "t cap"))
q.append("</svg>")
open(f"{OUT}/sb-01-plan.svg", "w").write("".join(q))

# ------------------------------------------------------------- interconnect
W3, H3 = 1000, 470
r = [hdr(W3, H3, "sensor-01 Option 0 interconnect")]
r.append(txt(W3 / 2, 30, "sensor-01 &#8212; Option 0 interconnect", "t hd"))
r.append(txt(W3 / 2, 50, "hb-01 replaces the perfboard and flying components. The XIAO and the Seeed driver board stay.", "t cap"))
B = [
    ("LiPo LP103454", "2000 mAh &#183; 56&#215;34.5&#215;10.6", 40, 90, 170, 56, "boxa"),
    ("panel USB-C", "&#9888; CC not exposed", 40, 190, 170, 56, "boxa"),
    ("hb-01", "divider &#183; CC 5.1k &#183; ESD&#10;encoder &#183; LED &#183; JSTs", 300, 130, 190, 108, "boxa"),
    ("XIAO ESP32-C6", "MCU &#183; charger &#183; radio", 580, 60, 190, 60, "box"),
    ("Seeed driver bd", "SSD1680 booster &#183; FPC", 580, 150, 190, 60, "box"),
    ("2.9&quot; e-paper", "296&#215;128 SSD1680", 830, 150, 130, 60, "box"),
    ("sb-01", "SHT40 satellite", 300, 300, 190, 56, "boxa"),
    ("EC11 + LED", "on hb-01", 40, 300, 170, 56, "box"),
]
for t1, t2, x, y, w, h, cls in B:
    r.append(f'<rect class="{cls}" x="{x}" y="{y}" width="{w}" height="{h}"/>')
    r.append(txt(x + w / 2, y + 22, t1, "t lbl"))
    for i, ln in enumerate(t2.split("&#10;")):
        r.append(txt(x + w / 2, y + 38 + i * 13, ln, "t sm"))
A = [(210, 118, 300, 160, "wire", "VBAT"), (210, 218, 300, 200, "wire", "VBUS"),
     (490, 160, 580, 100, "wire", "5V / GPIO"), (490, 200, 580, 185, "wire", "GPIO"),
     (770, 185, 830, 185, "wire", "24p FPC"), (395, 238, 395, 300, "wired", "I2C 4-wire"),
     (675, 120, 675, 150, "wire", "2&#215;7 socket"), (210, 328, 300, 250, "wire", "GPIO")]
for x1, y1, x2, y2, cls, lab in A:
    mx, my = (x1 + x2) / 2, (y1 + y2) / 2
    r.append(f'<path class="{cls}" d="M{x1} {y1} L{x2} {y2}"/>')
    r.append(f'<rect x="{mx-34}" y="{my-9}" width="68" height="15" fill="#fbfbf9" rx="2"/>')
    r.append(txt(mx, my + 2, lab, "t sm"))
r.append(txt(W3 / 2, 400, "Dashed = the one link that must stay a cable: the SHT40 has to sit in the airflow path,", "t cap"))
r.append(txt(W3 / 2, 416, "away from MCU heat. Putting it on hb-01 would defeat the sensor.", "t cap"))
r.append(txt(W3 / 2, 444, "&#9888; The panel pigtail&#8217;s CC pins are inside the moulding &#8212; C-to-C charging needs hb-01&#8217;s receptacle", "t cap"))
r.append(txt(W3 / 2, 458, "or a different pigtail. It cannot be fixed with two resistors.", "t cap"))
r.append("</svg>")
open(f"{OUT}/interconnect.svg", "w").write("".join(r))
print("wrote", sorted(os.listdir(OUT)))
