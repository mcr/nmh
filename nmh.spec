# Assumes that rpmbuild was invoked main nmh directory using "make rpm".
# "make rpm" uses an RPM directory below the current directory.
# Note that Version cannot contain any dashes.

Name:        nmh
Version:     1.4.dev
Release:     1%{?dist}
Summary:     A capable mail handling system with a command line interface.

Group:       Applications/Internet
License:     BSD
URL:         http://savannah.nongnu.org/projects/nmh
Source0:     nmh-1.4-dev.tar.gz
BuildRoot:   %{_tmppath}/%{name}-%{version}-%{release}-build

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
cp -p %{srcdir}/nmh-1.4-dev.tar.gz $RPM_SOURCE_DIR


%build


%install
rm -rf $RPM_BUILD_ROOT

#### Assumes that configure was run with --prefix=/usr, at least.
#### The directory placements need to be cleaned up.
(cd %{srcdir}  &&  \
 make install DESTDIR=$RPM_BUILD_ROOT SETGID_MAIL=)

#### Not sure why .gz needs to be appended to man file names here.
#### But without it, the man files don't show the .gz extension:
find $RPM_BUILD_ROOT ! -type d -print | sed "s#^$RPM_BUILD_ROOT##g" > files

#### Should do the following with an install target in docs/Makefile.
#### Note that these are excluded from files above because they're
#### added with doc's in the %files section below.
mkdir -p docs
cp -p %{srcdir}/VERSION %{srcdir}/COPYRIGHT .
for i in COMPLETION-* DIFFERENCES FAQ MAIL.FILTERING README* TODO; do
  cp -p %{srcdir}/docs/$i docs
done


%clean
rm -rf files $RPM_BUILD_ROOT docs COPYRIGHT VERSION


%files -f files
%defattr(-,root,root,-)
%doc COPYRIGHT VERSION
%doc docs/COMPLETION-* docs/DIFFERENCES docs/FAQ docs/MAIL.FILTERING
%doc docs/README* docs/TODO
