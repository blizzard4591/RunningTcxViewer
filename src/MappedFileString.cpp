#include "MappedFileString.hpp"

#include <iostream>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Memoryapi.h>

MappedFileString::MappedFileString(std::string const& fqfn) {
	m_fileHandle = CreateFileA(fqfn.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (m_fileHandle == INVALID_HANDLE_VALUE) {
		std::cerr << "Internal Error: Failed to open file '" << fqfn << "', last error = " << GetLastError() << std::endl;
		throw;
	}
	LARGE_INTEGER m_fileSize;
	m_fileSize.QuadPart = 0;
	if (!GetFileSizeEx(m_fileHandle, &m_fileSize)) {
		std::cerr << "Internal Error: Failed to get file size of '" << fqfn << "', last error = " << GetLastError() << std::endl;
		CloseHandle(m_fileHandle);
		throw;
	}

	if (m_fileSize.QuadPart == 0) {
		// Mapping an empty file will fail, so we should not try
		CloseHandle(m_fileHandle);
		m_fileHandle = INVALID_HANDLE_VALUE;
		m_mapping = NULL;
		m_baseAddress = NULL;
		m_view = std::string_view();
	} else {
		m_mapping = CreateFileMappingA(m_fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
		if (m_mapping == NULL) {
			std::cerr << "Internal Error: Failed to create file mapping to '" << fqfn << "', last error = " << GetLastError() << std::endl;
			CloseHandle(m_fileHandle);
			throw;
		}

		m_baseAddress = MapViewOfFile(m_mapping, FILE_MAP_READ, 0, 0, 0);
		if (m_baseAddress == NULL) {
			std::cerr << "Internal Error: Failed to map file '" << fqfn << "' into view, last error = " << GetLastError() << std::endl;
			CloseHandle(m_mapping);
			CloseHandle(m_fileHandle);
			throw;
		}
		m_view = std::string_view(static_cast<char const*>(m_baseAddress), m_fileSize.QuadPart);
	}
}
MappedFileString::~MappedFileString() {
	if (m_baseAddress != NULL) UnmapViewOfFile(m_baseAddress);
	if (m_mapping != NULL) CloseHandle(m_mapping);
	if (m_fileHandle != INVALID_HANDLE_VALUE) CloseHandle(m_fileHandle);
}
