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

#ifndef CPIO_H
#define CPIO_H

#include <ostream>
#include <string>
#include <list>
#include <memory>

//{{{ CPIOMember ---------------------------------------------------------------

class CPIOMember {

    protected:
        unsigned long m_ino;
        unsigned long m_mode;
        unsigned long m_uid;
        unsigned long m_gid;
        unsigned long m_nlink;
        unsigned long m_mtime;
        unsigned long m_filesize;
        unsigned long m_devmajor;
        unsigned long m_devminor;
        unsigned long m_rdevmajor;
        unsigned long m_rdevminor;
        std::string m_name;

    public:
        CPIOMember(std::string const &name);

        unsigned long ino() const
        { return m_ino; }

        unsigned long mode() const
        { return m_mode; }

        void uid(unsigned long val)
        { m_uid = val; }
        unsigned long uid() const
        { return m_uid; }

        void gid(unsigned long val)
        { m_gid = val; }
        unsigned long gid() const
        { return m_gid; }

        unsigned long nLink() const
        { return m_nlink; }

        void mtime(unsigned long val)
        { m_mtime = val; }
        unsigned long mtime() const
        { return m_mtime; }

        unsigned long fileSize() const
        { return m_filesize; }

        unsigned long devMajor() const
        { return m_devmajor; }

        unsigned long devMinor() const
        { return m_devminor; }

        unsigned long rDevMajor() const
        { return m_rdevmajor; }

        unsigned long rDevMinor() const
        { return m_rdevminor; }

        std::string const& name() const
        { return m_name; }

        virtual void writeData(std::ostream &os) const = 0;
};

//}}}
//{{{ CPIOTrailer --------------------------------------------------------------

class CPIOTrailer : public CPIOMember {
    public:
        CPIOTrailer();

        virtual void writeData(std::ostream &os) const;
};

//}}}
//{{{ CPIO_newc ----------------------------------------------------------------

/**
 * Producer of the new (SVR4) portable format, understood by the Linux
 * kernel as initramfs.
 */
class CPIO_newc {

        unsigned long m_size;

    public:
        using Member = std::shared_ptr<CPIOMember>;

        CPIO_newc()
            : m_size(0)
        { }

        void add(Member &&member)
        { m_members.push_back(member); }

        void write(std::ostream &os);

    protected:
        void writeMember(std::ostream &os, CPIOMember const &member);

        std::list<Member> m_members;
};

//}}}

#endif /* CPIO_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
