#define _POSIX_C_SOURCE 200809L
#include "job_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int job_store_init ( job_store_t* store ) {
    if ( store == NULL ) {
        return -1;
    }

    memset ( store, 0, sizeof ( *store ) );
    store->next_seq_num = 1;

    if ( pthread_mutex_init ( &store->lock, NULL ) != 0 ) {
        return -1;
    }
    return 0;
}

int worker_register ( job_store_t* store, int socket_fd, int max_slots, int* out_worker_id ) {
    if ( store == NULL || out_worker_id == NULL || max_slots <= 0 ) {
        return ERR_INVALID_STATE;
    }

    pthread_mutex_lock ( &store->lock );

    if ( store->worker_count >= MAX_WORKERS ) {
        pthread_mutex_unlock ( &store->lock );
        return ERR_FULL;
    }

    worker_t* w = &store->workers[ store->worker_count ];
    w->worker_id = (int)store->worker_count + 1;
    w->socket_fd = socket_fd;
    w->max_slots = max_slots;
    w->active_slots = 0;
    w->connected = true;

    store->worker_count++;
    *out_worker_id = w->worker_id;

    pthread_mutex_unlock ( &store->lock );
    return 0;
}

int job_submit ( job_store_t* store, const char* cmd, int timeout_sec, int priority, uint64_t* out_seq_num ) {
    if ( store == NULL || cmd == NULL || out_seq_num == NULL || timeout_sec <= 0 || priority < 0 || priority > 9 ) {
        return ERR_INVALID_STATE;
    }

    pthread_mutex_lock ( &store->lock );

    if ( store->job_count >= MAX_JOBS ) {
        pthread_mutex_unlock ( &store->lock );
        return ERR_FULL;
    }

    job_t* j = &store->jobs[ store->job_count ];
    memset ( j, 0, sizeof ( *j ) );

    j->command = strdup ( cmd );
    if ( j->command == NULL ) {
        pthread_mutex_unlock ( &store->lock );
        return -1;
    }

    j->seq_num = store->next_seq_num++;
    j->timeout_sec = timeout_sec;
    j->base_priority = priority;
    j->submitted_at = time ( NULL );
    j->state = JOB_QUEUED;
    j->assigned_worker_id = -1;

    store->job_count++;
    *out_seq_num = j->seq_num;
    pthread_mutex_unlock ( &store->lock );
    return 0;
}

void job_store_destroy ( job_store_t* store ) {
    if ( store == NULL ) {
        return;
    }

    for ( size_t i = 0; i < store->job_count; i++ ) {
        free ( store->jobs[ i ].command );
    }

    pthread_mutex_destroy ( &store->lock );
}

int job_get_status ( job_store_t* store, uint64_t seq_num, job_state_t* out_state, char* out_detail,
                     size_t detail_sz ) {
    if ( store == NULL || out_state == NULL || out_detail == NULL || detail_sz == 0 ) {
        return ERR_INVALID_STATE;
    }

    pthread_mutex_lock ( &store->lock );

    for ( size_t i = 0; i < store->job_count; i++ ) {
        if ( store->jobs[ i ].seq_num == seq_num ) {
            *out_state = store->jobs[ i ].state;
            snprintf ( out_detail, detail_sz, "%s", store->jobs[ i ].detail );
            pthread_mutex_unlock ( &store->lock );
            return 0;
        }
    }
    pthread_mutex_unlock ( &store->lock );
    return ERR_NOT_FOUND;
}

int worker_disconnect ( job_store_t* store, int worker_id ) {
    if ( store == NULL || worker_id <= 0 ) {
        return ERR_INVALID_STATE;
    }

    pthread_mutex_lock ( &store->lock );

    for ( size_t i = 0; i < store->worker_count; i++ ) {
        if ( store->workers[ i ].worker_id == worker_id ) {
            store->workers[ i ].connected = false;
            store->workers[ i ].socket_fd = -1;
            pthread_mutex_unlock ( &store->lock );
            return 0;
        }
    }

    pthread_mutex_unlock ( &store->lock );
    return ERR_NOT_FOUND;
}

int job_cancel ( job_store_t* store, uint64_t seq_num, bool* out_needs_worker_kill, int* out_worker_fd ) {
    if ( store == NULL || out_needs_worker_kill == NULL || out_worker_fd == NULL ) {
        return ERR_INVALID_STATE;
    }

    *out_needs_worker_kill = false;
    *out_worker_fd = -1;

    pthread_mutex_lock ( &store->lock );

    for ( size_t i = 0; i < store->job_count; i++ ) {
        job_t* job = &store->jobs[ i ];
        if ( job->seq_num == seq_num ) {
            if ( job->state == JOB_QUEUED ) {
                job->state = JOB_CANCELLED;
                snprintf ( job->detail, sizeof ( job->detail ), "CANCELLED" );
                pthread_mutex_unlock ( &store->lock );
                return 0;
            } else if ( job->state == JOB_RUNNING ) {
                job->cancel_pending = true;
                *out_needs_worker_kill = true;
                for ( size_t w = 0; w < store->worker_count; w++ ) {
                    worker_t* worker = &store->workers[ w ];

                    if ( worker->worker_id == job->assigned_worker_id && worker->connected ) {
                        *out_worker_fd = worker->socket_fd;
                        break;
                    }
                }
                pthread_mutex_unlock ( &store->lock );
                return 0;
            } else {
                pthread_mutex_unlock ( &store->lock );
                return ERR_INVALID_STATE;
            }
        }
    }

    pthread_mutex_unlock ( &store->lock );
    return ERR_NOT_FOUND;
}

int job_dispatch_next ( job_store_t* store, job_t* out_job_copy, int* out_worker_fd ) {
    if ( store == NULL || out_job_copy == NULL || out_worker_fd == NULL ) {
        return ERR_INVALID_STATE;
    }

    pthread_mutex_lock ( &store->lock );

    worker_t* worker = NULL;

    for ( size_t i = 0; i < store->worker_count; i++ ) {
        worker_t* w = &store->workers[ i ];

        if ( w->connected && w->max_slots > w->active_slots ) {
            worker = w;
            break;
        }
    }

    if ( worker == NULL ) {
        pthread_mutex_unlock ( &store->lock );
        return ERR_NO_WORKERS;
    }

    job_t* job = NULL;

    for ( size_t i = 0; i < store->job_count; i++ ) {
        job_t* j = &store->jobs[ i ];

        if ( j->state == JOB_QUEUED && ( job == NULL || j->seq_num < job->seq_num ) ) {
            job = j;
        }
    }

    if ( job == NULL ) {
        pthread_mutex_unlock ( &store->lock );
        return ERR_NOT_FOUND;
    }

    *out_job_copy = *job;
    out_job_copy->command = strdup ( job->command );
    out_job_copy->state = JOB_RUNNING;
    out_job_copy->assigned_worker_id = worker->worker_id;
    *out_worker_fd = worker->socket_fd;

    if ( out_job_copy->command == NULL ) {
        pthread_mutex_unlock ( &store->lock );
        return -1;
    }

    job->state = JOB_RUNNING;
    job->assigned_worker_id = worker->worker_id;
    worker->active_slots++;

    pthread_mutex_unlock ( &store->lock );
    return 0;
}

int job_complete ( job_store_t* store, uint64_t seq_num, int worker_id, job_state_t final_state, int exit_code,
                   const char* detail ) {
    if ( store == NULL || detail == NULL ||
         ( final_state != JOB_DONE && final_state != JOB_CANCELLED && final_state != JOB_KILLED ) ) {
        return ERR_INVALID_STATE;
    }

    pthread_mutex_lock ( &store->lock );

    for ( size_t i = 0; i < store->job_count; i++ ) {
        job_t* j = &store->jobs[ i ];

        if ( j->seq_num == seq_num ) {
            if ( j->state == JOB_RUNNING && j->assigned_worker_id == worker_id ) {
                j->state = final_state;
                j->exit_code = exit_code;
                snprintf ( j->detail, sizeof ( j->detail ), "%s", detail );
                j->assigned_worker_id = -1;

                for ( size_t k = 0; k < store->worker_count; k++ ) {
                    worker_t* w = &store->workers[ k ];

                    if ( w->worker_id == worker_id ) {
                        if ( w->active_slots > 0 ) {
                            w->active_slots--;
                            break;
                        }
                    }
                }
                pthread_mutex_unlock ( &store->lock );
                return 0;
            }
            pthread_mutex_unlock ( &store->lock );
            return ERR_INVALID_STATE;
        }
    }

    pthread_mutex_unlock ( &store->lock );
    return ERR_NOT_FOUND;
}