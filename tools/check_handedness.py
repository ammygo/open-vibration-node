# -*- coding: utf-8 -*-
# HANDEDNESS CHECK for bottom-mounted (B.Cu) IC footprints.
# Physics: chip lying lands-up on a table = datasheet BOTTOM VIEW (x east, y north).
# Lifting it straight up onto the board's bottom face changes nothing.
# => board pad WORLD positions (x_kicad, -y_kicad) must equal the datasheet
#    bottom-view land positions up to ONE common in-plane rotation (0/90/180/270).
# A mirrored footprint matches at NO rotation -> FAIL.
import re, math, sys, os

# IIS3DWB datasheet DS12569 BOTTOM VIEW land centers (mm, x east / y north),
# read from the package mechanical drawing:
#   pin1 top-right; 1-4 down the right edge; 5-7 bottom edge right->left;
#   8-11 up the left edge; 12-14 top edge left->right.
DS_BOTTOM = {
  1:( 1.2625,  0.75), 2:( 1.2625,  0.25), 3:( 1.2625, -0.25), 4:( 1.2625, -0.75),
  5:( 0.5,   -1.0125), 6:( 0.0,  -1.0125), 7:(-0.5,  -1.0125),
  8:(-1.2625, -0.75), 9:(-1.2625, -0.25), 10:(-1.2625, 0.25), 11:(-1.2625, 0.75),
  12:(-0.5,   1.0125), 13:( 0.0,  1.0125), 14:( 0.5,   1.0125),
}

def tok(s): return re.findall(r'\(|\)|"[^"]*"|[^\s()]+', s)
def parse(ts):
    it=iter(ts)
    def rd():
        n=[]
        for t in it:
            if t=='(':n.append(rd())
            elif t==')':return n
            else:
                if t[:1]=='"'==t[-1:]:t=t[1:-1]
                n.append(t)
        return n
    o=[]
    for t in it:
        if t=='(':o.append(rd())
    return o
def fa(n,name):
    r=[]
    if isinstance(n,list) and n and n[0]==name:r.append(n)
    for c in n:
        if isinstance(c,list):r.extend(fa(c,name))
    return r
def gg(n,name):
    for c in n:
        if isinstance(c,list) and c and c[0]==name:return c
def rot(px,py,a):
    a=math.radians(a); return (px*math.cos(a)-py*math.sin(a), px*math.sin(a)+py*math.cos(a))

def check(path):
    top=parse(tok(open(path,encoding='utf-8').read()))[0]
    for fp in fa(top,'footprint'):
        name=fp[1]
        if 'LGA-14' not in str(name): continue
        lay=gg(fp,'layer')[1]
        at=gg(fp,'at'); fx,fy=float(at[1]),float(at[2]); fang=float(at[3]) if len(at)>3 else 0.0
        pads={}
        for pad in fp:
            if isinstance(pad,list) and pad and pad[0]=='pad':
                pat=gg(pad,'at'); lx,ly=float(pat[1]),float(pat[2])
                rx,ry=rot(lx,ly,fang)
                # WORLD frame: x = kicad x, y = -kicad y (kicad y grows south)
                pads[int(pad[1])]=(fx+rx-150.0, -(fy+ry-100.0))
        assert lay=='B.Cu', "this check is written for the bottom-mounted sensor"
        print("="*64)
        print("HANDEDNESS CHECK:", os.path.basename(os.path.dirname(os.path.abspath(path))))
        best=None
        for ang in (0,90,180,270):
            errs=[]
            for pin,(dx,dy) in DS_BOTTOM.items():
                ex,ey=rot(dx,dy,ang)
                px,py=pads[pin]
                errs.append(math.hypot(px-ex,py-ey))
            m=max(errs)
            print(f"   rotation {ang:>3} deg: max pad error = {m:.4f} mm")
            if best is None or m<best[1]: best=(ang,m)
        ang,m=best
        if m<0.01:
            print(f"   >>> PASS: pads = datasheet BOTTOM VIEW rotated {ang} deg")
            print(f"   >>> chip is PLACEABLE; CPL rotation must be {ang} deg  (PnP file says 180)")
        else:
            print(f"   >>> FAIL: no rotation matches (best {ang} deg, err {m:.3f} mm)")
            print(f"   >>> FOOTPRINT IS MIRRORED - chip CANNOT be assembled correctly")
        return m<0.01

if __name__=="__main__":
    for p in sys.argv[1:]:
        check(p)
