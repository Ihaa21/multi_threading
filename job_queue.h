#pragma once

// TODO: Actually query this info
#define CACHE_LINE_SIZE 256

typedef void* job;

#define JOB_EXECUTE_TEMPLATE(name) void name(job Job)
typedef JOB_EXECUTE_TEMPLATE(job_execute_template);

struct job_header
{
    job_execute_template* Execute;
};

struct job_queue
{
    u32 MaxNumJobs;
    u32 NumJobsQueued;
    volatile u32* CurrReadPtr;
    volatile u32* CurrWritePtr;
    volatile u32* NumJobsDone;
    job* Jobs;
};

#include "job_queue.cpp"
