/*
 * (c) 2008, Bernhard Walle <bwalle@suse.de>, SUSE LINUX Products GmbH
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
#include <string>
#include <cstring>
#include <cstdlib>
#include <cerrno>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <libelf.h>
#include <gelf.h>

#include "global.h"
#include "kelf.h"
#include "util.h"
#include "debug.h"
#include "fileutil.h"

using std::string;
using std::strerror;
using std::getenv;

// -----------------------------------------------------------------------------
std::string Util::getArch()
{
    struct utsname utsname;

    int ret = uname(&utsname);
    if (ret < 0) {
        throw KSystemError("uname failed", errno);
    }

    string arch = string(utsname.machine);
    if (arch == "i486" || arch == "i586" || arch == "i686") {
        return string("i386");
    } else {
        return arch;
    }
}

// -----------------------------------------------------------------------------
std::string Util::getKernelRelease()
{
    struct utsname utsname;

    int ret = uname(&utsname);
    if (ret < 0) {
        throw KSystemError("uname failed", errno);
    }

    return utsname.release;
}


// -----------------------------------------------------------------------------
bool Util::isGzipFile(int fd)
{
    unsigned char buffer[2];

    off_t oret = lseek(fd, 0, SEEK_SET);
    if (oret == (off_t)-1) {
        throw KSystemError("Cannot lseek() in Util::isGzipFile()", errno);
    }

    /* read the first 2 bytes */
    int ret = read(fd, buffer, 2);
    if (ret < 0)
        throw KSystemError("Util::isGzipFile: read failed", errno);

    return buffer[0] == 0x1f && buffer[1] == 0x8b;
}

// -----------------------------------------------------------------------------
bool Util::isGzipFile(const string &file)
{
    return isGzipFile(FileDescriptor(file, O_RDONLY));
}

// -----------------------------------------------------------------------------
bool Util::isElfFile(int fd)
{
    int     dupfd;
    gzFile  fp = NULL;
    int     err;
    char    buffer[EI_MAG3+1];
    
    Debug::debug()->trace("isElfFile(%d)", fd);

    // the dup() is because we want to keep the fd open on gzclose()
    dupfd = dup(fd);
    if (dupfd < 0) {
        throw KError("gzfile dup failed");
    }
    lseek(dupfd, 0, SEEK_SET);

    fp = gzdopen(dupfd, "r");
    if (!fp) {
        close(dupfd);
        throw KError("gzopen failed");
    }

    err = gzread(fp, buffer, EI_MAG3+1);
    if (err != (EI_MAG3+1)) {
        gzclose(fp);
        throw KError("IdentifyKernel::isElfFile: Couldn't read bytes");
    }

    gzclose(fp);

    return buffer[EI_MAG0] == ELFMAG0 && buffer[EI_MAG1] == ELFMAG1 &&
            buffer[EI_MAG2] == ELFMAG2 && buffer[EI_MAG3] == ELFMAG3;
}

// -----------------------------------------------------------------------------
bool Util::isElfFile(const string &file)
{
    return isElfFile(FileDescriptor(file, O_RDONLY));
}

// -----------------------------------------------------------------------------
bool Util::isXenCoreDump(const string &file)
{
    Debug::debug()->trace("isXenCoreDump(%s)", file.c_str());

    KElf kelf(file);
    for (size_t i = 0; i < kelf.phdrNum(); ++i) {
        GElf_Phdr phdr;
        kelf.getPhdr(i, &phdr);
        if (phdr.p_type != PT_NOTE)
            continue;

        KElf::MappedData mapped = kelf.map(phdr.p_offset, phdr.p_filesz);
        size_t to_read = phdr.p_filesz;
        char *p = mapped->data + phdr.p_offset - mapped->offset;
        while (to_read) {
            Elf64_Nhdr *hdr = reinterpret_cast<Elf64_Nhdr*>(p);
            if (to_read < sizeof(*hdr))
                break;
            to_read -= sizeof(*hdr);
            p += sizeof(*hdr);

            size_t notesz =
                ((hdr->n_namesz + 3) & (-(Elf64_Word)4)) +
                ((hdr->n_descsz + 3) & (-(Elf64_Word)4));
            if (to_read < notesz)
                break;

            if ((hdr->n_namesz == 3 && !memcmp(p, "Xen", 3)) ||
                (hdr->n_namesz == 4 && !memcmp(p, "Xen", 4))) {
                Debug::debug()->dbg("Xen ELF note found");
                return true;
            }

            to_read -= notesz;
            p += notesz;
        }
    }

    Debug::debug()->dbg("NOT a Xen core dump", file.c_str());
    return false;
}

// -----------------------------------------------------------------------------
bool Util::isX86(const string &arch)
{
    return arch == "i386" || arch == "i686" ||
        arch == "i586" || arch == "x86_64";
}

// -----------------------------------------------------------------------------
void Util::daemonize()
{
    int i;
    int maxfd;
    pid_t pid;

    pid = fork();
    if (pid)
        exit(0);
    else if (pid < 0)
        throw KSystemError("fork() failed.", errno);

    if (setsid() < 0)
        throw KSystemError("Cannot become session leader.", errno);

    if (chdir("/") < 0) {
        throw KSystemError("chdir(/) failed", errno);
    }

    umask(0);

    /* close all open file descriptors */
    maxfd = sysconf(_SC_OPEN_MAX);
    for(i = maxfd; i > 0; i--)
       close(i);
}

// -----------------------------------------------------------------------------
bool Util::isZero(const char *buffer, size_t size)
{
    for (size_t i = 0; i < size; i++)
        if (buffer[i] != 0)
            return false;

    return true;
}

// -----------------------------------------------------------------------------
string Util::getHostDomain()
{
    struct utsname my_utsname;
    int ret = uname(&my_utsname);
    if (ret != 0)
        throw KSystemError("uname() failed.", errno);

    string nodename = my_utsname.nodename;
    string domainname = my_utsname.domainname;

    if (domainname.size() == 0)
        return nodename;
    else
        return nodename + "." + domainname;
}

// -----------------------------------------------------------------------------
ssize_t Util::findBytes(const unsigned char *haystack, size_t haystack_len,
                        const unsigned char *needle, size_t needle_len)
{
    for (size_t i = 0; i < (haystack_len-needle_len); i++) {
        bool found = true;
        for (size_t j = 0; j < needle_len; j++) {
            if (haystack[i+j] != needle[j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return i;
        }
    }

    return -1;
}

// -----------------------------------------------------------------------------
string Util::getenv(const string &env, const string &defaultValue, bool *isDefault)
{
    char *ret = ::getenv(env.c_str());
    if (ret == NULL) {
        if (isDefault) {
            *isDefault = true;
        }
        return defaultValue;
    } else {
        if (isDefault) {
            *isDefault = false;
        }
        return string(ret);
    }
}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
