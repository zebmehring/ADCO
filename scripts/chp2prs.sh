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
  dir=${var%/*.chp};
  dir=${dir##*/};
  var=${var%.chp};
  var=${var##*/};
  echo ===${var}===
  if [ "$OPTIMIZATION" -eq 1 ]; then
    rm -f ${PROJECT_PATH}/act/${dir}/${var}.act;
    rm -f ${PROJECT_PATH}/prs/${dir}/${var}.prs;
    ${PROJECT_PATH}/chp/bin/zcc -O1 -o ${var}.act ${PROJECT_PATH}/chp/${dir}/${var}.chp;
    ${AFLAT_PATH}/aflatv2 ${PROJECT_PATH}/act/${dir}/${var}.act > ${PROJECT_PATH}/prs/${dir}/${var}.prs;
  elif [ "$BUNDLED" -eq 1 ]; then
    rm -f ${PROJECT_PATH}/act/${dir}/bundled_${var}.act;
    rm -f ${PROJECT_PATH}/prs/${dir}/bundled_${var}.prs;
    ${PROJECT_PATH}/chp/bin/zcc -b -o ${var}.act ${PROJECT_PATH}/chp/${dir}/${var}.chp;
    ${AFLAT_PATH}/aflatv2 ${PROJECT_PATH}/act/${dir}/bundled_${var}.act > ${PROJECT_PATH}/prs/${dir}/bundled_${var}.prs;
  else
    rm -f ${PROJECT_PATH}/act/${dir}/${var}.act;
    rm -f ${PROJECT_PATH}/prs/${dir}/${var}.prs;
    ${PROJECT_PATH}/chp/bin/zcc -o ${var}.act ${PROJECT_PATH}/chp/${dir}/${var}.chp;
    ${AFLAT_PATH}/aflatv2 ${PROJECT_PATH}/act/${dir}/${var}.act > ${PROJECT_PATH}/prs/${dir}/${var}.prs;
  fi
done
