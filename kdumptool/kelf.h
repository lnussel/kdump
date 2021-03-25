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
#ifndef KELF_H
#define KELF_H

#include <string>
#include <memory>

#include <sys/types.h>
#include <libelf.h>
#include <gelf.h>

#include "global.h"

//{{{ KElfErrorCode ------------------------------------------------------------

/**
 * Class for libelf errors.
 */
class KElfErrorCode : public KErrorCode {
    public:
        KElfErrorCode(int code)
            : KErrorCode(code)
        {}

        virtual std::string message(void) const;
};

typedef KCodeError<KElfErrorCode> KElfError;

//}}}
//{{{ KElf ---------------------------------------------------------------------

/**
 * Class for accessing ELF files.
 */
class KElf {
    protected:
        int m_fd;
        size_t m_phdrnum;
        size_t m_shdrnum;

    public:
        /**
         * Open an ELF file by path.
         */
        KElf(std::string const &path);
        ~KElf();

        size_t phdrNum() const
        { return m_phdrnum; }

        /**
         * Get an ELF program header.
         *
         * @param[in] index Program header index.
         * @param[out] phdr Filled with program header data on success.
         */
        void getPhdr(int index, GElf_Phdr *phdr) const;

        size_t shdrNum() const
        { return m_shdrnum; }

        /**
         * Get an ELF section descriptor.
         *
         * @param[in] index Section index.
         * @returns Section descriptor.
         */
        Elf_Scn *getScn(int index) const;

        struct Mapping {
            char *data;
            Elf *elf;
            off_t offset;
            size_t length;

            Mapping(off_t _offset, size_t _length);
            ~Mapping();
        };
        typedef std::unique_ptr<struct Mapping> MappedData;
        MappedData map(off_t offset, size_t length);

        /**
         * Get data for the ELF DYNAMIC segment.
         *
         * @returns dynamic segment data,
         *          or @c nullptr if there is no dynamic segment
         */
        Elf_Data *dynamicData();

    private:
        MappedData m_map;
        long m_pagesize;

        // DYNAMIC segment
        Elf_Data *m_dynamic;
        MappedData m_dynamicmap;

        /**
         * Find the end of ELF program and section headers.
         *
         * On 32-bit systems, the whole ELF file cannot be mapped,
         * because a core dump may be larger than process virtual
         * address space. To parse the file with libelf, it is
         * necessary to map at least as much of the beginning of
         * the ELF file to cover all program and section headers.
         *
         * @returns end offset of program and section headers
         */
        off_t _headersEndOffset();
};

//}}}

#endif /* KELF_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
