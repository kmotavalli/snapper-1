/*
 * Copyright (c) [2011-2012] Novell, Inc.
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail, you may
 * find current contact information at www.novell.com.
 */


#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <mntent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <asm/types.h>

#include "snapper/Log.h"
#include "snapper/Filesystem.h"
#include "snapper/Snapper.h"
#include "snapper/SnapperTmpl.h"
#include "snapper/SystemCmd.h"
#include "snapper/SnapperDefines.h"


#define BTRFS_IOCTL_MAGIC 0x94
#define BTRFS_PATH_NAME_MAX 4087
#define BTRFS_SUBVOL_NAME_MAX 4039
#define BTRFS_SUBVOL_RDONLY (1ULL << 1)

#define BTRFS_IOC_SUBVOL_CREATE _IOW(BTRFS_IOCTL_MAGIC, 14, struct btrfs_ioctl_vol_args)
#define BTRFS_IOC_SNAP_DESTROY _IOW(BTRFS_IOCTL_MAGIC, 15, struct btrfs_ioctl_vol_args)
#define BTRFS_IOC_SNAP_CREATE_V2 _IOW(BTRFS_IOCTL_MAGIC, 23, struct btrfs_ioctl_vol_args_v2)

struct btrfs_ioctl_vol_args
{
    __s64 fd;
    char name[BTRFS_PATH_NAME_MAX + 1];
};

struct btrfs_ioctl_vol_args_v2
{
    __s64 fd;
    __u64 transid;
    __u64 flags;
    __u64 unused[4];
    char name[BTRFS_SUBVOL_NAME_MAX + 1];
};


namespace snapper
{

    Filesystem*
    Filesystem::create(const string& fstype, const string& subvolume)
    {
	if (fstype == "btrfs")
	    return new Btrfs(subvolume);

	if (fstype == "ext4")
	    return new Ext4(subvolume);

	throw InvalidConfigException();
    }


    SDir
    Filesystem::openSubvolumeDir() const
    {
	SDir subvolume_dir(subvolume);

	return subvolume_dir;
    }


    SDir
    Filesystem::openInfoDir(unsigned int num) const
    {
	SDir infos_dir = openInfosDir();
	SDir info_dir(infos_dir, decString(num));

	return info_dir;
    }


    Btrfs::Btrfs(const string& subvolume)
	: Filesystem(subvolume)
    {
	if (access(BTRFSBIN, X_OK) != 0)
	{
	    throw ProgramNotInstalledException(BTRFSBIN " not installed");
	}
    }


    void
    Btrfs::createConfig() const
    {
	SDir subvolume_dir = openSubvolumeDir();

	if (!create_subvolume(subvolume_dir.fd(), ".snapshots"))
	{
	    y2err("create subvolume failed errno:" << errno << " (" << stringerror(errno) << ")");
	    throw CreateConfigFailedException("creating btrfs snapshot failed");
	}
    }


    void
    Btrfs::deleteConfig() const
    {
	SDir subvolume_dir = openSubvolumeDir();

	if (!delete_subvolume(subvolume_dir.fd(), ".snapshots"))
	{
	    y2err("delete subvolume failed errno:" << errno << " (" << stringerror(errno) << ")");
	    throw DeleteConfigFailedException("deleting btrfs snapshot failed");
	}
    }


    string
    Btrfs::infosDir() const
    {
	return (subvolume == "/" ? "" : subvolume) + "/.snapshots";
    }


    string
    Btrfs::snapshotDir(unsigned int num) const
    {
	return (subvolume == "/" ? "" : subvolume) + "/.snapshots/" +
	    decString(num) + "/snapshot";
    }


    SDir
    Btrfs::openInfosDir() const
    {
	SDir subvolume_dir = openSubvolumeDir();
	SDir infos_dir(subvolume_dir, ".snapshots");

	struct stat stat;
	if (infos_dir.stat(".", &stat, AT_SYMLINK_NOFOLLOW) != 0)
	{
	    throw IOErrorException();
	}

	if (stat.st_uid != 0 || stat.st_gid != 0)
	{
	    y2err("owner of .snapshots wrong");
	    throw IOErrorException();
	}

	return infos_dir;
    }


    SDir
    Btrfs::openSnapshotDir(unsigned int num) const
    {
	SDir info_dir = openInfoDir(num);
	SDir snapshot_dir(info_dir, "snapshot");

	return snapshot_dir;
    }


    void
    Btrfs::createSnapshot(unsigned int num) const
    {
	SDir subvolume_dir = openSubvolumeDir();
	SDir info_dir = openInfoDir(num);

	if (!create_snapshot(subvolume_dir.fd(), info_dir.fd(), "snapshot"))
	{
	    y2err("create snapshot failed errno:" << errno << " (" << stringerror(errno) << ")");
	    throw CreateSnapshotFailedException();
	}
    }


    void
    Btrfs::deleteSnapshot(unsigned int num) const
    {
	SDir info_dir = openInfoDir(num);

	if (!delete_subvolume(info_dir.fd(), "snapshot"))
	{
	    y2err("delete snapshot failed errno:" << errno << " (" << stringerror(errno) << ")");
	    throw DeleteSnapshotFailedException();
	}
    }


    bool
    Btrfs::isSnapshotMounted(unsigned int num) const
    {
	return true;
    }


    void
    Btrfs::mountSnapshot(unsigned int num) const
    {
    }


    void
    Btrfs::umountSnapshot(unsigned int num) const
    {
    }


    bool
    Btrfs::checkSnapshot(unsigned int num) const
    {
	try
	{
	    SDir info_dir = openInfoDir(num);

	    struct stat stat;
	    int r = info_dir.stat("snapshot", &stat, AT_SYMLINK_NOFOLLOW);
	    // check st_ino == 256 is copied from btrfsprogs
	    return r == 0 && stat.st_ino == 256 && S_ISDIR(stat.st_mode);
	}
	catch (const IOErrorException& e)
	{
	    return false;
	}
    }


    bool
    Btrfs::create_subvolume(int fddst, const string& name) const
    {
	struct btrfs_ioctl_vol_args args;
	memset(&args, 0, sizeof(args));

	strncpy(args.name, name.c_str(), BTRFS_PATH_NAME_MAX);

        return ioctl(fddst, BTRFS_IOC_SUBVOL_CREATE, &args) == 0;
    }


    bool
    Btrfs::create_snapshot(int fd, int fddst, const string& name) const
    {
	struct btrfs_ioctl_vol_args_v2 args;
	memset(&args, 0, sizeof(args));

	args.fd = fd;
	args.flags |= BTRFS_SUBVOL_RDONLY;
	strncpy(args.name, name.c_str(), BTRFS_SUBVOL_NAME_MAX);

	return ioctl(fddst, BTRFS_IOC_SNAP_CREATE_V2, &args) == 0;
    }


    bool
    Btrfs::delete_subvolume(int fd, const string& name) const
    {
	struct btrfs_ioctl_vol_args args;
	memset(&args, 0, sizeof(args));

	strncpy(args.name, name.c_str(), BTRFS_PATH_NAME_MAX);

	return ioctl(fd, BTRFS_IOC_SNAP_DESTROY, &args) == 0;
    }


    Ext4::Ext4(const string& subvolume)
	: Filesystem(subvolume)
    {
	if (access(CHSNAPBIN, X_OK) != 0)
	{
	    throw ProgramNotInstalledException(CHSNAPBIN " not installed");
	}

	if (access(CHATTRBIN, X_OK) != 0)
	{
	    throw ProgramNotInstalledException(CHATTRBIN " not installed");
	}
    }


    void
    Ext4::createConfig() const
    {
	int r1 = mkdir((subvolume + "/.snapshots").c_str(), 700);
	if (r1 == 0)
	{
	    SystemCmd cmd1(CHATTRBIN " +x " + quote(subvolume + "/.snapshots"));
	    if (cmd1.retcode() != 0)
		throw CreateConfigFailedException("chattr failed");
	}
	else if (errno != EEXIST)
	{
	    y2err("mkdir failed errno:" << errno << " (" << stringerror(errno) << ")");
	    throw CreateConfigFailedException("mkdir failed");
	}

	int r2 = mkdir((subvolume + "/.snapshots/.info").c_str(), 700);
	if (r2 == 0)
	{
	    SystemCmd cmd2(CHATTRBIN " -x " + quote(subvolume + "/.snapshots/.info"));
	    if (cmd2.retcode() != 0)
		throw CreateConfigFailedException("chattr failed");
	}
	else if (errno != EEXIST)
	{
	    y2err("mkdir failed errno:" << errno << " (" << stringerror(errno) << ")");
	    throw CreateConfigFailedException("mkdir failed");
	}
    }


    void
    Ext4::deleteConfig() const
    {
	int r1 = rmdir((subvolume + "/.snapshots/.info").c_str());
	if (r1 != 0)
	{
	    y2err("rmdir failed errno:" << errno << " (" << stringerror(errno) << ")");
	    throw DeleteConfigFailedException("rmdir failed");
	}

	int r2 = rmdir((subvolume + "/.snapshots").c_str());
	if (r2 != 0)
	{
	    y2err("rmdir failed errno:" << errno << " (" << stringerror(errno) << ")");
	    throw DeleteConfigFailedException("rmdir failed");
	}
    }


    string
    Ext4::infosDir() const
    {
	return (subvolume == "/" ? "" : subvolume) + "/.snapshots/.info";
    }


    string
    Ext4::snapshotDir(unsigned int num) const
    {
	return subvolume + "@" + decString(num);
    }


    string
    Ext4::snapshotFile(unsigned int num) const
    {
	return (subvolume == "/" ? "" : subvolume) + "/.snapshots/" + decString(num);
    }


    SDir
    Ext4::openInfosDir() const
    {
	// TODO
    }


    SDir
    Ext4::openSnapshotDir(unsigned int num) const
    {
	// TODO
    }


    void
    Ext4::createSnapshot(unsigned int num) const
    {
	SystemCmd cmd1(TOUCHBIN " " + quote(snapshotFile(num)));
	if (cmd1.retcode() != 0)
	    throw CreateSnapshotFailedException();

	SystemCmd cmd2(CHSNAPBIN " +S " + quote(snapshotFile(num)));
	if (cmd2.retcode() != 0)
	    throw CreateSnapshotFailedException();
    }


    void
    Ext4::deleteSnapshot(unsigned int num) const
    {
	SystemCmd cmd(CHSNAPBIN " -S " + quote(snapshotFile(num)));
	if (cmd.retcode() != 0)
	    throw DeleteSnapshotFailedException();
    }


    bool
    Ext4::isSnapshotMounted(unsigned int num) const
    {
	FILE* f = setmntent("/etc/mtab", "r");
	if (!f)
	{
	    y2err("setmntent failed");
	    throw IsSnapshotMountedFailedException();
	}

	bool mounted = false;

	struct mntent* m;
	while ((m = getmntent(f)))
	{
	    if (strcmp(m->mnt_type, "rootfs") == 0)
		continue;

	    if (m->mnt_dir == snapshotDir(num))
	    {
		mounted = true;
		break;
	    }
	}

	endmntent(f);

	return mounted;
    }


    void
    Ext4::mountSnapshot(unsigned int num) const
    {
	if (isSnapshotMounted(num))
	    return;

	SystemCmd cmd1(CHSNAPBIN " +n " + quote(snapshotFile(num)));
	if (cmd1.retcode() != 0)
	    throw MountSnapshotFailedException();

	int r1 = mkdir(snapshotDir(num).c_str(), 0755);
	if (r1 != 0 && errno != EEXIST)
	{
	    y2err("mkdir failed errno:" << errno << " (" << stringerror(errno) << ")");
	    throw MountSnapshotFailedException();
	}

	SystemCmd cmd2(MOUNTBIN " -t ext4 -r -o loop,noload " + quote(snapshotFile(num)) +
		       " " + quote(snapshotDir(num)));
	if (cmd2.retcode() != 0)
	    throw MountSnapshotFailedException();
    }


    void
    Ext4::umountSnapshot(unsigned int num) const
    {
	if (!isSnapshotMounted(num))
	    return;

	SystemCmd cmd1(UMOUNTBIN " " + quote(snapshotDir(num)));
	if (cmd1.retcode() != 0)
	    throw UmountSnapshotFailedException();

	SystemCmd cmd2(CHSNAPBIN " -n " + quote(snapshotFile(num)));
	if (cmd2.retcode() != 0)
	    throw UmountSnapshotFailedException();

	rmdir(snapshotDir(num).c_str());
    }


    bool
    Ext4::checkSnapshot(unsigned int num) const
    {
	return checkNormalFile(snapshotFile(num));
    }

}
