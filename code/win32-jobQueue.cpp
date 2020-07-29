#include "win32-jobQueue.h"
global_variable const u16 JOB_QUEUE_INVALID_JOB_ID = 0xFFFF;
global_variable const JobQueueTicket JOB_QUEUE_INVALID_TICKET = 
                                                       JOB_QUEUE_INVALID_JOB_ID;
internal bool jobQueueInit(JobQueue* jobQueue)
{
	*jobQueue = {};
	static_assert(CARRAY_SIZE(jobQueue->jobs) < JOB_QUEUE_INVALID_JOB_ID);
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
	{
		jobQueue->jobs[ji].completed = true;
	}
	return true;
}
internal void jobQueueDestroy(JobQueue* jobQueue)
{
	DeleteCriticalSection(&jobQueue->lock);
	CloseHandle(jobQueue->hSemaphore);
	jobQueue->hSemaphore = NULL;
}
internal JobQueueTicket jobQueuePrintTicket(JobQueue* jobQueue, 
                                            size_t jobIndex)
{
	kassert(jobIndex < CARRAY_SIZE(jobQueue->jobs));
	return (static_cast<u32>(jobQueue->jobs[jobIndex].salt)<<16) | 
		static_cast<u32>(jobIndex);
}
internal JobQueueTicket jobQueuePostJob(JobQueue* jobQueue, 
                                        fnSig_jobQueueFunction* function, 
                                        void* data)
{
	if(!function)
		return JOB_QUEUE_INVALID_TICKET;
	EnterCriticalSection(&jobQueue->lock);
	defer(LeaveCriticalSection(&jobQueue->lock));
	if(jobQueue->incompleteJobCount >= CARRAY_SIZE(jobQueue->jobs))
	/* if the # of incomplete jobs is the size of the array, that means the job 
		queue is full & is working on them */
	{
		return JOB_QUEUE_INVALID_TICKET;
	}
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
	kassert(newJobIndex < CARRAY_SIZE(jobQueue->jobs));
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
	{
		KLOG(ERROR, "Failed to release semaphore! getlasterror=%i", 
		     GetLastError());
	}
	return jobQueuePrintTicket(jobQueue, newJobIndex);
}
internal bool jobQueueTicketIsValid(JobQueue* jobQueue, JobQueueTicket* ticket)
{
	const size_t ticketJobId = (*ticket) & JOB_QUEUE_INVALID_JOB_ID;
	if(ticketJobId == JOB_QUEUE_INVALID_JOB_ID 
		|| ticketJobId >= CARRAY_SIZE(jobQueue->jobs))
	{
		*ticket = JOB_QUEUE_INVALID_TICKET;
		return false;
	}
	const u16 ticketSalt = (*ticket) >> 16;
	EnterCriticalSection(&jobQueue->lock);
	defer(LeaveCriticalSection(&jobQueue->lock));
	if(jobQueue->jobs[ticketJobId].salt == ticketSalt 
		&& !jobQueue->jobs[ticketJobId].completed)
	{
		return true;
	}
	*ticket = JOB_QUEUE_INVALID_TICKET;
	return false;
}
internal bool jobQueueJobIsDone(JobQueue* jobQueue, JobQueueTicket* ticket)
{
	const size_t ticketJobId = (*ticket) & JOB_QUEUE_INVALID_JOB_ID;
	if(ticketJobId == JOB_QUEUE_INVALID_JOB_ID 
		|| ticketJobId >= CARRAY_SIZE(jobQueue->jobs))
	{
		*ticket = JOB_QUEUE_INVALID_TICKET;
		return false;
	}
	const u16 ticketSalt = (*ticket) >> 16;
	EnterCriticalSection(&jobQueue->lock);
	defer(LeaveCriticalSection(&jobQueue->lock));
	if(jobQueue->jobs[ticketJobId].salt == ticketSalt)
	{
		if(jobQueue->jobs[ticketJobId].completed)
		{
			*ticket = JOB_QUEUE_INVALID_TICKET;
			return true;
		}
		else
		{
			return false;
		}
	}
	// If the job's salt is different from the ticket's salt we can pretty 
	//	safely assume that the job was completed since we check to see if the 
	//	ticket was the universal invalid ticket earlier in this function.
	// What most likely has happened is that enough time has passed that this 
	//	job slot was completed and replaced with another new job.
	*ticket = JOB_QUEUE_INVALID_TICKET;
	return true;
}
internal JobQueueJob* jobQueueTakeJob(JobQueue* jobQueue)
{
	if(TryEnterCriticalSection(&jobQueue->lock))
	{
		defer(LeaveCriticalSection(&jobQueue->lock));
		if(jobQueue->availableJobCount)
		{
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
		}
	}
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
	{
		KLOG(ERROR, "Wait failed! getlasterror=%i", GetLastError());
	}
}
internal void jobQueueMarkJobCompleted(JobQueue* jobQueue, JobQueueJob* job)
{
	EnterCriticalSection(&jobQueue->lock);
	defer(LeaveCriticalSection(&jobQueue->lock));
	job->taken     = false;
	job->completed = true;
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