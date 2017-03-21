#ifndef __FAST_SGX_CALLS_H
#define __FAST_SGX_CALLS_H


// #include <stdlib.h>
#include <sgx_spinlock.h>
#include <stdbool.h>
// #include "utils.h"


#pragma GCC diagnostic ignored "-Wunused-function"

typedef unsigned long int pthread_t;

typedef struct {
    pthread_t       responderThread;
    sgx_spinlock_t  spinlock;
    void*           data;
    uint16_t        callID;
    bool            keepPolling;
    bool            runFunction;
    bool            isDone;
    bool            busy;
} HotCall;

typedef struct 
{
    uint16_t numEntries;
    void (**callbacks)(void*);
} HotCallTable;

#define HOTCALL_INITIALIZER  {0, SGX_SPINLOCK_INITIALIZER, NULL, 0, true, false, false, false }

static void HotCall_init( HotCall* hotCall )
{
    hotCall->responderThread    = 0;
    hotCall->spinlock           = SGX_SPINLOCK_INITIALIZER;
    hotCall->data               = NULL; 
    hotCall->callID             = 0;
    hotCall->keepPolling        = true;
    hotCall->runFunction        = false;
    hotCall->isDone             = false;
    hotCall->busy               = false;
}

static inline void _mm_pause(void) __attribute__((always_inline));
static inline void _mm_pause(void)
{
    __asm __volatile(
        "pause"
    );
}


static inline int HotCall_requestCall( HotCall* hotCall, uint16_t callID, void *data )
{
    int i = 0;
    const uint32_t MAX_RETRIES = 10;
    uint32_t numRetries = 0;
    //REquest call
    while( true ) {
        sgx_spin_lock( &hotCall->spinlock );
        if( hotCall->busy == false ) {
            hotCall->busy        = true;
            hotCall->isDone      = false;
            hotCall->runFunction = true;
            hotCall->callID      = callID;
            hotCall->data        = data;
            sgx_spin_unlock( &hotCall->spinlock );
            break;
        }
        //else:
        sgx_spin_unlock( &hotCall->spinlock );

        numRetries++;
        if( numRetries > MAX_RETRIES )
            return -1;

        for( i = 0; i<3; ++i)
            _mm_pause();
    }

    //wait for answer
    while( true )
    {
        sgx_spin_lock( &hotCall->spinlock );
        if( hotCall->isDone == true ){
            hotCall->busy = false;
            sgx_spin_unlock( &hotCall->spinlock );
            break;
        }

        sgx_spin_unlock( &hotCall->spinlock );
        for( i = 0; i<3; ++i)
            _mm_pause();
    }

    return numRetries;
}

static inline void HotCall_waitForCall( HotCall *hotCall, HotCallTable* callTable )  __attribute__((always_inline));
static inline void HotCall_waitForCall( HotCall *hotCall, HotCallTable* callTable ) 
{
    static int i;
    // volatile void *data;
    while( true )
    {
        sgx_spin_lock( &hotCall->spinlock );
        if( hotCall->keepPolling != true ) {
            sgx_spin_unlock( &hotCall->spinlock );
            break;
        }

        if( hotCall->runFunction )
        {
            volatile uint16_t callID = hotCall->callID;
            void *data = hotCall->data;
            sgx_spin_unlock( &hotCall->spinlock );
            if( callID < callTable->numEntries ) {
                // printf( "Calling callback %d\n", callID );
                callTable->callbacks[ callID ]( data );
            }
            else {
                // printf( "Invalid callID\n" );
                // exit(42);
            }
            // DoWork( hotCall->data );
            // data = (int*)hotCall->data;
            // printf( "Enclave: Data is at %p\n", data );
            // *data += 1;
            sgx_spin_lock( &hotCall->spinlock );
            hotCall->isDone      = true;
            hotCall->runFunction = false;
        }
        
        sgx_spin_unlock( &hotCall->spinlock );
        for( i = 0; i<3; ++i)
            _mm_pause();
        
        // _mm_pause();
        //     _mm_pause();
        // _mm_pause();
    }

}
static inline void StopResponder( HotCall *hotCall );
static inline void StopResponder( HotCall *hotCall )
{
    sgx_spin_lock( &hotCall->spinlock );
    hotCall->keepPolling = false;
    sgx_spin_unlock( &hotCall->spinlock );
}



#endif