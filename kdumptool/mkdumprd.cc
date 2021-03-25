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

#include <iostream>
#include <memory>

#include "global.h"
#include "debug.h"
#include "config.h"
#include "optionparser.h"
#include "configuration.h"
#include "install.h"
#include "fileutil.h"

#define PROGRAM_NAME                "mkdumprd"
#define PROGRAM_VERSION_STRING      PROGRAM_NAME " " PACKAGE_VERSION
#define DEFAULT_CONFIG              "/etc/sysconfig/kdump"

static const char SYSTEMD_UTIL_DIR[] = "/usr/lib/systemd";
static const char SYSTEM_UNIT_DIR[] = "/usr/lib/systemd/system";

// programs to be installed in the systemd util dir
static const char* const systemd_utils[] = {
    "systemd",
    "systemd-shutdown",
    NULL
};

// standard systemd units to be copied from the running system
static const char* const system_units[] = {
    "final.target",
    "reboot.target",
    "shutdown.target",
    "sysinit.target",
    "systemd-reboot.service",
    "umount.target",
    NULL
};

// systemd unit links
static struct {
    const char *path;
    const char *target;
} const system_unit_links[] = {
    { "default.target", "kdump.target" },
    { "ctrl-alt-del.target", "reboot.target" },
    { NULL, NULL }
};

using std::cerr;
using std::endl;
using std::string;

class MakeDumpRamDisk {

        Initrd m_cpio;

    public:
        MakeDumpRamDisk()
        { }

        bool parseCommandLine(int argc, char *argv[]);
        void printVersion();
        int execute();

    private:
        FilePath systemUnitPath(const char *name);
};

// -----------------------------------------------------------------------------
static void close_file(int error, void *arg)
{
    (void)error;
    fclose((FILE *)arg);
}

// -----------------------------------------------------------------------------
bool MakeDumpRamDisk::parseCommandLine(int argc, char *argv[])
{
    OptionParser optionParser;

    // global options
    bool doHelp = false;
    FlagOption helpOption(
        "help", 'h', &doHelp,
        "Print help output");
    optionParser.addGlobalOption(&helpOption);

    bool doVersion = false;
    FlagOption versionOption(
        "version", 'v', &doVersion,
        "Print version information and exit");
    optionParser.addGlobalOption(&versionOption);

    bool debugEnabled = false;
    FlagOption debugOption(
        "debug", 'D', &debugEnabled,
        "Print debugging output");
    optionParser.addGlobalOption(&debugOption);

    string logFileName;
    StringOption logFileOption(
        "logfile", 'L', &logFileName,
        "Use the specified logfile for debugging output");
    optionParser.addGlobalOption(&logFileOption);

    string configFileName(DEFAULT_CONFIG);
    StringOption configFileOption(
        "configfile", 'F', &configFileName,
        "Use the specified configuration file instead of " DEFAULT_CONFIG);
    optionParser.addGlobalOption(&configFileOption);

    optionParser.parse(argc, argv);

    // handle global operation
    if (doHelp) {
        optionParser.printHelp(cerr, PROGRAM_VERSION_STRING);
        return false;
    }
    if (doVersion) {
        printVersion();
        return false;
    }

    // debug messages
    if (logFileOption.isSet() && debugEnabled) {
        FILE *fp = fopen(logFileName.c_str(), "a");
        if (fp) {
            Debug::debug()->setFileHandle(fp);
            on_exit(close_file, fp);
        }
        Debug::debug()->dbg("STARTUP ----------------------------------");
    } else if (debugEnabled)
        Debug::debug()->setStderrLevel(Debug::DL_TRACE);

    Configuration *config = Configuration::config();
    config->readFile(configFileName);

    return true;
}

// -----------------------------------------------------------------------------
void MakeDumpRamDisk::printVersion()
{
    cerr << PROGRAM_VERSION_STRING << endl;
}

// -----------------------------------------------------------------------------
FilePath MakeDumpRamDisk::systemUnitPath(const char *name)
{
    return FilePath(SYSTEM_UNIT_DIR).appendPath(name);
}

// -----------------------------------------------------------------------------
int MakeDumpRamDisk::execute()
{
    // systemd binaries
    for (auto name = systemd_utils; *name; ++name) {
        FilePath path(SYSTEMD_UTIL_DIR);
        path.appendPath(*name);
        m_cpio.installProgram(path, SYSTEMD_UTIL_DIR);
    }

    // link to /init
    FilePath systemd(SYSTEMD_UTIL_DIR + 1);
    systemd.appendPath("systemd");
    m_cpio.symlink(systemd, "/init");

    // systemd system units
    for (auto name = system_units; *name; ++name)
        m_cpio.installFile(systemUnitPath(*name), SYSTEM_UNIT_DIR);
    m_cpio.installData("kdump.target", SYSTEM_UNIT_DIR);

    // additional systemd links
    for (auto link = system_unit_links; link->path; ++link)
        m_cpio.symlink(link->target, systemUnitPath(link->path));

    m_cpio.write(std::cout);

    return 0;
}

// -----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    int rc = 0;

    try {
        MakeDumpRamDisk mkdumprd;
        if (mkdumprd.parseCommandLine(argc, argv))
            rc = mkdumprd.execute();
    } catch (const KError &kerr) {
        cerr << kerr.what() << endl;
        rc = -1;
    } catch (const std::exception &ex) {
        cerr << "Fatal exception: " << ex.what() << endl;
        rc = -1;
    }

    return rc;
}
