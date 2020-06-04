#include "win32-jobQueue.h"
internal bool jobQueueInit(JobQueue* jobQueue)
{
	InitializeCriticalSection(&jobQueue->lock);
	jobQueue->hSemaphore = 
		CreateSemaphore(nullptr, 0, CARRAY_COUNT(jobQueue->jobs), nullptr);
	if(!jobQueue->hSemaphore)
	{
		KLOG(ERROR, "Failed to create job queue semaphore! getlasterror=%i",
		     GetLastError());
		return false;
	}
	return true;
}
internal void jobQueueDestroy(JobQueue* jobQueue)
{
	DeleteCriticalSection(&jobQueue->lock);
	CloseHandle(jobQueue->hSemaphore);
	jobQueue->hSemaphore = NULL;
}
internal bool jobQueuePostJob(JobQueue* jobQueue, 
                              fnSig_jobQueueFunction* function, void* data)
{
	EnterCriticalSection(&jobQueue->lock);
	defer(LeaveCriticalSection(&jobQueue->lock));
	if(jobQueue->availableJobCount >= CARRAY_COUNT(jobQueue->jobs))
	{
		return false;
	}
	const size_t newJobIndex = 
		(jobQueue->nextJobIndex + jobQueue->availableJobCount) % 
		CARRAY_COUNT(jobQueue->jobs);
	if(jobQueue->jobs[newJobIndex].taken)
	{
		return false;
	}
	jobQueue->jobs[newJobIndex].function = function;
	jobQueue->jobs[newJobIndex].data     = data;
	jobQueue->availableJobCount++;
	jobQueue->incompleteJobCount++;
	if(!ReleaseSemaphore(jobQueue->hSemaphore, 1, nullptr))
	{
		KLOG(ERROR, "Failed to release semaphore! getlasterror=%i", 
		     GetLastError());
	}
	return true;
}
internal JobQueueJob* jobQueueTakeJob(JobQueue* jobQueue)
{
	if(TryEnterCriticalSection(&jobQueue->lock))
	{
		defer(LeaveCriticalSection(&jobQueue->lock));
		if(jobQueue->availableJobCount)
		{
			JobQueueJob* job = &jobQueue->jobs[jobQueue->nextJobIndex];
			jobQueue->nextJobIndex = 
				(jobQueue->nextJobIndex + 1) % CARRAY_COUNT(jobQueue->jobs);
			jobQueue->availableJobCount--;
			job->taken = true;
			return job;
		}
	}
	return nullptr;
}
internal bool jobQueueHasIncompleteJobs(JobQueue* jobQueue)
{
	return jobQueue->incompleteJobCount;
}
internal void jobQueueWaitForWork(JobQueue* jobQueue)
{
	const DWORD waitResult = 
		WaitForSingleObject(jobQueue->hSemaphore, INFINITE);
	if(waitResult == WAIT_FAILED)
	{
		KLOG(ERROR, "Wait failed! getlasterror=%i", GetLastError());
	}
}
internal void jobQueueMarkJobCompleted(JobQueue* jobQueue, JobQueueJob* job)
{
	EnterCriticalSection(&jobQueue->lock);
	defer(LeaveCriticalSection(&jobQueue->lock));
	job->taken = false;
	kassert(jobQueue->incompleteJobCount > 0);
	jobQueue->incompleteJobCount--;
}
internal bool jobQueuePerformWork(JobQueue* jobQueue, u32 threadId)
{
	JobQueueJob*const job = jobQueueTakeJob(jobQueue);
	if(!job)
	{
		return false;
	}
	job->function(job->data, threadId);
	jobQueueMarkJobCompleted(jobQueue, job);
	return true;
}