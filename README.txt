CONTRIBUTIONS

N/A

REPORT

Amount of time taken to sort test data by threshold value (real time):

Threshold 2097152:  0.379s
Threshold 1048576:  0.228s
Threshold 524288:   0.151s
Threshold 262144:   0.136s
Threshold 131072:   0.144s
Threshold 65536:    0.142s
Threshold 32768:    0.148s
Threshold 16384:    0.159s

Possible Explanation for Results:

The increase in speed caused by the parallel execution of sections of the array being sorted 
is dependent on the fact that different CPU cores sort parts of the array concurrently. 
The first few times the threshold is lowered, the increase in parallelism leads to greater
speed. However, as the threshold is lowered more and more, meaning that a greater number of
sections would technically be available to be sorted at once by different CPU cores in their 
different sorting processes, in reality there come to be so many more processes than CPU cores
available that the effect of parallel execution is negligible. Instead of the many sorting 
processes running concurrently, a limited number of CPU cores are switching between processes 
in an order decided by the OS kernel. Because a) the effects of parallelism become negligible, 
b) switches between processes take time, and c) in general quicksort is faster than mergesort
on smaller datasets, like the ones left in a lower threshold, the new latest stages of a
parallel mergesort added when the threshold lowers first show a diminished speedup, then 
actually begin to slow the total runtime down slightly. 
