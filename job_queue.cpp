
#include <mutex>
#include <intrin.h>
#include "memory\memory.h"

//
// NOTE: Job queue functions
//

inline job_queue JobQueueInit(linear_arena* Arena, u32 MaxNumJobs)
{
    job_queue Result = {};

    Result.MaxNumJobs = MaxNumJobs;
    // NOTE: Need to be aligned to 4 bytes https://docs.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-interlockeddecrement
    Result.NumJobsDone = (volatile u32*)PushSizeAligned(Arena, CACHE_LINE_SIZE, 4);
    Result.NumJobsDone[0] = 0;
    Result.CurrReadPtr = (volatile u32*)PushSizeAligned(Arena, CACHE_LINE_SIZE, 4);
    Result.CurrReadPtr[0] = 0;
    Result.CurrWritePtr = (volatile u32*)PushSizeAligned(Arena, CACHE_LINE_SIZE, 4);
    Result.CurrWritePtr[0] = 0;
    Result.Jobs = PushArrayAligned(Arena, job, Result.MaxNumJobs, CACHE_LINE_SIZE);

    return Result;
}

inline void JobQueueAddJob(job_queue* JobQueue, job* Job)
{
    Assert(JobQueue->NumJobsQueued < JobQueue->MaxNumJobs);
    
    u32 CurrWritePtr = *JobQueue->CurrWritePtr;
    JobQueue->NumJobsQueued += 1;

    MemoryBarrier();
    
    JobQueue->Jobs[CurrWritePtr] = *Job;
    u32 NewWritePtr = (CurrWritePtr + 1) % JobQueue->MaxNumJobs;

    MemoryBarrier();

    InterlockedExchange(JobQueue->CurrWritePtr, NewWritePtr);
}

inline b32 JobQueueTryExecuteJob(job_queue* JobQueue)
{
    u32 JobId = 0xFFFFFFFF;
    {
        u32 CurrReadPtr = *JobQueue->CurrReadPtr;
        MemoryBarrier();
        while (CurrReadPtr != *JobQueue->CurrWritePtr)
        {
            u32 NextReadPtr = (CurrReadPtr + 1) % JobQueue->MaxNumJobs;
            MemoryBarrier();
            u32 StoredReadPtr = InterlockedCompareExchange(JobQueue->CurrReadPtr, NextReadPtr, CurrReadPtr);
            MemoryBarrier();
            if (CurrReadPtr == StoredReadPtr)
            {
                // NOTE: We successfully exchanged the value, break
                JobId = CurrReadPtr;
                break;
            }
            else
            {
                MemoryBarrier();
                CurrReadPtr = *JobQueue->CurrReadPtr;
            }
        }
    }
    MemoryBarrier();

    b32 Result = JobId != 0xFFFFFFFF;
    if (Result)
    {
        // NOTE: In case our decrement actually got ThreadNumJobsLeft when it was at 0
        job Job = JobQueue->Jobs[JobId];
        job_header* Header = (job_header*)Job;
        Header->Execute(Job);
        MemoryBarrier();
        InterlockedIncrement(JobQueue->NumJobsDone);
    }

    return Result;
}

inline void JobQueueWaitUntilEmpty(job_queue* JobQueue)
{
    while (JobQueue->NumJobsDone[0] < JobQueue->NumJobsQueued)
    {
        JobQueueTryExecuteJob(JobQueue);
    }
    
    JobQueue->NumJobsQueued = 0;
    JobQueue->NumJobsDone[0] = 0;
}
