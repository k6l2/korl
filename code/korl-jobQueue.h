#pragma once
#include "korl-globalDefines.h"
#define KORL_JOB_QUEUE_FUNCTION(name) void name(void* data, u32 threadId)
typedef KORL_JOB_QUEUE_FUNCTION(korl_fnSig_jobQueueFunction);
korl_internal void korl_jobQueue_initialize(void);
#if 0//@TODO; recycle
using JobQueueTicket = u32;
/** 
 * @return the job ticket which uses `function`.  If `function` is nullptr, the 
 *         return value is guaranteed to be an INVALID ticket.
*/
#define PLATFORM_POST_JOB(name) JobQueueTicket name(fnSig_jobQueueFunction* function, void* data)
#define PLATFORM_JOB_VALID(name) bool name(JobQueueTicket* ticket)
#define PLATFORM_JOB_DONE(name) bool name(JobQueueTicket* ticket)





internal bool 
    jobQueueInit(JobQueue* jobQueue);
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
internal void 
    jobQueueWaitForWork(JobQueue* jobQueue);
internal JobQueueJob* 
    jobQueueTakeJob(JobQueue* jobQueue);
internal void 
    jobQueueMarkJobCompleted(JobQueue* jobQueue, JobQueueJob* job);
#endif
