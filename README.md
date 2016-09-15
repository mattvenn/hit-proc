# hit-proc

reads from stdin or a file

## options

*  -0 channel 0 offset (mV)
*  -1 channel 1 offset (mV)
*  -s start thresh (mV)
*  -e end thresh (mv)
*  -l number of samples signal has to be lower than end thresh to end hit
*  -p number of samples hit starts before threshold
*  -i filename, if not given use stdin
*  -b samples to read per chunk (default 1024)
*  -m maximum samples to read before ending a hit

## todo

* output to a csv file with correct format, waiting on lua input spec
