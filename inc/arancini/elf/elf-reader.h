#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <set>

#include <elf.h>

namespace arancini::elf {
enum class section_type {
	null_section = 0,
	progbits = 1,
	symbol_table = 2,
	string_table = 3,
	relocation_addend = 4,
	dynamic =6,
	relocation = 9,
	dynamic_symbol_table = 11,
	relr = 19
};
enum class section_flags { shf_write = 1 };

class section {
public:
	section(const void *data, off_t address, size_t data_size, section_type type, const std::string &name, section_flags flags, off_t offset)
		: data_(data)
		, address_(address)
		, data_size_(data_size)
		, offset_(offset)
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
	off_t file_offset() const { return offset_; }

private:
	const void *data_;
	off_t address_;
	size_t data_size_;
	off_t offset_;

	section_type type_;
	std::string name_;
	section_flags flags_;
};

class null_section : public section {
public:
	null_section(const std::string &name, section_flags flags, off_t offset)
		: section(nullptr, 0, 0, section_type::null_section, name, flags, offset)
	{
	}
};

class symbol {
public:
	symbol(const std::string &name, uint64_t value, size_t size, int shidx, unsigned char info, unsigned char other)
		: name_(name)
		, value_(value)
		, size_(size)
		, shidx_(shidx)
		, info_(info)
		, other_(other)
	{}

	const std::string &name() const { return name_; }

	uint64_t value() const { return value_; }
	size_t size() const { return size_; }

	int section_index() const { return shidx_; }

	unsigned char info() const { return info_; }

	bool is_func() const { return ELF64_ST_TYPE(info_) == STT_FUNC; }

	friend bool operator<(const symbol &lhs, const symbol &rhs) {
		return lhs.value_ < rhs.value_;
	}

	[[nodiscard]] bool is_global() const { return ELF64_ST_BIND(info_) == STB_GLOBAL; }
	[[nodiscard]] bool is_weak() const { return ELF64_ST_BIND(info_) == STB_WEAK; }

	[[nodiscard]] const char *type() const
	{
		switch (ELF64_ST_TYPE(info_)) {
		case STT_FILE:
			// return "STT_FILE";
		case STT_NOTYPE:
			return "STT_NOTYPE";
		case STT_OBJECT:
			return "STT_OBJECT";
		case STT_FUNC:
			return "STT_FUNC";
		case STT_SECTION:
			return "STT_SECTION";
		case STT_COMMON:
			return "STT_COMMON";
		case STT_TLS:
			return "STT_TLS";
		case STT_NUM:
			return "STT_NUM";
		case STT_GNU_IFUNC:
			return "STT_GNU_IFUNC";
		default:
			return "UNKNOWN";
		}
	}

	[[nodiscard]] bool is_hidden() const { return ELF64_ST_VISIBILITY(other_) == STV_HIDDEN; }
	[[nodiscard]] bool is_internal() const { return ELF64_ST_VISIBILITY(other_) == STV_INTERNAL; }
	[[nodiscard]] bool is_protected() const { return ELF64_ST_VISIBILITY(other_) == STV_PROTECTED; }

private:
	std::string name_;
	uint64_t value_;
	size_t size_;
	int shidx_;
	unsigned char info_;
	unsigned char other_;
};

class symbol_table : public section {
public:
	symbol_table(const void *data, off_t address, size_t data_size, const std::vector<symbol> &symbols, const std::string &name, section_flags flags,
		section_type type, off_t offset)
		: section(data, address, data_size, type, name, flags, offset)
		, symbols_(symbols)
	{
	}

	const std::vector<symbol> symbols() const { return symbols_; }

private:
	std::vector<symbol> symbols_;
};

class string_table : public section {
public:
	string_table(
		const void *data, off_t address, size_t data_size, const std::vector<std::string> &strings, const std::string &name, section_flags flags, off_t offset)
		: section(data, address, data_size, section_type::string_table, name, flags, offset)
		, strings_(strings)
	{
	}

private:
	std::vector<std::string> strings_;
};

class rela {
public:
	rela(int type, uint64_t addend, uint64_t symbol, uint64_t offset)
		: type_(type)
		, addend_(addend)
		, symbol_(symbol)
		, offset_(offset)
	{
	}

	[[nodiscard]] bool is_relative() const { return type_on_host() == R_RISCV_RELATIVE; }
	[[nodiscard]] bool is_irelative() const { return type_ == R_X86_64_IRELATIVE; }
	[[nodiscard]] bool is_tpoff() const { return type_ == R_X86_64_TPOFF64; }
	[[nodiscard]] bool is_dtpmod() const { return type_ == R_X86_64_DTPMOD64; }
	[[nodiscard]] int type_on_host() const
	{
#if defined(ARCH_RISCV64)
		switch (type_) {
		case R_X86_64_NONE:
			return R_RISCV_NONE;
		case R_X86_64_64:
			return R_RISCV_64;
		case R_X86_64_PC32:
			return R_RISCV_32_PCREL;
		case R_X86_64_COPY:
			return R_RISCV_COPY;
		case R_X86_64_GLOB_DAT:
		case R_X86_64_JUMP_SLOT:
			return R_RISCV_64;
		case R_X86_64_RELATIVE:
			return R_RISCV_RELATIVE;
		}
#elif defined(ARCH_AARCH64)
        switch (type_) {
        case R_X86_64_NONE:
            return R_AARCH64_NONE;
        case R_X86_64_64:
            return R_AARCH64_ABS64;
        case R_X86_64_PC32:
            return R_AARCH64_PREL32;
        case R_X86_64_COPY:
            return R_AARCH64_COPY;
        case R_X86_64_GLOB_DAT:
        case R_X86_64_JUMP_SLOT:
            return R_AARCH64_ABS64;
        case R_X86_64_RELATIVE:
            return R_AARCH64_RELATIVE;
        }
#endif
		return 0x1111;
	};

	[[nodiscard]] int type() const { return type_; }
	[[nodiscard]] uint64_t addend() const { return addend_; }
	[[nodiscard]] uint64_t symbol() const { return symbol_; }
	[[nodiscard]] uint64_t offset() const { return offset_; }

private:
	int type_;
	uint64_t addend_;
	uint64_t symbol_;
	uint64_t offset_;
};

class rela_table : public section {
public:
	rela_table(
		const void *data, off_t address, size_t data_size, const std::string &name, section_flags flags, const std::vector<rela> &relocations, off_t offset)
		: section(data, address, data_size, section_type::relocation_addend, name, flags, offset)
		, relocations_(relocations)
	{
	}

	[[nodiscard]] const std::vector<rela> &relocations() const { return relocations_; }

private:
	std::vector<rela> relocations_;
};

class relr_array : public section {
public:
	relr_array(
		const void *data, off_t address, size_t data_size, const std::string &name, section_flags flags, const std::vector<uint64_t> &relocations, off_t offset)
		: section(data, address, data_size, section_type::relr, name, flags, offset)
		, relocations_(relocations)
	{
	}

	[[nodiscard]] const std::vector<uint64_t> &relocations() const { return relocations_; }

private:
	/// Each entry represents the runtime virtual address the RELR relocation needs to be applied to. This is probably different from the file offset.
	std::vector<uint64_t> relocations_;
};

class plt_table : public section {
public:
	plt_table(
		const void *data, off_t address, size_t data_size, const std::string &name, section_flags flags, const std::vector<std::pair<unsigned long, unsigned long>> &stubs, off_t offset)
		: section(data, address, data_size, section_type::progbits, name, flags, offset)
		, stubs_(stubs)
	{}

	[[nodiscard]] const std::vector<std::pair<unsigned long, unsigned long>> &stubs() const { return stubs_; }
private:
	std::vector<std::pair<unsigned long, unsigned long>> stubs_;
};

enum class program_header_type { null_program_header, loadable, dynamic, interp, note, shlib, program_headers, tls };

class program_header {
public:
	program_header(
		program_header_type type, const void *data, off_t address, size_t data_size, size_t mem_size, uint32_t flags, uint32_t offset, uint64_t align)
		: type_(type)
		, data_(data)
		, address_(address)
		, data_size_(data_size)
		, mem_size_(mem_size)
		, flags_(flags)
		, offset_(offset)
		, align_(align)
	{
	}

	program_header_type type() const { return type_; }
	const void *data() const { return data_; }
	off_t address() const { return address_; }
	size_t data_size() const { return data_size_; }
	size_t mem_size() const { return mem_size_; }
	[[nodiscard]] uint32_t flags() const { return flags_; }
	[[nodiscard]] uint32_t offset() const { return offset_; }
	[[nodiscard]] uint64_t align() const { return align_; }

private:
	program_header_type type_;
	const void *data_;
	off_t address_;
	size_t data_size_;
	size_t mem_size_;
	uint32_t flags_;
	uint32_t offset_;
	uint64_t align_;
};

enum class elf_type { none, rel, exec, dyn, core };

class elf_reader {
public:
	elf_reader(const std::string &filename);
	~elf_reader();

	void parse();

	elf_type type() const { return type_; }

	off_t get_entrypoint() const { return entrypoint_; }

	const std::vector<std::shared_ptr<section>> &sections() const { return sections_; }
	const std::vector<std::shared_ptr<program_header>> &program_headers() const { return program_headers_; }

	std::shared_ptr<section> get_section(int index) const { return sections_[index]; }
	std::shared_ptr<program_header> get_program_header(int index) const { return program_headers_[index]; }

	uint64_t read_relr_addend(off_t off) const { return read64(off); }

private:
	void *elf_data_;
	size_t elf_data_size_;
	off_t entrypoint_;

	elf_type type_;

	std::vector<std::shared_ptr<section>> sections_;
	std::vector<std::shared_ptr<program_header>> program_headers_;

	void parse_program_headers(off_t offset, int count, size_t size);

	void parse_sections(off_t offset, int count, size_t size, int name_table_index);
	void parse_section(
		section_type type, section_flags flags, const std::string &name, off_t address, off_t offset, size_t size, off_t link_offset, size_t entry_size);

	void parse_symbol_table(
		section_flags flags, const std::string &name, off_t address, off_t offset, size_t size, off_t link_offset, size_t entry_size, section_type type);

	void parse_relocation_addend_table(
		section_flags flags, const std::string &sec_name, off_t address, off_t offset, size_t size, off_t link_offset, size_t entry_size);

	void parse_relr(section_flags flags, const std::string &sec_name, off_t address, off_t offset, size_t size, off_t link_offset, size_t entry_size);

	void parse_progbits(section_flags flags, const std::string &sec_name, off_t address, off_t offset, size_t size, off_t link_offset, size_t entry_size);

	void parse_dynamic(section_flags flags, const std::string &sec_name, off_t address, off_t offset, size_t size, off_t link_offset, size_t entry_size);

	void parse_plt();
	void parse_got();
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
