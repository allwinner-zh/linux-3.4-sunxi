#!/bin/bash 
set -e 

generate_rootfs() 
{ 
if [ -d skel ]; then 
     (cd skel;find . |fakeroot cpio -o -Hnewc |gzip > ../"$1") 
else 
     echo "skel not there run buil.sh e sun4i_rootf.gz first" 
     exit 1 
fi 
} 
 
extract_rootfs() 
{ 
if [ -f "$1" ]; then
     rm -rf skel && mkdir skel 
     gzip -dc $1 | (cd skel;fakeroot cpio -iv) 
else 
     echo "$1 not there" 
     exit 1 
fi 
} 
 
 
if [ $# -ne 2 ]; then 
     echo -e "please input correct parameters!" 
     echo -e "\t[build.sh e rootfs.cpio.gz] to extract the rootfs template to skel folder" 
     echo -e "\tthen make some changes in the skel folder" 
     echo -e "\t[build.sh c rootfs.cpio.gz] to create the rootfs from the skel folder" 
     exit 1 
fi 
 
if [ "$1" = "e" ]; then 
     extract_rootfs $2 
elif [ "$1" = "c" ]; then 
     generate_rootfs $2 
else 
     echo "Wrong arguments" 
     exit 1 
fi 
