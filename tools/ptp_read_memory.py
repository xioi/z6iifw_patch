import sys, struct, time, subprocess, usb.core, usb.util
VID=0x04B0; EP_OUT=0x02; EP_IN=0x81
# 反复 kill 抢占进程 + 尝试 claim，最多 15 轮
dev = None
for attempt in range(15):
    subprocess.run("kill -9 $(pgrep -f ptpcamerad) 2>/dev/null; kill -9 $(pgrep -f 'Webcam Utility') 2>/dev/null", shell=True)
    time.sleep(0.15)
    dev = usb.core.find(idVendor=VID)
    if dev is None: continue
    try:
        dev.set_configuration()
        usb.util.claim_interface(dev, 0)
        print(f"claim 成功 (第{attempt+1}轮)")
        break
    except usb.core.USBError:
        dev = None
        continue
if dev is None:
    print("无法 claim（进程持续抢占）")
    sys.exit(1)
def send(code, params):
    hdr = struct.pack('<IHHI', 12+4*len(params), 1, code & 0xFFFF, 1)
    hdr += struct.pack('<%dI' % len(params), *params)
    dev.write(EP_OUT, hdr, 15000)
def drain(max_s):
    data=b''; t0=time.time()
    while time.time()-t0<max_s:
        try: ch=bytes(dev.read(EP_IN,8192,6000))
        except usb.core.USBTimeoutError: break
        except usb.core.USBError: break
        if not ch: break
        data+=ch
    return data
send(0x1002,[1,0,0]); drain(12)
send(0x94FD,[0xF001,0,0,0,0]); drain(12)
send(0x94FD,[0xF032,0xC181A13C,4,0,0])
raw = drain(30)
if len(raw) >= 12:
    length, = struct.unpack_from('<I', raw, 0)
    payload = raw[12:12+length-12]
    if len(payload) >= 4:
        val = struct.unpack('<I', payload[:4])[0]
        print(f">>> 0xc181a13c (累计时长) = {val} ({val//60}m{val%60:02d}s)")
    else:
        print(f"payload {len(payload)}B: {payload.hex()}")
else:
    print(f"无响应 {len(raw)}B")
