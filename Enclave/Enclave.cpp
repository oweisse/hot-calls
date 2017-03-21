//Author: Ofir Weisse, www.OfirWeisse.com, email: oweisse (at) umich (dot) edu
//Based on ISCA 2017 "HotCalls" paper. 
//Link to the paper can be found at http://www.ofirweisse.com/previous_work.html
//If you make nay use of this code for academic purpose, please cite the paper. 

// MIT License

// Copyright (c) 2016 oweisse

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


/*
 * Copyright (C) 2011-2016 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */

#include "Enclave.h"
#include "Enclave_t.h"  /* print_string */

#include "../include/common.h"


void MyCustomEcall( void* data )
{
	int *counter = (int*)data;
	*counter += 1;
}

void EcallStartResponder( HotCall* hotEcall )
{
	void (*callbacks[1])(void*);
    callbacks[0] = MyCustomEcall;

    HotCallTable callTable;
    callTable.numEntries = 1;
    callTable.callbacks  = callbacks;

    HotCall_waitForCall( hotEcall, &callTable );
}

void EcallMeasureHotOcallsPerformance( uint64_t*     performanceCounters, 
                                       uint64_t      numRepeats,
                                       HotCall*      hotOcall )
{
	printf( "Running %s\n", __func__ );

	int         expectedData = 1;
	OcallParams *ocallParams = (OcallParams *)hotOcall->data;
	ocallParams->cyclesCount = &performanceCounters[ 0 ];

	const uint16_t requestedCallID = 0;
	HotCall_requestCall( hotOcall, requestedCallID, ocallParams ); //Setup startTime to current rdtscp()
	for( uint64_t i=0; i < numRepeats; ++i ) {
		ocallParams->cyclesCount = &performanceCounters[ i ];
		HotCall_requestCall( hotOcall, requestedCallID, ocallParams );

		expectedData++;
        if( ocallParams->counter != expectedData ){
            printf( "Error! ocallParams->counter is different than expected: %d != %d\n", ocallParams->counter, expectedData );
        }
	}
}

void EcallMeasureSDKOcallsPerformance( uint64_t*     performanceCounters, 
                                       uint64_t      numRepeats,
                                       OcallParams*  ocallParams )
{
	printf( "Running %s\n", __func__ );

	int         expectedData = 1;
	ocallParams->cyclesCount = &performanceCounters[ 0 ];

	const uint16_t requestedCallID = 0;
	MyCustomOcall( ocallParams ); //Setup startTime to current rdtscp()
	for( uint64_t i=0; i < numRepeats; ++i ) {
		ocallParams->cyclesCount = &performanceCounters[ i ];
		MyCustomOcall( ocallParams );

		expectedData++;
        if( ocallParams->counter != expectedData ){
            printf( "Error! ocallParams->counter is different than expected: %d != %d\n", ocallParams->counter, expectedData );
        }
	}
}
/* 
 * printf: 
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */
void printf(const char *fmt, ...)
{
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
}
