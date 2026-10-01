"""Desktop-pet enclosure: a head for the face.

Everything is built from the numbers below, so a part that doesn't fit is a
one-line change. Units are millimetres. Axes, looking at the pet's face:
x to its left and right (0 in the middle), y from the front face (0) to the
back, z up from the desk (0).

    python model.py            writes stl/*.stl in print orientation

Parts (none needs supports):
    shell       the head, with the RFID reader's holder inside its right
                wall; printed upside down, top on the bed
    visor       the black screen bezel; printed face down
    base        the floor, with the Black Pill up on its stand; printed flat
"""
import math
import os

import numpy as np
from manifold3d import CrossSection, Manifold

SEG = 64            # segments in a full circle

# ---------------------------------------------------------------- the head
W, H = 66.0, 58.0               # outside width and height (the depth, D, follows
                                # from the RFID reader below)
R_SIDE = 10.0                   # radius of the four vertical edges
R_TOP = 6.0                     # radius where the top meets the sides
WALL = 2.4                      # side walls (6 lines of a 0.4 mm nozzle)
ROOF = 2.0                      # the top
FIT = 0.25                      # clearance between parts that slide together

# ---------------------------------------------------------------- RFID reader
# RFID-RC522: PCB 60 x 40, its 8-pin header right-angled off one short
# edge, so the jumper plugs carry on in line 16 mm past it. It stands
# inside the right wall (x > 0), long side front to back, antenna end at
# the front and plugs toward the back, so a card tapped on the outside of
# the head reads through about 6 mm of plastic. It's what sets the head's
# depth: the board, its plugs and room for the wires to turn have to fit
# between the round corners.
RFID_L, RFID_H, RFID_T = 60.0, 40.0, 1.6
RFID_GAP = 2.0          # back of the PCB to the inside of the wall: clears the
                        # header's solder stubs and lets the board's front end
                        # reach into the round corner
RFID_PLUGS = 16.0       # header plastic and jumper plugs past the board's end
RFID_BEND = 5.0         # then room for the wires to turn
RFID_FLOOR = 8.6        # lowest the holder reaches: the base's rim comes up to 8.0
RFID_LIFT = 2.0         # how far it slides up into its top slot to swing in
RFID_GRIP = 34.0        # the groove and the top lip run along the antenna end
                        # only, clear of the parts at the header end

_ri = R_SIDE - WALL                                   # inside corner radius
RFID_X_BACK = W / 2 - WALL - RFID_GAP                 # the PCB's back face
RFID_X_FRONT = RFID_X_BACK - RFID_T
RFID_Y0 = (WALL + _ri) - math.sqrt(_ri ** 2 - (RFID_X_BACK - (W / 2 - WALL - _ri)) ** 2) + 0.4
RFID_Y1 = RFID_Y0 + RFID_L + 0.5
D = math.ceil((RFID_Y1 + RFID_PLUGS + RFID_BEND + WALL) * 2) / 2   # outside depth
RFID_GROOVE = RFID_FLOOR + 0.4                        # the groove's bottom, at the wall
RFID_Z0 = RFID_GROOVE + RFID_T                        # the board's bottom, seated
RFID_Z1 = RFID_Z0 + RFID_H                            # and its top

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
# WeAct STM32F4x1: 52.81 x 20.78, chip, buttons and USB-C on top, the USB-C
# overhanging its end by ~0.8. The headers point DOWN, so every jumper plug
# hangs underneath: 2.5 of header plastic, 14 of plug, then the wire's bend.
# The 4-pin SWD header sits on that side too, in the middle of the far end.
BP_L, BP_W, BP_T = 52.81, 20.78, 1.6
BP_LIFT = 23.5                  # board bottom above the base floor (plugs + 7 mm to bend)
BP_ROW_OUT = 8.89               # outer face of the header strips, from the middle
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

# where the Black Pill ends up
BP_Y1 = D - WALL - USB_GAP              # its USB end
BP_Y0 = BP_Y1 - BP_L                    # its far end
BP_BOTTOM = FLOOR + BP_LIFT
BP_TOP = BP_BOTTOM + BP_T
USB_Z = BP_TOP + 1.65                   # middle of the USB-C receptacle


# ================================================================ helpers

def rrect(w, h, r):
    """A rounded rectangle centred on the origin, as a CrossSection."""
    r = max(0.01, min(r, w / 2 - 0.01, h / 2 - 0.01))
    c = CrossSection.circle(r, SEG)
    pts = [(sx * (w / 2 - r), sy * (h / 2 - r)) for sx in (-1, 1) for sy in (-1, 1)]
    return CrossSection.batch_hull([c.translate(p) for p in pts])


def box(x0, x1, y0, y1, z0, z1):
    x0, x1 = sorted((x0, x1))
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
            s += Manifold.batch_hull([
                box(sx * x_wall, sx * (x_wall - 1.6), yc - 4, yc + 4, rim_top + 0.1, rim_top + 0.6),
                box(sx * x_wall, sx * (x_wall - 0.01), yc - 4, yc + 4, rim_top + 0.1, rim_top + 2.4)])

    # The face opening, for the visor's boss
    face = rrect(BOSS_W + 2 * 0.2, BOSS_H + 2 * 0.2, BOSS_R + 0.2).translate((0, OLED_Z))
    s -= plate_xz(face, -1, WALL + 1)
    # the OLED header's solder joints poke out of the PCB's front above the glass
    s -= box(-6.8, 6.8, WALL - 1.2, WALL + 1, OLED_Z + 10.0, OLED_Z + OLED_H / 2 + 0.6)

    # USB-C port, centred on the board's connector, with a soft outer edge
    s -= plate_xz(rrect(USB_W, USB_H, 3.0).translate((0, USB_Z)), D - WALL - 1, D + 1)
    s -= Manifold.batch_hull([
        plate_xz(rrect(USB_W, USB_H, 3.0).translate((0, USB_Z)), D - 0.8, D - 0.79),
        plate_xz(rrect(USB_W + 1.6, USB_H + 1.6, 3.8).translate((0, USB_Z)), D, D + 0.01)])

    # Windows the base's snaps catch in, left and right
    for sx in (-1, 1):
        s -= box(sx * (W / 2 + 1), sx * (W / 2 - WALL - 1), SNAP_Y - SNAP_W / 2 - 0.5,
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

    s += rfid_holder(outer)
    # a faint ring on the outside, over the antenna: tap here
    ring_y, ring_z = RFID_Y0 + 19.0, (RFID_Z0 + RFID_Z1) / 2
    ring = Manifold.cylinder(2.0, 12.0, 12.0, SEG) - \
        Manifold.cylinder(2.0, 10.8, 10.8, SEG).translate((0, 0, -0.5))
    ring = ring.rotate((0, 90, 0)).translate((W / 2 - 0.6, ring_y, ring_z))
    s -= ring
    return s


def rfid_holder(outer):
    """The RC522 inside the right wall, held without screws.

    It hangs with its top edge in a slot and its bottom edge in a groove.
    To fit it, tilt it, push its top up into the slot, swing the bottom in
    over the groove's edge and let it drop. The groove's inner side slopes
    at 45 degrees and the slot's lip runs up into the roof, so both print
    without support with the head upside down. Everything is trimmed to the
    head's outside, which is what joins it to the round front corner.

    The groove and the lip only run along the antenna end of the board
    (RFID_GRIP): the crystal and the header sit at the other end, close to
    the edges, on the side facing in."""
    xb, xf = RFID_X_BACK, RFID_X_FRONT
    y0, y1 = RFID_Y0, RFID_Y1
    out = W / 2 + 1                       # past the outside, trimmed later
    grip = y0 + RFID_GRIP

    # the groove: solid up to the wall behind the board, and a 45 degree
    # slope in front of it that the board's bottom edge rests against
    h = box(xb, out, y0, grip, RFID_FLOOR, RFID_GROOVE)
    h += Manifold.batch_hull([
        box(xb - 3.0, xb, y0, grip, RFID_FLOOR, RFID_FLOOR + 0.01),
        box(xb - 3.0, xb - 2.99, y0, grip, RFID_FLOOR, RFID_GROOVE + 3.0),
        box(xb - 0.01, xb, y0, grip, RFID_FLOOR, RFID_GROOVE)])

    # the top: solid behind the board up into the roof, a lip in front of
    # it, and the slot between them deep enough to lift the board into
    slot_top = RFID_Z1 + RFID_LIFT + 0.4
    lip_in = xf - 0.35
    top = box(lip_in - 1.2, out, y0, grip, RFID_Z1 - 2.0, H)
    top -= box(lip_in, xb, y0 - 1, grip + 1, RFID_Z1 - 3.0, slot_top)
    h += top

    # behind the board: two ribs it leans on, between the groove and the top
    for yc in (y0 + 18.0, y0 + 40.0):
        h += box(xb, out, yc - 1.0, yc + 1.0, RFID_FLOOR, RFID_Z1)

    # in front of its antenna end, a rib it stops against; and behind its
    # other end, a short one low down, under where the header sits
    h += box(xf - 1.0, out, y0 - 2.0, y0 - 0.3, RFID_FLOOR, slot_top)
    h += box(xf - 1.0, out, y1 + 0.3, y1 + 2.3, RFID_FLOOR, RFID_Z0 + 5.0)
    return h ^ outer


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


def black_pill_stand():
    """The Black Pill held up high by its edges, so everything underneath hangs
    free: both rows of jumper plugs, the SWD header in the middle of the far
    end (and a plug on it, if one's ever wanted), and any parts on that side.

    Four springy posts stand outside the rows of plugs. Each has a ledge under
    the board's edge, in the 1.5 mm strip between the edge and the header
    strip, and a clip over its top, so the board is held edge-on at four
    points. The two front posts wrap round the board's corners: that's what
    takes the push of the USB cable going in (the back wall takes the pull
    coming out). Nothing reaches over the board's top but the clips."""
    y0, y1, bottom, top = BP_Y0, BP_Y1, BP_BOTTOM, BP_TOP
    edge = BP_W / 2
    ledge_in = BP_ROW_OUT + 0.3             # clear of the header strip's outer face
    s = None
    for sx in (-1, 1):
        for yc, front in ((y0 + 3.0, True), (y1 - 5.0, False)):
            post = box(sx * (edge + 0.2), sx * (edge + 2.4), yc - 3, yc + 3, FLOOR - 0.01, top + 1.3)
            # its foot runs front to back, so the floor beside it stays clear
            post += Manifold.batch_hull([
                box(sx * (edge + 0.2), sx * (edge + 2.4), yc - 5, yc + 5, FLOOR - 0.01, FLOOR + 0.5),
                box(sx * (edge + 0.2), sx * (edge + 2.4), yc - 3, yc + 3, FLOOR - 0.01, FLOOR + 5.0)])
            # the ledge under the board's edge; its underside slopes at 45 degrees
            # so it prints without support
            post += Manifold.batch_hull([
                box(sx * ledge_in, sx * (edge + 0.2), yc - 3, yc + 3, bottom - 1.0, bottom),
                box(sx * (edge + 0.19), sx * (edge + 0.2), yc - 3, yc + 3, bottom - 2.2, bottom)])
            # the clip: flat underneath over the board's edge, sloped on top so
            # the board pushes the posts apart on its way down
            post += Manifold.batch_hull([
                box(sx * (edge - 0.7), sx * (edge + 0.2), yc - 3, yc + 3, top + 0.1, top + 0.4),
                box(sx * (edge + 0.19), sx * (edge + 0.2), yc - 3, yc + 3, top + 0.1, top + 1.3)])
            if front:
                # round the corner: a stop against the board's end, outside the
                # header strip, so the SWD header between the two stops is clear
                post += box(sx * (BP_ROW_OUT - 1.9), sx * (edge + 2.4), y0 - 2.4, y0 - 0.2,
                            FLOOR - 0.01, top - 0.1)
                post += Manifold.batch_hull([
                    box(sx * (BP_ROW_OUT - 1.9), sx * (edge + 2.4), y0 - 5.0, y0 - 0.2, FLOOR - 0.01, FLOOR + 0.5),
                    box(sx * (BP_ROW_OUT - 1.9), sx * (edge + 2.4), y0 - 2.4, y0 - 0.2, FLOOR - 0.01, FLOOR + 4.0)])
            s = post if s is None else s + post
    return s


def base():
    b = plate_xy(base_outline(), 0, FLOOR)

    # the rim that rises inside the walls
    rim = plate_xy(base_outline(), FLOOR - 0.01, FLOOR + LIP_H) - \
        plate_xy(base_outline(LIP_T), FLOOR - 1, FLOOR + LIP_H + 1)
    # open under the Black Pill's USB end, where its first plugs hang down low
    rim -= box(-11.0, 11.0, D - WALL - 6, D, FLOOR - 0.5, FLOOR + LIP_H + 1)
    x_in = W / 2 - WALL - FIT
    for sx in (-1, 1):
        # cut the snap tab free on both sides so it can flex
        for dy in (-SNAP_W / 2 - 1.0, SNAP_W / 2):
            rim -= box(sx * (x_in + 1), sx * (x_in - LIP_T - 1), SNAP_Y + dy, SNAP_Y + dy + 1.0,
                       FLOOR + 0.8, FLOOR + LIP_H + 1)
        # the catch: flat on the bottom, sloped on top so the shell rides over it
        catch_bottom = SNAP_Z0 + 0.3
        rim += Manifold.batch_hull([
            box(sx * (x_in - 0.1), sx * (x_in + 0.7), SNAP_Y - SNAP_W / 2 + 0.5,
                SNAP_Y + SNAP_W / 2 - 0.5, catch_bottom, catch_bottom + 1.0),
            box(sx * (x_in - 0.1), sx * (x_in + 0.01), SNAP_Y - SNAP_W / 2 + 0.5,
                SNAP_Y + SNAP_W / 2 - 0.5, catch_bottom, FLOOR + LIP_H)])
    b += rim
    b += black_pill_stand()

    # vents under where the ESP-01 and its regulator go (right side)
    for i in range(7):
        y = 22 + i * 4.0
        b -= box(15, 25, y, y + 1.6, -1, FLOOR + 1)
    # a finger notch at the back edge to pull the base out
    b -= box(-7, 7, D - WALL - FIT - 2.5, D, -1, 1.2)
    return b


# ================================================================ export

def print_pose(name, m):
    """Turn a part from its place on the pet into the way it goes on the bed."""
    if name == "shell":                       # upside down: rotate about y, not mirror
        m = m.rotate((0, 180, 0))
    elif name == "visor":                     # face down
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
    return {"shell": shell(), "visor": visor(), "base": base()}


if __name__ == "__main__":
    os.makedirs("stl", exist_ok=True)
    for name, m in parts().items():
        tm = write_stl(print_pose(name, m), f"stl/{name}.stl")
        size = tm.bounds[1] - tm.bounds[0]
        print(f"{name:6s} {size[0]:6.1f} x {size[1]:6.1f} x {size[2]:6.1f} mm  "
              f"{tm.volume / 1000:6.1f} cm3  watertight={tm.is_watertight}  tris={len(tm.faces)}")
