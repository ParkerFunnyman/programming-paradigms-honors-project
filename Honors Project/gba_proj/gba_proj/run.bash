#!/usr/bin/env sh -x
make clean .
make build .
echo Change this link to point to your mGBA exe file
mgba-qt gba_proj.gba