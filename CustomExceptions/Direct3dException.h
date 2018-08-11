#pragma once

#include <exception>
#include <string>

class Direct3dException : public std::exception
{
public:
	Direct3dException(std::string errorMessage);
	const char* what() const noexcept;

private:
	std::string m_errorMessage;
};
