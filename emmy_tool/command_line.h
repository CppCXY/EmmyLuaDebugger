#pragma once

#include <string>
#include <map>
#include <set>
#include <vector>

struct Option
{
	std::string Value = "";
	bool RestOfAll = false;
};

/**
 * \brief �ض��﷨�ṹ�������н�������
 */
class CommandLine
{
public:
	/**
	 * \brief ��ӽ����Ĳ���
	 * \param restOfAll Ϊtrue ��ʾ�˲���֮������в�����ͬ���ɸò���������
	 */
	void AddArg(const std::string& name, bool restOfAll = false);

	/**
	 * \brief ��ӽ���Ŀ������ attach
	 */
	void AddTarget(const std::string& name, bool isParse = true);

	std::string GetTarget() const noexcept;

	std::string GetArg(const std::string& name) const noexcept;

	std::string GetArg(int index) const noexcept;
	
	bool Parse(int argc, char** argv);

private:
	std::map<std::string, Option> _args;
	std::vector<std::string> _argvs;
	std::map<std::string, bool> _targets;
	std::string _currentTarget;
};
