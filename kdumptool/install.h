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

#endif /* INSTALL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
