#!/bin/bash
# 抓 gphoto2 movie=1 的完整 USB 字节 (LIBUSB_DEBUG=4)
for i in $(seq 1 15); do
  kill -9 $(pgrep -f ptpcamerad) 2>/dev/null; kill -9 $(pgrep -f "Webcam Utility") 2>/dev/null
  sleep 0.3
  LIBUSB_DEBUG=4 gphoto2 --set-config movie=1 > /dev/null 2>/tmp/gphoto_usb.log
  if grep -qE "movie|0x920a|0x9435" /tmp/gphoto_usb.log 2>/dev/null || [ $i -eq 15 ]; then
    echo "完成(${i}轮), log: /tmp/gphoto_usb.log ($(wc -l < /tmp/gphoto_usb.log 2>/dev/null) 行)"
    break
  fi
done
grep -iE "0x920a|0x9435|0x2001|0x200f|transfer|submit" /tmp/gphoto_usb.log 2>/dev/null | head -30
