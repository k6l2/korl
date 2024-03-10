#include "korl-jobQueue.h"
#include "korl-windows-globalDefines.h"
#include "korl-memory.h"
#include "korl-interface-platform.h"
#include "utility/korl-pool.h"
#include "utility/korl-checkCast.h"
/** Job_State will only ever increase, and once \c COMPLETED state is reached, 
 * the job can only be removed from the queue by the user via a ticket. */
typedef enum _Korl_JobQueue_Job_State
{
    _KORL_JOBQUEUE_JOB_STATE_POSTED,// the job has been added to the job queue, but no thread has accepted/taken this job yet
    _KORL_JOBQUEUE_JOB_STATE_TAKEN,// this job has been taken by a thread, and work is currently being performed via the job's `function` callback
    _KORL_JOBQUEUE_JOB_STATE_COMPLETED,// the job is complete; we expect the code which originally posted the job to check for completion status at some point in the future; if the job poster never checks for the job's completion status, this should be considered a bug
} _Korl_JobQueue_Job_State;
typedef struct _Korl_JobQueue_Job
{
    _Korl_JobQueue_Job_State     state;
    korl_fnSig_jobQueueFunction* function;
    void*                        data;
} _Korl_JobQueue_Job;
typedef struct _Korl_JobQueue_WorkerThreadContext
{
    bool exit;// we should be able to gracefully end a worker thread by raising this flag
} _Korl_JobQueue_WorkerThreadContext;
typedef struct _Korl_JobQueue_Context
{
    Korl_Memory_AllocatorHandle         allocator;
    CRITICAL_SECTION                    lock;// locker for access to `allocator`
    HANDLE                              semaphore;// sync primitive for access to `jobPool`
    Korl_Pool                           jobPool;
    u16                                 workerThreadCount;
    _Korl_JobQueue_WorkerThreadContext* workerThreadContexts;
} _Korl_JobQueue_Context;
korl_global_variable _Korl_JobQueue_Context _korl_jobQueue_context;
#if 0//@TODO; recycle
struct JobQueue
{
    /* @robustness: consider changing the way jobs are posted & returned to the 
        pool.  Currently, JobQueueTickets are composed of a job index & a 
        rolling salt value.  This allows the JobQueue to reuse the same job 
        index after the job is completed with reasonable certainty that the 
        program's behavior will not break, since the JobQueue would have to 
        issue a ton of jobs to roll the salt of that slot all the way back to 
        the original value by the time the caller wants to redeem the completed 
        ticket.  Wouldn't it be a lot simpler to just have each job be an 
        allocation which remains unusable until the job ticket is redeemed by 
        the caller (pool allocator)? */
    /* @config: Setting a jobs array size >= 0xFFFF will require a bunch of code 
        changes elsewhere due to the size of JobQueueTicket! */
    JobQueueJob      jobs[256];
    size_t           nextJobIndex;
    size_t           availableJobCount;
    size_t           incompleteJobCount;
    CRITICAL_SECTION lock;
    HANDLE           hSemaphore;
};
struct W32ThreadInfo
{
    bool running;
    u32 index;
    JobQueue* jobQueue;
};
#endif
korl_internal DWORD WINAPI _korl_jobQueue_workerThread(_In_ LPVOID lpParameter)
{
    _Korl_JobQueue_Context*const             context             = &_korl_jobQueue_context;
    _Korl_JobQueue_WorkerThreadContext*const workerThreadContext = KORL_C_CAST(_Korl_JobQueue_WorkerThreadContext*, lpParameter);
    const u16                                workerThreadIndex   = korl_checkCast_i$_to_u16(workerThreadContext - context->workerThreadContexts);
    korl_log(INFO, "korl-jobqueue worker thread %hu started!", workerThreadIndex);
    /* set the thread name to make it easier to debug */
    WCHAR threadNameBuffer[32];
    KORL_WINDOWS_CHECK_HRESULT(StringCchPrintfW(threadNameBuffer, CARRAY_SIZE(threadNameBuffer), L"KORL-Windows-Worker-%hu", workerThreadIndex));
    KORL_WINDOWS_CHECK_HRESULT(SetThreadDescription(GetCurrentThread(), threadNameBuffer));
    /* enter work loop */
    while(!workerThreadContext->exit)
    {
        #if 0// @TODO; recycle
        jobQueueWaitForWork(workerThreadContext->jobQueue);
        JobQueueJob*const job = jobQueueTakeJob(workerThreadContext->jobQueue);
        if(job)
        {
            job->function(job->data, workerThreadContext->index);
            jobQueueMarkJobCompleted(workerThreadContext->jobQueue, job);
        }
        #endif
    }
    return 0;
}
korl_internal void korl_jobQueue_initialize(void)
{
    /* create a memory allocator for korl-jobqueue first, since we are going to 
        need it during some initialization processes */
    Korl_Memory_AllocatorHandle allocator;
    {
        KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
        heapCreateInfo.initialHeapBytes = korl_math_kilobytes(8);
        allocator = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_CRT, L"korl-jobQueue", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
    }
    /* before initializing the jobqueue context, we query the system for 
        multi-threading info, such as logical core count */
    LONG logicalCoreCount = 0;
    {
        DWORD length = 0;
        korl_assert(!GetLogicalProcessorInformation(NULL, &length));
        KORL_WINDOWS_CHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION logicalProcessorInfo = KORL_C_CAST(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, korl_allocateDirty(allocator, length));
        KORL_WINDOWS_CHECK(GetLogicalProcessorInformation(logicalProcessorInfo, &length));
        const PSYSTEM_LOGICAL_PROCESSOR_INFORMATION logicalProcessorInfoEnd = KORL_C_CAST(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, KORL_C_CAST(u8*, logicalProcessorInfo) + length);
        for(; logicalProcessorInfo < logicalProcessorInfoEnd; logicalProcessorInfo++)
        {
            switch(logicalProcessorInfo->Relationship)
            {
            case RelationProcessorCore:// the # of logical cores is represented by the # of set bits in ProcessorMask
                logicalCoreCount += korl_checkCast_u$_to_u32(PopulationCount64(logicalProcessorInfo->ProcessorMask));
                break;
            default:
                break;
            }
        }
    }
    korl_assert(logicalCoreCount > 0);// just don't support systems which we can't determine a logical core count for now
    /* initialize the jobqueue context */
    _Korl_JobQueue_Context*const context = &_korl_jobQueue_context;
    InitializeCriticalSection(&context->lock);
    context->allocator = allocator;
    context->semaphore = CreateSemaphore(NULL/*securityAttribs*/, 0/*initialCount*/, logicalCoreCount, NULL/*name*/);
    KORL_WINDOWS_CHECK(context->semaphore);
    korl_pool_initialize(&context->jobPool, allocator, sizeof(_Korl_JobQueue_Job), 1024);
    context->workerThreadCount = korl_checkCast_u$_to_u16(logicalCoreCount) - 1;
    if(context->workerThreadCount)
        context->workerThreadContexts = KORL_C_CAST(_Korl_JobQueue_WorkerThreadContext*, korl_allocate(allocator, context->workerThreadCount * sizeof(*context->workerThreadContexts)));
    /* spawn job queue worker threads */
    for(u16 i = 0; i < context->workerThreadCount; i++)
    {
        DWORD threadId;
        const HANDLE hThread = CreateThread(NULL/*securityAttribs*/, 0/*stackSize; 0=>default*/
                                           ,_korl_jobQueue_workerThread
                                           ,context->workerThreadContexts + i
                                           ,0/*creationFlags*/, &threadId);
        KORL_WINDOWS_CHECK(hThread);
        KORL_WINDOWS_CHECK(CloseHandle(hThread));// we no longer need the HANDLE to the thread, so we can close it right now (this will not terminate the thread)
    }
}
#if 0//@TODO; recycle
internal JobQueueTicket jobQueuePrintTicket(JobQueue* jobQueue, size_t jobIndex)
{
    korlAssert(jobIndex < CARRAY_SIZE(jobQueue->jobs));
    return (static_cast<u32>(jobQueue->jobs[jobIndex].salt)<<16) | 
            static_cast<u32>(jobIndex + 1);
}
internal bool jobQueueParseTicket(
    JobQueueTicket* ticket, u16* o_salt, u16* o_jobIndex)
{
    u16 rawJobIndex = (*ticket) & 0xFFFF;
    if(!rawJobIndex)
    {
        *ticket = 0;
        return false;
    }
    *o_salt     = (*ticket) >> 16;
    *o_jobIndex = rawJobIndex - 1;
    return true;
}
internal bool jobQueueInit(JobQueue* jobQueue)
{
    *jobQueue = {};
    /* ensure that the job queue index + 1 can fit in the # of bits required to 
        fit the index into a JobQueueTicket */
    static_assert(CARRAY_SIZE(jobQueue->jobs) < 0xFFFF);
    InitializeCriticalSection(&jobQueue->lock);
    jobQueue->hSemaphore = 
        CreateSemaphore(nullptr, 0, CARRAY_SIZE(jobQueue->jobs), nullptr);
    if(!jobQueue->hSemaphore)
    {
        KLOG(ERROR, "Failed to create job queue semaphore! getlasterror=%i",
            GetLastError());
        return false;
    }
    for(size_t ji = 0; ji < CARRAY_SIZE(jobQueue->jobs); ji++)
        jobQueue->jobs[ji].completed = true;
    return true;
}
internal void jobQueueDestroy(JobQueue* jobQueue)
{
    DeleteCriticalSection(&jobQueue->lock);
    CloseHandle(jobQueue->hSemaphore);
    jobQueue->hSemaphore = NULL;
}
internal JobQueueTicket jobQueuePostJob(
    JobQueue* jobQueue, fnSig_jobQueueFunction* function, void* data)
{
    if(!function)
        return 0;
    EnterCriticalSection(&jobQueue->lock);
    defer(LeaveCriticalSection(&jobQueue->lock));
    if(jobQueue->incompleteJobCount >= CARRAY_SIZE(jobQueue->jobs))
        /* if the # of incomplete jobs is the size of the array, that means the 
            job queue is full & is working on them */
        return 0;
    /* iterate over all job slots and find the first one which has been 
        completed */
    size_t newJobIndex = CARRAY_SIZE(jobQueue->jobs);
    for(size_t ji = 0; ji < CARRAY_SIZE(jobQueue->jobs); ji++)
    {
        const size_t nextIndexCandidate = 
            (jobQueue->nextJobIndex + ji) % CARRAY_SIZE(jobQueue->jobs);
        if(jobQueue->jobs[nextIndexCandidate].completed)
        {
            newJobIndex = nextIndexCandidate;
            break;
        }
    }
    korlAssert(newJobIndex < CARRAY_SIZE(jobQueue->jobs));
    if(jobQueue->incompleteJobCount == 0)
        jobQueue->nextJobIndex = newJobIndex;
    jobQueue->jobs[newJobIndex].taken     = false;
    jobQueue->jobs[newJobIndex].completed = false;
    jobQueue->jobs[newJobIndex].salt      += 1;
    jobQueue->jobs[newJobIndex].function  = function;
    jobQueue->jobs[newJobIndex].data      = data;
    jobQueue->availableJobCount++;
    jobQueue->incompleteJobCount++;
    if(!ReleaseSemaphore(jobQueue->hSemaphore, 1, nullptr))
        KLOG(ERROR, "Failed to release semaphore! getlasterror=%i", 
            GetLastError());
    return jobQueuePrintTicket(jobQueue, newJobIndex);
}
internal bool jobQueueTicketIsValid(JobQueue* jobQueue, JobQueueTicket* ticket)
{
    u16 ticketSalt, ticketJobId;
    if(!jobQueueParseTicket(ticket, &ticketSalt, &ticketJobId))
    {
        *ticket = 0;
        return false;
    }
    EnterCriticalSection(&jobQueue->lock);
    defer(LeaveCriticalSection(&jobQueue->lock));
    if(         jobQueue->jobs[ticketJobId].salt == ticketSalt 
            && !jobQueue->jobs[ticketJobId].completed)
        return true;
    *ticket = 0;
    return false;
}
internal bool jobQueueJobIsDone(JobQueue* jobQueue, JobQueueTicket* ticket)
{
    u16 ticketSalt, ticketJobId;
    if(!jobQueueParseTicket(ticket, &ticketSalt, &ticketJobId))
    {
        *ticket = 0;
        return false;
    }
    EnterCriticalSection(&jobQueue->lock);
    defer(LeaveCriticalSection(&jobQueue->lock));
    if(jobQueue->jobs[ticketJobId].salt == ticketSalt)
    {
        if(jobQueue->jobs[ticketJobId].completed)
        {
            *ticket = 0;
            return true;
        }
        else
            return false;
    }
    /* If the job's salt is different from the ticket's salt we can pretty 
        safely assume that the job was completed since we check to see if the 
        ticket was the universal invalid ticket earlier in this function.
     What most likely has happened is that enough time has passed that this 
        job slot was completed and replaced with another new job. */
    *ticket = 0;
    return true;
}
internal JobQueueJob* jobQueueTakeJob(JobQueue* jobQueue)
{
    EnterCriticalSection(&jobQueue->lock);
    defer(LeaveCriticalSection(&jobQueue->lock));
    if(!jobQueue->availableJobCount)
    {
        return nullptr;
    }
    for(size_t ji = 0; ji < CARRAY_SIZE(jobQueue->jobs); ji++)
    {
        const size_t jiNext = 
            (jobQueue->nextJobIndex + ji) % CARRAY_SIZE(jobQueue->jobs);
        JobQueueJob* job = &jobQueue->jobs[jiNext];
        if(job->completed || job->taken)
            continue;
        jobQueue->nextJobIndex = 
            (jiNext + 1) % CARRAY_SIZE(jobQueue->jobs);
        jobQueue->availableJobCount--;
        job->taken = true;
        return job;
    }
    KLOG(ERROR, "jobQueue available job count == %i, but no jobs "
        "found!", jobQueue->availableJobCount);
    return nullptr;
}
internal bool jobQueueHasIncompleteJobs(JobQueue* jobQueue)
{
    EnterCriticalSection(&jobQueue->lock);
    defer(LeaveCriticalSection(&jobQueue->lock));
    return jobQueue->incompleteJobCount;
}
internal void jobQueueWaitForWork(JobQueue* jobQueue)
{
    const DWORD waitResult = 
        WaitForSingleObject(jobQueue->hSemaphore, INFINITE);
    if(waitResult == WAIT_FAILED)
        KLOG(ERROR, "Wait failed! getlasterror=%i", GetLastError());
}
internal void jobQueueMarkJobCompleted(JobQueue* jobQueue, JobQueueJob* job)
{
    EnterCriticalSection(&jobQueue->lock);
    defer(LeaveCriticalSection(&jobQueue->lock));
    job->taken     = false;
    job->completed = true;
    korlAssert(jobQueue->incompleteJobCount > 0);
    jobQueue->incompleteJobCount--;
}
#endif
