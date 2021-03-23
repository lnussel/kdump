/*
 * (c) 2021, Petr Tesarik <ptesarik@suse.de>, SUSE Linux Software Solutions GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <cerrno>
#include <cstring>
#include <string>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>
#include <elf.h>
#include <libelf.h>
#include <gelf.h>

#include "kelf.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
static const int ELFDATA_MINE = ELFDATA2LSB;
#else
static const int ELFDATA_MINE = ELFDATA2MSB;
#endif

using std::string;

//{{{ KElfErrorCode ------------------------------------------------------------

// -----------------------------------------------------------------------------
string KElfErrorCode::message(void) const
{
    return elf_errmsg(getCode());
}

//}}}
//{{{ KElf::Mapping ------------------------------------------------------------

// -----------------------------------------------------------------------------
KElf::Mapping::Mapping(off_t _offset, size_t _length)
    : data(static_cast<char*>(MAP_FAILED)), elf(nullptr),
      offset(_offset), length(_length)
{
}

// -----------------------------------------------------------------------------
KElf::Mapping::~Mapping()
{
    if (elf)
        elf_end(elf);
    if (data != MAP_FAILED)
        munmap(data, length);
}

//}}}
//{{{ KElf ---------------------------------------------------------------------

// -----------------------------------------------------------------------------
KElf::KElf(std::string const &path)
    : m_fd(-1), m_pagesize(sysconf(_SC_PAGESIZE))
{
    m_fd = open(path.c_str(), O_RDONLY);
    if (m_fd < 0)
        throw KSystemError("Cannot open ELF file", errno);

    if (elf_version(EV_CURRENT) == EV_NONE)
        throw KError("libelf is out of date.");

    // One page should always be enough for ELF file headers
    m_map = map(0, m_pagesize);

    char *ident = m_map->data;
    if (memcmp(ident, ELFMAG, SELFMAG))
        throw KError("Not an ELF file");
    if (ident[EI_VERSION] != EV_CURRENT)
        throw KError("Unsupported ELF version");
    if (ident[EI_DATA] != ELFDATA_MINE)
        throw KError("Unsupported ELF data encoding");

    // map enough of the file to read program headers and section headers
    off_t endphdr, endshdr;
    if (ident[EI_CLASS] == ELFCLASS32) {
        Elf32_Ehdr *ehdr = reinterpret_cast<Elf32_Ehdr*>(m_map->data);
        endphdr = ehdr->e_phoff + ehdr->e_phentsize * ehdr->e_phnum;
        endshdr = ehdr->e_shoff + ehdr->e_shentsize * ehdr->e_shnum;
    } else if (ident[EI_CLASS] == ELFCLASS64) {
        Elf64_Ehdr *ehdr = reinterpret_cast<Elf64_Ehdr*>(m_map->data);
        endphdr = ehdr->e_phoff + ehdr->e_phentsize * ehdr->e_phnum;
        endshdr = ehdr->e_shoff + ehdr->e_shentsize * ehdr->e_shnum;
    } else
        throw KError("Unsupported ELF class");

    off_t endhdr = std::max(endphdr, endshdr);
    if (endhdr > m_pagesize)
        m_map = map(0, endhdr);

    m_map->elf = elf_memory(m_map->data, m_map->length);
    if (!m_map->elf)
        throw KElfError("elf_memory() failed", elf_errno());

    if (elf_getphdrnum(m_map->elf, &m_phdrnum))
        throw KElfError("Cannot count ELF program headers", elf_errno());
    if (elf_getshdrnum(m_map->elf, &m_shdrnum))
        throw KElfError("Cannot count ELF section headers", elf_errno());
}

// -----------------------------------------------------------------------------
KElf::~KElf()
{
    if (m_fd >= 0)
        close(m_fd);
}

// -----------------------------------------------------------------------------
KElf::MappedData KElf::map(off_t offset, size_t length)
{
    // align offset
    size_t part = offset % m_pagesize;
    offset -= part;
    length += part;

    // align length
    part = length % m_pagesize;
    if (part)
        length += m_pagesize - part;

    MappedData ret(new struct Mapping(offset, length));
    void *addr = mmap(nullptr, length, PROT_READ, MAP_PRIVATE, m_fd, offset);
    ret->data = static_cast<char*>(addr);
    if (ret->data == MAP_FAILED) {
	addr = mmap(nullptr, length, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        ret->data = static_cast<char*>(addr);
	if (ret->data == MAP_FAILED)
	    throw KSystemError("Cannot map ELF data", errno);

	char *p = ret->data;
	while (length) {
	    ssize_t rd = pread(m_fd, p, length, offset);
	    if (rd < 0)
		throw KSystemError("ELF read error", errno);
	    else if (!rd)
		break;

	    p += rd;
            offset += rd;
	    length -= rd;
	}
    }

    return ret;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
