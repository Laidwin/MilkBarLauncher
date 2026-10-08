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

#include <algorithm>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace
{
	// Set by the launcher when it starts Cemu; the default matches the launcher's.
	std::string LauncherSocketPath()
	{
		if (const char* path = getenv("BOTWM_LAUNCHER_SOCKET"); path && *path)
			return path;
		if (const char* runtime = getenv("XDG_RUNTIME_DIR"); runtime && *runtime)
			return std::string(runtime) + "/botwm-launcher.sock";
		return "/tmp/botwm-launcher.sock";
	}

	bool ReadAll(int socket, void* buffer, size_t size)
	{
		char* bytes = static_cast<char*>(buffer);
		while (size > 0)
		{
			ssize_t received = recv(socket, bytes, size, 0);
			if (received <= 0)
				return false;
			bytes += received;
			size -= received;
		}
		return true;
	}

	bool WriteAll(int socket, const void* buffer, size_t size)
	{
		const char* bytes = static_cast<const char*>(buffer);
		while (size > 0)
		{
			ssize_t sent = send(socket, bytes, size, MSG_NOSIGNAL);
			if (sent <= 0)
				return false;
			bytes += sent;
			size -= sent;
		}
		return true;
	}
}

void namedPipeClass::createServer()
{
	std::string path = LauncherSocketPath();
	Logging::LoggerService::LogDebug("Connecting to launcher socket " + path);

	sockaddr_un address{};
	address.sun_family = AF_UNIX;
	strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);

	// Like the Windows pipe client: retry until the launcher is listening.
	while (true)
	{
		int candidate = ::socket(AF_UNIX, SOCK_STREAM, 0);
		if (candidate >= 0 && connect(candidate, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0)
		{
			this->socket = candidate;
			return;
		}
		if (candidate >= 0)
			close(candidate);
		Sleep(100);
	}
}

// Messages are a 4-byte little-endian length followed by the payload.
bool namedPipeClass::read(char* buffer, size_t size)
{
	if (this->pendingOffset >= this->pendingMessage.size())
	{
		uint32_t length = 0;
		if (!ReadAll(this->socket, &length, sizeof(length)))
		{
			Sleep(100); // The launcher is gone; don't spin.
			return false;
		}

		this->pendingMessage.resize(length);
		this->pendingOffset = 0;
		if (!ReadAll(this->socket, this->pendingMessage.data(), length))
		{
			this->pendingMessage.clear();
			return false;
		}
	}

	size_t chunk = std::min(size, this->pendingMessage.size() - this->pendingOffset);
	memcpy(buffer, this->pendingMessage.data() + this->pendingOffset, chunk);
	memset(buffer + chunk, 0, size - chunk);
	this->pendingOffset += chunk;

	return this->pendingOffset >= this->pendingMessage.size();
}

bool namedPipeClass::write(const char* buffer, size_t size)
{
	uint32_t length = static_cast<uint32_t>(size);
	return WriteAll(this->socket, &length, sizeof(length)) && WriteAll(this->socket, buffer, size);
}

#endif
