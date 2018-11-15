#! /bin/bash

PROJECT_PATH=/home/user/Documents/ADCO
AFLAT_PATH=/home/user/Downloads

pushd ${PROJECT_PATH}/chp > /dev/null;
make clean > /dev/null;
make;
popd > /dev/null;

for var in "$@"
do
    var=${var%.chp};
    var=${var##*/};
    rm -f ${PROJECT_PATH}/act/tst/${var}.act;
    rm -f ${PROJECT_PATH}/prs/${var}.prs;
    ${PROJECT_PATH}/chp/bin/zcc -b ${PROJECT_PATH}/chp/tst/${var}.chp > ${PROJECT_PATH}/act/tst/${var}.act;
    ${AFLAT_PATH}/aflatv2 ${PROJECT_PATH}/act/tst/${var}.act > ${PROJECT_PATH}/prs/${var}.prs;
done
