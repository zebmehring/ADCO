#! /bin/bash

for var in "$@"
do
    rm -f test/test_$var;
    printf "import \"$var\";\n\n" >> test/test_$var;
    cat $var | grep -oP "(?<=defproc ).+(?=\s+\()" >> test/test_$var;
    truncate -s -1 test/test_$var;
    printf " t;\n" >> test/test_$var;
done
