#include "command_line.h"

void CommandLine::AddArg(const std::string& name, bool restOfAll)
{
	_args.insert({name, Option{"", restOfAll}});
}

void CommandLine::AddTarget(const std::string& name, bool isParse)
{
	_targets.insert({name, isParse});
}

std::string CommandLine::GetTarget() const noexcept
{
	return _currentTarget;
}

std::string CommandLine::GetArg(const std::string& name) const noexcept
{
	if (_args.count(name))
	{
		return _args.at(name).Value;
	}
	return "";
}

std::string CommandLine::GetArg(int index) const noexcept
{
	if(static_cast<std::size_t>(index) < _argvs.size())
	{
		return _argvs[index];
	}
	else
	{
		return "";
	}
}

bool CommandLine::Parse(int argc, char** argv)
{
	if (argc < 2)
	{
		return false;
	}
	_currentTarget = argv[1];

	if (_targets.count(_currentTarget) == 0)
	{
		return false;
	}

	bool isParse = _targets.at(_currentTarget);
	_argvs.reserve(argc);
	for (int i = 0; i != argc; i++)
	{
		_argvs.emplace_back(argv[i]);
	}

	if (!isParse)
	{
		return true;
	}

	// index = 0 �Ĳ����ǳ�����
	for (int index = 1; index < argc; index++)
	{
		std::string current = argv[index];
		if (current.empty())
		{
			continue;
		}
		// not empty
		if (current[0] == '-')
		{
			// ����֧��-dir������ʽ
			std::string optionName = current.substr(1);
			// ����ò���������
			if (_args.count(optionName) == 0)
			{
				return false;
			}
			Option& option = _args[optionName];
			std::string optionValue;
			optionValue.reserve(128);

			// ��ѡ��֮��û�нӲ���
			// Ŀǰû��֧��boolѡ��ı�Ҫ
			if (argc <= (index + 1))
			{
				return false;
			}

			do
			{
				if (argc <= (index + 1))
				{
					break;
				}

				std::string value = argv[++index];
				if (value.empty())
				{
					continue;
				}
				if (option.RestOfAll)
				{
					optionValue.append(" ").append(value);
				}
				else
				{
					// ��Ϊֵ�Ǳ�һ�����Ű�������
					if (value[0] == '\"' || value[0] == '\'')
					{
						optionValue = value.substr(1, value.size() - 2);
					}
					else
					{
						optionValue = value;
					}
					break;
				}
			}
			while (true);

			option.Value = std::move(optionValue);
		}
	}
	return true;
}
