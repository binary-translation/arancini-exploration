#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace arancini::elf {
enum class section_type { null_section = 0, progbits = 1, symbol_table = 2, string_table = 3 };
enum class section_flags { shf_write = 1 };

class section {
public:
	section(const void *data, off_t address, size_t data_size, section_type type, const std::string &name, section_flags flags)
		: data_(data)
		, address_(address)
		, data_size_(data_size)
		, type_(type)
		, name_(name)
		, flags_(flags)
	{
	}

	const std::string &name() const { return name_; }
	section_type type() const { return type_; }
	section_flags flags() const { return flags_; }

	const void *data() const { return data_; }
	size_t data_size() const { return data_size_; }

	off_t address() const { return address_; }

private:
	const void *data_;
	off_t address_;
	size_t data_size_;

	section_type type_;
	std::string name_;
	section_flags flags_;
};

class null_section : public section {
public:
	null_section(const std::string &name, section_flags flags)
		: section(nullptr, 0, 0, section_type::null_section, name, flags)
	{
	}
};

class symbol {
public:
	symbol(const std::string &name, uint64_t value, size_t size, int shidx)
		: name_(name)
		, value_(value)
		, size_(size)
		, shidx_(shidx)
	{
	}

	const std::string &name() const { return name_; }

	uint64_t value() const { return value_; }
	size_t size() const { return size_; }

	int section_index() const { return shidx_; }

private:
	std::string name_;
	uint64_t value_;
	size_t size_;
	int shidx_;
};

class symbol_table : public section {
public:
	symbol_table(const void *data, off_t address, size_t data_size, const std::vector<symbol> &symbols, const std::string &name, section_flags flags)
		: section(data, address, data_size, section_type::symbol_table, name, flags)
		, symbols_(symbols)
	{
	}

	const std::vector<symbol> symbols() const { return symbols_; }

private:
	std::vector<symbol> symbols_;
};

class string_table : public section {
public:
	string_table(const void *data, off_t address, size_t data_size, const std::vector<std::string> &strings, const std::string &name, section_flags flags)
		: section(data, address, data_size, section_type::string_table, name, flags)
		, strings_(strings)
	{
	}

private:
	std::vector<std::string> strings_;
};

class elf_reader {
public:
	elf_reader(const std::string &filename);
	~elf_reader();

	void parse();

	const std::vector<std::shared_ptr<section>> &sections() const { return sections_; }

	std::shared_ptr<section> get_section(int index) const { return sections_[index]; }

private:
	void *elf_data_;
	size_t elf_data_size_;

	std::vector<std::shared_ptr<section>> sections_;

	void parse_sections(off_t offset, int count, size_t size, int name_table_index);
	void parse_section(
		section_type type, section_flags flags, const std::string &name, off_t address, off_t offset, size_t size, off_t link_offset, size_t entry_size);
	void parse_symbol_table(section_flags flags, const std::string &name, off_t address, off_t offset, size_t size, off_t link_offset, size_t entry_size);

#if __cplusplus > 202002L
	constexpr const void *get_data_ptr(off_t offset) const { return (const void *)((uintptr_t)elf_data_ + offset); }
#else
	const void *get_data_ptr(off_t offset) const { return (const void *)((uintptr_t)elf_data_ + offset); }
#endif

	template <typename T> constexpr T read(off_t offset) const { return *(const T *)get_data_ptr(offset); }

	uint8_t read8(off_t offset) const { return read<uint8_t>(offset); }
	uint16_t read16(off_t offset) const { return read<uint16_t>(offset); }
	uint32_t read32(off_t offset) const { return read<uint32_t>(offset); }
	uint64_t read64(off_t offset) const { return read<uint64_t>(offset); }

	std::string readstr(off_t offset) const { return std::string((const char *)get_data_ptr(offset)); }
};
} // namespace arancini::elf
