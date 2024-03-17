#include "korl-jobQueue.h"
#include "korl-windows-globalDefines.h"
#include "korl-memory.h"
#include "korl-interface-platform.h"
#include "utility/korl-checkCast.h"
#include "utility/korl-utility-string.h"
typedef struct _Korl_JobQueue_WorkerThreadContext
{
    bool exit;// we should be able to gracefully end a worker thread by raising this flag
} _Korl_JobQueue_WorkerThreadContext;
typedef struct _Korl_JobQueue_Context
{
    Korl_Memory_AllocatorHandle         allocator;
    CRITICAL_SECTION                    lock;// locker for access to all members of this struct _except_ `semaphore` I think; this includes all memory occupied by individual jobs inside `jobPool`
    HANDLE                              semaphore;// sync primitive for access to `jobPool`
    Korl_Pool                           jobPool;// pool of Korl_JobQueue_Job objects
    u16                                 workerThreadCount;
    _Korl_JobQueue_WorkerThreadContext* workerThreadContexts;
    u32                                 incompleteJobCount;// the # of jobs in `jobPool` whose state is != `KORL_JOBQUEUE_JOB_STATE_COMPLETED`
    u32                                 postedJobCount;// the # of jobs in `jobPool` whose state is == `KORL_JOBQUEUE_JOB_STATE_POSTED`
} _Korl_JobQueue_Context;
korl_global_variable _Korl_JobQueue_Context _korl_jobQueue_context;
korl_internal DWORD WINAPI _korl_jobQueue_workerThread(_In_ LPVOID lpParameter)
{
    _Korl_JobQueue_Context*const             context             = &_korl_jobQueue_context;
    _Korl_JobQueue_WorkerThreadContext*const workerThreadContext = KORL_C_CAST(_Korl_JobQueue_WorkerThreadContext*, lpParameter);
    const u16                                workerThreadIndex   = korl_checkCast_i$_to_u16(workerThreadContext - context->workerThreadContexts);
    korl_log(INFO, "korl-jobqueue worker thread %hu started!", workerThreadIndex);
    /* set the thread name to make it easier to debug */
    WCHAR threadNameBuffer[32];
    korl_assert(0 < korl_string_formatBufferUtf16(threadNameBuffer, sizeof(threadNameBuffer), L"KORL-Windows-Worker-%hu", workerThreadIndex));
    KORL_WINDOWS_CHECK_HRESULT(SetThreadDescription(GetCurrentThread(), threadNameBuffer));
    /* enter work loop */
    while(!workerThreadContext->exit)
    {
        korl_jobQueue_waitForWork();
        Korl_JobQueue_Job*const job = korl_jobQueue_takeJob();
        if(job)
        {
            job->function(job->data, workerThreadIndex);
            korl_jobQueue_job_markCompleted(job);
        }
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
    korl_pool_initialize(&context->jobPool, allocator, sizeof(Korl_JobQueue_Job), 1024);
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
korl_internal void korl_jobQueue_shutDown(void)
{
    _Korl_JobQueue_Context*const context = &_korl_jobQueue_context;
    DeleteCriticalSection(&context->lock);
    CloseHandle(context->semaphore);
    korl_pool_destroy(&context->jobPool);
    korl_memory_zero(context, sizeof(context));
}
korl_internal void korl_jobQueue_waitForWork(void)
{
    _Korl_JobQueue_Context*const context = &_korl_jobQueue_context;
    const DWORD waitResult = WaitForSingleObject(context->semaphore, INFINITE);
    if(waitResult == WAIT_FAILED)
        korl_logLastError("Wait failed");
}
korl_internal KORL_POOL_CALLBACK_FOR_EACH(_korl_jobQueue_takeJob_poolForEach_getPostedJob)
{
    Korl_JobQueue_Job**const postedJobResult = KORL_C_CAST(Korl_JobQueue_Job**, userData);
    Korl_JobQueue_Job*const  job             = KORL_C_CAST(Korl_JobQueue_Job*, item);
    if(job->state == KORL_JOBQUEUE_JOB_STATE_POSTED)
    {
        *postedJobResult = job;
        return KORL_POOL_FOR_EACH_DONE;
    }
    return KORL_POOL_FOR_EACH_CONTINUE;
}
korl_internal Korl_JobQueue_Job* korl_jobQueue_takeJob(void)
{
    _Korl_JobQueue_Context*const context = &_korl_jobQueue_context;
    Korl_JobQueue_Job*           result  = NULL;
    EnterCriticalSection(&context->lock);
    if(!context->postedJobCount)
        goto return_result;// if there are no posted jobs, we can just return NULL
    korl_pool_forEach(&context->jobPool, _korl_jobQueue_takeJob_poolForEach_getPostedJob, &result);
    if(result)
    {
        result->state = KORL_JOBQUEUE_JOB_STATE_TAKEN;
        context->postedJobCount--;
    }
    else
        korl_log(ERROR, "postedJobCount == %u, but no jobs found!", context->postedJobCount);
    return_result:
        LeaveCriticalSection(&context->lock);
        return result;
}
korl_internal void korl_jobQueue_job_markCompleted(Korl_JobQueue_Job* job)
{
    _Korl_JobQueue_Context*const context = &_korl_jobQueue_context;
    EnterCriticalSection(&context->lock);
    korl_assert(job->state == KORL_JOBQUEUE_JOB_STATE_TAKEN);
    job->state = KORL_JOBQUEUE_JOB_STATE_COMPLETED;
    korl_assert(context->incompleteJobCount > 0);
    context->incompleteJobCount--;
    LeaveCriticalSection(&context->lock);
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
internal bool jobQueueHasIncompleteJobs(JobQueue* jobQueue)
{
    EnterCriticalSection(&jobQueue->lock);
    defer(LeaveCriticalSection(&jobQueue->lock));
    return jobQueue->incompleteJobCount;
}
#endif
