#! /bin/bash

PROJECT_PATH=/home/user/Documents/ADCO

for var in "$@"
do
  var=${var##*/};
  if [ ! -f $PROJECT_PATH/prs/tst/$var ]; then
    echo "$var not found."
    continue
  fi
  cat $PROJECT_PATH/scripts/watch_and_run | prsim $PROJECT_PATH/prs/tst/$var
done
