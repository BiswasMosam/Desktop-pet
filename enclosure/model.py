"""Desktop-pet enclosure: a head for the face.

Everything is built from the numbers below, so a part that doesn't fit is a
one-line change. Units are millimetres. Axes, looking at the pet's face:
x to its left and right (0 in the middle), y from the front face (0) to the
back, z up from the desk (0).

    python model.py            writes stl/*.stl in print orientation

Parts (none needs supports):
    shell       the head; printed upside down, top on the bed
    visor       the black screen bezel; printed face down
    base        the floor with the Black Pill's cradle; printed flat
    ears_round  a pair of round ears; printed flat
    ears_cat    a pair of pointed ears; printed flat
"""
import math
import os

import numpy as np
from manifold3d import CrossSection, Manifold

SEG = 64            # segments in a full circle

# ---------------------------------------------------------------- the head
W, D, H = 66.0, 70.0, 58.0      # outside width, depth, height
R_SIDE = 10.0                   # radius of the four vertical edges
R_TOP = 6.0                     # radius where the top meets the sides
WALL = 2.4                      # side walls (6 lines of a 0.4 mm nozzle)
ROOF = 2.0                      # the top
FIT = 0.25                      # clearance between parts that slide together

# ---------------------------------------------------------------- the OLED
# 0.96" SSD1306 I2C module: PCB 27.3 x 27.8, M2 holes 2 mm in from each edge
OLED_W, OLED_H = 27.3, 27.8
OLED_HOLE_X, OLED_HOLE_Z = 11.65, 11.9      # hole centres from the PCB centre
OLED_Z = 32.05                              # PCB centre height on the face
# The lit area (21.74 x 10.86) sits above the PCB centre, away from the ribbon
# at the bottom of the glass. If your screen shows cut off, change this.
AA_UP = 1.45
WINDOW_W, WINDOW_H = 25.0, 15.0             # opening in front of the lit area
GLASS_W, GLASS_H, GLASS_DEPTH = 27.4, 21.0, 1.9
PEG_D, PEG_LEN = 1.9, 2.6
BEZEL_W, BEZEL_H, BEZEL_T, BEZEL_R = 38.0, 32.0, 1.6, 6.0
BOSS_W, BOSS_H, BOSS_R = 31.6, 26.0, 4.0     # the part of the visor in the wall

# ---------------------------------------------------------------- Black Pill
# WeAct STM32F4x1: 52.81 x 20.78, USB-C on the top at one end, overhanging ~0.8
BP_L, BP_W, BP_T = 52.81, 20.78, 1.6
BP_RIB = 4.0                    # board bottom above the base floor (pin tails)
USB_W, USB_H = 14.0, 8.6        # port in the back wall, roomy for big plug overmoulds
USB_GAP = 1.2                   # board end to the inside of the back wall

# ---------------------------------------------------------------- the base
FLOOR = 2.4
LIP_T, LIP_H = 1.6, 5.6         # rim that rises inside the walls
SNAP_Y = D / 2                  # snaps on the left and right, halfway back
SNAP_W = 10.0
SNAP_Z0, SNAP_Z1 = 4.5, 7.5     # the windows in the shell they catch in

# ---------------------------------------------------------------- touch sensor
# TTP223 module, 15 x 11, pad side up against the thinned roof
TOUCH_L, TOUCH_W = 15.0, 11.0
TOUCH_Y = D / 2
TOUCH_ROOF = 1.0                # roof left over the pad

# ---------------------------------------------------------------- ears
EAR_X, EAR_Y = 18.0, 24.0       # where they sit on the roof
EAR_T = 5.0                     # thickness, front to back
PEG_X, PEG_Y, PEG_Z = 3.0, EAR_T, 5.0


# ================================================================ helpers

def rrect(w, h, r):
    """A rounded rectangle centred on the origin, as a CrossSection."""
    r = max(0.01, min(r, w / 2 - 0.01, h / 2 - 0.01))
    c = CrossSection.circle(r, SEG)
    pts = [(sx * (w / 2 - r), sy * (h / 2 - r)) for sx in (-1, 1) for sy in (-1, 1)]
    return CrossSection.batch_hull([c.translate(p) for p in pts])


def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube((x1 - x0, y1 - y0, z1 - z0)).translate((x0, y0, z0))


def plate_xz(section, y0, y1):
    """Extrude a CrossSection drawn in (x, z) along y, from y0 to y1.
    Rotating +90 about x takes (x, y, z) to (x, -z, y), so the drawing's
    second axis becomes height and the extrusion runs toward -y."""
    return section.extrude(y1 - y0).rotate((90, 0, 0)).translate((0, y1, 0))


def plate_xy(section, z0, z1):
    return section.extrude(z1 - z0).translate((0, 0, z0))


def head_solid(w, d, h, r_side, r_top, z0=0.0, steps=10, printable=True):
    """Rounded box: vertical edges r_side, top edges r_top, flat bottom at z0.
    Built as the hull of stacked rounded-rectangle slices.

    The shell prints upside down, so its top edge meets the bed, and the last
    stretch of a quarter round there would overhang almost flat. With
    `printable` the round stops at 45 degrees and a 45 degree chamfer carries
    on to the top: it still reads as a round, and nothing droops."""
    stop = math.pi / 4 if printable else math.pi / 2
    slices = [plate_xy(rrect(w, d, r_side), z0, h - r_top)]
    for i in range(1, steps + 1):
        a = stop * i / steps
        inset = r_top * (1 - math.cos(a))
        z = h - r_top + r_top * math.sin(a)
        s = rrect(w - 2 * inset, d - 2 * inset, max(0.5, r_side - inset))
        slices.append(plate_xy(s, z - 0.01, z))
    if printable:
        inset = r_top * (1 - math.cos(stop)) + (h - z)
        s = rrect(w - 2 * inset, d - 2 * inset, max(0.5, r_side - inset))
        slices.append(plate_xy(s, h - 0.01, h))
    return Manifold.batch_hull(slices).translate((0, d / 2, 0))


# ================================================================ parts

def shell():
    outer = head_solid(W, D, H, R_SIDE, R_TOP)
    inner = head_solid(W - 2 * WALL, D - 2 * WALL, H - ROOF, R_SIDE - WALL,
                       max(0.5, R_TOP - ROOF), z0=-1, printable=False).translate((0, WALL, 0))
    s = outer - inner

    # Stops the base's rim meets, so it can't be pushed in past its seat.
    # Their tops are chamfered: upside down on the bed, that's the side that
    # would otherwise overhang.
    rim_top = FLOOR + LIP_H
    x_wall = W / 2 - WALL
    for sx in (-1, 1):
        for yc in (16.0, D - 16.0):
            x0, x1 = sorted((sx * x_wall, sx * (x_wall - 1.6)))
            xw0, xw1 = sorted((sx * x_wall, sx * (x_wall - 0.01)))
            s += Manifold.batch_hull([box(x0, x1, yc - 4, yc + 4, rim_top + 0.1, rim_top + 0.6),
                                      box(xw0, xw1, yc - 4, yc + 4, rim_top + 0.1, rim_top + 2.4)])

    # The face opening, for the visor's boss
    face = rrect(BOSS_W + 2 * 0.2, BOSS_H + 2 * 0.2, BOSS_R + 0.2).translate((0, OLED_Z))
    s -= plate_xz(face, -1, WALL + 1)
    # the OLED header's solder joints poke out of the PCB's front above the glass
    s -= box(-6.8, 6.8, WALL - 1.2, WALL + 1, OLED_Z + 10.0, OLED_Z + OLED_H / 2 + 0.6)

    # USB-C port, centred on the board's connector, with a soft outer edge
    usb_z = FLOOR + BP_RIB + BP_T + 1.65
    port = rrect(USB_W, USB_H, 3.0).translate((0, usb_z))
    s -= plate_xz(port, D - WALL - 1, D + 1)
    flare = Manifold.batch_hull([
        plate_xz(rrect(USB_W, USB_H, 3.0).translate((0, usb_z)), D - 0.8, D - 0.79),
        plate_xz(rrect(USB_W + 1.6, USB_H + 1.6, 3.8).translate((0, usb_z)), D, D + 0.01)])
    s -= flare

    # Windows the base's snaps catch in, left and right
    for sx in (-1, 1):
        x0 = sx * (W / 2 + 1)
        x1 = sx * (W / 2 - WALL - 1)
        s -= box(min(x0, x1), max(x0, x1), SNAP_Y - SNAP_W / 2 - 0.5,
                 SNAP_Y + SNAP_W / 2 + 0.5, SNAP_Z0, SNAP_Z1)

    # Touch sensor pocket under the middle of the roof, the roof thinned over it
    top_in = H - ROOF
    pw, pl = TOUCH_W + 0.6, TOUCH_L + 0.6
    frame = box(-pw / 2 - 1.2, pw / 2 + 1.2, TOUCH_Y - pl / 2 - 1.2, TOUCH_Y + pl / 2 + 1.2,
                top_in - 3.0, top_in + 0.5)
    frame -= box(-pw / 2, pw / 2, TOUCH_Y - pl / 2, TOUCH_Y + pl / 2, top_in - 4, top_in + 0.6)
    frame -= box(-pw / 2 + 1.5, pw / 2 - 1.5, TOUCH_Y + pl / 2 - 0.5, TOUCH_Y + pl / 2 + 2,
                 top_in - 4, top_in + 0.6)          # opening for the header pins
    s += frame
    s -= box(-pw / 2, pw / 2, TOUCH_Y - pl / 2, TOUCH_Y + pl / 2, top_in - 0.1,
             H - TOUCH_ROOF)

    # Ear sockets: a slot through the roof and a sleeve below it
    for sx in (-1, 1):
        cx = sx * EAR_X
        sleeve = box(cx - PEG_X / 2 - 1.4, cx + PEG_X / 2 + 1.4, EAR_Y - PEG_Y / 2 - 1.4,
                     EAR_Y + PEG_Y / 2 + 1.4, top_in - 4.0, top_in + 0.5)
        s += sleeve
        s -= box(cx - PEG_X / 2 - 0.15, cx + PEG_X / 2 + 0.15, EAR_Y - PEG_Y / 2 - 0.15,
                 EAR_Y + PEG_Y / 2 + 0.15, top_in - 5, H + 1)
    return s


def visor():
    """Bezel on the outside, boss through the wall, OLED on four pegs behind."""
    bezel = plate_xz(rrect(BEZEL_W, BEZEL_H, BEZEL_R).translate((0, OLED_Z)), -BEZEL_T, 0)
    boss = plate_xz(rrect(BOSS_W, BOSS_H, BOSS_R).translate((0, OLED_Z)), -0.01, WALL)
    v = bezel + boss

    aa_z = OLED_Z + AA_UP
    v -= plate_xz(rrect(WINDOW_W, WINDOW_H, 1.5).translate((0, aa_z)), -BEZEL_T - 1, WALL + 1)
    v -= Manifold.batch_hull([                      # a small bevel round the window
        plate_xz(rrect(WINDOW_W, WINDOW_H, 1.5).translate((0, aa_z)), -BEZEL_T + 0.8, -BEZEL_T + 0.81),
        plate_xz(rrect(WINDOW_W + 1.6, WINDOW_H + 1.6, 2.3).translate((0, aa_z)), -BEZEL_T - 0.01, -BEZEL_T)])
    # the glass sits in a recess in the back of the boss
    v -= plate_xz(rrect(GLASS_W, GLASS_H, 0.8).translate((0, OLED_Z)), WALL - GLASS_DEPTH, WALL + 1)
    # room for the header's solder joints along the top of the PCB
    v -= box(-6.8, 6.8, WALL - 2.0, WALL + 1, OLED_Z + 10.7, OLED_Z + BOSS_H / 2 + 1)

    for sx in (-1, 1):
        for sz in (-1, 1):
            peg = Manifold.cylinder(PEG_LEN, PEG_D / 2, PEG_D / 2 - 0.3, SEG)
            peg = peg.rotate((-90, 0, 0)).translate((sx * OLED_HOLE_X, WALL, OLED_Z + sz * OLED_HOLE_Z))
            v += peg
    return v


def base_outline(shrink=0.0):
    w = W - 2 * WALL - 2 * FIT - 2 * shrink
    d = D - 2 * WALL - 2 * FIT - 2 * shrink
    return rrect(w, d, max(0.5, R_SIDE - WALL - FIT - shrink)).translate((0, D / 2))


def base():
    b = plate_xy(base_outline(), 0, FLOOR)

    # the rim that rises inside the walls
    rim = plate_xy(base_outline(), FLOOR - 0.01, FLOOR + LIP_H) - \
        plate_xy(base_outline(LIP_T), FLOOR - 1, FLOOR + LIP_H + 1)
    rim -= box(-12.5, 12.5, D - WALL - 6, D, FLOOR - 0.5, FLOOR + LIP_H + 1)   # USB end
    x_in = W / 2 - WALL - FIT
    for sx in (-1, 1):
        # cut the snap tab free on both sides so it can flex
        for dy in (-SNAP_W / 2 - 1.0, SNAP_W / 2):
            x0, x1 = sorted((sx * (x_in + 1), sx * (x_in - LIP_T - 1)))
            rim -= box(x0, x1, SNAP_Y + dy, SNAP_Y + dy + 1.0, FLOOR + 0.8, FLOOR + LIP_H + 1)
        # the catch: flat on the bottom, sloped on top so the shell rides over it
        catch_bottom = SNAP_Z0 + 0.3
        catch = Manifold.batch_hull([
            box(*sorted((sx * (x_in - 0.1), sx * (x_in + 0.7))), SNAP_Y - SNAP_W / 2 + 0.5,
                SNAP_Y + SNAP_W / 2 - 0.5, catch_bottom, catch_bottom + 1.0),
            box(*sorted((sx * (x_in - 0.1), sx * (x_in + 0.01))), SNAP_Y - SNAP_W / 2 + 0.5,
                SNAP_Y + SNAP_W / 2 - 0.5, catch_bottom, FLOOR + LIP_H)])
        rim += catch
    b += rim

    # The Black Pill's cradle, USB end at the back
    y1 = D - WALL - USB_GAP                 # board's USB end
    y0 = y1 - BP_L                          # board's far end
    top = FLOOR + BP_RIB + BP_T             # top of the board
    b += box(-4, 4, y0 + 2, y1 - 2, FLOOR - 0.01, FLOOR + BP_RIB)            # rib under it
    half = BP_W / 2 + 0.2
    for sx in (-1, 1):
        x0, x1 = sorted((sx * half, sx * (half + 1.6)))
        b += box(x0, x1, y0 - 0.2, y1 + 0.2, FLOOR - 0.01, top + 0.5)       # side rails
        # lips over the board's edge near the USB end, so it can't lift
        lx0, lx1 = sorted((sx * (half - 0.6), sx * (half + 1.6)))
        b += box(lx0, lx1, y1 - 9, y1 + 0.2, top + 0.1, top + 1.0)
    # the far end snaps under a lip on a springy end wall
    b += box(-5.5, 5.5, y0 - 1.8, y0 - 0.2, FLOOR - 0.01, top + 1.2)   # between the header rows
    lip = Manifold.batch_hull([
        box(-5.5, 5.5, y0 - 0.2, y0 + 0.6, top + 0.1, top + 0.5),
        box(-5.5, 5.5, y0 - 0.2, y0 - 0.19, top + 0.1, top + 1.2)])
    b += lip

    # vents under where the ESP-01 and its regulator go (right side)
    for i in range(7):
        y = 22 + i * 4.0
        b -= box(15, 25, y, y + 1.6, -1, FLOOR + 1)
    # a finger notch at the back edge to pull the base out
    b -= box(-7, 7, D - WALL - FIT - 2.5, D, -1, 1.2)
    return b


def ear(kind):
    """One ear, standing on z=0 with its peg below, profile in (x, z)."""
    if kind == "round":
        prof = CrossSection.circle(8.5, SEG).translate((0, 7.0))
        prof = prof + rrect(15.0, 7.0, 0.5).translate((0, 3.5))
        detail = CrossSection.circle(4.6, SEG).translate((0, 7.6))
    else:
        prof = CrossSection.batch_hull([CrossSection.circle(2.2, SEG).translate(p)
                                        for p in [(-6.3, 2.2), (6.3, 2.2), (-0.9, 13.2)]])
        prof = prof + rrect(17.0, 4.0, 0.5).translate((0, 2.0))
        detail = CrossSection.batch_hull([CrossSection.circle(1.2, SEG).translate(p)
                                          for p in [(-3.6, 3.4), (3.2, 3.4), (-1.0, 10.0)]])
    prof = prof - CrossSection.square((40, 40)).translate((-20, -40))   # flat on the roof
    e = plate_xz(prof, EAR_Y - EAR_T / 2, EAR_Y + EAR_T / 2)
    e -= plate_xz(detail, EAR_Y - EAR_T / 2 - 1, EAR_Y - EAR_T / 2 + 0.8)  # inner ear, front
    peg = box(-PEG_X / 2, PEG_X / 2, EAR_Y - PEG_Y / 2, EAR_Y + PEG_Y / 2, -PEG_Z, 0.01)
    return e + peg


# ================================================================ assembly and export

def ears(kind):
    """The pair, in place on the head."""
    left = ear(kind).translate((-EAR_X, 0, H))
    right = ear(kind).mirror((1, 0, 0)).translate((EAR_X, 0, H)) if kind == "cat" \
        else ear(kind).translate((EAR_X, 0, H))
    return left, right


def print_pose(name, m):
    """Turn a part from its place on the pet into the way it goes on the bed."""
    if name == "shell":                       # upside down: rotate about y, not mirror
        m = m.rotate((0, 180, 0))
    elif name == "visor":                     # face down
        m = m.rotate((-90, 0, 0))
    elif name.startswith("ear"):              # lying on its back, inner ear up
        m = m.rotate((-90, 0, 0))
    lo = np.array(m.bounding_box()[:3])
    hi = np.array(m.bounding_box()[3:])
    return m.translate((-(lo[0] + hi[0]) / 2, -(lo[1] + hi[1]) / 2, -lo[2]))


def write_stl(m, path):
    import trimesh
    mesh = m.to_mesh()
    tm = trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3],
                         faces=np.asarray(mesh.tri_verts), process=True)
    tm.export(path)
    return tm


def parts():
    er = ears("round")
    ec = ears("cat")
    return {
        "shell": shell(),
        "visor": visor(),
        "base": base(),
        "ears_round": er[0] + er[1],
        "ears_cat": ec[0] + ec[1],
    }


if __name__ == "__main__":
    os.makedirs("stl", exist_ok=True)
    for name, m in parts().items():
        if name.startswith("ears"):
            # print the pair side by side, both lying flat
            left, right = ears(name.split("_")[1])
            a = print_pose("ear", left)
            b = print_pose("ear", right)
            span = a.bounding_box()[3] - a.bounding_box()[0]
            m_print = a.translate((-(span / 2 + 3), 0, 0)) + b.translate((span / 2 + 3, 0, 0))
        else:
            m_print = print_pose(name, m)
        tm = write_stl(m_print, f"stl/{name}.stl")
        bb = tm.bounds
        size = bb[1] - bb[0]
        print(f"{name:11s} {size[0]:6.1f} x {size[1]:6.1f} x {size[2]:6.1f} mm  "
              f"{tm.volume / 1000:6.1f} cm3  watertight={tm.is_watertight}  tris={len(tm.faces)}")
