"""Drive the pet over USB and grab frames with SN."""
import time
import serial

W, H = 128, 64


def decode(hex_text):
    data = bytes.fromhex(hex_text.strip())
    assert len(data) == 1024, len(data)
    return [[(data[(y // 8) * 128 + x] >> (y % 8)) & 1 for x in range(W)]
            for y in range(H)]


class Pet:
    def __init__(self, port="COM8"):
        self.ser = serial.Serial(port, 115200, timeout=1)
        self.buf = b""

    def send(self, line):
        self.ser.write((line + "\n").encode())

    def _line(self, deadline):
        while time.time() < deadline:
            if b"\n" in self.buf:
                line, self.buf = self.buf.split(b"\n", 1)
                return line.decode("ascii", "replace").strip()
            self.buf += self.ser.read(self.ser.in_waiting or 1)
        return None

    def ask(self, line, want, timeout=2.0):
        self.send(line)
        deadline = time.time() + timeout
        while True:
            got = self._line(deadline)
            if got is None:
                raise TimeoutError(line)
            if got.startswith(want):
                return got

    def hello(self):
        return self.ask("HI", "PET")

    def snap(self):
        return decode(self.ask("SN", "SN ")[3:])

    def record(self, seconds, feed=None):
        """[(t, frame)] for `seconds`; feed(t) may send lines between frames."""
        out, t0 = [], time.time()
        while (t := time.time() - t0) < seconds:
            if feed:
                feed(t)
            out.append((t, self.snap()))
        return out
