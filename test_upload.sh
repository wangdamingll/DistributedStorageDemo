./bin/mystop

make clean

make 

spawn-fcgi   -a  127.0.0.1   -p  8011    -f  ./bin/demo
spawn-fcgi   -a  127.0.0.1   -p  8014    -f  ./bin/upload
spawn-fcgi   -a  127.0.0.1   -p  8015    -f  ./bin/data
#spawn-fcgi   -a  127.0.0.1   -p  8016    -f  ./bin/echo


