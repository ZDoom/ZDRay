
#include "worker.h"
#include "math/mathlib.h"
#include "lightmap.h"
#include <vector>
#include <thread>
#include <algorithm>

extern int NumThreads;

void Worker::RunJob(int count, std::function<void(int)> callback)
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

			for (int i = threadIndex; i < count; i += numThreads)
			{
				callback(i);
			}

		}));
	}

	for (int i = 0; i < numThreads; i++)
		threads[i].join();
}
