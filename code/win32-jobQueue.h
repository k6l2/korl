#pragma once
#include "win32-main.h"
#define JOB_QUEUE_FUNCTION(name) void name(void* data, u32 threadId)
typedef JOB_QUEUE_FUNCTION(fnSig_jobQueueFunction);
struct JobQueueJob
{
	bool taken;
	fnSig_jobQueueFunction* function;
	void* data;
};
struct JobQueue
{
	JobQueueJob jobs[16];
	size_t nextJobIndex;
	size_t availableJobCount;
	size_t incompleteJobCount;
	CRITICAL_SECTION lock;
	HANDLE hSemaphore;
};
internal bool jobQueueInit(JobQueue* jobQueue);
/** @return false if the job could not be added to the queue (queue too full) */
internal bool jobQueuePostJob(JobQueue* jobQueue, 
                              fnSig_jobQueueFunction* function, void* data);
internal bool jobQueueHasIncompleteJobs(JobQueue* jobQueue);
internal void jobQueueWaitForWork(JobQueue* jobQueue);
/** @return true if work was performed */
internal bool jobQueuePerformWork(JobQueue* jobQueue, u32 threadId);