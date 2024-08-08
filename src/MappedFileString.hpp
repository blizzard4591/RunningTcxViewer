#pragma once

#include <string>

class MappedFileString {
public:
	MappedFileString(std::string const& fqfn);
	~MappedFileString();

	std::string_view const& GetView() const {
		return m_view;
	}
	std::string GetString() const {
		return std::string(m_view);
	}
private:
	void* m_fileHandle;
	void* m_mapping;
	void* m_baseAddress;
	std::string_view m_view;
};
