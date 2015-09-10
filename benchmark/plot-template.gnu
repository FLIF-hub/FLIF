set terminal pngcairo size 1280,900 dashed
set output 'compression-CATEGORY.png'

bm = 0.15
lm = 0.12
rm = 0.95
gap = 0.03
gap = 0.02
size = 0.75
kk = 0.8 # relative height of bottom plot
y1 = 0.0; y2 = 1.5; y3 = 1.5; y4 = 20.0
x1 = 0
x2 = NUMBER


set multiplot
set border 1+2+8
set xtics nomirror
set ytics nomirror
set ytics 0.1
set grid ytics back
set grid xtics back
set lmargin at screen lm
set rmargin at screen rm
set bmargin at screen bm
set tmargin at screen bm + size * kk
set key bottom right

set xlabel "images"
set ylabel "relative size of compressed file (smaller is better)"


plot [x1:x2][y1:y2]\
'<sort -n output_data/*.jp2.CATEGORY' using ($0):($1) with lines lw 3  lc rgb "#000088" lt 1 title "lossless JPEG 2000",\
'<sort -n output_data/*.bpg.CATEGORY' using ($0):($1) with lines lw 2  lc rgb "#AA00AA" lt 1 title "BPG -lossless",\
'<sort -n output_data/*.PNGadam7.CATEGORY' using ($0):($1) with lines lw 2 lc rgb "#00FF00" lt 1 title "PNG, Adam7 interlacing",\
'<sort -n output_data/*.PNG-orig.CATEGORY' using ($0):($1) with lines lw 1  lc rgb "#008800" lt 1 title "PNG (original)",\
'<sort -n output_data/*.PNG95.CATEGORY' using ($0):($1) with lines lw 3  lc rgb "#008800" lt 1 title "PNG (convert -quality 95, reference)",\
'<sort -n output_data/*.PNGcrushAdam7.CATEGORY' using ($0):($1) with lines lw 2 lc rgb "#00FF00" lt 2 title "PNG, Adam7, pngcrush -brute",\
'<sort -n output_data/*.PNGcrushpngout.CATEGORY' using ($0):($1) with lines lw 2 lc rgb "#008800" lt 2 title "PNG, pngcrush -brute, pngout",\
'<sort -n output_data/*.WebP.CATEGORY' using ($0):($1) with lines lw 3 lc rgb "#00AAAA" lt 1 title "WebP -lossless -m 6",\
'<sort -n output_data/*.FLIF-ni.CATEGORY' using ($0):($1) with lines lw 3 lc rgb "#DD2200" lt 1 title "FLIF -ni",\
'<sort -n output_data/*.FLIF.CATEGORY' using ($0):($1) with lines lw 3 lc rgb "#880000" lt 1 title "FLIF"


unset xtics
unset xlabel
unset ylabel
set title  "Image corpus: CATEGORY"

set border 2+4+8
set bmargin at screen bm + size * kk + gap
set tmargin at screen bm + size + gap

set key off

set logscale y
set ytics (1.5,2,3,4,8,16)

plot [x1:x2][y3:y4]\
'<sort -n output_data/*.jp2.CATEGORY' using ($0):($1) with lines lw 3  lc rgb "#000088" lt 1 title "lossless JPEG 2000",\
'<sort -n output_data/*.bpg.CATEGORY' using ($0):($1) with lines lw 2  lc rgb "#AA00AA" lt 1 title "BPG -lossless",\
'<sort -n output_data/*.PNGadam7.CATEGORY' using ($0):($1) with lines lw 2 lc rgb "#00FF00" lt 1 title "PNG, Adam7 interlacing",\
'<sort -n output_data/*.PNG-orig.CATEGORY' using ($0):($1) with lines lw 1  lc rgb "#008800" lt 1 title "PNG (original)",\
'<sort -n output_data/*.PNG95.CATEGORY' using ($0):($1) with lines lw 3  lc rgb "#008800" lt 1 title "PNG (convert -quality 95, reference)",\
'<sort -n output_data/*.PNGcrushAdam7.CATEGORY' using ($0):($1) with lines lw 2 lc rgb "#00FF00" lt 2 title "PNG, Adam7, pngcrush -brute",\
'<sort -n output_data/*.PNGcrushpngout.CATEGORY' using ($0):($1) with lines lw 2 lc rgb "#008800" lt 2 title "PNG, pngcrush -brute, pngout",\
'<sort -n output_data/*.WebP.CATEGORY' using ($0):($1) with lines lw 3 lc rgb "#00AAAA" lt 1 title "WebP -lossless -m 6",\
'<sort -n output_data/*.FLIF-ni.CATEGORY' using ($0):($1) with lines lw 3 lc rgb "#DD2200" lt 1 title "FLIF -ni",\
'<sort -n output_data/*.FLIF.CATEGORY' using ($0):($1) with lines lw 3 lc rgb "#880000" lt 1 title "FLIF"

