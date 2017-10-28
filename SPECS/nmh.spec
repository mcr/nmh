# This spec supports two methods of RPM creation:
#
# 1) Start from an nmh workspace, run configure as desired and then "make rpm".
#    For example, from a tarball:
#      $ tar xpf nmh-1.6.tar.gz
#      $ cd nmh-1.6
#      $ ./configure --with-cyrus-sasl --with-tls  &&  make rpm
#
#    Another example, starting from the source code repository:
#      $ git clone git://git.savannah.nongnu.org/nmh.git
#      $ cd nmh
#      $ ./autogen.sh
#      $ ./configure --with-cyrus-sasl --with-tls  &&  make rpm
#
# 2) Start with a source RPM and use rpmbuild.  Configure options are hard
#    coded below, but they can be overridden on the rpmbuild command line
#    with --define 'configure_opts --opt=value [...]'.
#    For example:
#      $ rpm -i nmh-1.4-0.fc16.src.rpm
#      $ rpmbuild --rmsource --rmspec \
#          --define '_sysconfdir /usr/local/nmh/etc' \
#          --define 'configure_opts --with-cyrus-sasl --with-tls' \
#          --bb ~/lib/rpmbuild/SPECS/nmh.spec
#
# If configure has previously been run successfully in the workspace,
# it will not be invoked again, even if configure_opts is defined.
#
# Note that "make rpm" sets _sysconfdir.  If configuring to install
# anyplace other than the default _sysconfdir, typically /etc, and
# you're not using this through "make rpm", be sure to set _sysconfdir.
#
# With kernel (fcntl, flock, or lockf) locking, or with dot locking
# and a lockdir that's writable by the user, bin/inc does not need to
# be setgid.  This spec assumes that.  But if needed, add something
# like this to the %files section to make bin/inc setgid:
#   %attr(2755,-,mail) /usr/local/nmh/bin/inc
#
# Note that Version cannot contain any dashes.
#
# The description, summary, and a few other tags were taken from the
# nmh.spec used to build the Fedora 15 nmh rpm.

Name:          nmh
Version:       %(sed "s/-/_/g" VERSION)
%define        rawversion %(cat VERSION)
Release:       1%{?dist}
Summary:       A capable MIME-email-handling system with a command-line interface
Group:         Applications/Internet
License:       BSD
URL:           http://savannah.nongnu.org/projects/nmh
BuildRequires: flex ncurses-devel
%define        tarfile nmh-%rawversion.tar.gz
Source0:       %tarfile
Source1:       VERSION
%define        srcdir %(pwd)


%description
nmh is a collection of single-purpose programs that send, receive,
show, search, and otherwise manipulate emails, including MIME.
They combine well with other Unix programs, easing the development
of custom shorthand commands as shell scripts.
Optional GUI interfaces are provided by the external xmh and exmh
projects.  nmh is a descendant of the RAND MH, Mail Handler, project.


%prep
if [ ! -f $RPM_SOURCE_DIR/%tarfile ]; then
  #### The tarfile wasn't already installed and we started with a
  #### workspace (using make rpm), so get it from there.
  [ -f %srcdir/%tarfile ]  ||  (cd %srcdir  &&  make dist)
  cp -p %srcdir/%tarfile $RPM_SOURCE_DIR/%tarfile
fi
[ -f $RPM_SOURCE_DIR/VERSION ]  ||  cp -p %srcdir/VERSION $RPM_SOURCE_DIR
%setup -q -n %name-%rawversion


%build
if [ -f %srcdir/config.status ]; then
  echo reusing existing configuration
  cp -pf %srcdir/config.status .
  ./config.status
else
  %if %{undefined configure_opts}
    %define configure_opts --with-cyrus-sasl
  %endif
  %configure %configure_opts
fi
make all dist


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

gz_manpages='-e '

if find $RPM_BUILD_ROOT -name 'inc.1*' | \
   egrep -q '/usr(/lib|/share)?/man/([^/]+/)?man'; then
  #### brp-compress will gzip the man pages, so account for that.
  gz_manpages='-e s#\(/man/man./.*\)#\1.gz#'
fi

#### etc is brought into files using %config{noreplace}
find $RPM_BUILD_ROOT -name etc -prune -o ! -type d -print | \
  sed -e "s#^$RPM_BUILD_ROOT##" "$gz_manpages" > nmh_files


%clean
rm -rf $RPM_BUILD_ROOT $RPM_BUILD_DIR/%buildsubdir


%files -f nmh_files
%defattr(-,root,root,-)
%config(noreplace) %_sysconfdir/*
