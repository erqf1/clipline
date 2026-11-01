"""Erzeugt das Clipline-Logo (assets/icon.ico, icon.png, icon.icns): verschwommene Farben hinter Milchglas (Acryl),
darauf das weiße Symbol. Gleicher Aufbau wie das Logo von Vidline, nur eigene Farben und eigenes Zeichen.
Braucht Pillow und PySide6. Aufruf aus dem Projektordner: python tools_make_icon.py"""
import struct
from io import BytesIO

from PIL import Image, ImageChops, ImageDraw, ImageFilter
from PySide6.QtCore import QByteArray
from PySide6.QtGui import QGuiApplication, QImage, QPainter
from PySide6.QtSvg import QSvgRenderer

S = 1024
# Farbflecken: Mitte x, y und Radius in Prozent der Kachel, dazu die Farbe
BLOBS = [(78, 20, 34, (255, 88, 92)), (18, 18, 32, (134, 82, 236)), (20, 82, 36, (196, 44, 124)), (84, 84, 34, (244, 138, 72)), (50, 50, 20, (96, 40, 70))]
GLYPH = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><path d="M37 27.48A26 26 0 1 0 66.71 30.08" fill="none" stroke="#fff" stroke-width="8.5" stroke-linecap="round"/><path d="M47.39 21.48L29.4 20.32L39.4 37.64z" fill="#fff"/><circle cx="50" cy="50" r="11" fill="#fff"/></svg>"""

app = QGuiApplication([])


def rounded(box, radius):
    m = Image.new('L', (S, S), 0)
    ImageDraw.Draw(m).rounded_rectangle(box, radius, fill=255)
    return m


def glyph_mask():
    r = QSvgRenderer(QByteArray(GLYPH.encode()))
    im = QImage(S, S, QImage.Format_ARGB32_Premultiplied)
    im.fill(0)
    p = QPainter(im)
    r.render(p)
    p.end()
    return Image.frombytes('RGBA', (S, S), bytes(im.constBits()), 'raw', 'BGRA').getchannel('A')


def tile():
    im = Image.new('RGB', (S, S), (40, 37, 44))
    d = ImageDraw.Draw(im)
    for x, y, r, col in BLOBS:
        c = S / 100
        d.ellipse(((x - r) * c, (y - r) * c, (x + r) * c, (y + r) * c), fill=col)
    im = im.filter(ImageFilter.GaussianBlur(170))
    # Tönung wie beim Acryl
    im = Image.blend(im, Image.new('RGB', (S, S), (30, 28, 34)), .26)
    out = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    mask = rounded((41, 41, 983, 983), 214)
    out.paste(im, (0, 0), mask)
    # Licht von oben und helle Glaskante
    sheen = Image.new('L', (S, S), 0)
    for y in range(41, 520):
        ImageDraw.Draw(sheen).line([(0, y), (S, y)], fill=int(46 * (1 - (y - 41) / 479) ** 2))
    out.paste(Image.new('RGBA', (S, S), (255, 255, 255, 255)), (0, 0), ImageChops.multiply(sheen, mask))
    rim = ImageChops.subtract(mask, rounded((51, 51, 973, 973), 204))
    fade = Image.new('L', (S, S), 0)
    for y in range(S):
        ImageDraw.Draw(fade).line([(0, y), (S, y)], fill=int(120 - 84 * y / S))
    out.paste(Image.new('RGBA', (S, S), (255, 255, 255, 255)), (0, 0), ImageChops.multiply(rim, fade))
    return out


def with_glyph(base):
    g = glyph_mask()
    out = base.copy()
    shadow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    shadow.paste(Image.new('RGBA', (S, S), (10, 8, 14, 150)), (0, 22), g)
    out.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(26)))
    out.paste(Image.new('RGBA', (S, S), (255, 255, 255, 245)), (0, 0), g)
    return out


BASE = tile()
M = with_glyph(BASE)


def png(size):
    b = BytesIO()
    M.resize((size, size), Image.LANCZOS).save(b, 'PNG')
    return b.getvalue()


sizes = [16, 24, 32, 48, 64, 128, 256]
data = [png(s) for s in sizes]
ico = struct.pack('<HHH', 0, 1, len(sizes))
off = 6 + 16 * len(sizes)
for s, d in zip(sizes, data):
    ico += struct.pack('<BBBBHHII', s % 256, s % 256, 0, 0, 1, 32, len(d), off)
    off += len(d)
open('assets/icon.ico', 'wb').write(ico + b''.join(data))
M.resize((256, 256), Image.LANCZOS).save('assets/icon.png')
chunks = b''
for tag, sz in ((b'ic07', 128), (b'ic08', 256), (b'ic09', 512), (b'ic10', 1024)):
    d = png(sz)
    chunks += tag + struct.pack('>I', len(d) + 8) + d
open('assets/icon.icns', 'wb').write(b'icns' + struct.pack('>I', len(chunks) + 8) + chunks)
# Kachel ohne Zeichen: das Programm malt Aufnahme- und Pause-Zustand selbst darauf (cliplineLogo in look.cpp)
BASE.resize((256, 256), Image.LANCZOS).save('assets/tile.png')
print('ok')
