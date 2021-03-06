#include "utils.h"
#include "cmdlineparser.h"
#include <QString>
#include <cstdlib>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <target.h>
#include <sys/mount.h>

Utils::Utils(Logger *logger)
{
    this->logger = logger;
}

QString Utils::getOSMCDev()
{
    char osmcdev[10];
    get_cmdline_option("osmcdev=", osmcdev, sizeof(osmcdev));
    return (QString(osmcdev).simplified());
}
void Utils::rebootSystem()
{
    /* Only reboot on real system */
    system("/bin/sh -c \"/bin/echo 1 > /proc/sys/kernel/sysrq\"");
    system("/bin/sh -c \"/bin/echo b > /proc/sysrq-trigger\"");
}

void inline Utils::updateDevTable()
{
    system("/usr/sbin/partprobe");
}

bool Utils::mklabel(QString device, bool isGPT)
{
    QProcess partedProcess;
    logger->addLine("Going to mklabel with device = " + device + " and isGPT " + isGPT);
    if (isGPT)
        partedProcess.start("/usr/sbin/parted -s " + device.toLocal8Bit() + " mklabel gpt");
    else
        partedProcess.start("/usr/sbin/parted -s " + device.toLocal8Bit() + " mklabel msdos");
    partedProcess.waitForFinished(-1);
    logger->addLine("mklabel finished with exitCode: " + partedProcess.exitCode());
    updateDevTable();
    return partedProcess.exitCode() == 0;
}

int Utils::getPartSize(QString device, QString fstype)
{
    QString command("/usr/sbin/parted -s " + device.toLocal8Bit() + " print | grep " + fstype + " | awk {'print $4'} | tr -d MB");
    QProcess partedProcess;
    partedProcess.start("/bin/sh -c \"" + command + "\"");
    partedProcess.waitForFinished(-1);
    int exitCode = partedProcess.exitCode();
    if (exitCode != 0)
        return -1;
    return QString(partedProcess.readAll()).toInt();
}

bool Utils::mkpart(QString device, QString fstype, QString start, QString end)
{
    logger->addLine("Calling mkpart for device: " + device + " and fs: " + fstype + " with start " + start + " and end " + end);
    QProcess partedProcess;
    partedProcess.start("/usr/sbin/parted -s " + device.toLocal8Bit() + " mkpart primary " + fstype + " " + start + " " + end);
    partedProcess.waitForFinished(-1);
    updateDevTable();
    logger->addLine("mkpart exitCode: " + QString::number(partedProcess.exitCode()));
    logger->addLine("stderr was " + QString(partedProcess.readAllStandardError()));
    return partedProcess.exitCode() == 0;
}

bool Utils::fmtpart(QString partition, QString fstype)
{
   logger->addLine("Calling fmtpart for partition " + partition + " and fstype " + fstype);
    QProcess mkfsProcess;
    if (fstype == "ext4")
    {
        logger->addLine("Command would be /usr/sbin/mkfs.ext4 " + partition);
        mkfsProcess.start("/usr/sbin/mkfs.ext4 " + partition);
    }
    else if (fstype == "vfat")
    {
        logger->addLine("Command would be /usr/sbin/mkfs.ext4 " + partition);
        mkfsProcess.start("/usr/sbin/mkfs.vfat -F 32 " + partition);
    }
    mkfsProcess.waitForFinished(-1);
    logger->addLine("fmtpart returned with " + QString::number(mkfsProcess.exitCode()));
    logger->addLine("full output was: " + QString(mkfsProcess.readAll()));
    logger->addLine("stderr was " + QString(mkfsProcess.readAllStandardError()));
    logger->addLine("stdout was "  +QString(mkfsProcess.readAllStandardOutput()));
    return mkfsProcess.exitCode() == 0;
}

void Utils::writeToFile(QFile &file, QStringList strings, bool append)
{
    if (append)
        file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    else
        file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream stream(&file);
    for (int i = 0; i < strings.count(); i++)
    {
        stream << strings.at(i);
    }
    stream.flush();
    file.close();
}

bool Utils::mountPartition(Target *device, QString path)
{
    QDir pathdir;
    pathdir.mkpath(path);
    if (path == QString(MNT_BOOT))
    {
        logger->addLine("Trying to mount to MNT_BOOT ("+QString(MNT_BOOT));
        logger->addLine("Using device.boot: " + device->getBoot() + " and FS: " + device->getBootFS());
        return (mount(device->getBoot().toLocal8Bit(), MNT_BOOT, device->getBootFS().toLocal8Bit(), (device->isBootRW() == true) ? 0 : 1, "") == 0) ? true : false;
    }
    else if (path == QString(MNT_ROOT))
    {
        logger->addLine("Trying to mount to MNT_ROOT ("+QString(MNT_ROOT));
        logger->addLine("Using device.root: " + device->getRoot());
        if (device->getRoot().contains(":/") && device->hasRootChanged())
        {
            logger->addLine("Assuming NFS mount.");
            /* This is an NFS share, use BusyBox */
            QProcess mountProcess;
            mountProcess.start("/bin/mount -t nfs -o nolock,noatime " + device->getRoot().toLocal8Bit() + " " +  MNT_ROOT);
            mountProcess.waitForFinished(-1);
            if (mountProcess.exitCode() == 0)
                return true;
            else
                return false;
        }

        else
        {
            return (mount(device->getRoot().toLocal8Bit(), MNT_ROOT, "ext4", 0, "") == 0) ? true : false;
        }
    }
    logger->addLine("Unsupported mountpoint.");
    return false;
}
