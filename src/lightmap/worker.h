
#pragma once

#include <functional>

class kexWorker
{
public:
	static void RunJob(int count, std::function<void(int i)> callback);
};
