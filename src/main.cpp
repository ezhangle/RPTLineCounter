#include "ThreadManager.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <limits>

#include <experimental/filesystem>
namespace std_fs = std::experimental::filesystem;


int main()
{
	// Thread manager instance
	auto tm = std::make_unique<ThreadManager>();

	std::string filepath = "C:\\in.rpt";

	std::cout << "Press enter to begin parsing '" << filepath << "'." << std::endl;
	std::getchar();

	// Get data length
	std_fs::path p{ filepath };
	p = std_fs::canonical(p);
	uintmax_t fileSize = std_fs::file_size(p);

	if (fileSize > std::numeric_limits<std::size_t>::max())
	{
		std::cout << "ERROR! Max file size exceeded (" << std::dec << std::numeric_limits<std::size_t>::max() << " bytes)." << std::endl;
		return -1;
	}

	size_t fileLength = (size_t) fileSize;

	// Open RPT log file
	std::ifstream f_rptFile(filepath, std::ios::binary | std::ios::in);

	// Open Output log file
	std::ofstream outfile(filepath + ".txt", std::ios::out | std::ios::binary | std::ios::trunc);

	// exit on failure
	if (!outfile.is_open())
	{
		std::cout << "Failed to open (for writing) file '" << filepath + ".txt" << "'." << std::endl;
		return -1;
	};
	if (!f_rptFile.is_open())
	{
		std::cout << "Failed to open file '" << filepath << "'." << std::endl;
		return -1;
	}

	outfile << "RPT File: " << filepath << std::endl << std::endl;

	size_t numTasks = 1;

	// Calculate total needed tasks
	size_t max_chunk_length = 204800; // Per thread/task...
	while (fileLength / numTasks > max_chunk_length)
	{
		++numTasks; // keep going until we get a chunk size <= max chunk size
	}
	size_t targetChunkLength = fileLength / numTasks;

	std::cout << "Chunk Size: " << std::to_string(targetChunkLength) << std::endl;

	// Add extra task to clean up the remainder
	if (fileLength - (numTasks * targetChunkLength) > 0)
		++numTasks;

	std::cout << "Num Tasks : " << std::to_string(numTasks) << std::endl;

	size_t consumedNumBytes = 0;
	size_t remainingNumBytes = fileLength;

	// Task vector
	std::vector<size_t> tasks{};

	// Generate 'numTasks' tasks with the provided 'TaskIn' data, split only on newlines
	std::string taskData{};
	
	// TODO: Consume during loop if taskCount > number of cores?
	while (consumedNumBytes < fileLength) // each task
	{
		if (tasks.size() >= numTasks)
			break;

		targetChunkLength = fileLength / numTasks;

		// Make sure we dont over-consume
		remainingNumBytes = fileLength - consumedNumBytes;
		targetChunkLength = (remainingNumBytes < targetChunkLength) ? remainingNumBytes : targetChunkLength;

		// Clear and resize string
		taskData.clear();
		taskData.reserve(targetChunkLength);
		
		// Grab lines until we reach or exceed our target chunk length
		std::string line;
		while (std::getline(f_rptFile, line))
		{
			consumedNumBytes += line.length() + 1; // +1 for '\n'
			taskData.append(line);

			// Last line check
			if (consumedNumBytes > fileLength)
			{
				--consumedNumBytes;
			}
			else
			{
				// Add the newline char
				taskData.push_back('\n');
			}

			// Check if we reached our target
			if (taskData.length() >= targetChunkLength)
				break;
		}

		// Spawn task
		tasks.push_back(tm.get()->StartTask(taskData));
	}
	f_rptFile.close();

	numTasks = tasks.size();

	// TODO: Gather data asynchronously;
	//	
	//	1. Via callback to mutex locked data combiner in caller - main. (HandleFinishedThreadData(*data) ?)
	//	2. or via blocking continous task combing in thread manager. (GetData(*data) ?)

	std::vector<std::pair<uint64_t, T_DataOut>> taskDataOut;
	T_DataOut data{};
	std::vector<std::string> unmatchedTasks;
	unmatchedTasks.resize(tasks.size());
	std::vector<std::string> unmatchedLines;

	std::cout << "Tasks consumed:";

	// Simple task completion combing loop
	while (tasks.size() > 0)
	{
		for (auto it = tasks.begin(); it != tasks.end();)
		{
			auto result = tm.get()->ConsumeTask(*it, &data, &unmatchedLines);

			if (result == ETaskResult_Wait)
			{
				it++;
			}
			else if (result == ETaskResult_Done)
			{
				std::cout << " #" << std::to_string(*it);

				std::string um;
				for (auto & line : unmatchedLines)
				{
					um.append(line);
				}
				unmatchedTasks[(*it)-1] = um;

				taskDataOut.emplace_back(*it, data);
				it = tasks.erase(it);
			}
			else if (result == ETaskResult_NotExist)
			{
				it = tasks.erase(it);
			}
			else if (result == ETaskResult_Error)
			{
				it = tasks.erase(it);
			}
			else
			{
				// error, corrupt data?
				it = tasks.erase(it);
				std::cout << "CRITICAL ERROR!" << std::endl;
				outfile << "CRITICAL ERROR!" << std::endl;
				std::getchar();
				return -1;
			}
		}

		std::this_thread::yield();
	}
	tasks.clear();

	std::cout << std::endl;

	T_DataOut total{};
	for (auto &TaskPair : taskDataOut)
	{
		for (auto &DataPair : TaskPair.second)
		{
			std::string reStr = DataPair.first;
			auto it_total = std::find_if(total.begin(), total.end(), [&reStr](auto other) { return reStr == other.first; });

			if (it_total == total.end())
			{
				total.push_back({ DataPair });
			}
			else
			{
				it_total->second += DataPair.second;
			}
		}
	}

	std::cout << "Consumed all tasks." << std::endl
		<< std::endl;
	std::cout << "Found the following counts: " << std::endl << std::endl;

	for (auto t : total)
	{
		std::cout << std::to_string(t.second) << "   matches with    " << t.first << std::endl;
		outfile << std::to_string(t.second) << "   matches with    " << t.first << std::endl;
	}

	outfile << std::endl;

	// Write unmatched lines
	std::string _buf = "";
	size_t bufMax = 10240;

	for (size_t i = 0; i < numTasks; i++)
	{
		_buf.append(unmatchedTasks[i]);
		if (_buf.length() > bufMax)
		{
			outfile.write(_buf.c_str(), _buf.length());
			_buf.clear();
		}
	}

	if (_buf.length() > 0)
		outfile.write(_buf.c_str(), _buf.length());

	std::cout << std::endl << "Output: " << filepath << ".txt";

	outfile.close();
	std::getchar();

    return 0;
}

