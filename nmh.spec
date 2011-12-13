# Assumes that rpmbuild was invoked in main nmh directory using "make
# rpm".  Therefore, configure must already have been run.  "make rpm"
# uses an RPM directory below the current directory.
#
# This file is intended to be zero maintenance, that's why it relies
# on the Makefile (and specifically on the nmhdist target).  If you
# really want to start with rpmbuild from a clean distribution, look
# at the rpm target in the main Makefile.in to see what it does.
#
# Note that Version cannot contain any dashes.

Name:        nmh
Version:     %version
Release:     0%{?dist}
Summary:     A capable mail handling system with a command line interface.
Group:       Applications/Internet
License:     BSD
URL:         http://savannah.nongnu.org/projects/nmh
Source:      %tarfile
# This should already be defined in /usr/lib/rpm/macros:
# BuildRoot:   %{_buildrootdir}/%{name}-%{version}-%{release}.%{_arch}

%description
Nmh is an email system based on the MH email system and is intended to
be a (mostly) compatible drop-in replacement for MH.  Nmh isn't a
single comprehensive program.  Instead, it consists of a number of
fairly simple single-purpose programs for sending, receiving, saving,
retrieving and otherwise manipulating email messages.  You can freely
intersperse nmh commands with other shell commands or write custom
scripts which utilize nmh commands.  If you want to use nmh as a true
email user agent, you'll want to also install exmh to provide a user
interface for it--nmh only has a command line interface.

%prep
#### The tarfile is only needed for the source rpm.
cp -p %srcdir/%tarfile $RPM_SOURCE_DIR


%build


%install
rm -rf $RPM_BUILD_ROOT doc
(cd %srcdir  &&  make install DESTDIR=$RPM_BUILD_ROOT SETGID_MAIL=)
#### Exclude docs from nmh_files because its files are added with the
#### %doc directive in the %files section below.
mv `find $RPM_BUILD_ROOT -type d -name doc` .
find $RPM_BUILD_ROOT ! -type d -print | sed "s#^$RPM_BUILD_ROOT##" > nmh_files


%clean
rm -rf $RPM_BUILD_ROOT $RPM_SOURCE_DIR/%tarfile nmh_files doc


%files -f nmh_files
%defattr(-,root,root,-)
%doc doc/*
