#pragma once

#include <string>

class MappedFileString {
public:
	MappedFileString(std::string const& fqfn);
	~MappedFileString();

	inline std::string_view const& GetView() const {
		return m_view;
	}
	inline std::string GetString() const {
#ifdef _MSC_VER
		return std::string(m_view);
#else
		return m_content;
#endif
	}
private:
	void* m_fileHandle = nullptr;
	void* m_mapping = nullptr;
	void* m_baseAddress = nullptr;
#ifndef _MSC_VER
	std::string m_content;
#endif
	std::string_view m_view;
};
