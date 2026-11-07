"""Local USB bench helper: bounded flash and explicit single-command probes."""
import argparse,fcntl,glob,os,re,select,struct,subprocess,termios,time,tty
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--flash');p.add_argument('--command',choices=list('cnaulprsm'));p.add_argument('--log');p.add_argument('--repeat',type=int,default=1);p.add_argument('--stop-after',type=float);a=p.parse_args()
if a.stop_after is not None and (a.command not in 'rsm' or not 0<a.stop_after<60):raise SystemExit('Stop timing requires a live command and 0..60 seconds')
if a.repeat not in (1,2,3) or (a.repeat!=1 and a.command!='p'):raise SystemExit('Repeat is supported only for playback, 1..3 times')
tool=os.environ.get('PICOTOOL','/Users/shuichi/.pico-sdk/picotool/2.2.0-a4/picotool/picotool')
serial='21E814F5B43CC6C7'
if a.flash:
    try: subprocess.run([tool,'reboot','-u','-f','--ser',serial],check=True,timeout=12)
    except subprocess.TimeoutExpired: print('Reboot request timed out; checking device through the subsequent load.')
    time.sleep(1)
    subprocess.run([tool,'load','-v',a.flash,'--ser',serial],check=True,timeout=30)
    subprocess.run([tool,'reboot','--ser',serial],check=True,timeout=12)
    time.sleep(2)
if a.command:
    ports=glob.glob('/dev/cu.usbmodem*')
    if len(ports)!=1: raise SystemExit('Expected exactly one USB serial port: '+repr(ports))
    fd=os.open(ports[0],os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK);data=bytearray()
    def collect(seconds):
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            if select.select([fd],[],[],0.25)[0]:data.extend(os.read(fd,16384))
    try:
        tty.setraw(fd)
        fcntl.ioctl(fd,termios.TIOCMBIC,struct.pack('I',termios.TIOCM_DTR));collect(1)
        data.clear()
        fcntl.ioctl(fd,termios.TIOCMBIS,struct.pack('I',termios.TIOCM_DTR));collect(3)
        text=data.decode(errors='replace')
        rows=re.findall(r'CD=(\d\d) CARD_mV=(\d+) PWR=(\d) BUS_EN=(\d) present=(\d) flags=([0-9a-fA-F]+)',text)
        if 'Bench probe: reference=3320 mV' not in text or len(rows)<3 or not all(r[0]=='00' and r[2:]==('0','0','1','00') for r in rows[-3:]):
            raise RuntimeError('Board not ready: '+text)
        if a.command in 'aul' and 'Audio capture:' not in text:raise RuntimeError('Wrong firmware')
        if a.command in 'rsm' and 'Live audio:' not in text:raise RuntimeError('Wrong firmware')
        if a.command=='n' and 'Note test:' not in text:raise RuntimeError('Wrong firmware')
        for repeat in range(a.repeat):
            os.write(fd,a.command.encode()); print('Sent command:',a.command,repeat+1,flush=True)
            if a.stop_after is not None:
                collect(a.stop_after);os.write(fd,b'x');collect(4)
            else:
                collect(3 if a.command=='p' else 65 if a.command=='s' else 18 if a.command=='r' else 15)
    finally:
        os.close(fd)
        text=data.decode(errors='replace')
        if a.log:Path(a.log).write_text(text)
        print('\n'.join(line for line in text.splitlines() if not line.startswith('PCM ')))
