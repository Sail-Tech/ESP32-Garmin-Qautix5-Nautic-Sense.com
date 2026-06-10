#!/usr/bin/env python3
# Render "Marine Console — Beacon (universal)" mockups at several device sizes.
# Faithfully mirrors the app's responsive layout: every coordinate scales from
# the 240 px design baseline by S = width/240 (same as the View's P()), so the
# same screens are shown as they render on each device. Round devices get a
# circular mask; Edge devices a rounded rectangle.
import math, os
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.dirname(os.path.abspath(__file__))

def rgb(h): return ((h >> 16) & 255, (h >> 8) & 255, h & 255)
BG=(0,0,0); FG=(255,255,255); LBL=rgb(0xB8C8D6); CYAN=rgb(0x00E0FF); GREEN=rgb(0x37FF5A)
AZ=rgb(0x33B5FF); RED=rgb(0xFF2E2E); BAR=rgb(0x0A57E6)
DISC=rgb(0x081521); DISC2=rgb(0x0E2236); LINE=rgb(0x3A4F73); MUTED=rgb(0xA8BAD0)
FB='/System/Library/Fonts/Supplemental/Arial Bold.ttf'
# base font sizes at the 240 px baseline
XT, SM, MD, LG, NUM = 16, 23, 26, 33, 46

class R:
    def __init__(self, W, H, shape):
        self.W=W; self.H=H; self.shape=shape; self.S=W/240.0
        self.im=Image.new('RGB',(W,H),BG); self.d=ImageDraw.Draw(self.im)
    def P(self,v): return v*self.S
    def cx(self): return self.W/2
    def fnt(self,px): return ImageFont.truetype(FB, max(8,int(round(px*self.S))))
    def txt(self,x,y,px,s,c,j='ma'): self.d.text((x,y),s,font=self.fnt(px),fill=c,anchor=j)

    def mask(self):
        if self.shape=='round':
            m=Image.new('L',(self.W,self.H),0)
            ImageDraw.Draw(m).ellipse([0,0,self.W-1,self.H-1],fill=255)
        else:
            m=Image.new('L',(self.W,self.H),0)
            ImageDraw.Draw(m).rounded_rectangle([0,0,self.W-1,self.H-1],radius=int(self.P(26)),fill=255)
        b=Image.new('RGB',(self.W,self.H),(28,28,31)); b.paste(self.im,(0,0),m); return b

    # ---- chrome ----
    def header(self,title):
        self.d.rectangle([0,0,self.W,self.P(26)],fill=BAR)
        self.txt(self.cx(),self.P(6),XT,title,FG)
    def footer(self,n):
        self.txt(self.cx(),self.P(206),XT,"%d/8  LINK OK"%n,GREEN)

    def block(self,yTop,yBot,label,value,unit,vc):
        rx=self.P(206)
        self.txt(rx,self.P(yTop),XT,label,LBL,'ra')
        self.txt(self.cx(),self.P(yTop+18),NUM,value,vc)
        self.txt(rx,self.P(yBot-16),XT,unit,LBL,'ra')

    # ---- compass ----
    def rose_label(self,cx,cy,rr,bearing,hdg,s,c):
        a=(bearing-hdg-90)*math.pi/180; self.txt(cx+math.cos(a)*rr,cy+math.sin(a)*rr-self.P(8),XT,s,c)
    def compass(self,cx,cy,r,hdg,cog):
        d=self.d
        d.ellipse([cx-r,cy-r,cx+r,cy+r],fill=DISC); d.ellipse([cx-r,cy-r,cx+r,cy+r],outline=LINE,width=max(1,int(self.P(2))))
        bb=[cx-(r+1),cy-(r+1),cx+(r+1),cy+(r+1)]
        d.arc(bb,200,270,fill=RED,width=int(self.P(5))); d.arc(bb,270,340,fill=GREEN,width=int(self.P(5)))
        for i in range(0,360,5):
            a=(i-hdg-90)*math.pi/180; cs=math.cos(a); sn=math.sin(a)
            ln,w,col=(self.P(14),int(self.P(3)),FG) if i%30==0 else ((self.P(9),max(1,int(self.P(2))),MUTED) if i%10==0 else (self.P(5),1,LINE))
            d.line([cx+cs*(r-self.P(3)),cy+sn*(r-self.P(3)),cx+cs*(r-self.P(3)-ln),cy+sn*(r-self.P(3)-ln)],fill=col,width=w)
        for b,s,c in [(0,"N",RED),(90,"E",FG),(180,"S",FG),(270,"W",FG)]: self.rose_label(cx,cy,r-self.P(28),b,hdg,s,c)
        if cog is not None:
            ca=(cog-hdg-90)*math.pi/180; d.line([cx,cy,cx+math.cos(ca)*r*0.82,cy+math.sin(ca)*r*0.82],fill=CYAN,width=max(1,int(self.P(2))))
        ir=r*0.5; d.ellipse([cx-ir,cy-ir,cx+ir,cy+ir],fill=DISC2,outline=LINE)
        d.polygon([(cx,cy-r+self.P(7)),(cx-self.P(7),cy-r-self.P(7)),(cx+self.P(7),cy-r-self.P(7))],fill=RED)
        self.txt(cx,cy-self.P(30),NUM,"%03d"%hdg,CYAN); self.txt(cx,cy+self.P(16),XT,"° TRUE",MUTED)
        if cog is not None: self.txt(cx,cy+self.P(32),XT,"COG %03d"%cog,CYAN)

    # ---- wind ----
    def wlabel(self,cx,cy,rr,deg,s,c):
        a=(deg-90)*math.pi/180; self.txt(cx+math.cos(a)*rr,cy+math.sin(a)*rr-self.P(8),XT,s,c)
    def needle(self,cx,cy,r,ang,color,w):
        a=(ang-90)*math.pi/180; cs=math.cos(a); sn=math.sin(a)
        rx=cx+cs*(r-self.P(6)); ry=cy+sn*(r-self.P(6)); self.d.line([cx+cs*(r*0.5),cy+sn*(r*0.5),rx,ry],fill=color,width=w)
        bx=cx+cs*(r-self.P(16)); by=cy+sn*(r-self.P(16)); px=-sn; py=cs
        self.d.polygon([(rx,ry),(bx+px*self.P(6),by+py*self.P(6)),(bx-px*self.P(6),by-py*self.P(6))],fill=color)
    def wind(self,cx,cy,r,awa,aws,twa,tws,apparent):
        d=self.d
        d.ellipse([cx-r,cy-r,cx+r,cy+r],fill=DISC); d.ellipse([cx-r,cy-r,cx+r,cy+r],outline=LINE,width=max(1,int(self.P(2))))
        bb=[cx-(r+1),cy-(r+1),cx+(r+1),cy+(r+1)]; d.arc(bb,90,270,fill=RED,width=int(self.P(5))); d.arc(bb,270,450,fill=GREEN,width=int(self.P(5)))
        for i in range(0,360,15):
            a=(i-90)*math.pi/180; cs=math.cos(a); sn=math.sin(a)
            ln,w=(self.P(13),int(self.P(3))) if i%90==0 else ((self.P(9),max(1,int(self.P(2)))) if i%30==0 else (self.P(5),1))
            d.line([cx+cs*(r-self.P(3)),cy+sn*(r-self.P(3)),cx+cs*(r-self.P(3)-ln),cy+sn*(r-self.P(3)-ln)],fill=MUTED,width=w)
        self.wlabel(cx,cy,r-self.P(22),0,"0",FG); self.wlabel(cx,cy,r-self.P(22),90,"90",GREEN)
        self.wlabel(cx,cy,r-self.P(22),180,"180",FG); self.wlabel(cx,cy,r-self.P(22),270,"90",RED)
        d.polygon([(cx,cy-r+self.P(6)),(cx-self.P(6),cy-r-self.P(6)),(cx+self.P(6),cy-r-self.P(6))],fill=FG)
        if apparent: self.needle(cx,cy,r,twa,GREEN,max(1,int(self.P(2)))); self.needle(cx,cy,r,awa,CYAN,int(self.P(4)))
        else: self.needle(cx,cy,r,awa,CYAN,max(1,int(self.P(2)))); self.needle(cx,cy,r,twa,GREEN,int(self.P(4)))
        ir=r*0.46; d.ellipse([cx-ir,cy-ir,cx+ir,cy+ir],fill=DISC2,outline=LINE)
        if apparent: self.txt(cx,cy-self.P(30),NUM,"%.1f"%aws,CYAN); self.txt(cx,cy+self.P(16),XT,"kt  ·  AWA %d°"%awa,CYAN)
        else: self.txt(cx,cy-self.P(30),NUM,"%.1f"%tws,GREEN); self.txt(cx,cy+self.P(16),XT,"kt  ·  TWA %d°"%twa,GREEN)

    # ---- AIS ----
    def tri(self,x,y,dirDeg,color):
        th=dirDeg*math.pi/180; dx=math.sin(th); dy=-math.cos(th); px=math.cos(th); py=math.sin(th); L=self.P(7); B=self.P(5)
        self.d.polygon([(x+dx*L,y+dy*L),(x-dx*L*0.7+px*B,y-dy*L*0.7+py*B),(x-dx*L*0.7-px*B,y-dy*L*0.7-py*B)],fill=color)
    def ais(self,cx,cy,Rr,hdg,count):
        d=self.d; d.ellipse([cx-Rr,cy-Rr,cx+Rr,cy+Rr],fill=DISC)
        for nm in (2,5,10):
            rr=Rr*nm/10.0; d.ellipse([cx-rr,cy-rr,cx+rr,cy+rr],outline=LINE,width=1)
            self.txt(cx-rr*0.707,cy-rr*0.707-self.P(7),XT,str(nm),MUTED)
        self.txt(cx,cy+Rr-self.P(14),XT,"NM",MUTED); d.line([cx,cy,cx,cy-Rr],fill=LINE,width=1)
        for brg,dist,crs in [[30,1.5,210],[75,3.0,290],[140,4.5,10],[200,7.0,90],[290,9.0,180],[330,6.0,45]]:
            a=(brg-hdg-90)*math.pi/180; rad=dist*Rr/10.0; tx=cx+math.cos(a)*rad; ty=cy+math.sin(a)*rad
            if dist<=5.0: self.tri(tx,ty,crs-hdg,CYAN)
            else: d.ellipse([tx-self.P(3),ty-self.P(3),tx+self.P(3),ty+self.P(3)],fill=FG)
        self.tri(cx,cy,0,GREEN)
        self.txt(cx,cy+self.P(40),XT,"%d TGT"%count,CYAN)

    def alarm(self,l1,l2,detail):
        self.d.rectangle([0,0,self.W,self.H],fill=RED)
        self.txt(self.cx(),self.P(60),MD,"! ALARM !",FG)
        self.txt(self.cx(),self.P(96),LG,l1,FG); self.txt(self.cx(),self.P(132),LG,l2,FG)
        if detail: self.txt(self.cx(),self.P(170),SM,detail,FG)
        self.txt(self.cx(),self.P(202),XT,"tap = OK",FG)

# ---- page builders ----
def page(W,H,shape,which):
    g=R(W,H,shape); cyc=g.P(124); cx=g.cx()
    if which==1: g.header("HEADING"); g.compass(cx,cyc,g.P(90),256,260); g.footer(1)
    elif which==2: g.header("APP WIND"); g.wind(cx,cyc,g.P(88),50,15.4,80,17.1,True); g.footer(2)
    elif which==3: g.header("TRUE WIND"); g.wind(cx,cyc,g.P(88),50,15.4,80,17.1,False); g.footer(3)
    elif which==4:
        g.header("SPEED"); g.block(32,116,"SOG","7.0","knots",GREEN)
        g.d.line([g.P(34),g.P(120),g.P(206),g.P(120)],fill=LBL); g.block(124,200,"DTW","2.8","nm to wpt",FG); g.footer(4)
    elif which==5:
        g.header("TRACK"); g.block(32,116,"XTE","0.03","nm off",FG)
        g.d.line([g.P(34),g.P(120),g.P(206),g.P(120)],fill=LBL); g.block(124,200,"BRG","264","° to wpt",AZ); g.footer(5)
    elif which==6:
        g.header("DEPTH"); g.block(32,116,"DEPTH","19.0","m under keel",CYAN)
        g.d.line([g.P(34),g.P(120),g.P(206),g.P(120)],fill=LBL); g.block(124,200,"WATER","21.2","°C",FG); g.footer(6)
    elif which==7:
        g.header("STATUS"); g.block(32,116,"GUST","18.5","knots",RED)
        g.d.line([g.P(34),g.P(120),g.P(206),g.P(120)],fill=LBL); g.block(124,200,"BATTERY","87","%",GREEN); g.footer(7)
    elif which==8: g.header("AIS"); g.ais(cx,cyc,g.P(90),256,6); g.footer(8)
    elif which==9: g.alarm("AIS","TARGET","1.2 nm  075°")
    return g.mask()

# flagship full set (round 454 — fēnix 8 / Venu 3 / epix)
NAMES=["1_heading","2_app_wind","3_true_wind","4_speed","5_track","6_depth","7_status","8_ais","9_alarm_ais"]
shots=[(NAMES[i], page(454,454,'round',i+1)) for i in range(9)]
for name,im in shots: im.save(os.path.join(OUT,name+".png"))

# contact sheet of the 9 flagship screens (3 cols)
def sheet(items, cols, label_map, fname, cellw=None):
    cells=[im for _,im in items]
    cw=cells[0].width; ch=cells[0].height; gap=18; lab=24
    rows=(len(cells)+cols-1)//cols
    W=gap+cols*(cw+gap); Hh=gap+rows*(ch+lab+gap)
    cs=Image.new('RGB',(W,Hh),(22,22,25)); cd=ImageDraw.Draw(cs)
    f=ImageFont.truetype(FB,16)
    for i,(name,im) in enumerate(items):
        r=i//cols; c=i%cols; x=gap+c*(cw+gap); y=gap+r*(ch+lab+gap)
        cd.text((x+cw//2,y),label_map[name],font=f,fill=(235,235,235),anchor="ma")
        cs.paste(im,(x,y+lab))
    cs.save(os.path.join(OUT,fname))

labels={"1_heading":"HEADING","2_app_wind":"APP WIND","3_true_wind":"TRUE WIND","4_speed":"SPEED",
        "5_track":"TRACK","6_depth":"DEPTH","7_status":"STATUS","8_ais":"AIS","9_alarm_ais":"ALARM (AIS)"}
sheet(shots,3,labels,"_contact_sheet.png")

# device sheet — the SAME HEADING page across sizes/shapes (the "universal" story)
DEVICES=[("454 round\nfenix 8 · Venu 3 · epix",454,454,'round'),
         ("416 round\nepix 2 · FR 265",416,416,'round'),
         ("390 round\nVenu 3S · vivoactive 5",390,390,'round'),
         ("260 round\nfenix 7 · FR 255",260,260,'round'),
         ("246x322\nEdge 540",246,322,'rect')]
# normalise heights for a tidy row
maxh=max(H for _,W,H,s in DEVICES)
dev=[]
for lbl,W,H,s in DEVICES:
    im=page(W,H,s,1)
    canvas=Image.new('RGB',(im.width,maxh),(22,22,25)); canvas.paste(im,(0,(maxh-im.height)//2))
    dev.append((lbl,canvas))
gap=20; lab=40
W=gap+sum(im.width+gap for _,im in dev); Hh=gap+maxh+lab
cs=Image.new('RGB',(W,Hh),(22,22,25)); cd=ImageDraw.Draw(cs)
f=ImageFont.truetype(FB,15)
x=gap
for lbl,im in dev:
    cd.multiline_text((x+im.width//2,gap),lbl,font=f,fill=(235,235,235),anchor="ma",align="center",spacing=4)
    cs.paste(im,(x,gap+lab)); x+=im.width+gap
cs.save(os.path.join(OUT,"_devices.png"))

print("OK ->",OUT)
print("files:", ", ".join(n+".png" for n,_ in shots), ", _contact_sheet.png, _devices.png")
