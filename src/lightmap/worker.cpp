
#include "worker.h"
#include "common.h"
#include <vector>
#include <thread>
#include <algorithm>
#undef min
#undef max

extern int NumThreads;

thread_local kexVec3 *colorSamples;

void kexWorker::RunJob(int count, std::function<void(int)> callback)
{
	int numThreads = NumThreads;
	if (numThreads <= 0)
		numThreads = std::thread::hardware_concurrency();
	if (numThreads <= 0)
		numThreads = 4;

	numThreads = std::min(numThreads, count);

	std::vector<std::thread> threads;
	for (int threadIndex = 0; threadIndex < numThreads; threadIndex++)
	{
		threads.push_back(std::thread([=]() {

			colorSamples = new kexVec3[1024 * 1024];

			int start = threadIndex * count / numThreads;
			int end = std::min((threadIndex + 1) * count / numThreads, count);
			for (int i = start; i < end; i++)
			{
				callback(i);
			}

			delete[] colorSamples;

		}));
	}

	for (int i = 0; i < numThreads; i++)
		threads[i].join();
}
