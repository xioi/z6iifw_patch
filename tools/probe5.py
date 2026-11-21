import sys, struct, time, subprocess, usb.core, usb.util
VID=0x04B0; EP_OUT=0x02; EP_IN=0x81
def killc(): subprocess.run("kill -9 $(pgrep -f ptpcamerad) 2>/dev/null; kill -9 $(pgrep -f 'Webcam Utility') 2>/dev/null", shell=True)
def connect():
    for _ in range(15):
        killc(); time.sleep(0.15)
        dev = usb.core.find(idVendor=VID)
        if dev is None: continue
        try: dev.set_configuration(); usb.util.claim_interface(dev, 0); return dev
        except usb.core.USBError: continue
    return None
def ptp(dev, code, params=None, timeout=25000):
    params = params or []
    hdr = struct.pack('<IHHI', 12+4*len(params), 1, code, 1) + struct.pack('<%dI'%len(params), *params)
    dev.write(EP_OUT, hdr, timeout)
    d=b''; t0=time.time()
    while time.time()-t0<8:
        try: ch=bytes(dev.read(EP_IN,8192,7000))
        except usb.core.USBTimeoutError: break
        except usb.core.USBError: break
        if not ch: break
        d+=ch
    return d
def rc_of(raw):
    i=0; last=0
    while i+12<=len(raw):
        length,=struct.unpack_from('<I',raw,i)
        t,=struct.unpack_from('<H',raw,i+4)
        c,=struct.unpack_from('<H',raw,i+6)
        if t==3: last=c
        i+=length if length>0 else 12
    return last
def data_of(raw):
    if len(raw)>=12:
        t,=struct.unpack_from('<H',raw,4)
        if t==2:
            l,=struct.unpack_from('<I',raw,0)
            return raw[12:l]
    return b''
dev = connect()
print("已连接")
ptp(dev, 0x1002, [1,0,0]); print(f"OpenSession+GetDevInfo rc={rc_of(ptp(dev,0x1001)):#x}")
r=ptp(dev, 0x90C2, [1]); print(f"ChangeCameraMode(1) rc={rc_of(r):#x} << 关键")
time.sleep(0.5)
r=ptp(dev, 0x9201); print(f"StartLiveView rc={rc_of(r):#x}")
time.sleep(2)
# 读 prohibit condition
raw=ptp(dev, 0x1015, [0xD0A4])
pd=data_of(raw)
if len(pd)>=4: print(f"ProhibitCond={struct.unpack('<I',pd[:4])[0]:#x}")
r=ptp(dev, 0x920a); print(f"StartMovieRec rc={rc_of(r):#x} << 是否成功")
time.sleep(5)
ptp(dev, 0x94FD, [0xF001,0,0,0,0])
for i in range(3):
    raw=ptp(dev, 0x94FD, [0xF032, 0xC181A13C, 4, 0, 0])
    data=data_of(raw)
    if len(data)>=4:
        v=struct.unpack('<I',data[:4])[0]
        print(f"  0xc181a13c={v} ({v//60}m{v%60:02d}s)")
    time.sleep(3)
r=ptp(dev, 0x920b); print(f"EndMovieRec rc={rc_of(r):#x}")
r=ptp(dev, 0x9202); print(f"EndLiveView rc={rc_of(r):#x}")
r=ptp(dev, 0x90C2, [0]); print(f"ChangeCameraMode(0) rc={rc_of(r):#x}")
