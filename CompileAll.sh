#!/usr/bin/bash

IFS="
"

PROJECTS="bigdisplay.yaml
bmp280_1.yaml
bmp280_2.yaml
bmp280_3.yaml
bmp280_4.yaml
cyd.yaml"

for PROJECT in $PROJECTS
do

echo $PROJECT
esphome compile "$PROJECT"

done

