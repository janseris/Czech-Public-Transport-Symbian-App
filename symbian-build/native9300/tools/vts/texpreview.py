import sys, math
sys.path.insert(0,'/home/claude/vts')
import vtsmesh, preview
from PIL import Image, ImageDraw
import numpy as np
def render(binp, texp, out, lat0, lon0, cam, yaw, pitch, W=640, H=400, maxtris=None):
    v, subs = vtsmesh.decode(open(binp,'rb').read())
    tex = np.asarray(Image.open(texp).convert('RGB')); th, tw = tex.shape[:2]
    img = np.zeros((H,W,3),np.uint8); img[:]=(170,200,235)
    zb = np.full((H,W),1e9)
    s = subs[0]; p = preview.enu(s['verts'], lat0, lon0); base=min(q[2] for q in p)
    cy,sy=math.cos(math.radians(yaw)),math.sin(math.radians(yaw)); cp,sp=math.cos(math.radians(pitch)),math.sin(math.radians(pitch))
    f=W*0.6
    def proj(q):
        x,y,z=q[0]-cam[0],q[1]-cam[1],(q[2]-base)-cam[2]
        r=x*cy-y*sy; fw=x*sy+y*cy; fw2=fw*cp+z*sp; up=-fw*sp+z*cp
        return (W/2+r*f/fw2, H/2-up*f/fw2, fw2)
    P=[proj(q) for q in p]
    faces = s['faces'] if maxtris is None else s['faces'][:maxtris]
    for fi,fc in enumerate(faces):
        tri=[P[k] for k in fc]
        if min(t[2] for t in tri)<1: continue
        uv=[s['uvs'][k] for k in s['tfaces'][fi]]
        xs=[t[0] for t in tri]; ys=[t[1] for t in tri]
        x0,x1=max(0,int(min(xs))),min(W-1,int(max(xs))+1); y0,y1=max(0,int(min(ys))),min(H-1,int(max(ys))+1)
        if x0>x1 or y0>y1: continue
        (ax,ay,az),(bx,by,bz),(cx,cy2,cz)=tri
        den=(by-cy2)*(ax-cx)+(cx-bx)*(ay-cy2)
        if abs(den)<1e-9: continue
        X,Y=np.meshgrid(np.arange(x0,x1+1)+.5,np.arange(y0,y1+1)+.5)
        l1=((by-cy2)*(X-cx)+(cx-bx)*(Y-cy2))/den; l2=((cy2-ay)*(X-cx)+(ax-cx)*(Y-cy2))/den; l3=1-l1-l2
        m=(l1>=0)&(l2>=0)&(l3>=0)
        if not m.any(): continue
        iz=l1/az+l2/bz+l3/cz; z=1/iz
        u=(l1*uv[0][0]/az+l2*uv[1][0]/bz+l3*uv[2][0]/cz)*z; vv=(l1*uv[0][1]/az+l2*uv[1][1]/bz+l3*uv[2][1]/cz)*z
        sub=zb[y0:y1+1,x0:x1+1]; m&=z<sub
        tx=np.clip((u*tw).astype(int),0,tw-1); ty=np.clip(((vv)*th).astype(int),0,th-1)
        sub[m]=z[m]; img[y0:y1+1,x0:x1+1][m]=tex[ty[m],tx[m]]
    Image.fromarray(img).save(out); print(out,len(faces))
if __name__=='__main__':
    render(sys.argv[1],sys.argv[2],sys.argv[3],float(sys.argv[4]),float(sys.argv[5]),(float(sys.argv[6]),float(sys.argv[7]),float(sys.argv[8])),float(sys.argv[9]),float(sys.argv[10]))
