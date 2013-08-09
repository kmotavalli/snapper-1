/*
 * Copyright (c) [2011-2013] Novell, Inc.
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


#ifndef SNAPPER_LVM_H
#define SNAPPER_LVM_H

/*
 * tools/errors.h from lvm2 source
 *
 * consider adding lvm2-devel dep for lvm2 enabled snapper
 */
#define EINVALID_CMD_LINE 3

#include "snapper/Filesystem.h"


namespace snapper
{
    struct LvmActivationException : public std::exception
    {
	explicit LvmActivationException() throw() {}
	virtual const char* what() const throw() { return "lvm snapshot activation exception"; }
    };


    struct LvmDeactivatationException : public std::exception
    {
	explicit LvmDeactivatationException() throw() {}
	virtual const char* what() const throw() { return "lvm snapshot deactivation exception"; }
    };


    class Lvm : public Filesystem
    {
    public:

	static Filesystem* create(const string& fstype, const string& subvolume);

	Lvm(const string& subvolume, const string& mount_type);

	virtual string fstype() const { return "lvm(" + mount_type + ")"; }

	virtual void createConfig() const;
	virtual void deleteConfig() const;

	virtual string snapshotDir(unsigned int num) const;
	virtual string snapshotLvName(unsigned int num) const;

	virtual SDir openInfosDir() const;
	virtual SDir openSnapshotDir(unsigned int num) const;

	virtual void createSnapshot(unsigned int num) const;
	virtual void deleteSnapshot(unsigned int num) const;

	virtual bool isSnapshotMounted(unsigned int num) const;
	virtual void mountSnapshot(unsigned int num) const;
	virtual void umountSnapshot(unsigned int num) const;

	virtual bool checkSnapshot(unsigned int num) const;

    private:

	const string mount_type;

	bool detectThinVolumeNames(const MtabData& mtab_data);

	void activateSnapshot(const string& vg_name, const string& lv_name) const;
	void deactivateSnapshot(const string& vg_name, const string& lv_name) const;
	bool detectInactiveSnapshot(const string& vg_name, const string& lv_name) const;

	string getDevice(unsigned int num) const;

	string vg_name;
	string lv_name;

	vector<string> mount_options;

    };

}


#endif
