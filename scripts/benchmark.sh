#! /bin/bash

PROJECT_PATH=/home/user/Documents/ADCO

for var in "$@"
do
  var=${var##*/};
  if [ ! -f $PROJECT_PATH/prs/benchmarks/$var ]; then
    echo "$var not found."
    continue
  fi
  touch /tmp/temp_$var;
  cat $PROJECT_PATH/scripts/watch_and_run > /tmp/temp_$var;
  name=${var%.prs};
  echo "dumptc $PROJECT_PATH/results/$name.tc" >> /tmp/temp_$var;
  cat /tmp/temp_$var | prsim $PROJECT_PATH/prs/benchmarks/$var > $PROJECT_PATH/results/${name}.sim;
  rm -f /tmp/temp_$var;
done
