#pragma once
#include "win32-main.h"
struct JobQueueJob
{
	bool taken;
	bool completed;
	// Each time a job is posted to this memory location, the salt is increased
	//	by one, giving us a fairly reasonable way of checking to see if this job 
	//	is the same as what it was when a JobQueueTicket was generated for us.
	u16 salt;
	fnSig_jobQueueFunction* function;
	void* data;
};
struct JobQueue
{
	/* @config: Setting a jobs array size >= 0xFFFF will require a bunch of code 
		changes elsewhere due to the size of JobQueueTicket! */
	JobQueueJob jobs[16];
	size_t nextJobIndex;
	size_t availableJobCount;
	size_t incompleteJobCount;
	CRITICAL_SECTION lock;
	HANDLE hSemaphore;
};
internal bool jobQueueInit(JobQueue* jobQueue);
/** 
 * @return Invalid JobQueueTicket if the job could not be added to the queue 
 *         (queue too full), or if `function` is nullptr.
 * */
internal JobQueueTicket jobQueuePostJob(JobQueue* jobQueue, 
                                        fnSig_jobQueueFunction* function, 
                                        void* data);
/** If the *ticket is found to be invalid, it is set to be a specific internal
 * 	value that is ALWAYS invalid to prevent it from ever becoming valid again.
 */
internal bool jobQueueTicketIsValid(JobQueue* jobQueue, JobQueueTicket* ticket);
/** If this function returns true, the ticket is automatically invalidated. 
 * @return true only if the ticket is valid AND the job is indeed done.
*/
internal bool jobQueueJobIsDone(JobQueue* jobQueue, JobQueueTicket* ticket);
internal bool jobQueueHasIncompleteJobs(JobQueue* jobQueue);
internal void jobQueueWaitForWork(JobQueue* jobQueue);
/** @return true if work was performed */
internal bool jobQueuePerformWork(JobQueue* jobQueue, u32 threadId);