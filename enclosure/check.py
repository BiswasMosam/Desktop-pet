"""Assemble the parts with stand-ins for the boards, report every overlap,
and write the assembly as assembly.glb for the renders.

    python check.py
"""
import numpy as np
import trimesh

import model as M
from model import box, plate_xz, rrect

# ---------------------------------------------------------------- stand-ins

y1, y0 = M.BP_Y1, M.BP_Y0
bp_bottom, bp_top, usb_z = M.BP_BOTTOM, M.BP_TOP, M.USB_Z


def pin_y(n):
    """Pin n of 20 along a row, counted from the USB end (WeAct drawing: 1.43 in, 2.54 pitch)."""
    return y1 - 1.43 - 2.54 * n


def black_pill():
    """Chip side up, headers pointing down: plastic under the board, solder stubs on top."""
    board = box(-M.BP_W / 2, M.BP_W / 2, y0, y1, bp_bottom, bp_top)
    usb = box(-4.5, 4.5, y1 + 0.8 - 7.35, y1 + 0.8, bp_top, bp_top + 3.26)
    tabs = [box(sx * 4.3 - 0.5, sx * 4.3 + 0.5, y1 - 6, y1 - 1, bp_bottom - 1.0, bp_bottom) for sx in (-1, 1)]
    m = board + usb + tabs[0] + tabs[1]
    for sx in (-1, 1):
        m += box(sx * 7.62 - 1.27, sx * 7.62 + 1.27, y0 + 0.13, y1 - 0.13, bp_bottom - 2.5, bp_bottom)
        m += box(sx * 7.62 - 0.9, sx * 7.62 + 0.9, y0 + 0.4, y1 - 0.4, bp_top, bp_top + 1.2)
    return m


def black_pill_duponts():
    """A jumper plug on every one of the 40 pins, hanging 14 mm under the header,
    so the stand clears them whichever pins are used."""
    m = None
    for sx in (-1, 1):
        for n in range(20):
            y = pin_y(n)
            h = box(sx * 7.62 - 1.27, sx * 7.62 + 1.27, y - 1.27, y + 1.27, bp_bottom - 16.5, bp_bottom - 2.5)
            m = h if m is None else m + h
    return m


def usb_plug():
    """A USB-C plug's overmould (12.35 x 6.5, the spec's maximum) fully home."""
    face = y1 + 0.8
    shell_ = box(-4.15, 4.15, face - 6.5, face, usb_z - 1.25, usb_z + 1.25)
    mould = box(-6.175, 6.175, face, face + 22, usb_z - 3.25, usb_z + 3.25)
    return shell_, mould


def oled():
    pcb = box(-M.OLED_W / 2, M.OLED_W / 2, M.WALL, M.WALL + 1.2,
              M.OLED_Z - M.OLED_H / 2, M.OLED_Z + M.OLED_H / 2)
    for sx in (-1, 1):
        for sz in (-1, 1):
            hole = trimesh_cyl = None
            from manifold3d import Manifold
            h = Manifold.cylinder(4, 1.05, 1.05, 32).rotate((-90, 0, 0)).translate(
                (sx * M.OLED_HOLE_X, M.WALL - 1, M.OLED_Z + sz * M.OLED_HOLE_Z))
            pcb -= h
    glass = box(-13.35, 13.35, M.WALL - 1.6, M.WALL, M.OLED_Z - 0.47 - 9.63, M.OLED_Z - 0.47 + 9.63)
    solder = box(-5.1, 5.1, M.WALL - 1.0, M.WALL, M.OLED_Z + 12.3 - 1.0, M.OLED_Z + 12.3 + 1.0)
    header = box(-5.08, 5.08, M.WALL + 1.2, M.WALL + 3.7, M.OLED_Z + 12.3 - 1.27, M.OLED_Z + 12.3 + 1.27)
    duponts = box(-5.08, 5.08, M.WALL + 3.7, M.WALL + 17.7, M.OLED_Z + 12.3 - 1.27, M.OLED_Z + 12.3 + 1.27)
    return pcb, glass, solder, header, duponts


def touch():
    top = M.H - M.TOUCH_ROOF
    board = box(-M.TOUCH_W / 2, M.TOUCH_W / 2, M.TOUCH_Y - M.TOUCH_L / 2, M.TOUCH_Y + M.TOUCH_L / 2,
                top - 1.6, top)
    chip = box(-3, 3, M.TOUCH_Y - 4, M.TOUCH_Y + 2, top - 2.8, top - 1.6)
    pins = box(-3.8, 3.8, M.TOUCH_Y + M.TOUCH_L / 2 - 2.5, M.TOUCH_Y + M.TOUCH_L / 2, top - 1.6 - 16.5, top - 1.6)
    return board + chip, pins


def loose():
    hc05 = box(-28.5, -13.0, 16, 53.3, M.FLOOR, M.FLOOR + 3.6)
    esp = box(13.0, 27.3, 20, 44.8, M.FLOOR, M.FLOOR + 3.0)
    ams = box(15.0, 26.0, 48, 60, M.FLOOR, M.FLOOR + 6.0)
    return hc05, esp, ams


# ---------------------------------------------------------------- checks

def vol(m):
    return m.volume() if not m.is_empty() else 0.0


if __name__ == "__main__":
    shell, visor, base = M.shell(), M.visor(), M.base()
    bp, dup = black_pill(), black_pill_duponts()
    plug_shell, plug_mould = usb_plug()
    pcb, glass, solder, header, oled_dup = oled()
    touch_board, touch_pins = touch()
    hc05, esp, ams = loose()

    solids = {"shell": shell, "visor": visor, "base": base}
    things = {"Black Pill": bp, "BP jumpers": dup, "USB plug mould": plug_mould,
              "OLED pcb": pcb, "OLED glass": glass, "OLED solder": solder,
              "OLED header": header, "OLED jumpers": oled_dup, "touch board": touch_board,
              "touch pins": touch_pins, "HC-05": hc05, "ESP-01": esp, "AMS1117": ams}

    print("overlaps (mm3), anything over ~0.01 is a clash:")
    bad = 0
    names = list(solids) + list(things)
    allm = {**solids, **things}
    for i, a in enumerate(names):
        for b in names[i + 1:]:
            if a in things and b in things:
                continue
            v = vol(allm[a] ^ allm[b])
            if v > 0.01:
                bad += 1
                print(f"  {a:14s} x {b:14s} {v:8.2f}")
    print("clashes:", bad)

    # how far the board can slide, and what the plug's mould clears
    print("USB port: board face at y=%.2f, wall inside y=%.2f, outside y=%.2f, centre z=%.2f" %
          (y1 + 0.8, M.D - M.WALL, M.D, usb_z))
    print("Black Pill bottom z=%.2f, plugs hang to z=%.2f, floor top z=%.2f: %.1f mm for the wires to bend" %
          (bp_bottom, bp_bottom - 16.5, M.FLOOR, bp_bottom - 16.5 - M.FLOOR))
    print("OLED lit area centre z=%.2f, window %.1f x %.1f" % (M.OLED_Z + M.AA_UP, M.WINDOW_W, M.WINDOW_H))

    # the assembly for renders: part name -> mesh
    def tm(m, color):
        mesh = m.to_mesh()
        t = trimesh.Trimesh(np.asarray(mesh.vert_properties)[:, :3], np.asarray(mesh.tri_verts))
        t.visual.face_colors = color
        return t

    scene = trimesh.Scene()
    colors = {"shell": [236, 236, 232, 255], "visor": [20, 22, 26, 255], "base": [200, 200, 196, 255],
              "Black Pill": [25, 25, 25, 255], "BP jumpers": [30, 30, 30, 255],
              "USB plug mould": [60, 60, 64, 255], "OLED pcb": [31, 79, 163, 255],
              "OLED glass": [8, 8, 10, 255], "OLED header": [20, 20, 20, 255],
              "OLED jumpers": [30, 30, 30, 255], "touch board": [200, 40, 40, 255],
              "touch pins": [30, 30, 30, 255], "HC-05": [30, 90, 200, 255], "ESP-01": [20, 20, 20, 255],
              "AMS1117": [30, 110, 60, 255]}
    for name, m in allm.items():
        if name == "OLED solder":
            continue
        scene.add_geometry(tm(m, colors[name]), node_name=name, geom_name=name)
    scene.export("assembly.glb")
    print("wrote assembly.glb")
