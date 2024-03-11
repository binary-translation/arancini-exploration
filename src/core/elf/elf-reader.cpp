#include "arancini/util/logger.h"
#include <arancini/elf/elf-reader.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <elf.h>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <set>

using namespace arancini::elf;

struct unmanaged_file {
	unmanaged_file(const unmanaged_file &) = delete;
	unmanaged_file(unmanaged_file &&) = delete;
	unmanaged_file &operator=(unmanaged_file) = delete;

	unmanaged_file(const std::string &filename)
		: fd_(-1)
		, size_(0)
		, has_size_(false)
	{
		fd_ = open(filename.c_str(), O_RDONLY);

		if (fd_ < 0) {
			throw std::runtime_error("unable to open file for reading '" + filename + "'");
		}
	}

	~unmanaged_file() { close(fd_); }

	off_t size() const
	{
		if (!has_size_) {
			struct stat64 st;
			if (fstat64(fd_, &st) < 0) {
				throw std::runtime_error("unable to stat file");
			}

			size_ = st.st_size;
			has_size_ = true;
		}

		return size_;
	}

	void *map()
	{
		void *ptr = mmap(nullptr, size(), PROT_READ, MAP_PRIVATE, fd_, 0);

		if (ptr == MAP_FAILED) {
			throw std::runtime_error("unable to map file");
		}

		return ptr;
	}

private:
	int fd_;
	mutable off_t size_;
	mutable bool has_size_;
};

elf_reader::elf_reader(const std::string &filename)
	: elf_data_(nullptr)
	, elf_data_size_(0)
	, entrypoint_(0)
{
	unmanaged_file f(filename);

	elf_data_size_ = f.size();
	elf_data_ = f.map();
}

elf_reader::~elf_reader() { munmap(elf_data_, elf_data_size_); }

void elf_reader::parse()
{
	uint32_t magic = read32(0);
	if (magic != 0x464c457f) {
		throw std::runtime_error("invalid magic number");
	}

	uint8_t cls = read8(4);
	switch (cls) {
	case 1: // 32-bit
		throw std::runtime_error("only 64-bit elf files are supported");
	case 2:
		break;

	default:
		throw std::runtime_error("invalid elf class");
	}

	Elf64_Ehdr elf_header = read<Elf64_Ehdr>(0);
	entrypoint_ = elf_header.e_entry;
	type_ = static_cast<elf_type>(elf_header.e_type);
	parse_sections(elf_header.e_shoff, elf_header.e_shnum, elf_header.e_shentsize, elf_header.e_shstrndx);
	parse_program_headers(elf_header.e_phoff, elf_header.e_phnum, elf_header.e_phentsize);
}

void elf_reader::parse_sections(off_t offset, int count, size_t size, int name_table_index)
{
	auto name_table_header = read<Elf64_Shdr>(offset + (size * name_table_index));
	off_t name_table_offset = name_table_header.sh_offset;

	for (int i = 0; i < count; i++) {
		auto section_header = read<Elf64_Shdr>(offset + (size * i));

		std::string name = readstr(name_table_offset + section_header.sh_name);

		auto link_section_header = read<Elf64_Shdr>(offset + (size * section_header.sh_link));
		off_t link_section_offset = link_section_header.sh_offset;

		parse_section((section_type)section_header.sh_type, (section_flags)section_header.sh_flags, name, section_header.sh_addr, section_header.sh_offset,
			section_header.sh_size, link_section_offset, section_header.sh_entsize);
	}
}

void elf_reader::parse_section(
	section_type type, section_flags flags, const std::string &name, off_t address, off_t offset, size_t size, off_t link_offset, size_t entry_size)
{
	switch (type) {
	case section_type::symbol_table:
	case section_type::dynamic_symbol_table:
		parse_symbol_table(flags, name, address, offset, size, link_offset, entry_size, type);
		break;
	case section_type::relocation_addend:
		parse_relocation_addend_table(flags, name, address, offset, size, link_offset, entry_size);
		break;
	case section_type::relr:
		parse_relr(flags, name, address, offset, size, link_offset, entry_size);
		break;
	case section_type::progbits:
		parse_progbits(flags, name, address, offset, size, link_offset, entry_size);
		break;
	case section_type::dynamic:
		parse_dynamic(flags, name, address, offset, size, link_offset, entry_size);
		break;
	default:
		sections_.push_back(std::make_shared<section>(get_data_ptr(offset), address, size, type, name, flags, offset));
		break;
	}
}

void elf_reader::parse_symbol_table(
	section_flags flags, const std::string &name, off_t address, off_t offset, size_t size, off_t link_offset, size_t entry_size, section_type type)
{
	std::vector<symbol> symbols;

	for (off_t i = 0; i < size; i += entry_size) {
		auto sym = read<Elf64_Sym>(offset + i);

		std::string name = readstr(link_offset + sym.st_name);
		symbols.push_back(symbol(name, sym.st_value, sym.st_size, sym.st_shndx, sym.st_info, sym.st_other));
	}

	sections_.push_back(std::make_shared<symbol_table>(get_data_ptr(offset), address, size, symbols, name, flags, type, offset));
}

void elf_reader::parse_program_headers(off_t offset, int count, size_t size)
{
	for (int i = 0; i < count; i++) {
		auto ph = read<Elf64_Phdr>(offset + (size * i));

		program_headers_.push_back(std::make_shared<program_header>(
			(program_header_type)ph.p_type, get_data_ptr(ph.p_offset), ph.p_vaddr, ph.p_filesz, ph.p_memsz, ph.p_flags, ph.p_offset, ph.p_align));
	}
}

void elf_reader::parse_relocation_addend_table(
	section_flags flags, const std::string &sec_name, off_t address, off_t offset, size_t size, off_t link_offset, size_t entry_size)
{
	std::vector<rela> relas;

	for (size_t i = 0; i < size; i += entry_size) {
		auto r = read<Elf64_Rela>(offset + i);

		relas.emplace_back(ELF64_R_TYPE(r.r_info), r.r_addend, ELF64_R_SYM(r.r_info), r.r_offset);
	}

	sections_.push_back(std::make_shared<rela_table>(get_data_ptr(offset), address, size, sec_name, flags, relas, offset));
}

void elf_reader::parse_relr(section_flags flags, const std::string &sec_name, off_t address, off_t offset, size_t size, off_t link_offset, size_t entry_size)
{
	std::vector<uint64_t> relrs;
	off_t where;

	for (size_t i = 0; i < size; i += entry_size) {
		auto entry = read<size_t>(offset + i);

		if ((entry & 1) == 0) {
			where = (off_t)(entry);

			relrs.push_back(where);
			where += 8;
		} else {
			for (long j = 0; (entry >>= 1) != 0; j++) {
				if ((entry & 1) != 0) {
					relrs.push_back(where + 8 * j);
				}
			}
			where += (CHAR_BIT * (sizeof(size_t)) - 1) * 8;
		}
	}

	sections_.push_back(std::make_shared<relr_array>(get_data_ptr(offset), address, size, sec_name, flags, relrs, offset));
}

struct [[gnu::packed]] plt_entry {
	unsigned char jumpq[6];
	unsigned char pushq[5];
	unsigned char jump[5];
};

void elf_reader::parse_progbits(section_flags flags, const std::string &sec_name, off_t address, off_t offset, size_t size, off_t link_offset, size_t entry_size) {
	if (sec_name == ".plt") {
		std::vector<std::pair<unsigned long, unsigned long>> stubs;

		// TODO: arch specific
		for (off_t i = 0; i < size; i+=sizeof(struct plt_entry)) {
			auto e = read<plt_entry>(offset+i);
			// offset from the jump instr
			// +   addr of the jump instr
			// +    len of the jump instr
			off_t got_addr = e.jumpq[3]<<8 | e.jumpq[2];
			got_addr += (address+i+6);

			stubs.push_back(std::make_pair(address+i, got_addr));
		}
		sections_.push_back(std::make_shared<plt_table>(get_data_ptr(offset), address, size, sec_name, flags, stubs, offset));
		return;
	}
	
	sections_.push_back(std::make_shared<section>(get_data_ptr(offset), address, size, section_type::progbits, sec_name, flags, offset));
}

void elf_reader::parse_dynamic(section_flags flags, const std::string &sec_name, off_t address, off_t offset, size_t size, off_t link_offset, size_t entry_size) {
	// maybe we need that later?
	sections_.push_back(std::make_shared<section>(get_data_ptr(offset), address, size, section_type::dynamic, sec_name, flags, offset));
}
