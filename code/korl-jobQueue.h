#pragma once
#include "korl-globalDefines.h"
#include "utility/korl-pool.h"

//@TODO: move this stuff into korl-interface-platform.h, as we want this API exposed to KORL users
typedef Korl_Pool_Handle Korl_JobQueue_JobTicket;
#define KORL_JOB_QUEUE_FUNCTION(name) void name(void* data, u32 threadId)
typedef KORL_JOB_QUEUE_FUNCTION(korl_fnSig_jobQueueFunction);

/** Job_State will only ever increase, and once \c COMPLETED state is reached, 
 * the job can only be removed from the queue by the user via a ticket. */
typedef enum Korl_JobQueue_Job_State
{
    KORL_JOBQUEUE_JOB_STATE_POSTED,// the job has been added to the job queue, but no thread has accepted/taken this job yet
    KORL_JOBQUEUE_JOB_STATE_TAKEN,// this job has been taken by a thread, and work is currently being performed via the job's `function` callback
    KORL_JOBQUEUE_JOB_STATE_COMPLETED,// the job is complete; we expect the code which originally posted the job to check for completion status at some point in the future; if the job poster never checks for the job's completion status, this should be considered a bug
} Korl_JobQueue_Job_State;
typedef struct Korl_JobQueue_Job
{
    Korl_JobQueue_Job_State      state;
    korl_fnSig_jobQueueFunction* function;
    void*                        data;
} Korl_JobQueue_Job;
korl_internal void               korl_jobQueue_initialize(void);
korl_internal void               korl_jobQueue_waitForWork(void);
korl_internal Korl_JobQueue_Job* korl_jobQueue_takeJob(void);
korl_internal void               korl_jobQueue_job_markCompleted(Korl_JobQueue_Job* job);
#if 0//@TODO; recycle
/** 
 * @return the job ticket which uses `function`.  If `function` is nullptr, the 
 *         return value is guaranteed to be an INVALID ticket.
*/
#define PLATFORM_POST_JOB(name) JobQueueTicket name(fnSig_jobQueueFunction* function, void* data)
#define PLATFORM_JOB_VALID(name) bool name(JobQueueTicket* ticket)
#define PLATFORM_JOB_DONE(name) bool name(JobQueueTicket* ticket)

/** 
 * @return Invalid JobQueueTicket if the job could not be added to the queue 
 * (queue too full), or if `function` is nullptr. */
internal JobQueueTicket 
    jobQueuePostJob(
        JobQueue* jobQueue, fnSig_jobQueueFunction* function, void* data);
/** If the *ticket is found to be invalid, it is set to be a specific internal
 * value that is ALWAYS invalid to prevent it from ever becoming valid again. */
internal bool 
    jobQueueTicketIsValid(JobQueue* jobQueue, JobQueueTicket* ticket);
/** If this function returns true, the ticket is automatically invalidated. 
 * @return true only if the ticket is valid AND the job is indeed done. */
internal bool 
    jobQueueJobIsDone(JobQueue* jobQueue, JobQueueTicket* ticket);
internal bool 
    jobQueueHasIncompleteJobs(JobQueue* jobQueue);
#endif
