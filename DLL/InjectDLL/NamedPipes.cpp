#include "Connectivity.h"

using namespace Connectivity;

#ifdef _WIN32

void namedPipeClass::createServer()
{
	bool pipeOpened = false;
	int tries = 0;
	HANDLE hPipeTemp;
	Logging::LoggerService::LogDebug("Connecting to named pipe");

	while (!pipeOpened)
	{
		hPipeTemp = CreateFile("\\\\.\\pipe\\languageConnectionPipe", GENERIC_ALL, 0, nullptr, OPEN_EXISTING, 0, nullptr);

		if (hPipeTemp == INVALID_HANDLE_VALUE)
		{
			tries++;
			continue;
		}

		pipeOpened = true;

		DWORD mode = PIPE_READMODE_MESSAGE;

		SetNamedPipeHandleState(hPipeTemp, &mode, nullptr, nullptr);

		this->hPipe = hPipeTemp;
	}
}

bool namedPipeClass::read(char* buffer, size_t size)
{
	DWORD read;
	return ReadFile(this->hPipe, buffer, (DWORD)size, &read, nullptr);
}

bool namedPipeClass::write(const char* buffer, size_t size)
{
	DWORD written;
	return WriteFile(this->hPipe, buffer, (DWORD)size, &written, nullptr);
}

#else

// TODO(linux): connect to the launcher over a Unix domain socket (step 4 of the Linux port).

void namedPipeClass::createServer()
{
	Logging::LoggerService::LogWarning("Launcher connection is not implemented on Linux yet.", __FUNCTION__);
}

bool namedPipeClass::read(char* buffer, size_t size)
{
	Sleep(1000);
	return false;
}

bool namedPipeClass::write(const char* buffer, size_t size)
{
	return false;
}

#endif
