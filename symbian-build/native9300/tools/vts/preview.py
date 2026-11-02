import sys, math
sys.path.insert(0, '/home/claude/vts')
import vtsmesh
from PIL import Image, ImageDraw
def enu(verts, lat0, lon0):
    a=6378137.0; e2=6.69437999014e-3
    la, lo = math.radians(lat0), math.radians(lon0)
    N=a/math.sqrt(1-e2*math.sin(la)**2)
    x0=N*math.cos(la)*math.cos(lo); y0=N*math.cos(la)*math.sin(lo); z0=N*(1-e2)*math.sin(la)
    out=[]
    for x,y,z in verts:
        dx,dy,dz=x-x0,y-y0,z-z0
        e=-math.sin(lo)*dx+math.cos(lo)*dy
        n=-math.sin(la)*math.cos(lo)*dx-math.sin(la)*math.sin(lo)*dy+math.cos(la)*dz
        u=math.cos(la)*math.cos(lo)*dx+math.cos(la)*math.sin(lo)*dy+math.sin(la)*dz
        out.append((e,n,u))
    return out
def render(path, out, lat0, lon0, tex=None, cam=(0,-250,120), yaw=0, pitch=-25, W=640, H=400):
    v, subs = vtsmesh.decode(open(path,'rb').read())
    img = Image.new('RGB',(W,H),(170,200,235)); dr = ImageDraw.Draw(img)
    tris=[]
    for s in subs:
        p = enu(s['verts'], lat0, lon0)
        g = min(q[2] for q in p)
        for f in s['faces']:
            a,b,c=[p[k] for k in f]
            tris.append((a,b,c))
    zs=[t[0][2] for t in tris]; base=min(min(t[k][2] for k in range(3)) for t in tris)
    cy,sy=math.cos(math.radians(yaw)),math.sin(math.radians(yaw)); cp,sp=math.cos(math.radians(pitch)),math.sin(math.radians(pitch))
    f=W*0.6; res=[]
    for t in tris:
        pts=[];dep=0;ok=True
        for (e,n,u) in t:
            x,y,z=e-cam[0],n-cam[1],(u-base)-cam[2]
            r=x*cy-y*sy; fw=x*sy+y*cy
            fw2=fw*cp+z*sp; up=-fw*sp+z*cp
            if fw2<1: ok=False;break
            pts.append((W/2+r*f/fw2, H/2-up*f/fw2)); dep+=fw2
        if not ok: continue
        (ax,ay,az),(bx,by,bz),(qx,qy,qz)=t
        nx=(by-ay)*(qz-az)-(bz-az)*(qy-ay); ny=(bz-az)*(qx-ax)-(bx-ax)*(qz-az); nz=(bx-ax)*(qy-ay)-(by-ay)*(qx-ax)
        l=math.sqrt(nx*nx+ny*ny+nz*nz) or 1
        sh=int(80+150*abs((0.4*nx+0.5*ny+0.75*nz)/l))
        res.append((dep,pts,(sh,sh,sh-10)))
    res.sort(key=lambda r:-r[0])
    for d,pts,c in res: dr.polygon(pts,fill=c)
    img.save(out); print(out, len(tris),'triangles, base',round(base,1))
if __name__=='__main__':
    render(sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4]))
