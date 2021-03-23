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

#include <ostream>
#include <iomanip>
#include <string>
#include <cerrno>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/sysmacros.h>

#include "global.h"
#include "cpio.h"

using std::ostream;
using std::setw;

//{{{ CPIOMember ---------------------------------------------------------------

// -----------------------------------------------------------------------------
CPIOMember::CPIOMember(std::string const &name)
    : m_ino(0), m_mode(0), m_uid(0), m_gid(0), m_nlink(1),
      m_mtime(0), m_filesize(0), m_devmajor(0), m_devminor(0),
      m_rdevmajor(0), m_rdevminor(0), m_name(name)
{
}

//}}}
//{{{ CPIOTrailer --------------------------------------------------------------

// -----------------------------------------------------------------------------
CPIOTrailer::CPIOTrailer()
    : CPIOMember("TRAILER!!!")
{
}

// -----------------------------------------------------------------------------
void CPIOTrailer::writeData(ostream &os) const
{
    // no data
}

//}}}
//{{{ CPIOSynth ----------------------------------------------------------------

int CPIOSynth::m_lastino;

//}}}
//{{{ CPIODirectory ------------------------------------------------------------

// -----------------------------------------------------------------------------
CPIODirectory::CPIODirectory(std::string const &name)
    : CPIOSynth(name)
{
    m_mode = 040000;            // Directory
};

// -----------------------------------------------------------------------------
void CPIODirectory::writeData(std::ostream &os) const
{
    // no data
}

//}}}
//{{{ CPIOMemory ---------------------------------------------------------------

// -----------------------------------------------------------------------------
CPIOMemory::CPIOMemory(std::string const &name, const char *buf, size_t len)
    : CPIOSynth(name), m_buf(buf)
{
    m_mode = 0100000;           // Regular file
    m_filesize = len;
}

// -----------------------------------------------------------------------------
void CPIOMemory::writeData(std::ostream &os) const
{
    os.write(m_buf, m_filesize);
}

//}}}
//{{{ CPIOFile -----------------------------------------------------------------

// -----------------------------------------------------------------------------
CPIOFile::CPIOFile(std::string const &dstpath, std::string const &srcpath)
    : CPIOMember(dstpath), m_srcpath(srcpath)
{
    struct stat s;
    if (stat(m_srcpath.c_str(), &s))
        throw KSystemError("Cannot stat " + srcpath, errno);

    m_ino = s.st_ino;
    m_mode = s.st_mode;
    m_uid = s.st_uid;
    m_gid = s.st_gid;
    m_mtime = s.st_mtim.tv_sec;
    m_filesize = s.st_size;
    m_devmajor = major(s.st_dev);
    m_devminor = minor(s.st_dev);
    m_rdevmajor = major(s.st_rdev);
    m_rdevminor = minor(s.st_rdev);
}

// -----------------------------------------------------------------------------
void CPIOFile::writeData(ostream &os) const
{
    std::ifstream fin(m_srcpath);
    os << fin.rdbuf();
}

//}}}
//{{{ CPIO_newc ----------------------------------------------------------------

// -----------------------------------------------------------------------------
bool CPIO_newc::add(Member &&member)
{
    return m_members.emplace(member->name(), member).second;
}

// -----------------------------------------------------------------------------
void CPIO_newc::write(ostream &os)
{
    static const unsigned long BLKLEN = 512;

    for (auto m : m_members)
        writeMember(os, *m.second);

    CPIOTrailer trailer;
    writeMember(os, trailer);

    if (m_size % BLKLEN)
        os << setw(512 - (m_size % BLKLEN)) << std::setfill('\0') << '\0';
}

// -----------------------------------------------------------------------------
void CPIO_newc::writeMember(ostream &os, CPIOMember const &member)
{
    static const char MAGIC[] = "070701";
    static const char pad[4] = { };

    std::streamsize padsz;

    os << MAGIC
       << std::hex << std::uppercase << std::setfill('0')
       << setw(8) << member.ino()
       << setw(8) << member.mode()
       << setw(8) << member.uid()
       << setw(8) << member.gid()
       << setw(8) << member.nLink()
       << setw(8) << member.mtime()
       << setw(8) << member.fileSize()
       << setw(8) << member.devMajor()
       << setw(8) << member.devMinor()
       << setw(8) << member.rDevMajor()
       << setw(8) << member.rDevMinor()
       << setw(8) << member.name().size() + 1
       << setw(8) << 0
       << member.name();
    m_size += 6 + 8 * 13 + member.name().size();

    padsz = 4 - (m_size % 4);
    os.write(pad, padsz);
    m_size += padsz;

    member.writeData(os);
    m_size += member.fileSize();

    padsz = 4 - (m_size % 4);
    if (padsz != 4) {
        os.write(pad, padsz);
        m_size += padsz;
    }
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
