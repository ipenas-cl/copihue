#!/bin/bash
set -e
col(){ case $1 in G) echo '#2f6b3a';; g) echo '#4caf50';; R) echo '#a3122a';; r) echo '#e63946';; p) echo '#ffb3bd';; y) echo '#f4d35e';; esac; }
rects(){ y=0; while IFS= read -r l; do for((x=0;x<${#l};x++)); do c=${l:x:1}; if [ "$c" != . ]; then echo -n "<rect x=\"$x\" y=\"$y\" width=\"1\" height=\"1\" fill=\"$(col $c)\"/>"; fi; done; y=$((y+1)); done < icon.txt; }
declare -A F
# 5x9 lowercase glyphs: rows 0-1 ascenders, 2-6 x-height, 7-8 descenders
F[c]=".........."".#####....#....#.....####"".........."
F[o]=".........."".###.#...##...##...#.###."".........."
F[p]="..........""####.#...##...#####.#....""#....#...."
F[i]="..#........""##....#....#....#...###."".........."
F[h]="#....#....""####.#...##...##...##...#"".........."
F[u]="..........""#...##...##...##..##.##.#"".........."
F[e]=".........."".###.#...#######.....####"".........."
word(){ ox=${2:-27}; oy=${3:-7}; for ch in c o p i h u e; do g=${F[$ch]}; for((k=0;k<45;k++)); do if [ "${g:k:1}" = "#" ]; then echo -n "<rect x=\"$((ox+k%5))\" y=\"$((oy+k/5))\" width=\"1\" height=\"1\" fill=\"$1\"/>"; fi; done; ox=$((ox+6)); done; }
WI=$(rects)
H='<svg xmlns="http://www.w3.org/2000/svg" shape-rendering="crispEdges"'
echo "$H viewBox=\"0 0 24 24\" width=\"512\" height=\"512\"><title>Copihue</title>$WI</svg>" > copihue-pixel-icon.svg
echo "$H viewBox=\"-2 -2 28 28\" width=\"512\" height=\"512\"><title>Copihue</title><rect x=\"-2\" y=\"-2\" width=\"28\" height=\"28\" fill=\"#14161c\"/>$WI</svg>" > copihue-pixel-icon-dark.svg
for m in light:#f5f1e8:#1d1f27 dark:#14161c:#f5f1e8; do IFS=: read n bg fg <<<"$m"
echo "$H viewBox=\"-2 -2 74 28\" width=\"1110\" height=\"420\"><title>Copihue</title><rect x=\"-2\" y=\"-2\" width=\"74\" height=\"28\" fill=\"$bg\"/>$WI$(word $fg)<rect x=\"27\" y=\"18\" width=\"41\" height=\"1\" fill=\"#e63946\"/></svg>" > copihue-pixel-wordmark-$n.svg; done
for f in copihue-*.svg; do rsvg-convert "$f" -o "${f%.svg}.png"; done
cat > cs.svg <<E
<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="1600" height="900"><rect width="1600" height="900" fill="#888"/>
<image x="40" y="40" width="380" height="380" xlink:href="copihue-pixel-icon.png"/>
<image x="460" y="40" width="380" height="380" xlink:href="copihue-pixel-icon-dark.png"/>
<image x="880" y="60" width="680" height="257" xlink:href="copihue-pixel-wordmark-light.png"/>
<image x="40" y="480" width="1060" height="401" xlink:href="copihue-pixel-wordmark-dark.png"/>
<image x="1160" y="560" width="64" height="64" xlink:href="copihue-pixel-icon-dark.png"/><image x="1260" y="576" width="32" height="32" xlink:href="copihue-pixel-icon-dark.png"/><image x="1320" y="584" width="16" height="16" xlink:href="copihue-pixel-icon-dark.png"/></svg>
E
rsvg-convert cs.svg -o contact-sheet.png; rm cs.svg
# GitHub social preview 1280x640 (10px per pixel)
echo "$H xmlns:xlink=\"http://www.w3.org/1999/xlink\" viewBox=\"0 0 128 64\" width=\"1280\" height=\"640\"><title>Copihue</title><rect width=\"128\" height=\"64\" fill=\"#14161c\"/><g transform=\"translate(4 14) scale(1.25)\">$WI</g><g transform=\"translate(38 12) scale(2)\">$(word '#f5f1e8' 0 0)</g><rect x=\"38\" y=\"34\" width=\"82\" height=\"1\" fill=\"#e63946\"/><text x=\"38\" y=\"44\" font-family=\"DejaVu Sans Mono, monospace\" font-size=\"3.6\" fill=\"#ffb3bd\">un lenguaje de sistemas hecho en Chile</text><text x=\"38\" y=\"51\" font-family=\"DejaVu Sans Mono, monospace\" font-size=\"3\" fill=\"#8a8f9c\">github.com/ipenas-cl/copihue · archivos .lap</text></svg>" > copihue-pixel-social-preview.svg
rsvg-convert copihue-pixel-social-preview.svg -o copihue-pixel-social-preview.png
