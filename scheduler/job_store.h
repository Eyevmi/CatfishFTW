#ifndef JOB_STORE_H
#define JOB_STORE_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define MAX_JOBS 1024
#define MAX_WORKERS 64

// Error codes
#define ERR_NOT_FOUND -1
#define ERR_INVALID_STATE -2
#define ERR_FULL -3
#define ERR_NO_WORKERS -4

typedef enum { JOB_QUEUED, JOB_RUNNING, JOB_DONE, JOB_CANCELLED, JOB_KILLED } job_state_t;

typedef struct {
    uint64_t seq_num;  // 1, 2, 3... (submission order)
    char* command;     // Heap-allocated copy via strdup()
    int timeout_sec;
    int base_priority;  // 0..9
    time_t submitted_at;

    job_state_t state;
    int exit_code;      // Meaningful when state == JOB_DONE
    char detail[ 64 ];  // e.g., "DONE 0", "KILLED TIMEOUT", "CANCELLED"

    int assigned_worker_id;  // -1 when not running
    bool cancel_pending;
} job_t;

typedef struct {
    int worker_id;
    int socket_fd;
    int max_slots;     // K
    int active_slots;  // 0 <= active_slots <= max_slots
    bool connected;
} worker_t;

typedef struct {
    pthread_mutex_t lock;
    job_t jobs[ MAX_JOBS ];
    size_t job_count;
    uint64_t next_seq_num;

    worker_t workers[ MAX_WORKERS ];
    size_t worker_count;
} job_store_t;

// Lifecycle
int job_store_init ( job_store_t* store );
void job_store_destroy ( job_store_t* store );

// Worker management
int worker_register ( job_store_t* store, int socket_fd, int max_slots, int* out_worker_id );
int worker_disconnect ( job_store_t* store, int worker_id );

// Core job operations
int job_submit ( job_store_t* store, const char* cmd, int timeout_sec, int priority, uint64_t* out_seq_num );
int job_get_status ( job_store_t* store, uint64_t seq_num, job_state_t* out_state, char* out_detail, size_t detail_sz );
int job_cancel ( job_store_t* store, uint64_t seq_num, bool* out_needs_worker_kill, int* out_worker_fd );

// Scheduling & completion
int job_dispatch_next ( job_store_t* store, job_t* out_job_copy, int* out_worker_fd );
int job_complete ( job_store_t* store, uint64_t seq_num, int worker_id, job_state_t final_state, int exit_code,
                   const char* detail );

#endif