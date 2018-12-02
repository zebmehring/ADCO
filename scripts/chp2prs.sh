#! /bin/bash

PROJECT_PATH=/home/user/Documents/ADCO
AFLAT_PATH=/home/user/Downloads
BUNDLED=0
OPTIMIZATION=0

pushd ${PROJECT_PATH}/chp > /dev/null;
make clean > /dev/null;
make;
popd > /dev/null;

function usage()
{
    echo "chp2prs [[-b | --bundle_data] | [-O1]] <chp>*"
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
    -O1)
      OPTIMIZATION=1
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
  if [ "$OPTIMIZATION" -eq 1 ]; then
    ${PROJECT_PATH}/chp/bin/zcc -O1 -o ${var}.act ${PROJECT_PATH}/chp/tst/${var}.chp;
  elif [ "$BUNDLED" -eq 1 ]; then
    ${PROJECT_PATH}/chp/bin/zcc -b -o ${var}.act ${PROJECT_PATH}/chp/tst/${var}.chp;
  else
    ${PROJECT_PATH}/chp/bin/zcc -o ${var}.act ${PROJECT_PATH}/chp/tst/${var}.chp;
  fi
  ${AFLAT_PATH}/aflatv2 ${PROJECT_PATH}/act/tst/${var}.act > ${PROJECT_PATH}/prs/${var}.prs;
done
