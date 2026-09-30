"""Erzeugt assets/icon.ico, icon.icns und icon.png – gleiches Design wie cliplineLogo() in src/look.cpp."""
import math
import struct
import sys

from PySide6.QtCore import QBuffer, QByteArray, QIODevice, QPointF, QRectF, Qt
from PySide6.QtGui import QColor, QGuiApplication, QImage, QLinearGradient, QPainter, QPainterPath, QPen

app = QGuiApplication(sys.argv)


def draw(size):
    im = QImage(size, size, QImage.Format_ARGB32)
    im.fill(Qt.transparent)
    p = QPainter(im)
    p.setRenderHint(QPainter.Antialiasing)
    p.scale(size / 100.0, size / 100.0)
    g = QLinearGradient(0, 0, 100, 100)
    g.setColorAt(0, QColor("#ff5f6d"))
    g.setColorAt(1, QColor("#8b5cf6"))
    p.setPen(Qt.NoPen)
    p.setBrush(g)
    p.drawRoundedRect(QRectF(4, 4, 92, 92), 24, 24)
    gl = QLinearGradient(0, 4, 0, 50)
    gl.setColorAt(0, QColor(255, 255, 255, 60))
    gl.setColorAt(1, QColor(255, 255, 255, 0))
    p.setBrush(gl)
    p.drawRoundedRect(QRectF(4, 4, 92, 46), 24, 24)
    p.setPen(QPen(Qt.white, 8.5, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin))
    p.setBrush(Qt.NoBrush)
    p.drawArc(QRectF(24, 24, 52, 52), 120 * 16, 290 * 16)
    a = math.radians(120)
    px, py = 50 + 26 * math.cos(a), 50 - 26 * math.sin(a)
    dx, dy = math.sin(a), math.cos(a)
    nx, ny = math.cos(a), -math.sin(a)
    p.setPen(Qt.NoPen)
    p.setBrush(Qt.white)
    h = QPainterPath()
    h.moveTo(px + dx * 12, py + dy * 12)
    h.lineTo(px + nx * 10 - dx * 3, py + ny * 10 - dy * 3)
    h.lineTo(px - nx * 10 - dx * 3, py - ny * 10 - dy * 3)
    h.closeSubpath()
    p.drawPath(h)
    p.setBrush(QColor(255, 255, 255, 235))
    p.drawEllipse(QPointF(50, 50), 11, 11)
    p.end()
    return im


def png(im):
    ba = QByteArray()
    buf = QBuffer(ba)
    buf.open(QIODevice.WriteOnly)
    im.save(buf, "PNG")
    return bytes(ba)


sizes = [16, 24, 32, 48, 64, 128, 256]
data = [png(draw(s)) for s in sizes]
ico = struct.pack("<HHH", 0, 1, len(sizes))
off = 6 + 16 * len(sizes)
for s, d in zip(sizes, data):
    ico += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(d), off)
    off += len(d)
open("assets/icon.ico", "wb").write(ico + b"".join(data))
draw(256).save("assets/icon.png")
draw(512).save("docs/logo.png")
chunks = b""
for tag, sz in ((b"ic07", 128), (b"ic08", 256), (b"ic09", 512), (b"ic10", 1024)):
    d = png(draw(sz))
    chunks += tag + struct.pack(">I", len(d) + 8) + d
open("assets/icon.icns", "wb").write(b"icns" + struct.pack(">I", len(chunks) + 8) + chunks)
print("ok")
