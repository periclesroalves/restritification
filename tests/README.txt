-----------------------------------------------------
                  SPEC CPU - 2006
-----------------------------------------------------
SPEC is intended to be executed in the cuda server.

Installation
------------
$ cd /home/nazare/SPEC_CPU2006v1.1
$ ./install.sh -d <path-to-repo>/tests/speccpu2006
$ cd <path-to-repo>/tests/speccpu2006
$ . ./shrc
$ cp ../RESTRITIFICATION.cfg config/

Running tests
-------------
Change config/RESTRITIFICATION.cfg to reflect the right paths to the compiler
build you want to test as well as the test parameters in the header (number 
of executions, workload size, etc.)

Execute the tests with:
$ runspec --config RESTRITIFICATION.cfg <benchmarks-to-test>

LLVM 3.5 is capable of compiling the following benchmarks:
401.bzip2 403.gcc 429.mcf 433.milc 444.namd 445.gobmk 447.dealII 450.soplex
453.povray 456.hmmer 458.sjeng 462.libquantum 464.h264ref 470.lbm 471.omnetpp
473.astar 482.sphinx3 483.xalancbmk 998.specrand 999.specrand
