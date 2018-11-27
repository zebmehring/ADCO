#! /bin/bash

PROJECT_PATH=/home/user/Documents/ADCO
AFLAT_PATH=/home/user/Downloads
BUNDLED=0

pushd ${PROJECT_PATH}/chp > /dev/null;
make clean > /dev/null;
make;
popd > /dev/null;

function usage()
{
    echo "chp2prs [-b | --bundle_data] <chp>*"
}

while [ "$1" != "" ]; do
  PARAM=`echo $1 | awk -F= '{print $1}'`
  case $PARAM in
    -h | --help)
      usage
      exit
      ;;
    -b | --bundle_data)
      BUNDLED=1
      ;;
    *)
      break
  esac
  shift
done

for var in "$@"
do
  var=${var%.chp};
  var=${var##*/};
  rm -f ${PROJECT_PATH}/act/tst/${var}.act;
  rm -f ${PROJECT_PATH}/prs/${var}.prs;
  if [ ${BUNDLED} = 1 ]; then
    ${PROJECT_PATH}/chp/bin/zcc -b ${PROJECT_PATH}/chp/tst/${var}.chp > ${PROJECT_PATH}/act/tst/${var}.act;
  else
    ${PROJECT_PATH}/chp/bin/zcc ${PROJECT_PATH}/chp/tst/${var}.chp > ${PROJECT_PATH}/act/tst/${var}.act;
  fi
  ${AFLAT_PATH}/aflatv2 ${PROJECT_PATH}/act/tst/${var}.act > ${PROJECT_PATH}/prs/${var}.prs;
done
