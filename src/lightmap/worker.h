
#pragma once

#include <functional>

class Worker
{
public:
	static void RunJob(int count, std::function<void(int i)> callback);
};
