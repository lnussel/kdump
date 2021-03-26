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

#ifndef SAVEDIRS_H
#define SAVEDIRS_H

#include <memory>

#include "global.h"
#include "rootdirurl.h"
#include "mounts.h"

//{{{ SaveDirMapping -----------------------------------------------------------

/**
 * Mapping between a savedir URL and its location in the initrd.
 */
struct SaveDirMapping {
    /**
     * Parsed destination URL.
     */
    RootDirURL url;

    /**
     * Pointer to the corresponding mount point,
     * or @c nullptr if this destination does not need a mount
     * (non-file destination).
     */
    std::shared_ptr<PathMountPoint> mp;

    /**
     * Unique mount index in initrd. Valid only for file destinations,
     * otherwise undefined.
     */
    unsigned mountidx;

    SaveDirMapping(std::string const& _url, std::string const& _rootdir)
        : url(_url, _rootdir), mp(nullptr)
    { }
};

//}}}
//{{{ SaveDirs -----------------------------------------------------------------

/**
 * Parsed save directories
 */
class SaveDirs {
    public:
        typedef std::list<SaveDirMapping> List;

        SaveDirs(StringList savedirs, DevicePathResolver& dpr);

        List const& list() const
        { return m_list; }

    private:
        List m_list;
};

//}}}

#endif /* SAVEDIRS_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
