#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "job_store.h"

int main ( void ) {
    job_store_t store;
    assert ( job_store_init ( &store ) == 0 );

    // Register 1 worker with 1 slot (socket fd = 10)
    int worker_id = 0;
    assert ( worker_register ( &store, 10, 1, &worker_id ) == 0 );

    // Submit two jobs
    uint64_t id1 = 0, id2 = 0;
    assert ( job_submit ( &store, "echo first", 5, 0, &id1 ) == 0 );
    assert ( job_submit ( &store, "echo second", 5, 0, &id2 ) == 0 );
    assert ( id1 == 1 && id2 == 2 );

    // First dispatch must pick oldest job (id1)
    job_t dispatched;
    int target_fd = -1;
    assert ( job_dispatch_next ( &store, &dispatched, &target_fd ) == 0 );
    assert ( dispatched.seq_num == id1 );
    assert ( strcmp ( dispatched.command, "echo first" ) == 0 );
    assert ( target_fd == 10 );
    free ( dispatched.command );

    // Worker's 1 slot is full -> second dispatch must report no free workers
    assert ( job_dispatch_next ( &store, &dispatched, &target_fd ) == ERR_NO_WORKERS );

    // Complete id1 -> slot frees up -> id2 can now dispatch
    assert ( job_complete ( &store, id1, worker_id, JOB_DONE, 0, "DONE 0" ) == 0 );
    assert ( job_dispatch_next ( &store, &dispatched, &target_fd ) == 0 );
    assert ( dispatched.seq_num == id2 );
    free ( dispatched.command );

    // Cancel id2 while RUNNING -> should request worker kill on fd 10
    bool needs_kill = false;
    int kill_fd = -1;
    assert ( job_cancel ( &store, id2, &needs_kill, &kill_fd ) == 0 );
    assert ( needs_kill == true && kill_fd == 10 );

    // Worker confirms cancellation -> status becomes JOB_CANCELLED
    assert ( job_complete ( &store, id2, worker_id, JOB_CANCELLED, -1, "CANCELLED" ) == 0 );
    job_state_t st;
    char detail[ 64 ];
    assert ( job_get_status ( &store, id2, &st, detail, sizeof ( detail ) ) == 0 );
    assert ( st == JOB_CANCELLED && strcmp ( detail, "CANCELLED" ) == 0 );

    job_store_destroy ( &store );
    printf ( "Milestone 1 passed!\n" );
    return 0;
}