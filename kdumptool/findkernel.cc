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
#include <iostream>
#include <string>
#include <zlib.h>
#include <libelf.h>
#include <gelf.h>
#include <cerrno>
#include <fcntl.h>

#include "subcommand.h"
#include "debug.h"
#include "findkernel.h"
#include "util.h"
#include "kerneltool.h"
#include "configuration.h"
#include "fileutil.h"
#include "stringutil.h"
#include "kconfig.h"
#include "stringvector.h"
#include "kernelpath.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::list;


/**
 * Constant for the directory where the kernels are.
 */
#define BOOTDIR "/boot"

/**
 * Constant that defines the number of CPUs above that the auto-detection
 * should try to avoid that kernel.
 */
#define MAXCPUS_KDUMP 1024

//{{{ FindKernel ---------------------------------------------------------------

// -----------------------------------------------------------------------------
FindKernel::FindKernel()
{}

// -----------------------------------------------------------------------------
const char *FindKernel::getName() const
{
    return "find_kernel";
}

// -----------------------------------------------------------------------------
bool FindKernel::getPaths(FilePath &kernelimage, FilePath &initrd)
{
    Debug::debug()->trace("FindKernel::getPaths()");

    const string &kernelver = Configuration::config()->KDUMP_KERNELVER.value();

    // user has specified a specific kernel, check that first
    if (kernelver.size() > 0) {
        kernelimage = findForVersion(kernelver);
        if (kernelimage.empty()) {
            Debug::debug()->info(
                "KDUMP_KERNELVER is set to '%s', but no such kernel exists.",
                kernelver.c_str());
            return false;
        }

        // suitable?
        if (!suitableForKdump(kernelimage, false)) {
            Debug::debug()->info(
                "Kernel '%s' is not suitable for kdump.",
                kernelimage.c_str());
            Debug::debug()->info("Please change KDUMP_KERNELVER.");
            return false;
        }
    } else {
        kernelimage = findKernelAuto();
        if (kernelimage.empty())
            return false;
    }

    // found!
    KernelPath kpath(kernelimage);
#if HAVE_FADUMP
    const bool use_fadump =
        Configuration::config()->KDUMP_FADUMP.value();
#else
    const bool use_fadump = false;
#endif

    initrd = kpath.initrdPath(use_fadump);

    Debug::debug()->trace("FindKernel::getPaths(): kernel=%s, initrd=%s",
                          kernelimage.c_str(), initrd.c_str());
    return true;
}

// -----------------------------------------------------------------------------
void FindKernel::execute()
{
    Debug::debug()->trace("FindKernel::execute()");

    FilePath kernelimage, initrd;
    if (!getPaths(kernelimage, initrd)) {
        cerr << "No suitable kdump kernel found." << endl;
        setErrorCode(-1);
        return;
    }

    cout << "Kernel:\t" << kernelimage << endl;
    cout << "Initrd:\t" << initrd << endl;
}

// -----------------------------------------------------------------------------
bool FindKernel::suitableForKdump(const string &kernelImage, bool strict)
{
    KernelTool kt(kernelImage);

    // if that's not a special kdump kernel, it must be relocatable
    // TODO: check about start address, don't trust the naming
    if (isKdumpKernel(kernelImage)) {
        Debug::debug()->dbg("%s is kdump kernel, no need for relocatable check",
            kernelImage.c_str());
    } else {
        bool relocatable = kt.isRelocatable();
        Debug::debug()->dbg("%s is %s", kernelImage.c_str(),
            relocatable ? "relocatable" : "not relocatable");
        if (!relocatable) {
            return false;
        }
    }

    Kconfig *kconfig = kt.retrieveKernelConfig();
    KconfigValue kv;
    bool isxen;

    // Avoid Xenlinux kernels, because they do not run on bare metal
    kv = kconfig->get("CONFIG_X86_64_XEN");
    isxen = (kv.getType() == KconfigValue::T_TRISTATE &&
             kv.getTristateValue() == KconfigValue::ON);
    if (!isxen) {
        kv = kconfig->get("CONFIG_X86_XEN");
        isxen = (kv.getType() == KconfigValue::T_TRISTATE &&
                 kv.getTristateValue() == KconfigValue::ON);
    }
    if (isxen) {
        Debug::debug()->dbg("%s is a Xen kernel. Avoid.",
            kernelImage.c_str());
	delete kconfig;
	return false;
    }

    if (strict) {
        string arch = Util::getArch();

        // avoid large number of CPUs on x86 since that increases
        // memory size constraints of the capture kernel
        if (arch == "i386" || arch == "x86_64") {
            kv = kconfig->get("CONFIG_NR_CPUS");
            if (kv.getType() == KconfigValue::T_INTEGER &&
                    kv.getIntValue() > MAXCPUS_KDUMP) {
                Debug::debug()->dbg("NR_CPUS of %s is %d >= %d. Avoid.",
                    kernelImage.c_str(), kv.getIntValue(), MAXCPUS_KDUMP);
                delete kconfig;
                return false;
            }
        }

        // avoid realtime kernels
        kv = kconfig->get("CONFIG_PREEMPT_RT");
        if (kv.getType() != KconfigValue::T_INVALID) {
            Debug::debug()->dbg("%s is realtime kernel. Avoid.",
                kernelImage.c_str());
            delete kconfig;
            return false;
        }
    }

    delete kconfig;

    return true;
}

// -----------------------------------------------------------------------------
FilePath FindKernel::findForVersion(const string &kernelver)
{
    Debug::debug()->trace("FindKernel::findForVersion(%s)", kernelver.c_str());

    for (auto imagename : KernelPath::imageNames(Util::getArch())) {
        FilePath kernel = BOOTDIR;
        if (kernelver.size() == 0) {
            kernel.appendPath(imagename);
        } else {
            kernel.appendPath(imagename+"-"+kernelver);
        }

        Debug::debug()->dbg("findForVersion: Trying %s", kernel.c_str());
        if (kernel.exists()) {
            Debug::debug()->dbg("%s exists", kernel.c_str());
            return kernel;
        }
    }

    return "";
}

// -----------------------------------------------------------------------------
FilePath FindKernel::findKernelAuto()
{
    Debug::debug()->trace("FindKernel::findKernelAuto()");

    KString runningkernel = Util::getKernelRelease();
    Debug::debug()->trace("Running kernel: %s", runningkernel.c_str());
    
    // $(uname -r) == KERNELVERSION
    // KERNELVERSION := BASEVERSION + '-' + FLAVOUR

    // 1. Use BASEVERSION-kdump
    StringVector elements = runningkernel.split('-');
    elements[elements.size()-1] = "kdump";
    string testkernel = elements.join('-');
    Debug::debug()->dbg("---------------");
    Debug::debug()->dbg("findKernelAuto: Trying %s", testkernel.c_str());
    FilePath testkernelimage = findForVersion(testkernel);
    if (testkernelimage.size() > 0 && suitableForKdump(testkernelimage, true)) {
        return testkernelimage;
    }

    // 2. Use kdump
    testkernel = "kdump";
    Debug::debug()->dbg("---------------");
    Debug::debug()->dbg("findKernelAuto: Trying %s", testkernel.c_str());
    testkernelimage = findForVersion(testkernel);
    if (testkernelimage.size() > 0 && suitableForKdump(testkernelimage, true)) {
        return testkernelimage;
    }

    // 3. Use KERNELVERSION
    testkernel = runningkernel;
    Debug::debug()->dbg("---------------");
    Debug::debug()->dbg("findKernelAuto: Trying %s", testkernel.c_str());
    testkernelimage = findForVersion(testkernel);
    if (testkernelimage.size() > 0 && suitableForKdump(testkernelimage, true)) {
        return testkernelimage;
    }

    // 4. Use BASEVERSION-default
    elements = runningkernel.split('-');
    elements[elements.size()-1] = "default";
    testkernel = elements.join('-');
    Debug::debug()->dbg("---------------");
    Debug::debug()->dbg("findKernelAuto: Trying %s", testkernel.c_str());
    testkernelimage = findForVersion(testkernel);
    if (testkernelimage.size() > 0 && suitableForKdump(testkernelimage, true)) {
        return testkernelimage;
    }

    // 5. Use ""
    testkernel = "";
    Debug::debug()->dbg("---------------");
    Debug::debug()->dbg("findKernelAuto: Trying %s", testkernel.c_str());
    testkernelimage = findForVersion(testkernel);
    if (testkernelimage.size() > 0 && suitableForKdump(testkernelimage, true)) {
        return testkernelimage;
    }

    // 6. Use KERNELVERSION unstrict
    testkernel = runningkernel;
    Debug::debug()->dbg("---------------");
    Debug::debug()->dbg("findKernelAuto: Trying %s", testkernel.c_str());
    testkernelimage = findForVersion(testkernel);
    if (testkernelimage.size() > 0 && suitableForKdump(testkernelimage, false)) {
        return testkernelimage;
    }

    // 7. Use BASEVERSION-default unstrict
    elements = runningkernel.split('-');
    elements[elements.size()-1] = "default";
    testkernel = elements.join('-');
    Debug::debug()->dbg("---------------");
    Debug::debug()->dbg("findKernelAuto: Trying %s", testkernel.c_str());
    testkernelimage = findForVersion(testkernel);
    if (testkernelimage.size() > 0 && suitableForKdump(testkernelimage, false)) {
        return testkernelimage;
    }

    // 8. Use "" unstrict
    testkernel = "";
    Debug::debug()->dbg("---------------");
    Debug::debug()->dbg("findKernelAuto: Trying %s", testkernel.c_str());
    testkernelimage = findForVersion(testkernel);
    if (testkernelimage.size() > 0 && suitableForKdump(testkernelimage, false)) {
        return testkernelimage;
    }

    return "";
}

// -----------------------------------------------------------------------------
bool FindKernel::isKdumpKernel(const KString &kernelimage)
{
    bool ret = kernelimage.endsWith("kdump");
    Debug::debug()->trace("FindKernel::isKdumpKernel(%s)=%s",
        kernelimage.c_str(), ret ? "true" : "false");
    return ret;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
