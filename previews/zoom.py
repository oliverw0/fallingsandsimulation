import sys
from PIL import Image
# zoom.py sheet.png fw fh cells(list r,c) scale out
im=Image.open(sys.argv[1]); fw,fh=int(sys.argv[2]),int(sys.argv[3]); Z=int(sys.argv[5])
cells=[tuple(map(int,x.split(','))) for x in sys.argv[4].split(';')]
out=Image.new('RGBA',(fw*Z*len(cells)//4,fh*Z//4),(0,0,0,255))
for k,(r,c) in enumerate(cells):
    cr=im.crop((c*fw,r*fh,(c+1)*fw,(r+1)*fh)).resize((fw*Z//4,fh*Z//4),Image.NEAREST)
    out.paste(cr,(k*fw*Z//4,0))
out.save(sys.argv[6])
