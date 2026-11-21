import sys, struct, time, subprocess, usb.core, usb.util
VID=0x04B0; EP_OUT=0x02; EP_IN=0x81
# 尼康 PTP 命令/属性
OC_START_LIVEVIEW=0x9201; OC_END_LIVEVIEW=0x9202
OC_START_MOVIE=0x920a;   OC_END_MOVIE=0x920b
OC_CHANGE_APPMODE=0x9435
DPC_APPMODE=0xD1F0; DPC_LIVEVIEW_STATUS=0xD1A2; DPC_RECORD_MEDIA=0xD10B
DPC_PROHIBIT=0xD0A4
# 标准 PTP
OC_GET_PROP=0x1015; OC_SET_PROP=0x1016

def kill_compete():
    subprocess.run("kill -9 $(pgrep -f ptpcamerad) 2>/dev/null; kill -9 $(pgrep -f 'Webcam Utility') 2>/dev/null", shell=True)

def connect():
    for _ in range(15):
        kill_compete(); time.sleep(0.15)
        dev = usb.core.find(idVendor=VID)
        if dev is None: continue
        try:
            dev.set_configuration(); usb.util.claim_interface(dev, 0)
            return dev
        except usb.core.USBError:
            continue
    return None

def ptp_cmd(dev, code, params=None, data_out=None, read_data=True, timeout=20000):
    """发 PTP 命令. data_out=要发送的数据(bytes). 返回(data, resp_code)"""
    params = params or []
    hdr = struct.pack('<IHHI', 12+4*len(params), 1, code, 1)
    hdr += struct.pack('<%dI' % len(params), *params)
    dev.write(EP_OUT, hdr, timeout)
    if data_out is not None:
        # 数据阶段 OUT
        dhdr = struct.pack('<IHHI', 12+len(data_out), 2, code, 1)
        dev.write(EP_OUT, dhdr + data_out, timeout)
    if not read_data:
        return b'', 0
    # 读数据阶段(IN) + 响应
    data = b''
    try:
        chunk = bytes(dev.read(EP_IN, 8192, timeout))
        if len(chunk) >= 12:
            length, = struct.unpack_from('<I', chunk, 0)
            typ, = struct.unpack_from('<H', chunk, 4)
            if typ == 2:
                data = chunk[12:length]
                resp = bytes(dev.read(EP_IN, 8192, timeout))
                rc, = struct.unpack_from('<H', resp, 6) if len(resp)>=8 else (0,)
                return data, rc
            else:
                rc, = struct.unpack_from('<H', chunk, 6)
                return data, rc
    except usb.core.USBTimeoutError:
        pass
    except usb.core.USBError:
        pass
    return data, 0

def drain(dev, max_s=8):
    d=b''; t0=time.time()
    while time.time()-t0<max_s:
        try: ch=bytes(dev.read(EP_IN,8192,6000))
        except usb.core.USBTimeoutError: break
        except usb.core.USBError: break
        if not ch: break
        d+=ch
    return d

def get_prop(dev, dpc, size=1):
    data, rc = ptp_cmd(dev, OC_GET_PROP, [dpc])
    return data, rc

def set_prop(dev, dpc, val_bytes):
    # Set: 命令 + 数据阶段(OUT) 属性值
    hdr = struct.pack('<IHHI', 12+4, 1, OC_SET_PROP, 1) + struct.pack('<I', dpc)
    dev.write(EP_OUT, hdr, 20000)
    time.sleep(0.1)
    dhdr = struct.pack('<IHHI', 12+len(val_bytes), 2, OC_SET_PROP, 1) + val_bytes
    dev.write(EP_OUT, dhdr, 20000)
    time.sleep(0.2)
    return drain(dev, 5)

dev = connect()
if dev is None:
    print("无法连接相机"); sys.exit(1)
print("已连接")

# 1. OpenSession
drain(dev, 2)
ptp_cmd(dev, 0x1002, [1,0,0]); drain(dev, 8)
print("OpenSession 完成")

# 2. ApplicationMode = 1
data, rc = get_prop(dev, DPC_APPMODE)
cur = data[0] if len(data)>=1 else -1
print(f"ApplicationMode 当前 = {cur}")
if cur != 1:
    set_prop(dev, DPC_APPMODE, b'\x01')
    print("已设 ApplicationMode=1")
    time.sleep(0.5)

# 3. ChangeApplicationMode(1)
ptp_cmd(dev, OC_CHANGE_APPMODE, [1]); drain(dev, 8)
print("ChangeApplicationMode(1) 完成")

# 4. LiveView
data, _ = get_prop(dev, DPC_LIVEVIEW_STATUS)
lv = data[0] if len(data)>=1 else 0
print(f"LiveViewStatus = {lv}")
if lv == 0:
    set_prop(dev, DPC_RECORD_MEDIA, b'\x01')
    ptp_cmd(dev, OC_START_LIVEVIEW); drain(dev, 10)
    print("StartLiveView 完成")
    time.sleep(1.5)

# 5. 检查禁用条件
data, _ = get_prop(dev, DPC_PROHIBIT, 4)
if len(data)>=4:
    prob = struct.unpack('<I', data[:4])[0]
    print(f"MovRecProhibitCondition = {prob:#x}")
    if prob:
        print("  (有位设置, 但尝试继续)")

# 6. 开始录像
ptp_cmd(dev, OC_START_MOVIE); drain(dev, 10)
print(">>> StartMovieRec 已发送")
time.sleep(5)

# 7. F001 + F032 读内存
drain(dev, 3)
ptp_cmd(dev, 0x94FD, [0xF001,0,0,0,0]); drain(dev, 8)
print("F001 完成")
for i in range(3):
    data, rc = ptp_cmd(dev, 0x94FD, [0xF032, 0xC181A13C, 4, 0, 0])
    if len(data)>=4:
        v = struct.unpack('<I', data[:4])[0]
        print(f"  0xc181a13c = {v} ({v//60}m{v%60:02d}s)")
    else:
        print(f"  读取失败 rc={rc:#x} data={data.hex()}")
    time.sleep(3)

# 8. 结束录像
ptp_cmd(dev, OC_END_MOVIE); drain(dev, 10)
print(">>> EndMovieRec 已发送（录像应已停止）")
