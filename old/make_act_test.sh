#! /bin/bash

for var in "$@"
do
    rm -f ~/Documents/ADCO/act/tst/test/test_$var;
    printf "import \"~/Documents/ADCO/act/tst/$var\";\n\n" >> ~/Documents/ADCO/act/tst/test/test_$var;
    cat $var | grep -oP "(?<=defproc ).+(?=\s+\()" >> ~/Documents/ADCO/act/tst/test/test_$var;
    truncate -s -1 ~/Documents/ADCO/act/tst/test/test_$var;
    printf " t;\n" >> ~/Documents/ADCO/act/tst/test/test_$var;
done
