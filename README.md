solowan-simulator
=================

Download and compile:
--------
```
git clone https://github.com/centeropenmiddleware/solowan-simulator.git
cd solowan-simulator
make
```

Optimizer:
----------
Usage:
```
./optimizer -f file_name [-f ...] [options]
OPTIONS:
-r <number> - Number of transmissions (Default 1)
-o <filename> - Output file name (Default wan.out)
-s <filename> - Statistics file name (Default stat.csv
```
Example:
```
./optimizer -f data.txt -f test.txt -r 2 -s statistics.txt -o wan_message.out
```

Deoptimizer:
------------
Usage:
```
./deoptimizer -f file_name [options]
OPTIONS:
-m <filename> - Metadata file name
```
Example:
```
./deoptimizer -f wan_message.out -m wan_message.out.meta
```
