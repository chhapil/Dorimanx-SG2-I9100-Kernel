#!/bin/sh
md5sum READY-JB/system/chainfire/SuperSU.apk | awk '{print $1}' > READY-JB/system/chainfire/SuperSU.apk.md5;
echo "Done!";
cat READY-JB/system/chainfire/SuperSU.apk.md5;
