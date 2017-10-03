#pragma once

namespace MyScript
{
	//	Check if p is of type T
	template <typename T, typename U>
	bool is_type(U* p)
	{
		if (dynamic_cast<T*>(p) != nullptr)
			return true;
		else
			return false;
	}

	//	Replace all occurences of s1 by s2 in s, and return result
	std::string replace_all(std::string s, std::string s1, std::string s2)
	{
		size_t start_pos = 0;
		while ((start_pos = s.find(s1, start_pos)) != std::string::npos)
		{
			s.replace(start_pos, s1.length(), s2);
			start_pos += s2.length();
		}

		return s;
	}
}