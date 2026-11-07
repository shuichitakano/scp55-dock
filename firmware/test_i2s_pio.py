"""Execute the small PIO loop and check its wire protocol with distinct L/R words."""
from pathlib import Path
import re
program=[];labels={}
for line in (Path(__file__).parent/'rp2354b_diag/scp55_i2s.pio').read_text().splitlines():
    line=line.split(';')[0].strip()
    if not line or line.startswith('.'):continue
    if line.endswith(':'):labels[line.removeprefix('public ')[:-1]]=len(program)
    else:
        op,side=line.split('side');program.append((op.strip(),int(side.strip(),2)))
words=[0x1234abcd,0x80017ffe,0x55aaaa55]
bits=[(word>>i)&1 for word in words for i in range(31,-1,-1)]
pc=labels['entry_point'];x=0;data=0;used=0;clock=0;edges=[]
while len(edges)<len(bits)+1:
    op,side=program[pc];nxt=(pc+1)%len(program)
    if op.startswith('out'):
        data=bits[used];used+=1
    elif op.startswith('set'):x=int(op.split(',')[1])
    elif op.startswith('jmp'):
        target=op.split()[-1]
        if x:nxt=labels[target]
        x=(x-1)&0xffffffff
    else:raise AssertionError(op)
    if not clock and side&1:edges.append((data,side>>1))
    clock=side&1;pc=nxt
edges=edges[1:] # entry instruction precedes the first data bit
assert [b for b,lr in edges]==bits,'I2S word serialization changed'
for i,(_,lr) in enumerate(edges):
    # LRCK changes on the preceding channel LSB (one clock before the MSB).
    expected=0 if (i+1)%32<16 else 1
    assert lr==expected,(i,lr,expected)
print('I2S left/right order and one-bit LRCK lead passed.')
