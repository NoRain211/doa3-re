import struct,sys,hashlib
p=sys.argv[1]; d=open(p,'rb').read()
base,=struct.unpack_from('<I',d,0x104)
n,addr=struct.unpack_from('<II',d,0x160)
off=addr-base
for i in range(n):
    name,ma,mi,b,fl=struct.unpack_from('<8sHHHH',d,off+16*i)
    print(name.rstrip(bytes(1)).decode('ascii','replace'), b)
ns,sh=struct.unpack_from('<II',d,0x11C)
for i in range(ns):
    fl,va,vs,ra,rs,na=struct.unpack_from('<IIIIII',d,sh-base+56*i)
    no=na-base; nm=d[no:d.index(bytes(1),no)].decode('ascii','replace')
    print('section',nm,hex(va),vs)
print('sha256',hashlib.sha256(d).hexdigest())

