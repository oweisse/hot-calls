// ----------------------------------------
// HotCalls
// Copyright 2017 The Regents of the University of Michigan
// Ofir Weisse, Valeria Bertacco and Todd Austin

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ---------------------------------------------

//Author: Ofir Weisse, www.OfirWeisse.com, email: oweisse (at) umich (dot) edu
//Based on ISCA 2017 "HotCalls" paper. 
//Link to the paper can be found at http://www.ofirweisse.com/previous_work.html
//If you make nay use of this code for academic purpose, please cite the paper. 

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


#include <stdio.h>
#include <string.h>
#include <assert.h>

# include <unistd.h>
# include <pwd.h>
# define MAX_PATH FILENAME_MAX


#include "sgx_urts.h"
#include "App.h"
#include "Enclave_u.h"


#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include "../include/common.h"

sgx_enclave_id_t globalEnclaveID;

typedef sgx_status_t (*EcallFunction)(sgx_enclave_id_t, void* );

#define PERFORMANCE_MEASUREMENT_NUM_REPEATS 10000
#define MEASUREMENTS_ROOT_DIR               "measurments"

using namespace std;

inline __attribute__((always_inline))  uint64_t rdtscp(void)
{
        unsigned int low, high;

        asm volatile("rdtscp" : "=a" (low), "=d" (high));

        return low | ((uint64_t)high) << 32;
}

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] = {
    {
        SGX_ERROR_UNEXPECTED,
        "Unexpected error occurred.",
        NULL
    },
    {
        SGX_ERROR_INVALID_PARAMETER,
        "Invalid parameter.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_MEMORY,
        "Out of memory.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_LOST,
        "Power transition occurred.",
        "Please refer to the sample \"PowerTransition\" for details."
    },
    {
        SGX_ERROR_INVALID_ENCLAVE,
        "Invalid enclave image.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ENCLAVE_ID,
        "Invalid enclave identification.",
        NULL
    },
    {
        SGX_ERROR_INVALID_SIGNATURE,
        "Invalid enclave signature.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_EPC,
        "Out of EPC memory.",
        NULL
    },
    {
        SGX_ERROR_NO_DEVICE,
        "Invalid SGX device.",
        "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards."
    },
    {
        SGX_ERROR_MEMORY_MAP_CONFLICT,
        "Memory map conflicted.",
        NULL
    },
    {
        SGX_ERROR_INVALID_METADATA,
        "Invalid enclave metadata.",
        NULL
    },
    {
        SGX_ERROR_DEVICE_BUSY,
        "SGX device was busy.",
        NULL
    },
    {
        SGX_ERROR_INVALID_VERSION,
        "Enclave version was invalid.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ATTRIBUTE,
        "Enclave was not authorized.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_FILE_ACCESS,
        "Can't open enclave file.",
        NULL
    },
};

/* Check error conditions for loading enclave */
void print_error_message(sgx_status_t ret)
{
    size_t idx = 0;
    size_t ttl = sizeof sgx_errlist/sizeof sgx_errlist[0];

    for (idx = 0; idx < ttl; idx++) {
        if(ret == sgx_errlist[idx].err) {
            if(NULL != sgx_errlist[idx].sug)
                printf("Info: %s\n", sgx_errlist[idx].sug);
            printf("Error: %s\n", sgx_errlist[idx].msg);
            break;
        }
    }
    
    if (idx == ttl)
        printf("Error: Unexpected error occurred.\n");
}


/* OCall functions */
void ocall_print_string(const char *str)
{
    /* Proxy/Bridge will check the length and null-terminate 
     * the input string to prevent buffer overflow. 
     */
    printf("%s", str);
}

void* EnclaveResponderThread( void* hotEcallAsVoidP )
{
    //To be started in a new thread
    HotCall *hotEcall = (HotCall*)hotEcallAsVoidP;
    EcallStartResponder( globalEnclaveID, hotEcall );

    return NULL;
}

void MyCustomOcall( void* data )
{
    //Because RDTSCP is not allowed inside an enclave in SGX 1.x, we have to issue it here,
    //in the ocall. Therefore, instead of measuring enclave-->ocall-->enclave, we will measure 
    //ocall-->enclave-->next_ocall
    
    static uint64_t startTime     = 0;  

    OcallParams* ocallParams = (OcallParams*)data;

    *(ocallParams->cyclesCount)  = rdtscp() - startTime; //startTime was set in previous iteration (except when first called)
    ocallParams->counter++;

    startTime     = rdtscp(); //for next iteration
}

void* OcallResponderThread( void* hotCallAsVoidP )
{
    void (*callbacks[1])(void*);
    callbacks[0] = MyCustomOcall;

    HotCallTable callTable;
    callTable.numEntries = 1;
    callTable.callbacks  = callbacks;

    HotCall_waitForCall( (HotCall *)hotCallAsVoidP, &callTable );

    return NULL;
}

class HotCallsTesterError {};



class HotCallsTester {
public:
    HotCallsTester() {
        m_enclaveID = 0;

        if( initialize_enclave() < 0){
            printf("Enter a character before exit ...\n");
            getchar();
            throw HotCallsTesterError(); 
        }

        CreateMeasurementsDirectory();
    }

    ~HotCallsTester() {
        /* Destroy the enclave */
        sgx_destroy_enclave( m_enclaveID );
    }

    void Run( void ) {
        TestHotEcalls();
        TestHotOcalls();

        TestSDKEcalls();
        TestSDKOcalls();
    }

    void TestHotEcalls()
    {
        uint64_t performaceMeasurements[ PERFORMANCE_MEASUREMENT_NUM_REPEATS ]= {0};

        uint64_t    startTime       = 0;
        uint64_t    endTime         = 0;
        int         data            = 0;
        int         expectedData    = 0;
        HotCall     hotEcall        = HOTCALL_INITIALIZER;
        hotEcall.data               = &data; 

        globalEnclaveID = m_enclaveID;
        pthread_create(&hotEcall.responderThread, NULL, EnclaveResponderThread, (void*)&hotEcall);

        const uint16_t requestedCallID = 0;
        for( uint64_t i=0; i < PERFORMANCE_MEASUREMENT_NUM_REPEATS; ++i ) {
            startTime = rdtscp();
            HotCall_requestCall( &hotEcall, requestedCallID, &data );
            endTime   = rdtscp();
        
            performaceMeasurements[ i ] = endTime       - startTime;

            expectedData++;
            if( data != expectedData ){
                printf( "Error! Data is different than expected: %d != %d\n", data, expectedData );
            }
        }

        StopResponder( &hotEcall );
        ostringstream filename;
        filename <<  "HotEcall_latencies_in_cycles.csv";
        WriteMeasurementsToFile( filename.str(), 
                                 (uint64_t*)performaceMeasurements, 
                                 PERFORMANCE_MEASUREMENT_NUM_REPEATS ) ;
    }

    void TestSDKEcalls()
    {
        uint64_t performaceMeasurements[ PERFORMANCE_MEASUREMENT_NUM_REPEATS ]= {0};

        uint64_t    startTime       = 0;
        uint64_t    endTime         = 0;
        int         data            = 0;
        int         expectedData    = 0;

        globalEnclaveID = m_enclaveID;        

        const uint16_t requestedCallID = 0;
        for( uint64_t i=0; i < PERFORMANCE_MEASUREMENT_NUM_REPEATS; ++i ) {
            startTime = rdtscp();
            MyCustomEcall( m_enclaveID, &data );
            endTime   = rdtscp();
        
            performaceMeasurements[ i ] = endTime       - startTime;

            expectedData++;
            if( data != expectedData ){
                printf( "Error! Data is different than expected: %d != %d\n", data, expectedData );
            }
        }

        ostringstream filename;
        filename <<  "SDKEcall_latencies_in_cycles.csv";
        WriteMeasurementsToFile( filename.str(), 
                                 (uint64_t*)performaceMeasurements, 
                                 PERFORMANCE_MEASUREMENT_NUM_REPEATS ) ;
    }

    void TestHotOcalls()
    {
        uint64_t performaceMeasurements[ PERFORMANCE_MEASUREMENT_NUM_REPEATS ]= {0};

        OcallParams ocallParams;
        ocallParams.counter     = 0;
        HotCall     hotOcall    = HOTCALL_INITIALIZER;
        hotOcall.data           = &ocallParams;
        
        pthread_create( &hotOcall.responderThread, NULL, OcallResponderThread, (void*)&hotOcall );
       
        EcallMeasureHotOcallsPerformance( 
                m_enclaveID, 
                (uint64_t*)performaceMeasurements, 
                PERFORMANCE_MEASUREMENT_NUM_REPEATS,
                &hotOcall );
        StopResponder( &hotOcall );

        ostringstream filename;
        filename <<  "HotOcall_latencies_in_cycles.csv";
        WriteMeasurementsToFile( filename.str(), 
                                 (uint64_t*)performaceMeasurements, 
                                 PERFORMANCE_MEASUREMENT_NUM_REPEATS ) ;
    }

    void TestSDKOcalls()
    {
        uint64_t performaceMeasurements[ PERFORMANCE_MEASUREMENT_NUM_REPEATS ]= {0};

        OcallParams ocallParams;
        ocallParams.counter     = 0;
        HotCall     hotOcall    = HOTCALL_INITIALIZER;
        hotOcall.data           = &ocallParams;
        
        EcallMeasureSDKOcallsPerformance( 
                m_enclaveID, 
                (uint64_t*)performaceMeasurements, 
                PERFORMANCE_MEASUREMENT_NUM_REPEATS,
                &ocallParams );
        
        ostringstream filename;
        filename <<  "SDKOcall_latencies_in_cycles.csv";
        WriteMeasurementsToFile( filename.str(), 
                                 (uint64_t*)performaceMeasurements, 
                                 PERFORMANCE_MEASUREMENT_NUM_REPEATS ) ;
    }

private:
    /* Global EID shared by multiple threads */
    sgx_enclave_id_t m_enclaveID;

    int              m_sgxDriver;
    string           m_measurementsDir;

    void WriteMeasurementsToFile( string fileName, uint64_t* measurementsMatrix, size_t numRows )
    {
        string fileFullPath = m_measurementsDir + "/" + fileName;
        cout << "Writing results.. ";
        cout << fileFullPath << " ";
        ofstream measurementsFile;
        measurementsFile.open( fileFullPath, ios::app );
        for( size_t rowIdx = 0; rowIdx < numRows; ++rowIdx ) {
            measurementsFile << measurementsMatrix[ rowIdx ] << " ";
            measurementsFile << "\n";
        }
        
        measurementsFile.close();

        cout << "Done\n";
    }

    /* Initialize the enclave:
     *   Step 1: try to retrieve the launch token saved by last transaction
     *   Step 2: call sgx_create_enclave to initialize an enclave instance
     *   Step 3: save the launch token if it is updated
     */
    int initialize_enclave(void)
    {
        char token_path[MAX_PATH] = {'\0'};
        sgx_launch_token_t token = {0};
        sgx_status_t ret = SGX_ERROR_UNEXPECTED;
        int updated = 0;
        
        /* Step 1: try to retrieve the launch token saved by last transaction 
         *         if there is no token, then create a new one.
         */
        /* try to get the token saved in $HOME */
        const char *home_dir = getpwuid(getuid())->pw_dir;
        
        if (home_dir != NULL && 
            (strlen(home_dir)+strlen("/")+sizeof(TOKEN_FILENAME)+1) <= MAX_PATH) {
            /* compose the token path */
            strncpy(token_path, home_dir, strlen(home_dir));
            strncat(token_path, "/", strlen("/"));
            strncat(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME)+1);
        } else {
            /* if token path is too long or $HOME is NULL */
            strncpy(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME));
        }
        
        FILE *fp = fopen(token_path, "rb");
        if (fp == NULL && (fp = fopen(token_path, "wb")) == NULL) {
            printf("Warning: Failed to create/open the launch token file \"%s\".\n", token_path);
        }
        
        if (fp != NULL) {
            /* read the token from saved file */
            size_t read_num = fread(token, 1, sizeof(sgx_launch_token_t), fp);
            if (read_num != 0 && read_num != sizeof(sgx_launch_token_t)) {
                /* if token is invalid, clear the buffer */
                memset(&token, 0x0, sizeof(sgx_launch_token_t));
                printf("Warning: Invalid launch token read from \"%s\".\n", token_path);
            }
        }
        
        /* Step 2: call sgx_create_enclave to initialize an enclave instance */
        /* Debug Support: set 2nd parameter to 1 */
        ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, &token, &updated, &m_enclaveID, NULL);
        if (ret != SGX_SUCCESS) {
            printf("sgx_create_enclave returned 0x%x\n", ret);
            print_error_message(ret);
            if (fp != NULL) fclose(fp);
            return -1;
        }
        
        /* Step 3: save the launch token if it is updated */
        if (updated == FALSE || fp == NULL) {
            /* if the token is not updated, or file handler is invalid, do not perform saving */
            if (fp != NULL) fclose(fp);
            return 0;
        }
        
        /* reopen the file with write capablity */
        fp = freopen(token_path, "wb", fp);
        if (fp == NULL) return 0;
        size_t write_num = fwrite(token, 1, sizeof(sgx_launch_token_t), fp);
        if (write_num != sizeof(sgx_launch_token_t))
            printf("Warning: Failed to save launch token to \"%s\".\n", token_path);
        fclose(fp);
        printf("line: %d\n", __LINE__ );
        return 0;
    }

    void CreateMeasurementsDirectory()
    {
        char timestamp[ 100 ] = {0};
        GetTimeStamp( timestamp, 100 );
        // printf( "%s\n", timestamp);

        if( ! IsDirectoryExists( MEASUREMENTS_ROOT_DIR )  ) {
            printf( "Creating directory %s\n", MEASUREMENTS_ROOT_DIR );
            mkdir(MEASUREMENTS_ROOT_DIR, 0700);
        }

        m_measurementsDir = string( MEASUREMENTS_ROOT_DIR ) + "/" + timestamp;
        if( ! IsDirectoryExists( m_measurementsDir )  ) {
            printf( "Creating directory %s\n", m_measurementsDir.c_str() );
            mkdir(m_measurementsDir.c_str(), 0700);
        }
    }

    void GetTimeStamp( char *timestamp, size_t size )
    {
      time_t rawtime;
      struct tm * timeinfo;
      time (&rawtime);
      timeinfo = localtime(&rawtime);

      strftime(timestamp,size,"%Y-%m-%d_%H-%M-%S",timeinfo);
    }

    bool IsDirectoryExists( string path )
    {
        struct stat st = {0};

        return ! (stat(path.c_str(), &st) == -1);
    }
};

/* Application entry */
int SGX_CDECL main(int argc, char *argv[])
{
    (void)(argc);
    (void)(argv);

    HotCallsTester hotCallsTester;
    hotCallsTester.Run();

    return 0;
}

