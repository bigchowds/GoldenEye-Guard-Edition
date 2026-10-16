from pathlib import Path
import csv,zlib,shutil,hashlib
root=Path.cwd();rom=(root/'baserom.u.z64').read_bytes()
assert hashlib.sha1(rom).hexdigest()=='abe01e4aeb033b6c0836819f549c791b26cfde83'
for row in csv.reader((root/'scripts/filelist.u.csv').open()):
 if len(row)<5 or not row[0].isdigit():continue
 offset,size,name,compressed,extract=row[:5]
 if name.endswith('J.bin') and '/text/' in name:
  p=root/name
  if not p.exists():
   raw=rom[int(offset):int(offset)+int(size)]
   p.parent.mkdir(parents=True,exist_ok=True)
   p.write_bytes(zlib.decompress(raw[2:],-15) if compressed=='1' else raw)
for name in ['chrsilverwppk','chrgoldwppk']:
 shutil.copyfile(root/'assets/obseg/prop/PchrwppkZ.bin',root/f'assets/obseg/prop/P{name}Z.bin')
for name in ['text/u/LrunE.bin','text/LrunE.bin','setup/u/UsetuprunZ.bin','setup/UsetuprunZ.bin','brief/u/UbriefrunZ.bin','brief/UbriefrunZ.bin']:
 p=root/'assets/obseg'/name
 if p.exists():p.unlink()
print('Build assets prepared.')
