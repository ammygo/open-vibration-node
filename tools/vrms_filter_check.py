#!/usr/bin/env python3
"""Validation of the node's ISO 10816 velocity-RMS chain on synthetic signals.

Reproduces the table in docs/measurements.md ("Velocity RMS in the ISO 10816 band").
The chain mirrors the firmware exactly: mean removal -> 2nd-order Butterworth high-pass
8 Hz -> trapezoidal integration -> high-pass 8 Hz -> low-pass 1000 Hz -> RMS over the
window minus a settling period. Two cascaded 8 Hz sections give -3 dB at 10 Hz.
Pure Python, no dependencies, no hardware. Run: python3 vrms_filter_check.py
"""
import math

FS = 26667.0          # sensor output data rate, Hz
N = 8192              # samples per window (307 ms)
WARM = 2730           # settling samples excluded from the RMS
HP_FC, LP_FC = 8.0, 1000.0


class Biquad:
    def __init__(self, kind, fc, fs=FS):
        w = 2 * math.pi * fc / fs
        c, s = math.cos(w), math.sin(w)
        al = s / (2 * 0.70710678)      # Butterworth Q
        a0 = 1 + al
        if kind == "lp":
            self.b0 = (1 - c) / 2 / a0
            self.b1 = (1 - c) / a0
        else:
            self.b0 = (1 + c) / 2 / a0
            self.b1 = -(1 + c) / a0
        self.b2 = self.b0
        self.a1 = -2 * c / a0
        self.a2 = (1 - al) / a0
        self.z1 = self.z2 = 0.0

    def run(self, x):
        y = self.b0 * x + self.z1
        self.z1 = self.b1 * x - self.a1 * y + self.z2
        self.z2 = self.b2 * x - self.a2 * y
        return y


def vrms_iso(acc_mg):
    """Velocity RMS (mm/s) in the 10-1000 Hz band from an acceleration window in mg."""
    hp_a, hp_v, lp = Biquad("hp", HP_FC), Biquad("hp", HP_FC), Biquad("lp", LP_FC)
    mean = sum(acc_mg) / len(acc_mg)
    v = prev = q = 0.0
    n = 0
    for i, a in enumerate(acc_mg):
        x = hp_a.run((a - mean) * 0.00980665)      # m/s^2
        v += 0.5 * (x + prev) / FS                 # trapezoidal integration -> m/s
        prev = x
        y = lp.run(hp_v.run(v))
        if i >= WARM:
            q += y * y
            n += 1
    return math.sqrt(q / n) * 1000.0


def sine(f, amp_mg, offset=0.0):
    return [offset + amp_mg * math.sin(2 * math.pi * f * i / FS) for i in range(N)]


def expected(f, amp_mg):
    return amp_mg * 0.00980665 / (2 * math.pi * f) / math.sqrt(2) * 1000.0


if __name__ == "__main__":
    cases = [("10 Hz (band edge, -3 dB)", 10, 100, 0.707), ("24.7 Hz (1480 rpm)", 24.7, 100, 1),
             ("50 Hz", 50, 100, 1), ("100 Hz", 100, 100, 1), ("500 Hz", 500, 100, 1),
             ("1000 Hz (band edge, -3 dB)", 1000, 100, 0.707)]
    print("%-28s %12s %12s %8s" % ("signal (100 mg)", "expected", "chain", "error"))
    for name, f, amp, scale in cases:
        got, exp = vrms_iso(sine(f, amp)), expected(f, amp) * scale
        print("%-28s %7.3f mm/s %7.3f mm/s %+6.1f %%" % (name, exp, got, (got / exp - 1) * 100))
    for name, f in (("2 Hz, out of band", 2), ("3000 Hz, out of band", 3000)):
        print("%-28s %12s %7.3f mm/s   (unfiltered would be %.2f mm/s)" %
              (name, "-", vrms_iso(sine(f, 100)), expected(f, 100)))
    got, exp = vrms_iso(sine(24.7, 50, offset=1000)), expected(24.7, 50)
    print("%-28s %7.3f mm/s %7.3f mm/s %+6.1f %%" % ("1000 mg DC + 24.7 Hz 50 mg", exp, got, (got / exp - 1) * 100))
