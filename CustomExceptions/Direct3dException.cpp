#include "Direct3dException.h"

Direct3dException::Direct3dException(std::string errorMessage)
{
	m_errorMessage = errorMessage;
}

const char* Direct3dException::what() const noexcept
{
	return m_errorMessage.c_str();
}
