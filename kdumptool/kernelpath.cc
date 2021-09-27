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

#include <string>

#include "global.h"
#include "kernelpath.h"
#include "stringutil.h"
#include "util.h"
#include "debug.h"

//{{{ KernelPath ---------------------------------------------------------------

// -----------------------------------------------------------------------------
StringList KernelPath::imageNames(const std::string &arch)
{
    StringList ret;

    if (arch == "i386" || arch == "x86_64") {
        ret.emplace_back("vmlinuz");
        ret.emplace_back("vmlinux");
    } else if (arch == "ia64") {
        ret.emplace_back("vmlinuz");
    } else if (arch == "s390x") {
        ret.emplace_back("image");
    } else if (arch == "aarch64") {
        ret.emplace_back("Image");
    } else {
        ret.emplace_back("vmlinux");
    }

    return ret;
}

// -----------------------------------------------------------------------------
KernelPath::KernelPath(FilePath const &path)
{
    Debug::debug()->trace("KernelPath::KernelPath(%s)", path.c_str());

    m_directory = path.dirName();
    m_name = path.baseName();

    for (auto const pfx : imageNames(Util::getArch())) {
        if (m_name.startsWith(pfx)) {
            if (m_name.length() == pfx.length()) {
                m_name = pfx;
                break;
            }
            if (m_name[pfx.length()] == '-') {
                m_version.assign(m_name, pfx.length() + 1);
                m_name = pfx;
                break;
            }
        }
    }

    Debug::debug()->trace("directory=%s, name=%s, version=%s",
                          m_directory.c_str(), m_name.c_str(),
                          m_version.c_str());
}

// -----------------------------------------------------------------------------
bool KernelPath::isKdump()
{
    bool ret = m_version.endsWith("kdump");
    Debug::debug()->trace("KernelPath::isKdump(), version=%s: %s",
                          m_version.c_str(), ret ? "true" : "false");
    return ret;
}

// -----------------------------------------------------------------------------
FilePath KernelPath::configPath()
{
    FilePath ret(m_directory);
    ret.appendPath("config");
    if (!m_version.empty()) {
        ret.push_back('-');
        ret.append(m_version);
    }

    Debug::debug()->trace("KernelPath::configPath(): %s", ret.c_str());
    return ret;
}

// -----------------------------------------------------------------------------
FilePath KernelPath::initrdPath(bool fadump)
{
    FilePath ret(m_directory);
    ret.appendPath("initrd");
    if (!m_version.empty()) {
        ret.push_back('-');
        ret.append(m_version);
    }
    if (!fadump && !isKdump())
        ret.append("-kdump");

    Debug::debug()->trace("KernelPath::initrdPath(%s): %s",
                          fadump ? "true" : "false", ret.c_str());
    return ret;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
