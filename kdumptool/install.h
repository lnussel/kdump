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

#ifndef INSTALL_H
#define INSTALL_H

#include <list>
#include <string>

#include "fileutil.h"
#include "cpio.h"

//{{{ SharedDependencies -------------------------------------------------------

/**
 * Dependencies of shared objects.
 */
class SharedDependencies {
    typedef std::list<std::string> List;
    List m_list;

public:
    SharedDependencies(std::string const &path);

    using iterator = List::iterator;
    iterator begin()
    { return m_list.begin(); }
    iterator end()
    { return m_list.end(); }

    using const_iterator = List::const_iterator;
    const_iterator begin() const
    { return m_list.begin(); }
    const_iterator end() const
    { return m_list.end(); }
};

//}}}
//{{{ Initrd -------------------------------------------------------------------

class Initrd : public CPIO_newc {
    public:
        static const char DATA_DIRECTORY[];

        Initrd()
            : CPIO_newc()
        { }

        /**
         * Install a data file into a target directory.
         *
         * @param[in] name Path under the kdump data directory
         * @param[in] destdir Destination directory in the initrd
         * @returns @c true if the file was added,
         *          @c false if the target path was already in the archive
         */
        bool installData(const char *name, const char *destdir);
};

//}}}

#endif /* INSTALL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
