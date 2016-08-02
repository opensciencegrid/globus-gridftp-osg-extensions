#!/bin/sh -xe

OS_VERSION=$1

ls -l /home

# Clean the yum cache
yum -y clean all
yum -y clean expire-cache

# First, install all the needed packages.
rpm -Uvh https://dl.fedoraproject.org/pub/epel/epel-release-latest-${OS_VERSION}.noarch.rpm

# Broken mirror?
echo "exclude=mirror.beyondhosting.net" >> /etc/yum/pluginconf.d/fastestmirror.conf

yum -y install yum-plugin-priorities
rpm -Uvh https://repo.grid.iu.edu/osg/3.3/osg-3.3-el${OS_VERSION}-release-latest.rpm
yum -y install rpm-build git yum-utils gcc

# Prepare the RPM environment
mkdir -p /tmp/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
cat >> /etc/rpm/macros.dist << EOF
%dist .osg.el${OS_VERSION}
%osg 1
EOF

cp /globus-gridftp-osg-extensions/globus-gridftp-osg-extensions.spec /tmp/rpmbuild/SPECS
package_version=`grep Version globus-gridftp-osg-extensions/globus-gridftp-osg-extensions.spec | awk '{print $2}'`
pushd globus-gridftp-osg-extensions
git archive --format=tar --prefix=globus-gridftp-osg-extensions-${package_version}/ HEAD  | gzip >/tmp/rpmbuild/SOURCES/globus-gridftp-osg-extensions-${package_version}.tar.gz
popd

# Build a temporary SRPM with all the SRPM deps missing; this will allow us to determine the actual yum dependencies.
rpmbuild -bs --nodeps --define '_topdir /tmp/rpmbuild' -ba /tmp/rpmbuild/SPECS/globus-gridftp-osg-extensions.spec
yum-builddep -y /tmp/rpmbuild/SRPMS/globus-gridftp-osg-extensions*.src.rpm
# Build the RPM
rpmbuild --nodeps --define '_topdir /tmp/rpmbuild' -ba /tmp/rpmbuild/SPECS/globus-gridftp-osg-extensions.spec

# After building the RPM, try to install it
# Fix the lock file error on EL7.  /var/lock is a symlink to /var/run/lock
mkdir -p /var/run/lock

yum localinstall -y /tmp/rpmbuild/RPMS/noarch/globus-gridftp-osg-extensions-${package_version}*

