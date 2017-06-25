```
----------------------------------------
HotCalls
Copyright 2017 The Regents of the University of Michigan
Ofir Weisse, Valeria Bertacco and Todd Austin

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
---------------------------------------------
```

# HotCalls
## A fast alternative interface for SGX secure enclaves

Based on ISCA 2017 HotCalls paper, that can be found at (http://www.ofirweisse.com/previous_work.html).

### Build and run tests
`make; ./test_hotcalls`

The main benchmark function is at App/App.cpp: HotCallsTester::Run()

Measurements of different type of calls are in `measurements/<timestamp>` directory:

- HotEcall_latencies_in_cycles.csv
- HotOcall_latencies_in_cycles.csv
- SDKEcall_latencies_in_cycles.csv
- SDKOcall_latencies_in_cycles.csv

The number of iterations is defined by `PERFORMANCE_MEASUREMENT_NUM_REPEATS` at `App/App.cpp`.

The round trip time of calls is measured in cycles, using RDTSCP insturction. The overhead of the RDTSCP insturction is roughly 30 cylces, which should be substructed from the numbers in the `csv` files. Different machines may have different overheads for RDTSCP.  

NOTE: the file `spinlock.c` is taken from Intel's SGX SDK repository at (https://github.com/01org/linux-sgx)
