#!/bin/sh

CUR_PATH=${PWD}
#FILE_LIST="../../include/cv181x/cvi_comm_3a.h ../../include/cv181x/cvi_comm_isp.h ../../../../../include/cvi_comm_sns.h"
FILE_LIST="${CUR_PATH}/../include/isp/cv181x/cvi_comm_3a.h ${CUR_PATH}/../include/isp/cv181x/cvi_comm_isp.h"
OUTFILE="pqbin.$$"

for file in $FILE_LIST
do
    cat $file >> $OUTFILE 2>&1
    if [ $? -ne 0 ];then
        sed -i "/ISP_BIN_MD5/c\    ISP_BIN_MD5: \"NULL\"" package.yaml
        rm $OUTFILE -f
		echo "filecat error $?"
        exit
    fi
done
result=`md5sum $OUTFILE |cut -d" " -f1`

if [ $? -eq 0 ];then
	sed -i "/ISP_BIN_MD5/c\    ISP_BIN_MD5: \"${result}\"" package.yaml
	echo "pqbin md5sum success!"
else
	sed -i "/ISP_BIN_MD5/c\    ISP_BIN_MD5: \"NULL\"" package.yaml
	echo "md5sum error $?"
fi
rm $OUTFILE -f
