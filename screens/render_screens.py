#!/usr/bin/env python3
# Render Marine Console Q5 V5 demo screens (240x240, round-masked) to PNGs.
import math, os
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.dirname(os.path.abspath(__file__))   # write PNGs next to this script

def rgb(h): return ((h >> 16) & 255, (h >> 8) & 255, h & 255)
# vibrant palette (matches the app)
BG=(0,0,0); FG=(255,255,255); LBL=rgb(0xB8C8D6); CYAN=rgb(0x00E0FF); GREEN=rgb(0x37FF5A)
AZ=rgb(0x33B5FF); RED=rgb(0xFF2E2E); BAR=rgb(0x0A57E6)
DISC=rgb(0x081521); DISC2=rgb(0x0E2236); LINE=rgb(0x3A4F73); MUTED=rgb(0xA8BAD0)

FB='/System/Library/Fonts/Supplemental/Arial Bold.ttf'
def fnt(px): return ImageFont.truetype(FB, px)
# font-size proxies for Garmin fonts
XT, SM, MD, LG, NUM = 16, 23, 26, 33, 46
def txt(d,x,y,px,s,c,j='ma'): d.text((x,y),s,font=fnt(px),fill=c,anchor=j)

def f3(v): return "---" if v is None else "%03d"%int(v)
def f1(v): return "--.-" if v is None else "%.1f"%v
def f2(v): return "--.--" if v is None else "%.2f"%v
def iS(v): return "--" if v is None else str(int(v))

def base(title, n):
    im=Image.new('RGB',(240,240),BG); d=ImageDraw.Draw(im)
    d.rectangle([0,0,240,26],fill=BAR); txt(d,120,9,XT,title,FG)
    return im,d
def footer(d,n): txt(d,120,208,XT,"%d/8  LINK OK"%n,GREEN)
def rmask(im):
    m=Image.new('L',(240,240),0); ImageDraw.Draw(m).ellipse([0,0,239,239],fill=255)
    b=Image.new('RGB',(240,240),(30,30,33)); b.paste(im,(0,0),m); return b

def block(d,yTop,yBot,label,value,unit,vc):
    rx=206
    txt(d,rx,yTop,XT,label,LBL,'ra'); txt(d,120,yTop+16,NUM,value,vc); txt(d,rx,yBot-16,XT,unit,LBL,'ra')

# ---- compass (OCEAN) ----
def rose_label(d,cx,cy,rr,bearing,hdg,s,c):
    a=(bearing-hdg-90)*math.pi/180; txt(d,cx+math.cos(a)*rr,cy+math.sin(a)*rr-8,XT,s,c)
def compass(d,cx,cy,r,hdg,cog):
    d.ellipse([cx-r,cy-r,cx+r,cy+r],fill=DISC); d.ellipse([cx-r,cy-r,cx+r,cy+r],outline=LINE,width=2)
    bb=[cx-(r+1),cy-(r+1),cx+(r+1),cy+(r+1)]
    d.arc(bb,200,270,fill=RED,width=5); d.arc(bb,270,340,fill=GREEN,width=5)  # port/stbd top
    for i in range(0,360,5):
        a=(i-hdg-90)*math.pi/180; cs=math.cos(a); sn=math.sin(a)
        ln,w,col=(14,3,FG) if i%30==0 else ((9,2,MUTED) if i%10==0 else (5,1,LINE))
        d.line([cx+cs*(r-3),cy+sn*(r-3),cx+cs*(r-3-ln),cy+sn*(r-3-ln)],fill=col,width=w)
    for b,s,c in [(0,"N",RED),(90,"E",FG),(180,"S",FG),(270,"W",FG)]: rose_label(d,cx,cy,r-28,b,hdg,s,c)
    if cog is not None:
        ca=(cog-hdg-90)*math.pi/180; d.line([cx,cy,cx+math.cos(ca)*r*0.82,cy+math.sin(ca)*r*0.82],fill=CYAN,width=2)
    ir=int(r*0.5); d.ellipse([cx-ir,cy-ir,cx+ir,cy+ir],fill=DISC2,outline=LINE)
    d.polygon([(cx,cy-r+7),(cx-7,cy-r-7),(cx+7,cy-r-7)],fill=RED)
    txt(d,cx,cy-26,NUM,f3(hdg),CYAN); txt(d,cx,cy+14,XT,"° TRUE",MUTED)
    if cog is not None: txt(d,cx,cy+30,XT,"COG "+f3(cog),CYAN)

# ---- wind dial ----
def wlabel(d,cx,cy,rr,deg,s,c):
    a=(deg-90)*math.pi/180; txt(d,cx+math.cos(a)*rr,cy+math.sin(a)*rr-8,XT,s,c)
def needle(d,cx,cy,r,ang,color,w):
    a=(ang-90)*math.pi/180; cs=math.cos(a); sn=math.sin(a)
    rx=cx+cs*(r-6); ry=cy+sn*(r-6); d.line([cx+cs*(r*0.5),cy+sn*(r*0.5),rx,ry],fill=color,width=w)
    bx=cx+cs*(r-16); by=cy+sn*(r-16); px=-sn; py=cs
    d.polygon([(rx,ry),(bx+px*6,by+py*6),(bx-px*6,by-py*6)],fill=color)
def wind(d,cx,cy,r,awa,aws,twa,tws,apparent):
    d.ellipse([cx-r,cy-r,cx+r,cy+r],fill=DISC); d.ellipse([cx-r,cy-r,cx+r,cy+r],outline=LINE,width=2)
    bb=[cx-(r+1),cy-(r+1),cx+(r+1),cy+(r+1)]; d.arc(bb,90,270,fill=RED,width=5); d.arc(bb,270,450,fill=GREEN,width=5)
    for i in range(0,360,15):
        a=(i-90)*math.pi/180; cs=math.cos(a); sn=math.sin(a)
        ln,w=(13,3) if i%90==0 else ((9,2) if i%30==0 else (5,1))
        d.line([cx+cs*(r-3),cy+sn*(r-3),cx+cs*(r-3-ln),cy+sn*(r-3-ln)],fill=MUTED,width=w)
    wlabel(d,cx,cy,r-22,0,"0",FG); wlabel(d,cx,cy,r-22,90,"90",GREEN); wlabel(d,cx,cy,r-22,180,"180",FG); wlabel(d,cx,cy,r-22,270,"90",RED)
    d.polygon([(cx,cy-r+6),(cx-6,cy-r-6),(cx+6,cy-r-6)],fill=FG)
    if apparent: needle(d,cx,cy,r,twa,GREEN,2); needle(d,cx,cy,r,awa,CYAN,4)
    else: needle(d,cx,cy,r,awa,CYAN,2); needle(d,cx,cy,r,twa,GREEN,4)
    ir=int(r*0.46); d.ellipse([cx-ir,cy-ir,cx+ir,cy+ir],fill=DISC2,outline=LINE)
    if apparent:
        txt(d,cx,cy-30,NUM,f1(aws),CYAN); txt(d,cx,cy+16,XT,"kt  ·  AWA %d°"%awa,CYAN)
    else:
        txt(d,cx,cy-30,NUM,f1(tws),GREEN); txt(d,cx,cy+16,XT,"kt  ·  TWA %d°"%twa,GREEN)

# ---- AIS ----
def tri(d,x,y,dirDeg,color):
    th=dirDeg*math.pi/180; dx=math.sin(th); dy=-math.cos(th); px=math.cos(th); py=math.sin(th); L=7.0;B=5.0
    d.polygon([(x+dx*L,y+dy*L),(x-dx*L*0.7+px*B,y-dy*L*0.7+py*B),(x-dx*L*0.7-px*B,y-dy*L*0.7-py*B)],fill=color)
def ais(d,cx,cy,R,hdg):
    d.ellipse([cx-R,cy-R,cx+R,cy+R],fill=DISC)
    for nm in (2,5,10):
        rr=R*nm/10.0; d.ellipse([cx-rr,cy-rr,cx+rr,cy+rr],outline=LINE,width=1)
        txt(d,cx-rr*0.707,cy-rr*0.707-7,XT,str(nm),MUTED)
    txt(d,cx,cy+R-14,XT,"NM",MUTED); d.line([cx,cy,cx,cy-R],fill=LINE,width=1)
    for brg,dist,crs in [[30,1.5,210],[75,3.0,290],[140,4.5,10],[200,7.0,90],[290,9.0,180],[330,6.0,45]]:
        a=(brg-hdg-90)*math.pi/180; rad=dist*R/10.0; tx=cx+math.cos(a)*rad; ty=cy+math.sin(a)*rad
        if dist<=5.0: tri(d,tx,ty,crs-hdg,CYAN)
        else: d.ellipse([tx-3,ty-3,tx+3,ty+3],fill=FG)
    tri(d,cx,cy,0,GREEN)

# ---- alarm overlay ----
def alarm(line1,line2,detail):
    im=Image.new('RGB',(240,240),RED); d=ImageDraw.Draw(im)
    txt(d,120,38,SM,"! ALARM !",FG); txt(d,120,68,LG,line1,FG); txt(d,120,108,LG,line2,FG)
    if detail: txt(d,120,152,SM,detail,FG)
    txt(d,120,196,XT,"START = OK",FG); return rmask(im)

# demo values
def page1():
    im,d=base("HEADING",1); compass(d,120,124,90,256,260); footer(d,1); return rmask(im)
def page2():
    im,d=base("APP WIND",2); wind(d,120,124,88,50,15.4,80,17.1,True); footer(d,2); return rmask(im)
def page3():
    im,d=base("TRUE WIND",3); wind(d,120,124,88,50,15.4,80,17.1,False); footer(d,3); return rmask(im)
def page4():
    im,d=base("SPEED",4); block(d,32,116,"SOG","7.0","knots",GREEN); d.line([34,120,206,120],fill=LBL); block(d,124,200,"DTW","2.8","nm to wpt",FG); footer(d,4); return rmask(im)
def page5():
    im,d=base("TRACK",5); block(d,32,116,"XTE","0.03","nm off",FG); d.line([34,120,206,120],fill=LBL); block(d,124,200,"BRG","264","° to wpt",AZ); footer(d,5); return rmask(im)
def page6():
    im,d=base("DEPTH",6); block(d,32,116,"DEPTH","19.0","m under keel",CYAN); d.line([34,120,206,120],fill=LBL); block(d,124,200,"WATER","21.2","°C",FG); footer(d,6); return rmask(im)
def page7():
    im,d=base("STATUS",7); block(d,32,116,"GUST","18.5","knots",RED); d.line([34,120,206,120],fill=LBL); block(d,124,200,"BATTERY","87","%",GREEN); footer(d,7); return rmask(im)
def page8():
    im,d=base("AIS",8); ais(d,120,124,90,256); footer(d,8); return rmask(im)

shots=[("1_heading",page1()),("2_app_wind",page2()),("3_true_wind",page3()),("4_speed",page4()),
       ("5_track",page5()),("6_depth",page6()),("7_status",page7()),("8_ais",page8()),
       ("9_alarm_shallow",alarm("SHALLOW","WATER","DEPTH 17.0 m"))]
labels={"1_heading":"HEADING 1/8","2_app_wind":"APP WIND 2/8","3_true_wind":"TRUE WIND 3/8",
        "4_speed":"SPEED 4/8","5_track":"TRACK 5/8","6_depth":"DEPTH 6/8","7_status":"STATUS 7/8",
        "8_ais":"AIS 8/8","9_alarm_shallow":"ALARM (overlay)"}
for name,im in shots:
    im.save(os.path.join(OUT,name+".png"))
# contact sheet (3 cols)
cols=3; gap=16; cell=240; lab=22
rows=(len(shots)+cols-1)//cols
W=gap+cols*(cell+gap); H=gap+rows*(cell+lab+gap)
cs=Image.new('RGB',(W,H),(22,22,25)); cd=ImageDraw.Draw(cs)
for i,(name,im) in enumerate(shots):
    r=i//cols; c=i%cols; x=gap+c*(cell+gap); y=gap+r*(cell+lab+gap)
    cd.text((x+cell//2,y),labels[name],font=fnt(15),fill=(235,235,235),anchor="ma")
    cs.paste(im,(x,y+lab))
cs.save(os.path.join(OUT,"_contact_sheet.png"))
print("OK ->", OUT)
print("files:", ", ".join(n+".png" for n,_ in shots), ", _contact_sheet.png")
