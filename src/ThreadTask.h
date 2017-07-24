#pragma once
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <memory>
#include <regex>
#include <sstream>
#include <iostream>

// Regex strings to try and match with each line
static std::vector<std::pair<size_t, std::string>> GetRegexPatterns()
{
	return 
	{
		std::pair<size_t, std::string>{0, R"(Server: Object info [0-9]+:[0-9]+ not found\.)"											},
		std::pair<size_t, std::string>{0, R"(Can't change owner from [0-9]+ to [0-9]+)"													},
		std::pair<size_t, std::string>{0, R"(Server\: Object [0-9]+\:[0-9]+ not found \(message Type_[0-9]+\))"							},
		std::pair<size_t, std::string>{0, R"(Road not found)"																			},
		std::pair<size_t, std::string>{0, R"(No speaker given for [a-zA-Z]+ [a-zA-Z]+)"													},
		std::pair<size_t, std::string>{0, R"(soldier\[[a-zA-Z0-9_]+\]:Some of magazines weren't stored in soldier Vest or Uniform\?)"	},
		std::pair<size_t, std::string>{0, R"(Cannot create non-ai vehicle [a-zA-Z_0-9]+,)"												},
		std::pair<size_t, std::string>{0, R"(Out of path-planning region for R Charlie 1-3:1 at 7826.6,9809.4, node type Road)"			},
		std::pair<size_t, std::string>{0, R"(Protocol bin\\config\.bin\/RadioProtocolCZ\/: Missing word [a-zA-Z0-9_]+('[a-zA-Z_ ]+)*)"	},
		std::pair<size_t, std::string>{0, R"(Error: Object\([0-9]+ : [0-9]+\) not found)"												},
		std::pair<size_t, std::string>{0, R"(Protocol bin\\config\.bin\/RadioProtocolGRE\/: Missing word [a-zA-Z0-9_]+('[a-zA-Z_ ]+)*)"	},
		std::pair<size_t, std::string>{0, R"(Server: Network message [a-z0-9]+ is pending)"												},
		std::pair<size_t, std::string>{0, R"(Client: Remote object [0-9]+:[0-9]+ not found)"											},
		std::pair<size_t, std::string>{0, R"(Client: Object [0-9]+:[0-9]+ \(type [a-zA-Z_0-9]+\) not found.)"							}
	};
}

enum ETaskResult : uint32_t
{
	ETaskResult_Error,
	ETaskResult_Done,
	ETaskResult_Wait,
	ETaskResult_NotExist
};

typedef std::vector<std::pair<std::string, size_t>> T_DataOut;
struct ThreadLockedData
{
	std::string dataIn{ "" };
	T_DataOut dataOut{};
	std::vector<std::string> dataUnmatched{};

	// -1 = data in (begin); 0 = error; 1 = data out (finished)
	std::atomic<int> state{ -1 };
	ThreadLockedData(const std::string& in, const T_DataOut& out, const int s) :
		dataIn(in),
		dataOut(out),
		dataUnmatched({}),
		state(s)
	{ };
};

class ThreadTask
{
	// TODO: Add progress, deduce from bytes consumed from data

	const size_t m_taskId;
	std::shared_ptr<ThreadLockedData> m_dataPacket;

public:
	ThreadTask(size_t taskId, std::shared_ptr<ThreadLockedData> data) :
		m_taskId(taskId),
		m_dataPacket(data)
	{};
	~ThreadTask() {};

	void Run()
	{
		// Get patterns
		auto pats = GetRegexPatterns();

		// Get stringstream
		std::stringstream stream(m_dataPacket.get()->dataIn);

		// Loop each line from dataIn
		std::string line;
		while (std::getline(stream, line))
		{
			bool matched = false;
			// loop each pattern
			for (auto& it = pats.begin(); it != pats.end();)
			{
				auto match = std::regex_search(line, std::regex(it->second));
				if (match)
				{
					matched = true;
					++it->first;
					break;
				}
				++it;
			}

			if (!matched)
				m_dataPacket.get()->dataUnmatched.push_back(line + "\n");
		}

		// Add data to dataOut pakcket
		for (auto& pat : pats)
		{
			m_dataPacket.get()->dataOut.emplace_back( pat.second, pat.first );
		}

		// We're done, set state
		m_dataPacket.get()->state = 1;
	}

	size_t TaskId()
	{
		return m_taskId;
	}

	T_DataOut DataOut()
	{
		return m_dataPacket.get()->dataOut;
	}

	std::vector<std::string> UnmatchedOut()
	{
		return m_dataPacket.get()->dataUnmatched;
	}

	int GetState()
	{
		return m_dataPacket.get()->state;
	}
};