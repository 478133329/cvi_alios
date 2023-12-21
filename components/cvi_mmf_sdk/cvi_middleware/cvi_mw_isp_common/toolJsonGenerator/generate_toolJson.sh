#!/bin/bash
CHIP_ID=$1
DATE=$(date +%Y%m%d)
#toolJsonGenerator repo
GENERATOR_CHANGE_ID="I2db8c42"
GENERATOR_COMMIT_ID="8fa5a9ce"
GENERATOR_V="(${GENERATOR_CHANGE_ID},${GENERATOR_COMMIT_ID})"
ISP_BRANCH="master"
#isp repo from v3.0.0
ISP_CHANGE_ID="Iaa68769"
ISP_COMMIT_ID="a871ba61"
ISP_V="(${ISP_CHANGE_ID},${ISP_COMMIT_ID})"

#CUR_PATH=$(cd "$(dirname "$0")"; pwd)
CUR_PATH=${PWD}
DEVICE_FILE="${CUR_PATH}/device.json"
OUTPUT="pqtool_definition.json"
OUTPUT_TEMP="pqtool_definition_temp.json"

#config path by chip id
if [ $CHIP_ID == "cv181x" ]
then
    MW_INCLUDE_PATH=${CUR_PATH}/../../include
    ISP_INCLUDE_PATH=${MW_INCLUDE_PATH}/isp/$CHIP_ID
    RPCJSON_PATH=${CUR_PATH}/../../cvi_mw_isp_daemon
    OSDRV_INCLUDE_PATH=${CUR_PATH}/../../../cvi_osdrv/include/common/uapi/user
fi
HEADERLIST="$ISP_INCLUDE_PATH/cvi_comm_isp.h"
HEADERLIST+=" $ISP_INCLUDE_PATH/cvi_comm_3a.h"
HEADERLIST+=" $OSDRV_INCLUDE_PATH/cvi_comm_video.h"
HEADERLIST+=" $OSDRV_INCLUDE_PATH/cvi_comm_vo.h"
HEADERLIST+=" $MW_INCLUDE_PATH/cvi_comm_sns.h"
LEVELJSON=$CHIP_ID/level.json
LAYOUTJSON=$CHIP_ID/layout.json
RPCJSON=$RPCJSON_PATH/rpc.json
echo ${CUR_PATH}
cd ${CUR_PATH}
#reset&update device.json
cp ${DEVICE_FILE} ${DEVICE_FILE}.bak
sed -i 's/"FULL_NAME": ""/"FULL_NAME": "'"${CHIP_ID}"'"/g' ${DEVICE_FILE}
sed -i 's/"CODE_NAME": ""/"CODE_NAME": "'"${CHIP_ID}"'"/g' ${DEVICE_FILE}
sed -i 's/"SDK_VERSION": ""/"SDK_VERSION": "'"${DATE}"'"/g' ${DEVICE_FILE}
sed -i 's/"GENERATOR_VERSION": ""/"GENERATOR_VERSION": "'"${GENERATOR_V}"'"/g' ${DEVICE_FILE}
sed -i 's/"ISP_VERSION": ""/"ISP_VERSION": "'"${ISP_V}"'"/g' ${DEVICE_FILE}
sed -i 's/"ISP_BRANCH": ""/"ISP_BRANCH": "'"${ISP_BRANCH}"'"/g' ${DEVICE_FILE}

#generate pqtool_definition.json

python hFile2json.py  $LEVELJSON $LAYOUTJSON $RPCJSON $HEADERLIST
cp ${DEVICE_FILE}.bak ${DEVICE_FILE}
rm ${DEVICE_FILE}.bak

#cp pqtool_definition.json and cvi_pqtool_json.h
if [ -f ${OUTPUT} ]
then
    if [ -x "$(command -v zip)" ]
    then
        zip ${OUTPUT_TEMP} ${OUTPUT}
        mv ${OUTPUT_TEMP} ${OUTPUT}
    fi
    echo "Success!! generate cvi_pqtool_json.h to src dir of cvi_mw_isp_daemon done"
else
    echo "FAIL!! please check gen pqtool_definition.json flow"
fi



