//-----------------------------------------------------------------------------
// Note: this is a modified version of dlight. It is not the original software.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2013-2014 Samuel Villarreal
// svkaiser@gmail.com
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
//    1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
//
//   2. Altered source versions must be plainly marked as such, and must not be
//   misrepresented as being the original software.
//
//    3. This notice may not be removed or altered from any source
//    distribution.
//

#pragma once

#include <thread>
#include <mutex>

// There's a race condition in the code that causes it to sometimes fail if using multiple threads
//#define MAX_THREADS     128
#define MAX_THREADS     1

class kexWorker;

typedef void(*jobFunc_t)(void*, int);

struct jobFuncArgs_t
{
    kexWorker *worker;
    void *data;
    int jobID;
};

class kexWorker
{
public:
    kexWorker();
    ~kexWorker();

    void                RunThreads(const int count, void *data, jobFunc_t jobFunc);
    void                LockMutex();
    void                UnlockMutex();
    void                Destroy();

    bool                FinishedAllJobs() { return jobsWorked == numWorkLoad; }
    int                 DispatchJob() { int j = jobsWorked; jobsWorked++; return j; }
    void                RunJob(void *data, const int jobID) { job(data, jobID); }

    jobFuncArgs_t       *Args(const int id) { return &jobArgs[id]; }
    const int           JobsWorked() const { return jobsWorked; }

    static int          maxWorkThreads;

private:
    std::thread         threads[MAX_THREADS];
    jobFuncArgs_t       jobArgs[MAX_THREADS];
    std::mutex          mutex;
    jobFunc_t           job;
    int                 jobsWorked;
    int                 numWorkLoad;
};
