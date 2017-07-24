#pragma once
#include <string>
#include <vector>
#include <thread>
#include <memory>

#include "ThreadTask.h"

class ThreadManager
{

	size_t m_taskNum;

	std::vector<std::pair<size_t,std::unique_ptr<std::thread>>> m_threads;
	std::vector<std::shared_ptr<ThreadTask>> m_tasks;

public:

	ThreadManager() :
		m_taskNum(0),
		m_threads(),
		m_tasks()
	{}
	~ThreadManager() {};

	size_t StartTask(const std::string& dataIn)
	{
		// New ThreadTask (holding data packet) + increment task num for task ID
		m_tasks.push_back(std::make_shared<ThreadTask>(++m_taskNum, std::make_shared<ThreadLockedData>(dataIn, T_DataOut{}, -1)));

		// New Thread (^ Using created task instance)
		m_threads.push_back({m_taskNum, std::make_unique<std::thread>(&ThreadTask::Run, m_tasks.back())});

		return m_taskNum;
	}

	inline void JoinThread(ThreadTask* task)
	{
		for (auto it_thread = m_threads.begin(); it_thread != m_threads.end();)
		{
			if (it_thread->first == task->TaskId())
			{
				// Join thread
				it_thread->second->join();

				// Erase thread instance
				it_thread = m_threads.erase(it_thread);
			}
			else
			{
				++it_thread;
			}
		}
	}

	ETaskResult ConsumeTask(size_t taskId, T_DataOut* data, std::vector<std::string>* unmatchedLines)
	{
		unmatchedLines->clear();
		data->clear();

		for (auto it_task = m_tasks.begin(); it_task != m_tasks.end();)
		{
			auto task = it_task->get();

			if (task->TaskId() == taskId)
			{
				// Not finished yet
				if (task->GetState() == -1)
				{
					return ETaskResult_Wait;
				}

				// Finished
				else if (task->GetState() == 1)
				{
					JoinThread(task);

					// Copy out data to data pointer
					*data = task->DataOut();

					// Copy out unmatched lines
					*unmatchedLines = task->UnmatchedOut();

					// Erase task instance
					it_task = m_tasks.erase(it_task);

					return ETaskResult_Done;
				}
				// Error
				else
				{
					JoinThread(task);

					// Erase task instance
					it_task = m_tasks.erase(it_task);

					return ETaskResult_Error;
				}
			}
			else
			{
				++it_task;
			}
		}
		return ETaskResult_NotExist;
	}
};