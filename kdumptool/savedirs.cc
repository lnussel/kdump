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

#include <memory>
#include <map>
#include <string>

#include "savedirs.h"
#include "mounts.h"

using std::make_shared;
using std::shared_ptr;
using std::map;
using std::string;

//{{{ SaveDirs -----------------------------------------------------------------

SaveDirs::SaveDirs(StringList savedirs, DevicePathResolver& dpr)
{
    // mount target directory => first added mapping
    std::map<string, List::iterator> uniquemap;

    string rootdir;             // dummy
    unsigned mountidx = 0;
    for (auto const& dir : savedirs) {
        auto newmap = m_list.emplace(m_list.end(), dir, rootdir);

        // If this save directory is not a file, then no mount
        // point is necessary, and the mp remains @c nullptr.
        if (newmap->url.getProtocol() != URLParser::PROT_FILE)
            continue;

        auto mp = make_shared<PathMountPoint>(newmap->url.getRealPath(), dpr);
        auto result = uniquemap.emplace(mp->target(), newmap);
        if (result.second) {
            // not seen yet; use mount point and assign a new mount index
            newmap->mp = mp;
            newmap->mountidx = mountidx++;
        } else {
            // copy mount point and mountidx from an existing mapping
            auto const& prevmap = result.first->second;
            newmap->mp = prevmap->mp;
            newmap->mountidx = prevmap->mountidx;
        }
    }
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
