"""Analyze the bounded USB PCM dump; export the actual captured audio."""
import argparse,json,re,wave
from pathlib import Path
import numpy as np
p=argparse.ArgumentParser();p.add_argument('log');a=p.parse_args();path=Path(a.log);s=path.read_text()
chunks=[bytearray() for _ in range(4)]
for phase,offset,h in re.findall(r'PCM (\d) (\d+) ([0-9a-f]+)',s):
    buf=chunks[int(phase)]
    if len(buf)!=int(offset):raise ValueError('Missing/out-of-order PCM block')
    buf.extend(bytes.fromhex(h))
if list(map(len,chunks)) != [1024,2048,2048,1024]:raise ValueError('Incomplete capture')
result=[]
for i,buf in enumerate(chunks):
    x=np.frombuffer(buf,dtype=np.uint8).astype(float);y=x[256:]-np.mean(x[256:]);freq=np.fft.rfftfreq(len(y),1/5512.5);spec=abs(np.fft.rfft(y*np.hanning(len(y))));peaks=np.argsort(spec[1:])[-5:]+1
    result.append(dict(phase=i,count=len(x),min=int(min(x)),max=int(max(x)),mean=float(np.mean(x)),rms=float(np.std(x)),strongest_frequencies_hz=freq[peaks].tolist()))
path.with_suffix('.analysis.json').write_text(json.dumps(result,indent=2)+'\n')
# Match the board's 8x linear interpolation, gain, clipping, and silence pads.
x=np.frombuffer(b''.join(chunks),dtype=np.uint8).astype(np.int32);out=[0]*4410
for i in range(len(x)):
    v0=(int(x[i])-127)*4096;v1=(int(x[min(i+1,len(x)-1)])-127)*4096
    out.extend(max(-16384,min(16384,v0+int((v1-v0)*j/8))) for j in range(8))
out.extend([0]*4410)
with wave.open(str(path.with_suffix('.wav')),'wb') as w:
    w.setnchannels(1);w.setsampwidth(2);w.setframerate(44100);w.writeframes(np.asarray(out,dtype='<i2').tobytes())
print(json.dumps(result,indent=2))
