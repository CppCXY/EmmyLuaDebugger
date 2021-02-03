#pragma once

#include <string>
#include <map>
#include <type_traits>
#include <vector>

enum class CommandLineValueType
{
	Boolean = 0,
	Int = 1,
	String = 2,
};

struct CommandLineOption
{
	std::string Value;
	CommandLineValueType Type;
	bool RestOfAll = false;
};

class CommandLineException : std::exception
{
public:
	/**
	 * \param msg �쳣��Ϣ
	 * \remark vs��gcc���иù��еĲ�������
	 */
	CommandLineException(const char* msg)
		: std::exception(msg)
	{
	}
};

/**
 * \brief �ض��﷨�ṹ�������н�������
 */
class CommandLine
{
public:
	/**
	 * \brief ��ӽ������ַ�������
	 * \param restOfAll Ϊtrue ��ʾ�˲���֮������в�����ͬ���ɸò���������
	 */
	template <typename T>
	bool Add(const std::string& name, bool restOfAll = false)
	{
		CommandLineValueType type = CommandLineValueType::Boolean;
		// C++17 ������if constexpr
		if (std::is_same_v<T, bool>)
		{
			type = CommandLineValueType::Boolean;
		}
		else if (std::is_same_v<T, int>)
		{
			type = CommandLineValueType::Int;
		}
		else if (std::is_same_v<T, std::string>)
		{
			type = CommandLineValueType::String;
		}
		else
		{
			return false;
		}

		_args.insert({name, CommandLineOption{"", type, restOfAll}});
		return true;
	}

	/**
	 * \remark C++ 11 ��֧�� if constexpr������д������ֺܶ�
	 */
	template <typename T>
	T Get(const std::string& name) const
	{
		throw CommandLineException("unkown type");
	}

	template <>
	bool Get(const std::string& name) const
	{
		if (_args.count(name) == 0)
		{
			throw CommandLineException("arg not exist");
		}
		const CommandLineOption& option = _args.at(name);
		if (option.Type == CommandLineValueType::Boolean)
		{
			return option.Value == "true";
		}
		return false;
	}

	template <>
	int Get(const std::string& name) const
	{
		if (_args.count(name) == 0)
		{
			throw CommandLineException("arg not exist");
		}
		const CommandLineOption& option = _args.at(name);
		int intValue = std::stoi(option.Value);
		return intValue;
	}

	template <>
	std::string Get(const std::string& name) const
	{
		if (_args.count(name) == 0)
		{
			throw CommandLineException("arg not exist");
		}
		const CommandLineOption& option = _args.at(name);
		return option.Value;
	}

	/**
	 * \brief ��ӽ���Ŀ������ attach
	 * \param isParse �Ƿ������������
	 */
	void AddTarget(const std::string& name, bool isParse = true);

	std::string GetTarget() const noexcept;

	/**
	 * \brief ��˳���ȡ����
	 */
	std::string GetArg(int index) const noexcept;

	bool Parse(int argc, char** argv);

private:
	std::map<std::string, CommandLineOption> _args;
	std::vector<std::string> _argvs;
	std::map<std::string, bool> _targets;
	std::string _currentTarget;
};
