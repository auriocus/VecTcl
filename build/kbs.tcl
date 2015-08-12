#! /bin/sh
##
# @file kbs.tcl
#	Kitgen Build System
# @mainpage
# @synopsis{kbs.tcl ?-option? .. ?command? ..}
#
# For available options and commands see help() or type './kbs.tcl help'.
# Online documentation can be found at http://wiki.tcl.tk/18146
#
# The following common commands are supported:
#	- @b help	see help()
#	- @b doc	see doc()
#	- @b license	see license()
#	- @b config	see config()
#	- @b gui	see gui()
#
# The following package related commands are supported:
#	- @b require	see require()
#	- @b sources	see sources()
#	- @b make	see make()
#	- @b install	see install()
#	- @b clean	see clean()
#	- @b test	see test()
#	- @b distclean	see distclean()
#
# Tcl/Tk software building environment.
# Build of starpacks, starkits, binary extensions and other software.
# Already existing package definitions can be found under Package.
#
# @examples
# @call{get brief help text,./kbs.tcl
#tclsh ./kbs.tcl}
# @call{get full documentation in ./doc/kbs.html,./kbs.tcl doc}
# @call{start in graphical mode,./kbs.tcl gui}
# @call{build batteries included kbskit interpreter,./kbs.tcl -r -vq-bi install kbskit8.5}
# @call{get list of available packages,./kbs.tcl list}
#
# @author <jcw@equi4.com> Initial ideas and kbskit sources
# @author <r.zaumseil@freenet.de> kbskit TEA extension and development
#
# @version 0.4.7
#
# @copyright 
#	Call './kbs.tcl license' or search for 'set ::kbs(license)' in this file
#	for information on usage and redistribution of this file,
#	and for a DISCLAIMER OF ALL WARRANTIES.
#
# Startup code:
#@verbatim

# check startup dir containing current file\
if test ! -r ./kbs.tcl ; then \
  echo "Please start from directory containing the file 'kbs.tcl'"; exit 1 ;\
fi;
# bootstrap for building wish.. \
if test "`pwd`" = "/" ; then \
PREFIX=/`uname` ;\
else \
PREFIX=`pwd`/`uname` ;\
fi ;\
TKOPT="" ;\
case `uname` in \
  MINGW*) DIR="win"; EXE="${PREFIX}/bin/tclsh86s.exe" ;; \
  Darwin*) DIR="unix"; EXE="${PREFIX}/bin/tclsh8.6" ; TKOPT="--enable-aqua" ;; \
  *) DIR="unix"; EXE="${PREFIX}/bin/tclsh8.6" ;; \
esac ;\
if test ! -d sources ; then mkdir sources; fi;\
if test ! -x ${EXE} ; then \
  if test ! -d sources/tcl8.6 ; then \
    ( cd sources && wget http://prdownloads.sourceforge.net/tcl/tcl8.6.4-src.tar.gz && gunzip -c tcl8.6.4-src.tar.gz | tar xf - && rm tcl8.6.4-src.tar.gz && mv tcl8.6.4 tcl8.6 ) ; \
  fi ;\
  if test ! -d sources/tk8.6 ; then \
    ( cd sources && wget http://prdownloads.sourceforge.net/tcl/tk8.6.4-src.tar.gz && gunzip -c tk8.6.4-src.tar.gz | tar xf - && rm tk8.6.4-src.tar.gz && mv tk8.6.4 tk8.6 ) ; \
  fi ;\
  mkdir -p ${PREFIX}/tcl ;\
  ( cd ${PREFIX}/tcl && ../../sources/tcl8.6/${DIR}/configure --disable-shared --prefix=${PREFIX} --exec-prefix=${PREFIX} && make install-binaries install-libraries ) ;\
  rm -rf ${PREFIX}/tcl ;\
  mkdir -p ${PREFIX}/tk ;\
  ( cd ${PREFIX}/tk && ../../sources/tk8.6/${DIR}/configure --enable-shared --prefix=${PREFIX} --exec-prefix=${PREFIX} --with-tcl=${PREFIX}/lib $TKOPT && make install-binaries install-libraries ) ;\
fi ;\
exec ${EXE} "$0" ${1+"$@"}
#@endverbatim
#===============================================================================
catch {wm withdraw .};# do not show toplevel in command line mode

##	Array variable with static informations.
#	- @b version	current version and version of used kbskit
#	- @b license	license information
variable ::kbs
set ::kbs(version) {0.4.7};# current version and version of used kbskit
set ::kbs(license) {
This software is copyrighted by Rene Zaumseil (the maintainer).
The following terms apply to all files associated with the software
unless explicitly disclaimed in individual files.

This software is copyrighted by the Regents of the University of
California, Sun Microsystems, Inc., Scriptics Corporation, ActiveState
Corporation and other parties.  The following terms apply to all files
associated with the software unless explicitly disclaimed in
individual files.

The author hereby grant permission to use, copy, modify, distribute,
and license this software and its documentation for any purpose, provided
that existing copyright notices are retained in all copies and that this
notice is included verbatim in any distributions. No written agreement,
license, or royalty fee is required for any of the authorized uses.
Modifications to this software may be copyrighted by their authors
and need not follow the licensing terms described here, provided that
the new terms are clearly indicated on the first page of each file where
they apply.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
MODIFICATIONS.}
#===============================================================================

##	This namespace contain the external callable functions.
namespace eval ::kbs {
  namespace export help version kbs list info gui
  namespace export require source configure make install clean distclean
}
#-------------------------------------------------------------------------------

##	Display usage help message.
# @note	This is also the default action if no command was given.
# @examples
# @call{display usage help message,./kbs.tcl help}
proc ::kbs::help {} {
  puts "[::kbs::config::Get application]
Usage: kbs.tcl ?options? command ?args?

options (configuration variables are available with \[Get ..\]):
  -pkgfile=?file?   contain used Package definitions
                    (default is empty and use only internal definitions)
  -builddir=?dir?   set used building directory containing all package
                    specific 'makedir' (default is './build\$tcl_platform(os)')
  -i -ignore        ignore errors and proceed (default is disabled)
  -r -recursive     recursive Require packages (default is disabled)
  -v -verbose       display running commands and command output
  -CC=?command?     set configuration variable 'CC'
                    (default is 'gcc' or existing environment variable 'CC')
  -bi=?package ..?  set configuration variable 'bi' (default is '')
                    to list of packages for use in batteries included builds
  --enable-*
  --disable-*       set configuration variable '-*'
  Used external programs (default values are found with 'auto_execok'):
  -make=?command?   set configuration variable 'exec-make'
                    (default is first found 'gmake' or 'make')
  -cvs=?command?    set configuration variable 'exec-cvs' (default is 'cvs')
  -svn=?command?    set configuration variable 'exec-svn' (default is 'svn')
  -tar=?command?    set configuration variable 'exec-tar' (default is 'tar')
  -gzip=?command?   set configuration variable 'exec-gzip' (default is 'gzip')
  -bzip2=?command?  set configuration variable 'exec-bzip2' (default is 'bzip2')
  -git=?command?    set configuration variable 'exec-git' (default is 'git')
  -unzip=?command?  set configuration variable 'exec-unzip' (default is 'unzip')
  -wget=?command?   set configuration variable 'exec-wget' (default is 'wget')
  -doxygen=?command? set configuration variable 'exec-doxygen'
                    (default is 'doxygen') you need at least version 1.7.5
  Used interpreter in package scripts (default first found in '[::kbs::config::Get builddir]/bin')
  -kitcli=?command? set configuration variable 'kitcli' (default 'kbs*cli*')
  -kitdyn=?command? set configuration variable 'kitdyn' (default 'kbs*dyn*')
  -kitgui=?command? set configuration variable 'kitgui' (default 'kbs*gui*')
  Mk4tcl based 'tclkit' interpreter build options:
  -mk               add 'mk-cli|dyn|gui' to variable 'kit'
  -mk-cli           add 'mk-cli' to variable 'kit'
  -mk-dyn           add 'mk-dyn' to variable 'kit'
  -mk-gui           add 'mk-gui' to variable 'kit'
  -mk-bi            add 'mk-bi' to variable 'kit'
  -staticstdcpp     build with static libstdc++
  Vqtcl based 'tclkit lite' interpreter build options:
  -vq               add 'vq-cli|dyn|gui' to variable 'kit'
  -vq-cli           add 'vq-cli' to variable 'kit'
  -vq-dyn           add 'vq-dyn' to variable 'kit'
  -vq-gui           add 'vq-gui' to variable 'kit'
  -vq-bi            add 'vq-bi' to variable 'kit'
  If no interpreter option is given '-vq' will be asumed.

additional variables for use with \[Get ..\]):
  application       name of application including version number
  builddir          common build dir (can be set with -builddir=..)
  makedir           package specific dir under 'builddir'
  srcdir            package specific source dir under './sources/'
  builddir-sys
  makedir-sys
  srcdir-sys        system specific version (p.e. windows C:\\.. -> /..)
  sys               TEA specific platform subdir (win, unix)
  TCL*              TCL* variables from tclConfig.sh, loaded on demand
  TK*               TK* variables from tkConfig.sh, loaded on demand

command:
  help              this text
  doc               create program documentation (./doc/kbs.html)
  license           display license information
  config            display used values of configuration variables
  gui               start graphical user interface
  list ?pattern? .. list packages matching pattern (default is *)
                    Trailing words print these parts of the definition too.
  require pkg ..    return call trace of packages
  sources pkg ..    get package source files (under sources/)
  configure pkg ..  create 'makedir' (in 'builddir') and configure package
  make pkg ..       make package (in 'makedir')
  install pkg ..    install package (in 'builddir')
  test pkg ..       test package
  clean pkg ..      remove make targets
  distclean pkg ..  remove 'makedir'
'pkg' is used for glob style matching against available packages
(Beware, you need to hide the special meaning of * like foo\\*)

Startup configuration:
  Read files '\$(HOME)/.kbsrc' and './kbsrc'. Lines starting with '#' are
  treated as comments and removed. All other lines are concatenated and
  used as command line arguments.
  Read environment variable 'KBSRC'. The contents of this variable is used
  as command line arguments.

The following external programs are needed:
  * C-compiler, C++ compiler for metakit based programs (see -CC=)
  * make with handling of VPATH variables (gmake) (see -make=)
  * cvs, svn, tar, gzip, unzip, wget to get and extract sources
    (see -cvs= -svn= -tar= -gzip= -unzip= -wget= options)
  * msys (http://sourceforge.net/project/showfiles.php?group_id=10894) is
    used to build under Windows. You need to put the kbs-sources inside
    the msys tree (/home/..).
"
}
#-------------------------------------------------------------------------------

##	Create documentation from source file.
# @examples
# @call{create public documentation,./kbs.tcl doc}
proc ::kbs::doc {} {
  set myPwd [pwd]
  if {![file readable kbs.tcl]} {error "missing file ./kbs.tcl"}
  file mkdir doc
  set myFd [open Doxyfile w]
  puts $myFd "PROJECT_NUMBER		= [clock format [clock seconds] -format {%Y%m%d}]"
  puts $myFd {
PROJECT_NAME		= "Kitgen Build System"
OUTPUT_DIRECTORY	= ./doc
JAVADOC_AUTOBRIEF	= YES
QT_AUTOBRIEF		= YES
ALIASES			=
ALIASES += copyright="\par Copyright:"
ALIASES += examples="\par Examples:"
ALIASES += synopsis{1}="\par Synopsis:\n@verbatim \1 @endverbatim"
ALIASES += call{2}="\1 @verbatim \2 @endverbatim"
EXTRACT_ALL		= NO
INPUT			= kbs.tcl
SOURCE_BROWSER		= YES
INLINE_SOURCES		= YES
STRIP_CODE_COMMENTS	= NO
GENERATE_TREEVIEW	= YES
GENERATE_LATEX          = NO
}
  close $myFd
  exec $::kbs::config::_(exec-doxygen)
}
#-------------------------------------------------------------------------------

##	Display license information.
# @examples
# @call{display license information,./kbs.tcl license}
proc ::kbs::license {} {
  puts $::kbs(license)
}
#-------------------------------------------------------------------------------

##	Display names and values of configuration variables useable with 'Get'.
# @examples
# @call{display used values,./kbs.tcl config}
proc ::kbs::config {} {
  foreach myName [lsort [array names ::kbs::config::_]] {
    puts [format {%-20s = %s} "\[Get $myName\]" [::kbs::config::Get $myName]]
  }
}
#-------------------------------------------------------------------------------

##	Start graphical user interface.
# @examples
# @call{simple start with default options,./kbs.tcl gui}
# 
# @param[in] args	currently not used
proc ::kbs::gui {args} {
  ::kbs::gui::_init $args
}
#-------------------------------------------------------------------------------

##	Print available packages.
# @examples
# @call{list all packages starting with 'kbs',./kbs.tcl list kbs\*}
# @call{list all definitions of packages starting with 'kbs',./kbs.tcl list kbs\* Package}
# @call{list specific definition parts of packages starting with 'kbs',./kbs.tcl list kbs\* Require Source}
# 
# @param[in] pattern	global search pattern for packages (default '*')
# @param[in] args	which part should be printed (default all)
proc ::kbs::list {{pattern *} args} {
  if {$args eq {}} {
    puts [lsort -dict [array names ::kbs::config::packagescript $pattern]]
  } else {
    foreach myPkg [lsort -dict [array names ::kbs::config::packagescript $pattern]] {
      set myName	""
      set myVersion	""
      foreach myChar [split $myPkg {}] {
	if {$myVersion == "" && [string match {[A-Za-z_} $myChar]} {
	  append myName $myChar
        } else {
	  append myVersion $myChar
        }
      }
      puts "## @page	$myName\n# @version	$myVersion\n#@verbatim\nPackage $myPkg {"
      foreach {myCmd myScript} $::kbs::config::packagescript($myPkg) {
        if {$args eq {Package} || [lsearch $args $myCmd] >= 0} {
          puts "  $myCmd {$myScript}"
        }
      }
      puts "}\n#@endverbatim\n#-------------------------------------------------------------------------------"
    }
  }
}
#-------------------------------------------------------------------------------

##	Call the 'Require' part of the package definition.
#	Can be used to show dependencies of packages.
# @examples
# @call{show dependencies of package,./kbs.tcl -r require kbskit8.5}
# 
# @param[in] args	list of packages
proc ::kbs::require {args} {
  ::kbs::config::_init {Require} $args
}
#-------------------------------------------------------------------------------

##	Call the 'Require' and 'Source' part of the package definition
#	to get the sources of packages.
#	Sources are installed under './sources/'.
# @examples
# @call{get the sources of a package,./kbs.tcl sources kbskit8.5}
# @call{get the sources of a package and its dependencies,./kbs.tcl -r sources kbskit8.5}
# 
# @param[in] args	list of packages
proc ::kbs::sources {args} {
  ::kbs::config::_init {Require Source} $args
}
#-------------------------------------------------------------------------------

##	Call the 'Require', 'Source' and 'Configure' part of the package
#	definition. The configuration is done in 'makedir'.
# @examples
# @call{configure the package,./kbs.tcl configure kbskit8.5}
# @call{configure the package and its dependencies,./kbs.tcl -r configure kbskit8.5}
# 
# @param[in] args	list of packages
proc ::kbs::configure {args} {
  ::kbs::config::_init {Require Source Configure} $args
}
#-------------------------------------------------------------------------------

##	Call the 'Require', 'Source', 'Configure' and 'Make' part of the
#	package definition. The build is done in 'makedir'.
# @examples
# @call{make the package,./kbs.tcl make kbskit8.5}
# @call{make the package and its dependencies,./kbs.tcl -r make kbskit8.5}
# 
# @param[in] args	list of packages
proc ::kbs::make {args} {
  ::kbs::config::_init {Require Source Configure Make} $args
}
#-------------------------------------------------------------------------------

##	Call the 'Require', 'Source', 'Make' and 'Test' part of the package
#	definition. The testing starts in 'makedir'
# @examples
# @call{test the package,./kbs.tcl test kbskit8.5}
# 
# @param[in] args	list of packages
proc ::kbs::test {args} {
  ::kbs::config::_init {Require Source Make Test} $args
}
#-------------------------------------------------------------------------------

##	Call the 'Require', 'Source', 'Configure', 'Make' and 'Install' part of
#	the package definition. The install dir is 'builddir'.
# @examples
# @call{install the package,./kbs.tcl install kbskit8.5}
# @call{install the package and its dependencies,./kbs.tcl -r install kbskit8.5}
# 
# @param[in] args	list of packages
proc ::kbs::install {args} {
  ::kbs::config::_init {Require Source Configure Make Install} $args
}
#-------------------------------------------------------------------------------

##	Call the 'Clean' part of the package definition.
#	The clean starts in 'makedir'.
# @examples
# @call{clean the package,./kbs.tcl clean kbskit8.5}
# @call{clean the package and its dependencies,./kbs.tcl -r clean kbskit8.5}
# 
# @param[in] args	list of packages
proc ::kbs::clean {args} {
  ::kbs::config::_init {Clean} $args
}
#-------------------------------------------------------------------------------

##	Remove the 'makedir' of the package so everything can be rebuild again.
#	This is necessary if there are problems in the configuration part of
#	the package.
# @examples
# @call{remove the package,./kbs.tcl distclean kbskit8.5}
# @call{remove the package and its dependencies,./kbs.tcl -r distclean kbskit8.5}
# 
# @param[in] args	list of packages
proc ::kbs::distclean {args} {
  # save old body
  set myBody [info body ::kbs::config::Source]
  proc ::kbs::config::Source [info args ::kbs::config::Source] {
    set myDir [Get makedir]
    if {[file exist $myDir]} {
      puts "=== Distclean: $myDir"
      file delete -force $myDir
    }
  }
  ::kbs::config::_init {Require Source} $args
  # restore old body
  proc ::kbs::config::Source [info args ::kbs::config::Source] $myBody
}
#===============================================================================

##	Contain internally used functions and variables.
namespace eval ::kbs::config {
  namespace export Run Get Patch PatchOld Require Source Configure Make Install Clean Test
#-------------------------------------------------------------------------------

##	Internal variable containing top level script directory.
  variable maindir [file normalize [file dirname [info script]]]
#-------------------------------------------------------------------------------

##	Internal variable with parsed package definitions from *.kbs files.
#       'Include' parts are resolved.
  variable packages

#-------------------------------------------------------------------------------
##	Internal variable with original package definitions from *.kbs files.
  variable packagescript
#-------------------------------------------------------------------------------

##	Internal variable containing current package name.
  variable package
#-------------------------------------------------------------------------------

##	Internal variable containing list of already prepared packages.
  variable ready [list]
#-------------------------------------------------------------------------------

##	If set (-i or -ignore switch) then proceed in case of errors.
# @examples
# @call{try to build all given packages,
#./kbs.tcl -i install bwidget\* mentry\*
#./kbs.tcl -ignore install bwidget\* mentry\*
# }
  variable ignore
  set ignore 0
#-------------------------------------------------------------------------------

##	If set (-r or -recursive switch) then all packages under 'Require'
#	are also used.
# @examples
# @call{build all packages recursively,
#./kbs.tcl -r install kbskit8.5
#./kbs.tcl -recursive install kbskit8.5
# } 
  variable recursive
  set recursive 0
#-------------------------------------------------------------------------------

##	If set (-v or -verbose switch) then all stdout will be removed.
#
# @examples
# @call{print additional information while processing,
#./kbs.tcl -v -r install bwidget\*
#./kbs.tcl -verbose -r install bwidget\*
# }
  variable verbose
  set verbose 0
#-------------------------------------------------------------------------------

##	Define startup kbs package definition file.
#	Default is empty and use only internal definitions.
# @examples
# @call{start with own package definition file,./kbs.tcl -pkgfile=/my/package/file list}
  variable pkgfile
  set pkgfile {}
#-------------------------------------------------------------------------------

##	The array variable contain usefull information of the current building
#	process. All variables are provided with default values.
#	Changing of the default values can be done in the following order:
#	- file '$(HOME)/.kbsrc' and file './kbsrc' -- Lines starting with '#'
#	  are treated as comments and removed. All other lines are concatenated
#	  and used as command line arguments.
#	- environment variable 'KBSRC' -- The contents of this variable is used
#	  as command line arguments.
#	- command line 
#	It is also possible to set values in the 'Package' definition file
#	outside the 'Package' definition (p.e. 'set ::kbs::config::_(CC) g++').
# @examples
# @call{build debugging version,./kbs.tcl -CC=/my/cc --enable-symbols install tclx8.4}
# @call{create kbsmk8.5-[cli|dyn|gui] interpreter,./kbs.tcl -mk install kbskit8.5}
# @call{create kbsvq8.5-bi interpreter with packages,./kbs.tcl -vq-bi -bi="tclx8.4 tdom0.8.2" install kbskit8.5}
# @call{get list of available packages with,./kbs.tcl list}
# 
  variable _
  if {[info exist ::env(CC)]} {;# used compiler
    set _(CC)		$::env(CC)
  } else {
    set _(CC)		{gcc}
  }
  if {$::tcl_platform(platform) eq {windows}} {;# configuration system subdir
    set _(sys)		{win}
  } else {
    set _(sys)		{unix}
  }
  set _(exec-make)	[lindex "[auto_execok gmake] [auto_execok make] make" 0]
  set _(exec-cvs)	[lindex "[auto_execok cvs] cvs" 0]
  set _(exec-svn)	[lindex "[auto_execok svn] svn" 0]
  set _(exec-tar)	[lindex "[auto_execok tar] tar" 0]
  set _(exec-gzip)	[lindex "[auto_execok gzip] gzip" 0]
  set _(exec-bzip2)	[lindex "[auto_execok bzip2] bzip2" 0]
  set _(exec-git)	[lindex "[auto_execok git] git" 0]
  set _(exec-unzip)	[lindex "[auto_execok unzip] unzip" 0]
  set _(exec-wget)	[lindex "[auto_execok wget] wget" 0]
  set _(exec-doxygen)	[lindex "[auto_execok doxygen] doxygen" 0]
  set _(kitcli)		{}
  set _(kitdyn)		{}
  set _(kitgui)         {}
  set _(kit)		[list];# list of interpreters to build
  set _(bi)		[list];# list of packages for batteries included interpreter builds
  set _(staticstdcpp)	1;# build with static libstdc++
  set _(makedir)	{};# package specific build dir
  set _(makedir-sys)	{};# package and system specific build dir
  set _(srcdir)		{};# package specific source dir
  set _(srcdir-sys)	{};# package and system specific source dir
  set _(builddir)	[file join $maindir build[string map {{ } {}} $::tcl_platform(os)]]
  set _(builddir-sys)	$_(builddir)
  set _(application)	"Kitgen build system ($::kbs(version))";# application name
#-------------------------------------------------------------------------------
};# end of ::kbs::config

##	Return platfrom specific file name p.e. windows C:\... -> /...
#
# @param[in] file	file name to convert
proc ::kbs::config::_sys {file} {
  if {$::tcl_platform(platform) eq {windows} && [string index $file 1] eq {:}} {
    return "/[string tolower [string index $file 0]][string range $file 2 end]"
  } else {
    return $file
  }
}
#-------------------------------------------------------------------------------

##	Initialize variables with respect to given configuration options
#	and command.
#	Process command in separate interpreter.
#
# @param[in] used	list of available commands
# @param[in] list	list of packages
proc ::kbs::config::_init {used list} {
  variable packages
  variable package
  variable ignore
  variable interp

  # reset to clean state
  variable ready	[list]
  variable _
  array unset _ TCL_*
  array unset _ TK_*

  # create interpreter with commands
  lappend used Run Get Patch PatchOld
  set interp [interp create]
  foreach myProc [namespace export] {
    if {$myProc in $used} {
      interp alias $interp $myProc {} ::kbs::config::$myProc
    } else {
      $interp eval [list proc $myProc [info args ::kbs::config::$myProc] {}]
    }
  }
  # now process command
  foreach myPattern $list {
    set myTargets [array names packages $myPattern]
    if {[llength $myTargets] == 0} {
      return -code error "no targets found for pattern: '$myPattern'"
    }
    foreach package $myTargets {
      set _(makedir) [file join $_(builddir) $package]
      set _(makedir-sys) [file join $_(builddir-sys) $package]
      puts "=== Package eval: $package"
      if {[catch {$interp eval $packages($package)} myMsg]} {
        if {$ignore == 0} {
          interp delete $interp
	  set interp {}
          return -code error "=== Package failed for: $package\n$myMsg"
        }
        puts "=== Package error: $myMsg"
      }
      puts "=== Package done: $package"
    }
  }
  interp delete $interp
  set interp {}
}
#-------------------------------------------------------------------------------

##	The 'Package' command is available in definition files.
#	All 'Package' definitions will be saved for further use.
# @synopsis{Package name script}
#
# @param[in] name	unique name of package
# @param[in] script	contain one or more of the following definitions.
#		The common functions 'Run', 'Get' and 'Patch' can be used in
#		every 'script'. For a detailed description and command specific
#		additional functions look in the related commands.
#	'Require script'   -- define dependencies
#	'Source script'    -- method to get sources
#	'Configure script' -- configure package
#	'Make script'      -- build package
#	'Install script'   -- install package
#	'Clean script'     -- clean package
#	Special commands:
#	'Include package'  -- include current 'package' script. The command
#	use the current definitions (snapshot semantic).
proc ::kbs::config::Package {name script} {
  variable packages
  variable packagescript

  set packagescript($name) $script
  array set myTmp $script
  if {[info exists myTmp(Include)]} {
    array set myScript $packages($myTmp(Include))
  }
  if {[info exist packages($name)]} {
    array set myScript $packages($name)
  }
  array set myScript $script
  set packages($name) {}
  foreach myCmd {Require Source Configure Make Install Clean Test} {
    if {[info exists myScript($myCmd)]} {
      append packages($name) [list $myCmd $myScript($myCmd)]\n
    }
  }
}
#-------------------------------------------------------------------------------

##	Evaluate the given script.
#	Add additional packages with the 'Use' function.
# @synopsis{Require script}
#
#  @param script	containing package dependencies.
#	Available functions are: 'Run', 'Get', 'Patch'
#	'Use ?package..?' -- see Require-Use()
proc ::kbs::config::Require {script} {
  variable recursive
  if {$recursive == 0} return
  variable verbose
  variable interp
  variable package

  puts "=== Require $package"
  if {$verbose} {puts $script}
  interp alias $interp Use {} ::kbs::config::Require-Use
  $interp eval $script
  foreach my {Use} {interp alias $interp $my}
}

#-------------------------------------------------------------------------------

##	Define dependencies used with '-r' switch.
#	The given 'Package's in args will then be recursively called.
# @synopsis{Use ?package? ..}
#
# @param[in] args	one or more 'Package' names
proc ::kbs::config::Require-Use {args} {
  variable packages
  variable ready
  variable package
  variable ignore
  variable interp
  variable _
  puts "=== Require $args"

  set myPackage $package
  set myTargets [list]
  foreach package $args {
    # already loaded
    if {[lsearch $ready $package] != -1} continue
    # single target name
    if {[info exist packages($package)]} {
      set _(makedir) [file join $_(builddir) $package]
      set _(makedir-sys) [file join $_(builddir-sys) $package]
      puts "=== Require eval: $package"
      array set _ {srcdir {} srcdir-sys {}}
      if {[catch {$interp eval $packages($package)} myMsg]} {
        puts "=== Require error: $package\n$myMsg"
        if {$ignore == 0} {
          return -code error "Require failed for: $package"
        }
        foreach my {Link Cvs Svn Git Tgz Tbz2 Zip Http Wget Script Kit Tcl Libdir} {
          interp alias $interp $my;# clear specific procedures
        }
      }
      puts "=== Require done: $package"
      lappend ready $package
      continue
    }
    # nothing found
    return -code error "Require not found: $package"
  }
  set package $myPackage
  set _(makedir) [file join $_(builddir) $package]
  set _(makedir-sys) [file join $_(builddir-sys) $package]
  puts "=== Require leave: $args"
}
#-------------------------------------------------------------------------------

##	Procedure to build source tree of current 'Package' definition.
# @synopsis{Source script}
#
# @param[in] script	one or more of the following functions to get the sources
#		of the current package. The sources should be placed under
#		'./sources/'.
#	Available functions are: 'Run', 'Get', 'Patch'
#	'Cvs path ...' - call 'cvs -d path co -d 'srcdir' ...'
#	'Svn path'     - call 'svn co path 'srcdir''
#	'Http path'    - call 'http get path', unpack *.tar.gz, *.tar.bz2,
#			 *.tgz or *.tbz2 files
#	'Wget file'    - call 'wget file', unpack *.tar.gz *.tar.bz2,
#			  *.tgz  or *.tbz2 files
#	'Tgz file'     - call 'tar xzf file'
#	'Tbz2 file'    - call 'tar xjf file'
#	'Zip file'     - call 'unzip file'
#	'Link package' - use sources from "package"
#	'Script text'  - eval 'text'
proc ::kbs::config::Source {script} {
  variable interp
  variable package
  variable _

  ::kbs::gui::_state -running "" -package $package
  foreach my {Script Http Wget Link Cvs Svn Git Tgz Tbz2 Zip} {
    interp alias $interp $my {} ::kbs::config::Source- $my
  }
  array set _ {srcdir {} srcdir-sys {}}
  $interp eval $script
  foreach my {Script Http Wget Link Cvs Svn Git Tgz Tbz2 Zip} {
    interp alias $interp $my
  }
  if {$_(srcdir) eq {}} {
    return -code error "missing sources of package '$package'"
  }
  set _(srcdir-sys) [_sys $_(srcdir)]
}
#-------------------------------------------------------------------------------

##	Process internal 'Source' commands.
# @synopsis{
#	Link dir
#	Script tcl-script
#	Cvs path args
#	Svn args
#	Git args
#	Http url
#	Wget file
#	Tgz file
#	Tbz2 file
#	Zip file}
#
# @param[in] type	one of the valid source types, see Source().
# @param[in] args	depending on the given 'type' 
proc ::kbs::config::Source- {type args} {
  variable maindir
  variable package
  variable verbose
  variable pkgfile
  variable _

  cd [file join $maindir sources]
  switch -- $type {
    Link {
      if {$args == $package} {return -code error "wrong link source: $args"}
      set myDir [file join $maindir sources $args]
      if {![file exists $myDir]} {
        puts "=== Source $type $package"
        cd $maindir
        if {[catch {
          #exec [pwd]/kbs.tcl sources $args >@stdout 2>@stderr
          if {$verbose} {
            Run [info nameofexecutable] [pwd]/kbs.tcl -pkgfile=$pkgfile -builddir=$_(builddir) -v sources $args
          } else {
            Run [info nameofexecutable] [pwd]/kbs.tcl -pkgfile=$pkgfile -builddir=$_(builddir) sources $args
          }
        } myMsg]} {
          file delete -force $myDir
          if {$verbose} {puts $myMsg}
        }
      }
    } Script {
      set myDir [file join $maindir sources $package]
      if {![file exists $myDir]} {
        puts "=== Source $type $package"
        if {[catch {eval $args} myMsg]} {
          file delete -force $myDir
          if {$verbose} {puts $myMsg}
        }
      }
    } Cvs {
      set myDir [file join $maindir sources $package]
      if {![file exists $myDir]} {
        set myPath [lindex $args 0]
        set args [lrange $args 1 end]
        if {$args eq {}} { set args [file tail $myPath] }
        if {[string first @ $myPath] < 0} {set myPath :pserver:anonymous@$myPath}
        puts "=== Source $type $package"
	if {[catch {Run $_(exec-cvs) -d $myPath -z3 co -P -d $package {*}$args} myMsg]} {
          file delete -force $myDir
          if {$verbose} {puts $myMsg}
        }
      }
    } Svn {
      set myDir [file join $maindir sources $package]
        if {![file exists $myDir]} {
        puts "=== Source $type $package"
        if {[catch {Run $_(exec-svn) co {*}$args $package} myMsg]} {
          file delete -force $myDir
          if {$verbose} {puts $myMsg}
        }
      }
    } Git {
      set myDir [file join $maindir sources $package]
      if {![file exists $myDir]} {
        puts "=== Source $type $package"
	if {[catch {Run $_(exec-git) clone {*}$args $package} myMsg]} {
	  file delete -force $myDir
          if {$verbose} {puts $myMsg}
        }
      } else {
        puts "=== Source update $type $package"
        if {[catch {
	  set myOldpwd [pwd]
	  cd $package
	  Run $_(exec-git) pull
	  cd $myOldpwd
	} myMsg]} {
	  catch {cd $myOldpwd}
	  puts "=== Source update failed $type $package (ignored)"
	  #file delete -force $myDir
          if {$verbose} {puts $myMsg}
        }
      }
    } Http - Wget {
      set myDir [file join $maindir sources $package]
      if {![file exists $myDir]} {
        set myFile [file normalize ./[file tail $args]]
        puts "=== Source $type $package"
        if {[catch {
          Run $_(exec-wget) --no-check-certificate $args
          # unpack if necessary
          switch -glob $myFile {
            *.tgz - *.tar.gz - *.tgz?uuid=* - *.tar.gz?uuid=* {
              Source- Tgz $myFile
              file delete $myFile
            } *.tbz - *.tar.bz2 - *.tbz?uuid=* - *.tar.bz2?uuid=* {
              Source- Tbz2 $myFile
              file delete $myFile
            } *.zip - *.zip?uuid=* {
              Source- Zip $myFile
              file delete $myFile
            } *.kit {
              if {$::tcl_platform(platform) eq {unix}} {
                file attributes $myFile -permissions u+x
              }
              if {$myFile ne $myDir} {
                file mkdir $myDir
	        file rename $myFile $myDir
              }
            }
          }
        } myMsg]} {
          file delete -force $myDir $myFile
          if {$verbose} {puts $myMsg}
        }
      }
    } Tgz - Tbz2 - Zip {
      set myDir [file join $maindir sources $package]
      if {![file exists $myDir]} {
        puts "=== Source $type $package"
        if {[catch {
          file delete -force $myDir.tmp
          file mkdir $myDir.tmp
          cd $myDir.tmp
          if {$type eq {Tgz}} {Run $_(exec-gzip) -dc $args | $_(exec-tar) xf -}
          if {$type eq {Tbz2}} {Run $_(exec-bzip2) -dc $args | $_(exec-tar) xf -}
          if {$type eq {Zip}} {Run $_(exec-unzip) $args}
          cd [file join $maindir sources]
          set myList [glob $myDir.tmp/*]
          if {[llength $myList] == 1 && [file isdir $myList]} {
            file rename $myList $myDir
            file delete $myDir.tmp
          } else {
            file rename $myDir.tmp $myDir
          }
        } myMsg]} {
          file delete -force $myDir.tmp $myDir
          if {$verbose} {puts $myMsg}
        }
      }
    } default {
      return -code error "wrong type '$type'"
    }
  }
  if {[file exists $myDir]} {
    set _(srcdir) $myDir
  }
}
#-------------------------------------------------------------------------------

##	If 'makedir' not exist create it and eval script.
# @synopsis{Configure script}
#
# @param[in] script	tcl script to evaluate with one or more of the following
#		functions to help configure the current package
#	Available functions are: 'Run', 'Get', 'Patch'
#	'Kit ?main.tcl? ?pkg..?' -- see Configure-Kit()
proc ::kbs::config::Configure {script} {
  variable verbose
  variable interp

  set myDir [Get makedir]
  if {[file exist $myDir]} return
  puts "=== Configure $myDir"
  if {$verbose} {puts $script}
  foreach my {Config Kit} {
    interp alias $interp $my {} ::kbs::config::Configure-$my
  }
  file mkdir $myDir
  $interp eval [list cd $myDir]
  $interp eval $script
  foreach my {Config Kit} {interp alias $interp $my}
}
#-------------------------------------------------------------------------------
##	Call 'configure' with options.
# @examples
#	Configure [Get builddir-sys]/configure --enabled-shared=no
# @param [in] path	Path to configure script
# @param [in] args	Additional configure arguments
proc ::kbs::config::Configure-Config {path args} {
  variable _

  # collect available options
  set myOpts ""
  foreach l [split [exec env $path/configure --help] \n] {
    set l [string trimleft $l]
    if {[string range $l 0 8] == "--enable-"} {
      set myOpt [lindex [split $l " \t="] 0]
      set myOpt [string range [lindex [split $l " \t="] 0] 8 end]
      if {[info exists _($myOpt)]} {
        append myOpts " $_($myOpt)"
      }
    } elseif {[string range $l 0 12] == "--exec-prefix"} {
      append myOpts " --exec-prefix=[Get builddir-sys]"
    } elseif {[string range $l 0 7] == "--prefix"} {
      append myOpts " --prefix=[Get builddir-sys]"
    } elseif {[string range $l 0 16] == "--with-tclinclude"} {
    } elseif {[string range $l 0 9] == "--with-tcl"} {
      append myOpts " --with-tcl=[Get builddir-sys]/lib"
    } elseif {[string range $l 0 15] == "--with-tkinclude"} {
    } elseif {[string range $l 0 8] == "--with-tk"} {
      append myOpts " --with-tk=[Get builddir-sys]/lib"
    }
  }
  #TODO CFLAGS
  Run env CC=[Get CC] TCLSH_PROG=[Get builddir-sys]/bin/tclsh85 WISH_PROG=[Get builddir-sys]/bin/wish $path/configure {*}$myOpts {*}$args
}
##	This function create a 'makedir'/main.tcl with:
#	- common startup code
#	- require statement for each package in 'args' argument
#	- application startup from 'maincode' argument
# @synopsis{Kit maincode args}
# @examples
#	Package tksqlite0.5.8 ..
# 
# @param[in] maincode	startup code
# @param[in] args	additional args
proc ::kbs::config::Configure-Kit {maincode args} {
  variable _

  if {[file exists [file join [Get srcdir-sys] main.tcl]]} {
    return -code error "'main.tcl' existing in '[Get srcdir-sys]'"
  }
  # build standard 'main.tcl'
  set myFd [open main.tcl w]
  puts $myFd {#!/usr/bin/env tclkit
# startup
if {[catch {
  package require starkit
  if {[starkit::startup] eq "sourced"} return
}]} {
  namespace eval ::starkit { variable topdir [file dirname [info script]] }
  set auto_path [linsert $auto_path 0 [file join $::starkit::topdir lib]]
}
# used packages};# end of puts
  foreach myPkg $args {
    puts $myFd "package require $myPkg"
  }
  puts $myFd "# start application\n$maincode"
  close $myFd
}
#-------------------------------------------------------------------------------

##	Evaluate script in 'makedir'.
# @synopsis{Make script}
#
# @param[in] script	tcl script to evaluate with one or more of the following
#		functions to help building the current package
#	Available functions are: 'Run', 'Get', 'Patch'
#	'Kit name ?pkglibdir..?' -- see Make-Kit()
proc ::kbs::config::Make {script} {
  variable verbose
  variable interp

  set myDir [Get makedir]
  if {![file exist $myDir]} {
    return -code error "missing make directory: '$myDir'"
  }
  puts "=== Make $myDir"
  if {$verbose} {puts $script}
  interp alias $interp Kit {} ::kbs::config::Make-Kit
  $interp eval [list cd $myDir]
  $interp eval $script
  foreach my {Kit} {interp alias $interp $my}
}
#-------------------------------------------------------------------------------

##	The procedure links the 'name.vfs' in to the 'makedir' and create
#	foreach name in 'args' a link from 'builddir'/lib in to 'name.vfs'/lib.
#	The names in 'args' may subdirectories under 'builddir'/lib. In the
#	'name.vfs'/lib the leading directory parts are removed.
#	The same goes for 'name.vfs'.
#	- Kit name ?librarydir ..?
#	  Start in 'makedir'. Create 'name.vfs/lib'.
#	  When existing link 'main.tcl' to 'name.vfs'.
#	  Link everything from [Srcdir] into 'name.vfs'.
#	  Link all package library dirs in ''makedir'/name.vfs'/lib
# @synopsis{Kit name args}
# @examples
#	Package tksqlite0.5.8 ..
# 
#
# @param[in] name	name of vfs directory (without extension) to use
# @param[in] args	additional args
proc ::kbs::config::Make-Kit {name args} {
  variable _

  #TODO 'file link ...' does not work under 'msys'
  set myVfs $name.vfs
  file delete -force $myVfs
  file mkdir [file join $myVfs lib]
  if {[file exists main.tcl]} {
    file copy main.tcl $myVfs
  }
  foreach myPath [glob -nocomplain -directory [Get srcdir] -tails *] {
    if {$myPath in {lib CVS}} continue
    Run ln -s [file join [Get srcdir-sys] $myPath] [file join $myVfs $myPath]
  }
  foreach myPath [glob -nocomplain -directory [Get srcdir] -tails lib/*] {
    Run ln -s [file join [Get srcdir-sys] $myPath] [file join $myVfs $myPath]
  }
  foreach myPath $args {
    Run ln -s [file join [Get builddir-sys] lib $myPath]\
	[file join $myVfs lib [file tail $myPath]]
  }
}
#-------------------------------------------------------------------------------

##	Eval script in 'makedir'.
# @synopsis{Install script}
#
# @param[in] script	tcl script to evaluate with one or more of the following
#		functions to install the current package.
#	Available functions are: 'Run', 'Get', 'Patch'
#	'Libdir dirname' -- see Install-Libdir()
#	'Kit name args'  -- see Install-Kit()
#	'Tcl ?package?'  -- see Install-Tcl()
proc ::kbs::config::Install {script} {
  variable verbose
  variable interp

  set myDir [Get makedir]
  if {![file exist $myDir]} {
    return -code error "missing make directory: '$myDir'"
  }
  puts "=== Install $myDir"
  if {$verbose} {puts $script}
  foreach my {Kit Tcl Libdir} {
    interp alias $interp $my {} ::kbs::config::Install-$my
  }
  $interp eval [list cd $myDir]
  $interp eval $script
  foreach my {Kit Tcl Libdir} {interp alias $interp $my}
}
#-------------------------------------------------------------------------------

##	Move given 'dir' in 'builddir'tcl/lib to package name.
#	This function is necessary to install all packages with the same
#	naming convention (lower case name plus version number).
# @synopsis{Libdir dirname}
#
# @param[in] dirname	original package library dir,
#		not conforming lower case with version number
proc ::kbs::config::Install-Libdir {dirname} {
  variable verbose
  variable package

  set myLib [Get builddir]/lib
  if {[file exists $myLib/$dirname]} {
    if {$verbose} {puts "$myLib/$dirname -> $package"}
    # two steps to distinguish under windows lower and upper case names
    file delete -force $myLib/$dirname.Libdir
    file rename $myLib/$dirname $myLib/$dirname.Libdir
    file delete -force $myLib/$package
    file rename $myLib/$dirname.Libdir $myLib/$package
  } else {
    if {$verbose} {puts "skipping: $myLib/$dirname -> $package"}
  }
}
#-------------------------------------------------------------------------------

##	Without 'option' wrap kit and move to 'builddir'/bin otherwise with:
#	- @b -mk-cli create starpack with 'kbsmk*-cli*' executable
#	- @b -mk-dyn create starpack with 'kbsmk*-dyn*' executable
#	- @b -mk-gui create starpack with 'kbsmk*-gui*' executable
#	- @b -vq-cli create starpack with 'kbsvq*-cli*' executable
#	- @b -vq-dyn create starpack with 'kbsvq*-dyn*' executable
#	- @b -vq-gui create starpack with 'kbsvq*-gui*' executable
#	- @b ... create starpack with given option as executable
# @synopsis{Kit name args}
#
# @examples
#	Package tksqlite0.5.8 ..
# 
# @param[in] name	name of vfs directory (without extension) to use
# @param[in] args	additional args
# SOURCE
proc ::kbs::config::Install-Kit {name args} {
  variable _

  set myTmp [file join [Get builddir] bin]
  if {$args eq {-mk-cli}} {
    set myRun [glob $myTmp/kbsmk*-cli*]
  } elseif {$args eq {-mk-dyn}} {
    set myRun [glob $myTmp/kbsmk*-dyn*]
  } elseif {$args eq {-mk-gui}} {
    set myRun [glob $myTmp/kbsmk*-gui*]
  } elseif {$args eq {-vq-cli}} {
    set myRun [glob $myTmp/kbsvq*-cli*]
  } elseif {$args eq {-vq-dyn}} {
    set myRun [glob $myTmp/kbsvq*-dyn*]
  } elseif {$args eq {-vq-gui}} {
    set myRun [glob $myTmp/kbsvq*-gui*]
  } else {
    set myRun $args
  }
  set myExe {}
  foreach myExe [glob $myTmp/kbs*-cli* $myTmp/kbs*-dyn* $myTmp/kbs*-gui*] {
    if {$myExe ne $myRun} break
  }
  if {$myExe eq {}} { return -code error "no interpreter in '$myTmp'" }
  if {$myRun eq {}} {
    Run $myExe [file join [Get builddir] bin sdx.kit] wrap $name
    file rename -force $name [file join [Get builddir] bin $name.kit]
  } else {
    Run $myExe [file join [Get builddir] bin sdx.kit] wrap $name -runtime {*}$myRun
    if {$_(sys) eq {win}} {
      file rename -force $name [file join [Get builddir] bin $name.exe]
    } else {
      file rename -force $name [file join [Get builddir] bin]
    }
  }
}
#-------------------------------------------------------------------------------

##	Command to install tcl only packages.
#	Used in 'Install' part of 'Package' definitions.
# @synopsis{Tcl ?pkgname?}
#
# @examples
#	Package mentry-3.1 ..
# 
# @param[in] pkgname	install name of package, if missing then build from [Get srcdir]
# @param[in] subdir     source directory under [Get srcdir], default empty
proc ::kbs::config::Install-Tcl {{pkgname {}} {subdir {}}} {
  if {$pkgname eq {}} {
    set myDst [file join [Get builddir] lib [file tail [Get srcdir]]]
  } else {
    set myDst [file join [Get builddir] lib $pkgname]
  }
  file delete -force $myDst
  file copy -force [Get srcdir]/$subdir $myDst
  if {![file exists [file join $myDst pkgIndex.tcl]]} {
    foreach {myPkg myVer} [split [file tail $myDst] -] break;
    if {$myVer eq {}} {set myVer 0.0}
    set myRet "package ifneeded $myPkg $myVer \"\n"
    foreach myFile [glob -tails -directory $myDst *.tcl] {
      append myRet "  source \[file join \$dir $myFile\]\n"
    }
    set myFd [open [file join $myDst pkgIndex.tcl] w]
    puts $myFd "$myRet  package provide $myPkg $myVer\""
    close $myFd
  }
}
#-------------------------------------------------------------------------------

##	Eval script in 'makedir'.
# @synopsis{Test script}
#
# @param[in] script	tcl script to evaluate with one or more of the following
#		functions to help testing the current package
#		Available functions are: 'Run', 'Get', 'Patch'
#		'Kit name args' -- see Test-Kit()
proc ::kbs::config::Test {script} {
  variable verbose
  variable interp

  set myDir [Get makedir]
  if {![file exist $myDir]} return
  puts "=== Test $myDir"
  if {$verbose} {puts $script}
  interp alias $interp Kit {} ::kbs::config::Test-Kit
  $interp eval [list cd $myDir]
  $interp eval $script
  foreach my {Kit} {interp alias $interp $my}
}
#-------------------------------------------------------------------------------

##	Run kit file with given command line 'args'
# @synopsis{Kit mode name args}
#
# @examples
#	Package tksqlite0.5.8 ..
# 
# @param[in] name	name of vfs directory (without extension) to use
# @param[in] args	additional args
proc ::kbs::config::Test-Kit {name args} {
  variable _

  set myExe [file join [Get builddir] bin $name]
  if {[file exists $myExe]} {
    Run $myExe {*}$args
  } else {
    set myTmp [file join [Get builddir] bin]
    set myTmp [glob $myTmp/kbs*-gui* $myTmp/kbs*-dyn* $myTmp/kbs*-cli*]
    Run [lindex $myTmp 0] $myExe.kit {*}$args
  }
}
#-------------------------------------------------------------------------------

##	Eval script in 'makedir'.
# @synopsis{Clean script}
#
# @param[in] script	tcl script to evaluate with one or more of the following
#		functions to help cleaning the current package.
#		Available functions are: 'Run', 'Get', 'Patch'
proc ::kbs::config::Clean {script} {
  variable verbose
  variable interp

  set myDir [Get makedir]
  if {![file exist $myDir]} return
  puts "=== Clean $myDir"
  if {$verbose} {puts $script}
  $interp eval [list cd $myDir]
  $interp eval $script
}
#-------------------------------------------------------------------------------

##	Return value of given variable name.
#	If 'var' starts with 'TCL_' tclConfig.sh will be parsed for TCL_*
#	variables. If 'var' starts with 'TK_' tkConfig.sh will be parsed for
#	TK_* variables.
# @synopsis{Get var}
#
# @param[in] var	name of variable.
proc ::kbs::config::Get {var} {
  variable _

  if {[string index $var 0] eq {-}} {
    if {![info exists _($var)]} return
  } elseif {[string range $var 0 3] eq {TCL_} && ![info exists _(TCL_)]} {
    set myScript ""
    set myFd [open [file join $_(builddir) lib tclConfig.sh] r]
    set myC [read $myFd]
    close $myFd
    foreach myLine [split $myC \n] {
      if {[string range $myLine 0 3] ne {TCL_}} continue
      set myNr [string first = $myLine]
      if {$myNr == -1} continue
      append myScript "set _([string range $myLine 0 [expr {$myNr - 1}]]) "
      incr myNr 1
      append myScript [list [string map {' {}} [string range $myLine $myNr end]]]\n
    }
    eval $myScript
    set _(TCL_) 1
  } elseif {[string range $var 0 2] eq {TK_} && ![info exists _(TK_)]} {
    set myScript ""
    set myFd [open [file join $_(builddir) lib tkConfig.sh] r]
    set myC [read $myFd]
    close $myFd
    foreach myLine [split $myC \n] {
      if {[string range $myLine 0 2] ne {TK_}} continue
      set myNr [string first = $myLine]
      if {$myNr == -1} continue
      append myScript "set _([string range $myLine 0 [expr {$myNr - 1}]]) "
      incr myNr 1
      append myScript [list [string map {' {}} [string range $myLine $myNr end]]]\n
    }
    eval $myScript
    set tkConfig 1
  }
  return $_($var)
}
#-------------------------------------------------------------------------------

##	Apply a patch in unified diff format
# @synopsis{Patch directory striplevel patch}
#
# @examples
#	Patch [Get srcdir] 1 {
#.... here comes the output from diff -ru ...
# }
# @param[in] dir        root directory of the patch, usually srcdir
# @param[in] striplevel number of path elements to be removed from the diff header
# @param[in] patch      output of diff -ru

proc ::kbs::config::Patch {dir striplevel patch} {
	set patchlines [split $patch \n]
	set inhunk false
	set oldcode {}
	set newcode {}
	
	for {set lineidx 0} {$lineidx<[llength $patchlines]} {incr lineidx} {
		set line [lindex $patchlines $lineidx]
		if {[string match diff* $line]} {
			# a diff block starts. Next two lines should be
			# --- oldfile date time TZ
			# +++ newfile date time TZ
			incr lineidx
			set in [lindex $patchlines $lineidx]
			incr lineidx
			set out [lindex $patchlines $lineidx]

			if {![string match ---* $in] || ![string match +++* $out]} {
				puts $in
				puts $out
				return -code error "Patch not in unified diff format, line $lineidx $in $out"
			}

			# the quoting is compatible with list
			lassign $in -> oldfile
			lassign $out -> newfile

			set fntopatch [file join $dir {*}[lrange [file split $newfile] $striplevel end]]
			set inhunk false
			#puts "Found diffline for $fntopatch"
			continue
		}

		# state machine for parsing the hunks
		set typechar [string index $line 0]
		set codeline [string range $line 1 end]
		switch $typechar {
			@ {
				if {![regexp {@@\s+\-(\d+),(\d+)\s+\+(\d+),(\d+)\s+@@} $line \
					-> oldstart oldlen newstart newlen]} {
					return code -error "Erroneous hunk in line $lindeidx, $line"
				}
				# adjust line numbers for 0-based indexing
				incr oldstart -1
				incr newstart -1
				#puts "New hunk"
				set newcode {}
				set oldcode {}
				set inhunk true
			}
			- { # line only in old code
				if {$inhunk} {
					lappend oldcode $codeline
				}
			}
			+ { # line only in new code
				if {$inhunk} {
					lappend newcode $codeline
				}
			}
			" " { # common line
				if {$inhunk} {
					lappend oldcode $codeline
					lappend newcode $codeline
				}
			}
			default {
				# puts "Junk: $codeline";
				continue
			}
		}
		# test if the hunk is complete
		if {[llength $oldcode]==$oldlen && [llength $newcode]==$newlen} {
			set hunk [dict create \
				oldcode $oldcode \
				newcode $newcode \
				oldstart $oldstart \
				newstart $newstart]
			#puts "hunk complete: $hunk"
			set inhunk false
			dict lappend patchdict $fntopatch $hunk
		}
	}

	# now we have parsed the patch. Apply
	dict for {fn hunks} $patchdict {
		puts "Patching file $fn"
		if {[catch {open $fn} fd]} {
			set orig {}
		} else {
			set orig [split [read $fd] \n]
		}
		close $fd

		set patched $orig

		set fail false
		set already_applied false
		set hunknr 1
		foreach hunk $hunks {
			dict with hunk {
				set oldend [expr {$oldstart+[llength $oldcode]-1}]
				set newend [expr {$newstart+[llength $newcode]-1}]
				# check if the hunk matches
				set origcode [lrange $orig $oldstart $oldend]
				if {$origcode ne $oldcode} {
					set fail true
					puts "Hunk #$hunknr failed"
					# check if the patch is already applied
					set origcode_applied [lrange $orig $newstart $newend]
					if {$origcode_applied eq $newcode} {
						set already_applied true
						puts "Patch already applied"
					} else {
						puts "Expected:\n[join $oldcode \n]"
						puts "Seen:\n[join $origcode \n]"
					}
					break
				}
				# apply patch
				set patched [list {*}[lrange $patched 0 $newstart-1] {*}$newcode {*}[lrange $patched $oldend+1 end]]
			}
			incr hunknr
		}

		if {!$fail} {
			# success - write the result back
			set fd [open $fn w]
			puts -nonewline $fd [join $patched \n]
			close $fd
		}
	}
}


#-------------------------------------------------------------------------------

##	Patch files.
# @synopsis{PatchOld file lineoffste oldtext newtext}
#
# @examples
#	Patch [Get srcdir]/Makefile.in 139\
#        {INCLUDES       = @PKG_INCLUDES@ @TCL_INCLUDES@}\
#        {INCLUDES       = @TCL_INCLUDES@}
# 
# @param[in] file	name of file to patch
# @param[in] lineoffset	start point of patch, first line is 1
# @param[in] oldtext	part of file to replace
# @param[in] newtext	replacement text
proc ::kbs::config::PatchOld {file lineoffset oldtext newtext} {
  variable verbose

  set myFd [open $file r]
  set myC [read $myFd]
  close $myFd
  # find oldtext
  set myIndex 0
  for {set myNr 1} {$myNr < $lineoffset} {incr myNr} {;# find line
    set myIndex [string first \n $myC $myIndex]
    if {$myIndex == -1} {
      return -code error "failed Patch: '$file' at $lineoffset -> eof at line $myNr"
    }
    incr myIndex
  }
  # set begin and rest of string
  set myTest [string range $myC $myIndex end]
  set myC [string range $myC 0 [incr myIndex -1]]
  # test for newtext; patch already applied
  set myIndex [string length $newtext]
  if {[string compare -length $myIndex $newtext $myTest] == 0} {
    if {$verbose} {puts "patch line $lineoffset exists"}
    return
  }
  # test for oldtext; patch todo
  set myIndex [string length $oldtext]
  if {[string compare -length $myIndex $oldtext $myTest] != 0} {
    if {$verbose} {puts "---old version---:\n$oldtext\n---new version---:\n[string range $myTest 0 $myIndex]"}
    return -code error "failed Patch: '$file' at $lineoffset"
  }
  # apply patch
  append myC $newtext[string range $myTest $myIndex end]
  set myFd [open $file w]
  puts $myFd $myC
  close $myFd
  if {$verbose} {puts "applied Patch: '$file' at $lineoffset"}
}
#-------------------------------------------------------------------------------

##	The procedure call the args as external command with options.
#	The procedure is available in all script arguments.
#	If the 'verbose' switch is on the 'args' will be printed.
# @synopsis{Run args}
#
# @param[in] args	containing external command
proc ::kbs::config::Run {args} {
  variable _
  variable verbose
  variable maindir
  if {[info exists _(exec-[lindex $args 0])]} {
    set args [lreplace $args 0 0 $_(exec-[lindex $args 0])]
  }

  if {$verbose} {
    ::kbs::gui::_state -running $args
    puts $args
    exec {*}$args >@stdout 2>@stderr
  } else {
    ::kbs::gui::_state;# keep gui alive
    if {$::tcl_platform(platform) eq {windows}} {
      exec {*}$args >[file join $maindir __dev__null__] 2>@stderr
    } else {
      exec {*}$args >/dev/null 2>@stderr
    }
  }
}
#-------------------------------------------------------------------------------

##	Configure application with given command line arguments.
#
# @param[in] args	option list
proc ::kbs::config::_configure {args} {
  variable maindir
  variable pkgfile
  variable ignore
  variable recursive
  variable verbose
  variable _

  set myOpts {}
  # read configuration files
  foreach myFile [list [file join $::env(HOME) .kbsrc] [file join $maindir kbsrc]] {
    if {[file readable $myFile]} {
      puts "=== Read configuration file '$myFile'"
      set myFd [open $myFile r]
      append myOpts [read $myFd]\n
      close $myFd
    }
  }
  # read configuration variable
  if {[info exists ::env(KBSRC)]} {
    puts "=== Read configuration variable 'KBSRC'"
    append myOpts $::env(KBSRC)
  }
  # add all found configuration options to command line
  foreach myLine [split $myOpts \n] {
    set myLine [string trim $myLine]
    if {$myLine eq {} || [string index $myLine 0] eq {#}} continue
    set args "$myLine $args"
  }
  # start command line parsing
  set myPkgfile {}
  set myIndex 0
  foreach myCmd $args {
    switch -glob -- $myCmd {
      -pkgfile=* {
        set myPkgfile [file normalize [string range $myCmd 9 end]]
      } -builddir=* {
	set myFile [file normalize [string range $myCmd 10 end]]
        set _(builddir) $myFile
      } -bi=* {
        set _(bi) [string range $myCmd 4 end]
      } -CC=* {
        set _(CC) [string range $myCmd 4 end]
      } -i - -ignore {
        set ignore 1
      } -r - -recursive {
        set recursive 1
      } -v - -verbose {
	set verbose 1
      } --enable-* {
        set _([string range [lindex [split $myCmd {=}] 0] 8 end]) $myCmd
      } --disable-* {
        set _([string range $myCmd 9 end]) $myCmd
      } -make=* {
        set _(exec-make) [string range $myCmd 6 end]
      } -cvs=* {
        set _(exec-cvs) [string range $myCmd 5 end]
      } -svn=* {
        set _(exec-svn) [string range $myCmd 5 end]
      } -tar=* {
        set _(exec-tar) [string range $myCmd 5 end]
      } -gzip=* {
        set _(exec-gzip) [string range $myCmd 6 end]
      } -bzip2=* {
        set _(exec-bzip2) [string range $myCmd 7 end]
      } -unzip=* {
        set _(exec-unzip) [string range $myCmd 7 end]
      } -wget=* {
        set _(exec-wget) [string range $myCmd 6 end]
      } -doxygen=* {
        set _(exec-doxygen) [string range $myCmd 9 end]
      } -kitcli=* {
        set _(kitcli) [string range $myCmd 8 end]
      } -kitdyn=* {
        set _(kitdyn) [string range $myCmd 8 end]
      } -kitgui=* {
        set _(kitgui) [string range $myCmd 8 end]
      } -kitgui=* {
      } -mk {
        lappend _(kit) mk-cli mk-dyn mk-gui
      } -mk-cli {
        lappend _(kit) mk-cli
      } -mk-dyn {
        lappend _(kit) mk-dyn
      } -mk-gui {
        lappend _(kit) mk-gui
      } -mk-bi {
        lappend _(kit) mk-bi
      } -staticstdcpp {
        set _(staticstdcpp) 1
      } -vq {
        lappend _(kit) vq-cli vq-dyn vq-gui
      } -vq-cli {
        lappend _(kit) vq-cli
      } -vq-dyn {
        lappend _(kit) vq-dyn
      } -vq-gui {
        lappend _(kit) vq-gui
      } -vq-bi {
        lappend _(kit) vq-bi
      } -* {
        return -code error "wrong option: '$myCmd'"
      } default {
        set args [lrange $args $myIndex end]
        break
      }
    }
    incr myIndex
  }
  set _(builddir-sys) [_sys $_(builddir)]
  set _(kit) [lsort -unique $_(kit)];# all options only once
  if {$_(kit) eq {}} {set _(kit) {vq-cli vq-dyn vq-gui}};# default setting
  foreach my {cli dyn gui} {;# default settings
    if {$_(kit$my) eq {}} {
      set _(kit$my) [lindex [lsort [glob -nocomplain [file join $_(builddir) bin kbs*${my}*]]] 0]
    }
  }
  file mkdir [file join $_(builddir) bin] [file join $maindir sources]
  file mkdir [file join $_(builddir) lib]
  if {![file readable [file join $_(builddir) lib64]]} {
    file link [file join $_(builddir) lib64] [file join $_(builddir) lib]
  }
  # read kbs configuration file
  if {$myPkgfile ne {}} {
    puts "=== Read definitions from '$myPkgfile'"
    source $myPkgfile
    set pkgfile $myPkgfile
  }
  return $args
}
#-------------------------------------------------------------------------------

#===============================================================================

##	Contain variables and function of the graphical user interface.
namespace eval ::kbs::gui {
#-------------------------------------------------------------------------------

##	Containing internal gui state values.
  variable _
  set _(-command) {};# currently running command
  set _(-package) {};# current package 
  set _(-running) {};# currently executed command in 'Run'
  set _(widgets) [list];# list of widgets to disable if command is running
}
#-------------------------------------------------------------------------------

##	Build and initialize graphical user interface.
#
# @param[in] args	currently ignored
proc ::kbs::gui::_init {args} {
  variable _

  package require Tk

  grid rowconfigure . 5 -weight 1
  grid columnconfigure . 1 -weight 1

  # variables
  set w .var
  grid [::ttk::labelframe $w -text {Option variables} -padding 3]\
	-row 1 -column 1 -sticky ew
  grid columnconfigure $w 2 -weight 1

  grid [::ttk::label $w.1 -anchor e -text {-pkgfile=}]\
	-row 1 -column 1 -sticky ew
  grid [::ttk::label $w.2 -anchor w -relief ridge -textvariable ::kbs::config::pkgfile]\
	-row 1 -column 2 -sticky ew

  grid [::ttk::label $w.4 -anchor e -text {-builddir=}]\
	-row 2 -column 1 -sticky ew
  grid [::ttk::label $w.5 -anchor w -relief ridge -textvariable ::kbs::config::_(builddir)]\
	-row 2 -column 2 -sticky ew
  grid [::ttk::button $w.6 -width 3 -text {...} -command {::kbs::gui::_set_builddir} -padding 0]\
	-row 2 -column 3 -sticky ew

  grid [::ttk::label $w.7 -anchor e -text {-CC=}]\
	-row 3 -column 1 -sticky ew
  grid [::ttk::entry $w.8 -textvariable ::kbs::config::_(CC)]\
	-row 3 -column 2 -sticky ew
  grid [::ttk::button $w.9 -width 3 -text {...} -command {::kbs::gui::_set_exec CC {Select C-compiler}} -padding 0]\
	-row 3 -column 3 -sticky ew
  lappend _(widgets) $w.6 $w.8 $w.9

  set myRow 3
  set myW 9
  foreach myCmd [lsort [array names ::kbs::config::_ exec-*]] {
    set myCmd [string range $myCmd 5 end]
    incr myRow
    grid [::ttk::label $w.[incr myW] -anchor e -text "-${myCmd}="]\
	-row $myRow -column 1 -sticky ew
    grid [::ttk::entry $w.[incr myW] -textvariable ::kbs::config::_(exec-$myCmd)]\
	-row $myRow -column 2 -sticky ew
    lappend _(widgets) $w.$myW
    grid [::ttk::button $w.[incr myW] -width 3 -text {...} -command "::kbs::gui::_set_exec exec-${myCmd} {Select '${myCmd}' program}" -padding 0]\
	-row $myRow -column 3 -sticky ew
    lappend _(widgets) $w.$myW
  }

  # select options
  set w .sel
  grid [::ttk::labelframe $w -text {Select options} -padding 3]\
	-row 2 -column 1 -sticky ew
  grid columnconfigure $w 1 -weight 1
  grid columnconfigure $w 2 -weight 1
  grid columnconfigure $w 3 -weight 1

  grid [::ttk::checkbutton $w.1 -text -ignore -onvalue 1 -offvalue 0 -variable ::kbs::config::ignore]\
	-row 1 -column 1 -sticky ew
  grid [::ttk::checkbutton $w.2 -text -recursive -onvalue 1 -offvalue 0 -variable ::kbs::config::recursive]\
	-row 1 -column 2 -sticky ew
  grid [::ttk::checkbutton $w.3 -text -verbose -onvalue 1 -offvalue 0 -variable ::kbs::config::verbose]\
	-row 1 -column 3 -sticky ew

  lappend _(widgets) $w.1 $w.2 $w.3

  # toggle options
  set w .tgl
  grid [::ttk::labelframe $w -text {Toggle options} -padding 3]\
	-row 3 -column 1 -sticky ew
  grid columnconfigure $w 3 -weight 1
  grid columnconfigure $w 6 -weight 1

  set c 0
  set r 1
  set i 0
  foreach myOpt {-shared -symbols -64bit -64bit-vis -xft -corefoundation -aqua -framework} {
    grid [::ttk::label $w.[incr i] -text $myOpt= -anchor e]\
	-row $r -column [incr c] -sticky ew
    grid [::ttk::checkbutton $w.[incr i] -width 17\
	-onvalue --enable$myOpt -offvalue --disable$myOpt\
	-variable ::kbs::config::_($myOpt)\
	-textvariable ::kbs::config::_($myOpt)]\
	-row $r -column [incr c] -sticky ew
    lappend _(widgets) $w.$i
    if {$c >= 4} {
      set c 0
      incr r
    }
  }

  # kit build options
  set w .kit
  grid [::ttk::labelframe $w -text {Kit build options} -padding 3]\
	-row 4 -column 1 -sticky ew
  grid columnconfigure $w 2 -weight 1

  grid [::ttk::label $w.1 -text {'kit'} -anchor e]\
	-row 1 -column 1 -sticky ew
  grid [::ttk::combobox $w.2 -state readonly -textvariable ::kbs::config::_(kit) -values {mk-cli mk-dyn mk-gui mk-bi {mk-cli mk-dyn mk-gui} vq-cli vq-dyn vq-gui vq-bi {vq-cli vq-dyn vq-gui} {mk-cli mk-dyn mk-gui vq-cli vq-dyn vq-gui}}]\
	-row 1 -column 2 -sticky ew
  grid [::ttk::label $w.3 -text -bi= -anchor e]\
	-row 2 -column 1 -sticky ew
  grid [::ttk::entry $w.4 -textvariable ::kbs::config::_(bi)]\
	-row 2 -column 2 -sticky ew
  grid [::ttk::button $w.5 -text {set '-bi' with selected packages} -command {::kbs::gui::_set_bi} -padding 0]\
	-row 3 -column 2 -sticky ew

  # packages
  set w .pkg
  grid [::ttk::labelframe $w -text {Available Packages} -padding 3]\
	-row 5 -column 1 -sticky ew
  grid rowconfigure $w 1 -weight 1
  grid columnconfigure $w 1 -weight 1

  grid [::listbox $w.lb -yscrollcommand "$w.2 set" -selectmode extended]\
	-row 1 -column 1 -sticky nesw
  eval $w.lb insert end [lsort -dict [array names ::kbs::config::packages]]
  grid [::ttk::scrollbar $w.2 -orient vertical -command "$w.lb yview"]\
	-row 1 -column 2 -sticky ns

  # commands
  set w .cmd
  grid [::ttk::labelframe $w -text Commands -padding 3]\
	-row 6 -column 1 -sticky ew
  grid columnconfigure $w 1 -weight 1
  grid columnconfigure $w 2 -weight 1
  grid columnconfigure $w 3 -weight 1
  grid columnconfigure $w 4 -weight 1
  grid [::ttk::button $w.1 -text sources -command {::kbs::gui::_command sources}]\
	-row 1 -column 1 -sticky ew
  grid [::ttk::button $w.2 -text configure -command {::kbs::gui::_command configure}]\
	-row 1 -column 2 -sticky ew
  grid [::ttk::button $w.3 -text make -command {::kbs::gui::_command make}]\
	-row 1 -column 3 -sticky ew
  grid [::ttk::button $w.4 -text install -command {::kbs::gui::_command install}]\
	-row 1 -column 4 -sticky ew
  grid [::ttk::button $w.5 -text test -command {::kbs::gui::_command test}]\
	-row 2 -column 1 -sticky ew
  grid [::ttk::button $w.6 -text clean -command {::kbs::gui::_command clean}]\
	-row 2 -column 2 -sticky ew
  grid [::ttk::button $w.7 -text distclean -command {::kbs::gui::_command distclean}]\
	-row 2 -column 3 -sticky ew
  grid [::ttk::button $w.8 -text EXIT -command {exit}]\
	-row 2 -column 4 -sticky ew

  lappend _(widgets) $w.1 $w.2 $w.3 $w.4 $w.5 $w.6 $w.7 $w.8

  # status
  set w .state
  grid [::ttk::labelframe $w -text {Status messages} -padding 3]\
	-row 7 -column 1 -sticky ew
  grid columnconfigure $w 2 -weight 1

  grid [::ttk::label $w.1_1 -anchor w -text Command:]\
	-row 1 -column 1 -sticky ew
  grid [::ttk::label $w.1_2 -anchor w -relief sunken -textvariable ::kbs::gui::_(-command)]\
	-row 1 -column 2 -sticky ew
  grid [::ttk::label $w.2_1 -anchor w -text Package:]\
	-row 2 -column 1 -sticky ew
  grid [::ttk::label $w.2_2 -anchor w -relief sunken -textvariable ::kbs::gui::_(-package)]\
	-row 2 -column 2 -sticky nesw
  grid [::ttk::label $w.3_1 -anchor w -text Running:]\
	-row 3 -column 1 -sticky ew
  grid [::ttk::label $w.3_2 -anchor w -relief sunken -textvariable ::kbs::gui::_(-running) -wraplength 300]\
	-row 3 -column 2 -sticky ew

  wm title . [::kbs::config::Get application]
  wm protocol . WM_DELETE_WINDOW {exit}
  wm deiconify .
}
#-------------------------------------------------------------------------------

##	Set configuration variable 'builddir'.
proc ::kbs::gui::_set_builddir {} {
  set myDir [tk_chooseDirectory -parent . -title "Select 'builddir'"\
	-initialdir $::kbs::config::_(builddir)]
  if {$myDir eq {}} return
  file mkdir [file join $myDir bin]
  set ::kbs::config::_(builddir) $myDir
  set ::kbs::config::_(builddir-sys) [::kbs::config::_sys $myDir]
}
#-------------------------------------------------------------------------------

##	Set configuration variable of given 'varname'.
#
# @param[in] varname	name of configuration variable to set
# @param[in] title	text to display as title of selection window
proc ::kbs::gui::_set_exec {varname title} {
  set myFile [tk_getOpenFile -parent . -title $title\
	-initialdir [file dirname $::kbs::config::_($varname)]]
  if {$myFile eq {}} return
  set ::kbs::config::_($varname) $myFile
}
#-------------------------------------------------------------------------------

##	Set configuration variable 'bi'.
proc ::kbs::gui::_set_bi {} {
  set my [list]
  foreach myNr [.pkg.lb curselection] {
    lappend my [.pkg.lb get $myNr]
  }
  set ::kbs::config::_(bi) $my
}
#-------------------------------------------------------------------------------

##	Function to process currently selected packages and provide
#	feeedback results.
#
# @param[in] cmd	selected command from gui
proc ::kbs::gui::_command {cmd} {
  variable _

  set mySelection [.pkg.lb curselection]
  if {[llength $mySelection] == 0} {
    tk_messageBox -parent . -type ok -title {No selection} -message {Please select at least one package from the list.}
    return
  }
  foreach myW $_(widgets) { $myW configure -state disabled }
  set myCmd ::kbs::$cmd
  foreach myNr $mySelection {
    lappend myCmd [.pkg.lb get $myNr]
  }
  ::kbs::gui::_state -running "" -package "" -command "'$myCmd' ..."
  if {![catch {console show}]} {
    set myCmd "consoleinterp eval $myCmd"
  }
  if {[catch {{*}$myCmd} myMsg]} {
    tk_messageBox -parent . -type ok -title {Execution failed} -message "'$cmd $myTarget' failed!\n$myMsg" -icon error
    ::kbs::gui::_state -command "'$cmd $myTarget' failed!"
  } else {
    tk_messageBox -parent . -type ok -title {Execution finished} -message "'$cmd $myTarget' successfull." -icon info
    ::kbs::gui::_state -running "" -package "" -command "'$cmd $myTarget' done."
  }
  foreach myW $_(widgets) { $myW configure -state normal }
}
#-------------------------------------------------------------------------------

##	Change displayed state informations and update application.
# @param[in] args	list of option-value pairs with:
#   -running 'text' - text to display in the 'Running:' state
#   -package 'text' - text to display in the 'Package:' state
#   -command 'text' - text to display in the 'Command:' state
proc ::kbs::gui::_state {args} {
  variable _

  array set _ $args
  update
}
#-------------------------------------------------------------------------------

#===============================================================================

##	Parse the command line in search of options.
#	Process the command line to call one of the '::kbs::*' functions
#
# @param[in] argv	list of provided command line arguments
proc ::kbs_main {argv} {
  # parse options
  if {[catch {set argv [::kbs::config::_configure {*}$argv]} myMsg]} {
    puts stderr "Option error (try './kbs.tcl' to get brief help): $myMsg"
    exit 1
  }
  # try to execute command
  set myCmd [lindex $argv 0]
  if {[info commands ::kbs::$myCmd] ne ""} {
    if {[catch {::kbs::$myCmd {*}[lrange $argv 1 end]} myMsg]} {
      puts stderr "Error in execution of '$myCmd [lrange $argv 1 end]':\n$myMsg"
	  puts stderr $::errorInfo
      exit 1
    }
    if {$myCmd != "gui"} {
      exit 0
    }
  } elseif {$myCmd eq {}} {
    ::kbs::help
    exit 0
  } else {
    set myList {}
    foreach myKnownCmd [lsort [info commands ::kbs::*]] {
      lappend myList [namespace tail $myKnownCmd]
    }
    puts stderr "'$myCmd' not found, should be one of: [join $myList {, }]"
    exit 1
  }
}
#===============================================================================

# begin of DB
namespace eval ::kbs::config {
# @{
## @defgroup __
#@verbatim
Package __ {
  Source {
    if {![file exists sf.net]} { file mkdir sf.net }
    Link ../sf.net
  }
  Configure {}
  Make {
    # Build all files for distribution on sourceforge.
    # do everything from the main directory
    cd ../..
    if {$::tcl_platform(platform) == "windows"} {
      set MYEXE "./[exec uname]/bin/tclsh86s.exe kbs.tcl"
      set my0 {}
    } else {
      set MYEXE {./kbs.tcl}
      set my0 expect5.45
    }
    #
    lappend my0\
	bwidget1.9.8\
	gridplus2.10\
	icons1.2 img1.4.3\
	memchan2.3 mentry3.7\
	nsf2.0.0\
	ral0.11.2\
	tablelist5.13 tcllib1.17 tclx8.4 tdom0.8.3\
	tkcon tkdnd2.8 tklib0.6 tkpath0.3.3 tktable2.10 treectrl2.4.1 trofs0.4.8\
	udp1.0.11\
	wcb3.5\
	xotcl1.6.8
    if {$::tcl_platform(os) != "Darwin"} {lappend my0 rbc0.1}
    # 8.6 kbskit
    Run {*}$MYEXE -builddir=sf86 -v -r -vq -mk install kbskit8.6
    # 8.6 tksqlite
    Run {*}$MYEXE -builddir=sf86 -v -r install tksqlite0.5.11
    # 8.6 BI
    set my $my0
    lappend my itk4.0 iwidgets4.1
    Run {*}$MYEXE -builddir=sf86 -v -r -vq-bi -bi=$my install kbskit8.6
    # save results under sf.net
    set myPrefix "sf.net/[string map {{ } {}} $::tcl_platform(os)]_"
    foreach myFile [glob sf*/bin/kbs* sf*/bin/tksqlite*] {
      file copy -force $myFile $myPrefix[file tail $myFile]
    }
    if {![file exists sf.net/kbs.tgz]} {
      puts "+++ [clock format [clock seconds] -format %T] kbs.tgz"
      Run tar czf sf.net/kbs.tgz kbs.tcl sources
    }
if 0 {;# Testfile
  catch {package req x}
  foreach p [lsort [package names]] {
    if {$p == "vfs::template"} continue
    puts $p=[catch {package req $p}]
  }
}
  }
}
#@endverbatim
## @defgroup bwidget
#@verbatim
Package bwidget1.9.8 {
  Source {Wget http://prdownloads.sourceforge.net/tcllib/bwidget-1.9.8.tar.gz}
  Configure {}
  Install {
    file delete -force [Get builddir]/lib/[file tail [Get srcdir]]
    file copy -force [Get srcdir] [Get builddir]/lib
  }
  Test {
    cd [Get builddir]/lib/bwidget1.9.8/demo
    Run [Get kitgui] demo.tcl
  }
}
#@endverbatim
## @defgroup expect
#@verbatim
Package expect5.45 {
  Source {Cvs expect.cvs.sourceforge.net:/cvsroot/expect -r expect_5_45 expect}
  Configure {Config [Get srcdir-sys]}
  Make {Run make}
  Install {Run make install}
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup gridplus
#@verbatim
Package gridplus2.10 {
  Require {Use icons1.2 tablelist5.13}
  Source {Wget http://www.satisoft.com/tcltk/gridplus2/download/gridplus.zip}
  Configure {}
  Install {Tcl}
}
#@endverbatim
## @defgroup icons
#@verbatim
Package icons1.2 {
  Source {Wget http://www.satisoft.com/tcltk/icons/icons.tgz}
  Configure {}
  Install {Tcl}
}
#@endverbatim
## @defgroup img
# @bug #76 at https://sourceforge.net/p/tkimg/bugs/
#@verbatim
Package img1.4.1 {
  Source {Svn https://svn.code.sf.net/p/tkimg/code/trunk -r 343}
  Configure {
    PatchOld [Get srcdir]/Makefile.in 154 {install: collate install-man
} {install: collate
}
    Config [Get srcdir-sys]
  }
  Make {Run make}
  Install {
    Run make install
    Libdir Img1.4.1
  }
  Clean {Run make clean}
}
Package img1.4.3 {
  Source {Svn https://svn.code.sf.net/p/tkimg/code/trunk -r 374}
  Configure {
    Config [Get srcdir-sys]
  }
  Make {Run make collate}
  Install {
    Run make install-libraries
    Libdir Img1.4.3
  }
  Clean {Run make clean}
}
#@endverbatim
## @defgroup itcl
#@verbatim
Package itcl3.4 {
  Source {Cvs incrtcl.cvs.sourceforge.net:/cvsroot/incrtcl -D 2010-10-28 incrTcl}
  Configure {Config [Get srcdir-sys]/itcl}
  Make {Run make}
  Install {Run make install-binaries install-libraries install-doc}
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup itk
#@verbatim
Package itk3.4 {
  Require {Use itcl3.4}
  Source {Link itcl3.4}
  Configure {Config [Get srcdir-sys]/itk}
  Make {Run make}
  Install {Run make install-binaries install-libraries}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup itk
#@verbatim
Package itk4.0 {
  Source {Wget http://prdownloads.sourceforge.net/kbskit/itk/itk40.tgz}
  Configure {}
  Install {Tcl itk4.0 library}
}
#@endverbatim
## @defgroup iwidgets
# @bug Source: LD_LIBRARY_PATH setting
#@verbatim
Package iwidgets4.0.2 {
  Require {Use itk3.4}
  Source {
    Cvs incrtcl.cvs.sourceforge.net:/cvsroot/incrtcl -D 2010-10-28 iwidgets 
    PatchOld [Get srcdir]/Makefile.in 72 {		  @LD_LIBRARY_PATH_VAR@="$(EXTRA_PATH):$(@LD_LIBRARY_PATH_VAR@)"}  {		  LD_LIBRARY_PATH="$(EXTRA_PATH):$(@LD_LIBRARY_PATH_VAR@)"}
  }
  Configure {
  Config [Get srcdir-sys] -with-itcl=[Get srcdir-sys]/../itcl3.4
  }
  Make {Run make}
  Install {Run make install}
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup iwidgets
#@verbatim
Package iwidgets4.1 {
  Require {Use itk4.0}
  Source {Wget http://prdownloads.sourceforge.net/kbskit/itk/iwidgets41.tgz
Wget https://chiselapp.com/user/rene/repository/iwidgets/iwidgets.tgz?uuid=66a0503a53ed1c6bd09619fcbb6ae6fe3d938398}
  Configure {}
  Install {Tcl iwidgets4.1 library}
}
#@endverbatim
## @defgroup kbskit
#@verbatim
Package kbskit0.4 {
  Source {Cvs kbskit.cvs.sourceforge.net:/cvsroot/kbskit -r kbskit_0_4 kbskit}
}
#@endverbatim
## @defgroup kbskit
#@verbatim
Package kbskit8.5 {
  Require {
    Use kbskit0.4 sdx.kit
    Use tk8.5-static tk8.5 vfs1.4-static zlib1.2.8-static thread2.6.7 {*}[Get bi]
    if {[lsearch -glob [Get kit] {vq*}] != -1} { Use vqtcl4.1-static }
    if {[lsearch -glob [Get kit] {mk*}] != -1} { Use mk4tcl2.4.9.7-static itcl3.4 }
  }
  Source {Link kbskit0.4}
  Configure {Config [Get srcdir-sys] --disable-shared}
  Make {
    set MYMK "[Get builddir-sys]/lib/mk4tcl2.4.9.7-static/Mk4tcl.a "
    if {$::tcl_platform(os) == "Darwin"} {
      append MYMK "-static-libgcc -lstdc++ -framework CoreFoundation"
    } elseif {$::tcl_platform(os) == "SunOS" && [Get CC] == "cc"} {
      append MYMK "-lCstd -lCrun"
    } elseif {[Get staticstdcpp]} {
      append MYMK "-Wl,-Bstatic -lstdc++ -Wl,-Bdynamic -lm"
    }  else  {
      append MYMK "-lstdc++"
    }
    if {$::tcl_platform(platform) == "windows"} {
      set MYCLI "[Get builddir-sys]/lib/libtcl85s.a"
      append MYCLI " [Get builddir-sys]/lib/libz.a"
      append MYCLI " [Get builddir-sys]/lib/vfs1.4.1/vfs141.a"
      set MYGUI "[Get builddir-sys]/lib/libtk85s.a"
      set MYVQ "[Get builddir-sys]/lib/vqtcl4.1/vqtcl41.a"
    } else {
      set MYCLI "[Get builddir-sys]/lib/libtcl8.5.a"
      append MYCLI " [Get builddir-sys]/lib/libz.a"
      append MYCLI " [Get builddir-sys]/lib/vfs1.4.1/libvfs1.4.1.a"
      set MYGUI "[Get builddir-sys]/lib/libtk8.5.a"
      set MYVQ "[Get builddir-sys]/lib/vqtcl4.1/libvqtcl4.1.a"
    }
    if {[Get -threads] in {--enable-threads --enable-threads=yes {}}} {
      set MYKITVQ "thread2.6.7"
      set MYKITMK "thread2.6.7 itcl3.4"
    } else {
      set MYKITVQ ""
      set MYKITMK "itcl3.4"
    }
    foreach my [Get kit] {
      Run make MYCLI=$MYCLI MYGUI=$MYGUI MYVQ=$MYVQ MYKITVQ=$MYKITVQ MYMK=$MYMK MYKITMK=$MYKITMK MYKITBI=[Get bi] all-$my
    }
  }
  Install {foreach my [Get kit] {Run make install-$my}}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup kbskit
#@verbatim
Package kbskit8.6 {
  Require {
    Use kbskit0.4 sdx.kit
    Use tk8.6-static tk8.6 vfs1.4-static zlib1.2.8-static {*}[Get bi]
    if {[lsearch -glob [Get kit] {vq*}] != -1} { Use vqtcl4.1-static }
    if {[lsearch -glob [Get kit] {mk*}] != -1} { Use mk4tcl2.4.9.7-static}
  }
  Source {Link kbskit0.4}
  Configure {Config [Get srcdir-sys] --disable-shared}
  Make {
    set MYMK "[Get builddir-sys]/lib/mk4tcl2.4.9.7-static/Mk4tcl.a "
    if {$::tcl_platform(platform) == "windows"} {
      if {[Get staticstdcpp]} {
        append MYMK "[Get builddir-sys]/lib/libtclstub86s.a -static-libstdc++ -lstdc++"
      } else {
        append MYMK "[Get builddir-sys]/lib/libtclstub86s.a -lstdc++"
      }
    } elseif {$::tcl_platform(os) == "Darwin"} {
      append MYMK "-lstdc++ -framework CoreFoundation"
    } elseif {$::tcl_platform(os) == "SunOS" && [Get CC] == "cc"} {
      append MYMK "-lCstd -lCrun"
    } elseif {[Get staticstdcpp]} {
      append MYMK "-Wl,-Bstatic -lstdc++ -Wl,-Bdynamic -lm"
    }  else  {
      append MYMK "-lstdc++"
    }
    if {$::tcl_platform(platform) == "windows"} {
      set MYCLI "[Get builddir-sys]/lib/libtcl86s.a"
      append MYCLI " [Get builddir-sys]/lib/vfs1.4.1/vfs141.a"
      set MYGUI "[Get builddir-sys]/lib/libtk86s.a"
      set MYVQ "[Get builddir-sys]/lib/vqtcl4.1/vqtcl41.a [Get builddir-sys]/lib/libtclstub86s.a"
    } else {
      set MYCLI "[Get builddir-sys]/lib/libtcl8.6.a"
      append MYCLI " [Get builddir-sys]/lib/vfs1.4.1/libvfs1.4.1.a"
      set MYGUI "[Get builddir-sys]/lib/libtk8.6.a"
      set MYVQ "[Get builddir-sys]/lib/vqtcl4.1/libvqtcl4.1.a [Get builddir-sys]/lib/libtclstub8.6.a"
    }
    if {[Get -threads] in {--enable-threads --enable-threads=yes {}}} {
      set MYKITVQ "thread2.7.2 tdbc1.0.3 itcl4.0.3 sqlite3.8.8.3 tdbcmysql1.0.3 tdbcodbc1.0.3 tdbcpostgres1.0.3"
      set MYKITMK "thread2.7.2 tdbc1.0.3 itcl4.0.3 sqlite3.8.8.3 tdbcmysql1.0.3 tdbcodbc1.0.3 tdbcpostgres1.0.3"
    } else {
      set MYKITVQ "tdbc1.0.3 itcl4.0.3 sqlite3.8.8.3 tdbcmysql1.0.3 tdbcodbc1.0.3 tdbcpostgres1.0.3"
      set MYKITMK "tdbc1.0.3 itcl4.0.3 sqlite3.8.8.3 tdbcmysql1.0.3 tdbcodbc1.0.3 tdbcpostgres1.0.3"
    }
    foreach my [Get kit] {
      Run make MYCLI=$MYCLI MYGUI=$MYGUI MYVQ=$MYVQ MYKITVQ=$MYKITVQ MYMK=$MYMK MYKITMK=$MYKITMK MYKITBI=[Get bi] all-$my
    }
  }
  Install {foreach my [Get kit] {Run make install-$my}}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup kkgkit
#@verbatim
Package kkgkit {
}
# kkgkit is a copy of kbskit0.4 with changed tcl.rc and tclkit.ico
# rbc0.1 rbcGrAxis.c DEF_NUM_TICKS 10 (old=4)
Package kkgkit8.6 {
  Require {
    Use kkgkit sdx.kit
    Use tk8.6-static tk8.6 vfs1.4-static zlib1.2.8-static rbc0.1
    if {[lsearch -glob [Get kit] {vq*}] != -1} { Use vqtcl4.1-static }
  }
  Source {Link kkgkit}
  Configure {Config [Get srcdir-sys] --disable-shared}
  Make {
    if {$::tcl_platform(platform) == "windows"} {
      set MYCLI "[Get builddir-sys]/lib/libtcl86s.a"
#      append MYCLI " [Get builddir-sys]/lib/libz.a"
      append MYCLI " [Get builddir-sys]/lib/vfs1.4.1/vfs141.a"
      set MYGUI "[Get builddir-sys]/lib/libtk86s.a"
      set MYVQ "[Get builddir-sys]/lib/vqtcl4.1/vqtcl41.a [Get builddir-sys]/lib/libtclstub86s.a"
    } else {
      set MYCLI "[Get builddir-sys]/lib/libtcl8.6.a"
#      append MYCLI " [Get builddir-sys]/lib/libz.a"
      append MYCLI " [Get builddir-sys]/lib/vfs1.4.1/libvfs1.4.1.a"
      set MYGUI "[Get builddir-sys]/lib/libtk8.6.a"
      set MYVQ "[Get builddir-sys]/lib/vqtcl4.1/libvqtcl4.1.a [Get builddir-sys]/lib/libtclstub8.6.a"
    }
    if {[Get -threads] in {--enable-threads --enable-threads=yes {}}} {
      set MYKITVQ "thread2.7.2 tdbc1.0.3 sqlite3.8.8.3 tdbcodbc1.0.3 rbc0.1"
    } else {
      set MYKITVQ "tdbc1.0.3 sqlite3.8.8.3 tdbcodbc1.0.3 rbc0.1"
    }
    file delete -force [Get builddir]/lib/tk8.6/demos
    file delete -force [Get builddir]/lib/rpc0.1/demos
    foreach my [Get kit] {
      Run make MYCLI=$MYCLI MYGUI=$MYGUI MYVQ=$MYVQ MYKITVQ=$MYKITVQ all-$my
    }
  }
  Install {foreach my [Get kit] {Run make install-$my}}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup memchan
#@verbatim
Package memchan2.3 {
  Source {Wget http://prdownloads.sourceforge.net/memchan/Memchan2.3.tar.gz}
  Configure {Config [Get srcdir-sys]}
  Make {Run make binaries}
  Install {
    Run make install-binaries
    Libdir Memchan2.3
  }
  Clean {Run make clean}
}
#@endverbatim
## @defgroup mentry
#@verbatim
Package mentry3.7 {
  Require {Use wcb3.5}
  Source {Wget http://www.nemethi.de/mentry/mentry3.7.tar.gz}
  Configure {}
  Install {Tcl}
}
#@endverbatim
## @defgroup mk4tcl
#@verbatim
Package mk4tcl2.4.9.7 {
  Source {
    Wget https://github.com/jcw/metakit/tarball/2.4.9.7
    Tgz [Get builddir-sys]/../sources/2.4.9.7
    file delete -force [Get builddir-sys]/../sources/2.4.9.7
  }
}
  #Source {Wget http://equi4.com/pub/mk/metakit-2.4.9.7.tar.gz}
#@endverbatim
## @defgroup mk4tcl
# @bug Configure: CXXFLAGS wrong
# @bug Configure: include SunOS cc with problems on wide int
#@verbatim
Package mk4tcl2.4.9.7-static {
  Source {Link mk4tcl2.4.9.7}
  Configure {
    PatchOld [Get srcdir]/unix/Makefile.in 46 {CXXFLAGS = $(CXX_FLAGS)} {CXXFLAGS = -DSTATIC_BUILD $(CXX_FLAGS)}
    if {$::tcl_platform(os) == "SunOS" && [Get CC] == "cc"} {
      PatchOld [Get srcdir]/tcl/mk4tcl.h 9 "#include <tcl.h>\n\n" "#include <tcl.h>\n#undef TCL_WIDE_INT_TYPE\n"
    }
    Config [Get srcdir-sys]/unix --disable-shared --with-tcl=[Get builddir-sys]/include
  }
  Make {Run make tcl}
  Install {
    Run make install
    Libdir Mk4tcl
  }
}
#@endverbatim
## @defgroup nap
#@verbatim
Package nap7.0.0 {
  Source {Cvs tcl-nap.cvs.sourceforge.net:/cvsroot/tcl-nap -r nap-7-0-0 tcl-nap}
  Configure {Config [Get srcdir-sys]/[Get sys]}
  Make {Run make binaries}
  Install {Run make install-binaries}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup nsf
# @bug tclCompile.h not found
#@verbatim
Package nsf2.0.0 {
  Source {Wget http://prdownloads.sourceforge.net/next-scripting/nsf2.0.0.tar.gz}
  Configure {
    PatchOld [Get srcdir]/Makefile.in 213 \
{INCLUDES	= @PKG_INCLUDES@ @TCL_INCLUDES@ @NSF_BUILD_INCLUDE_SPEC@
} {INCLUDES	= @PKG_INCLUDES@ @TCL_INCLUDES@ @NSF_BUILD_INCLUDE_SPEC@ -I@TCL_SRC_DIR@/generic -I$(srcdir)/generic
}
    Config [Get srcdir-sys]
  }
  Make {
#    Run make genstubs
    Run make
  }
  Install {Run make install}
  Clean {Run make clean}
}
Package nsf2.0b5 {
  Source {Wget http://prdownloads.sourceforge.net/next-scripting/nsf2.0b5.tar.gz}
  Configure {
    PatchOld [Get srcdir]/Makefile.in 195 \
{		  TCLLIBPATH="$(top_builddir) ${srcdir} $(TCLLIBPATH)"} \
{		  TCLLIBPATH="${srcdir} $(top_builddir) $(TCLLIBPATH)"}
    PatchOld [Get srcdir]/Makefile.in 201 \
{INCLUDES	= @PKG_INCLUDES@ @TCL_INCLUDES@ @NSF_BUILD_INCLUDE_SPEC@
} {INCLUDES	= @PKG_INCLUDES@ @TCL_INCLUDES@ @NSF_BUILD_INCLUDE_SPEC@ -I@TCL_SRC_DIR@/generic
}
    Config [Get srcdir-sys]
  }
  Make {Run make genstubs
    Run make
  }
  Install {Run make install}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup ral
#@verbatim
Package ral0.10.2 {
  Source {Cvs tclral.cvs.sourceforge.net:/cvsroot/tclral -r VERSION_0_10_2 .}
  Configure {
    if {[Get sys] eq {unix}} {
      file attributes [Get srcdir]/tclconfig/install-sh -permissions u+x
    }
    Config [Get srcdir-sys]
  }
  Make {Run make CPPFLAGS=-I[Get srcdir-sys]/../tcl8.5/libtommath binaries}
  Install {Run make install-binaries}
  Clean {Run make clean}
}
Package ral0.11.2 {
  Source {Wget http://prdownloads.sourceforge.net/tclral/tclral-0.11.2.tar.gz}
  Configure {
    Config [Get srcdir-sys]
  }
  Make {Run make binaries libraries}
  Install {Run make install-binaries install-libraries}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup rbc
#@verbatim
Package rbc0.1 {
  Source {Svn https://svn.code.sf.net/p/rbctoolkit/code/trunk/rbc -r 49}
  Configure {
    proc MY {f} {
      set fd [open $f r];set c [read $fd];close $fd;
      regsub -all tkpWinRopModes $c tkpWinRopMode1 c
      regsub -all interp->result $c Tcl_GetString(Tcl_GetObjResult(interp)) c
      set fd [open $f w];puts $fd $c;close $fd;
    }
    MY [Get srcdir]/generic/rbcGrLine.c
    MY [Get srcdir]/generic/rbcTile.c
    MY [Get srcdir]/generic/rbcWinDraw.c
    MY [Get srcdir]/generic/rbcVecMath.c
    rename MY {}
    if {[Get sys] eq {unix}} {
      file attributes [Get srcdir]/tclconfig/install-sh -permissions u+x
    }
    Config [Get srcdir-sys]
  }
  Make {Run make}
  Install {Run make install}
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup sdx
#@verbatim
Package sdx.kit {
  Source {Wget http://tclkit.googlecode.com/files/sdx-20110317.kit}
  Configure {}
  Install {file copy -force [Get srcdir]/sdx-20110317.kit [Get builddir]/bin/sdx.kit}
}
#@endverbatim
## @defgroup snack
#@verbatim
Package snack2.2 {
  Source {Wget http://www.speech.kth.se/snack/dist/snack2.2.10.tar.gz}
  Configure {Config [Get srcdir-sys]/[Get sys] -libdir=[Get builddir-sys]/lib --includedir=[Get builddir-sys]/include}
  Make {Run make}
  Install {Run make install}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup tablelist
#@verbatim
Package tablelist5.13 {
  Source {Wget http://www.nemethi.de/tablelist/tablelist5.13.tar.gz}
  Configure {}
  Install {Tcl}
}
#@endverbatim
## @defgroup tcl
#@verbatim
Package tcl8.5 {
  Source {Wget http://prdownloads.sourceforge.net/tcl/tcl8.5.18-src.tar.gz}
  Configure {Config [Get srcdir-sys]/[Get sys]}
  Make {Run make}
  Install {Run make install-binaries install-libraries install-private-headers}
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup tcl
#@verbatim
Package tcl8.5-static {
  Source {Link tcl8.5}
  Configure {Config [Get srcdir-sys]/[Get sys] --disable-shared}
  Make {Run make}
  Install {Run make install-binaries install-libraries install-private-headers}
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup tcl
#@verbatim
Package tcl8.6 {
  Source {Wget http://prdownloads.sourceforge.net/tcl/tcl8.6.4-src.tar.gz}
  Configure {Config [Get srcdir-sys]/[Get sys]}
  Make {Run make}
  Install {Run make install install-private-headers}
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup tcl
#@verbatim
Package tcl8.6-static {
  Source {Link tcl8.6}
  Configure {Config [Get srcdir-sys]/[Get sys] --disable-shared}
  Make {Run make}
  Install {
    Run make install install-private-headers
#TODO siehe kbskit8.6
    if {[Get sys] eq {win}} {
      file copy -force [Get builddir]/tcl8.6-static/libtclstub86.a [Get builddir]/lib/libtclstub86s.a
      file copy -force [Get builddir]/tcl8.6-static/libtcl86.a [Get builddir]/lib/libtcl86s.a
    }
  }
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup tcllib
#@verbatim
Package tcllib1.17 {
  Source {Wget http://prdownloads.sourceforge.net/tcllib/tcllib-1.17.tar.gz}
  Configure {Config [Get srcdir-sys]}
  Make {}
  Install {Run make install-libraries}
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup tcloo
#@verbatim
Package tcloo0.6 {
  Source {Wget http://prdownloads.sourceforge.net/tcl/TclOO1.0.tar.gz}
  Configure {Config [Get srcdir-sys]}
  Make {Run make}
  Install {
    Run make install install-private-headers
    Libdir TclOO1.0
  }
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup tclx
#@verbatim
Package tclx8.4 {
  Source {Cvs tclx.cvs.sourceforge.net:/cvsroot/tclx -D 2010-10-28 tclx}
  Configure {Config [Get srcdir-sys]}
  Make {Run make binaries}
  Install {Run make install-binaries}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup tdom
#@verbatim
Package tdom0.8.3 {
  Source {Wget https://github.com/tDOM/tdom/archive/363cbda3ac91b8955edbfd2b64625c725385d5b4.tar.gz}
  Configure {Config [Get srcdir-sys]}
  Make {Run make binaries}
  Install {Run make install-binaries}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup thread
#@verbatim
Package thread2.6.7 {
  Source {Wget http://prdownloads.sourceforge.net/tcl/thread2.6.7.tar.gz}
  Configure {Config [Get srcdir-sys]}
  Make {Run make}
  Install {Run make install-binaries}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup thread
#@verbatim
Package thread2.6.7-static {
  Source {Link thread2.6.7}
  Configure {Config [Get srcdir-sys] --disable-shared}
  Make {Run make}
  Install {Run make install-binaries}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup tk
#@verbatim
Package tk8.5 {
  Require {Use tcl8.5}
  Source {Wget http://prdownloads.sourceforge.net/tcl/tk8.5.18-src.tar.gz}
  Configure {Config [Get srcdir-sys]/[Get sys]}
  Make {Run make}
  Install {Run make install install-private-headers}
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup tk
#@verbatim
Package tk8.5-static {
  Require {Use tcl8.5-static}
  Source {Link tk8.5}
  Configure {Config [Get srcdir-sys]/[Get sys] --disable-shared}
  Make {Run make}
  Install {Run make install install-private-headers}
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup tk
#@verbatim
Package tk8.6 {
  Require {Use tcl8.6}
  Source {Wget http://prdownloads.sourceforge.net/tcl/tk8.6.4-src.tar.gz}
  Configure {
#    if {$::tcl_platform(os) != "Darwin"} {
#    PatchOld [Get srcdir]/unix/configure 10370\
#{	    XFT_LIBS=`pkg-config --libs xft 2>/dev/null` || found_xft="no"}\
#{	    XFT_LIBS=`pkg-config --libs xft fontconfig 2>/dev/null` || found_xft="no"}
#    }
    if {$::tcl_platform(os) == "Darwin"} {
		set tkopt --enable-aqua
	} else {
		set tkopt ""
	}

    Config [Get srcdir-sys]/[Get sys] {*}$tkopt
  }
  Make {Run make}
  Install {Run make install install-private-headers}
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup tk
#@verbatim
Package tk8.6-static {
  Require {Use tcl8.6 tcl8.6-static}
  Source {Link tk8.6}
  Configure {
#    if {$::tcl_platform(os) != "Darwin"} {
#    PatchOld [Get srcdir]/unix/configure 10370\
#{	    XFT_LIBS=`pkg-config --libs xft 2>/dev/null` || found_xft="no"}\
#{	    XFT_LIBS=`pkg-config --libs xft fontconfig 2>/dev/null` || found_xft="no"}
#    }
    if {$::tcl_platform(os) == "Darwin"} {
		set tkopt --enable-aqua
	} else {
		set tkopt ""
	}
    Config [Get srcdir-sys]/[Get sys] --disable-shared {*}$tkopt
  }
  Make {Run make}
  Install {Run make install install-private-headers
    if {[Get sys] eq {win}} {
      file copy -force [Get builddir]/tk8.6-static/libtkstub86.a [Get builddir]/lib/libtkstub86s.a
      file copy -force [Get builddir]/tk8.6-static/libtk86.a [Get builddir]/lib/libtk86s.a
    }
  }
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup tkcon
#@verbatim
Package tkcon {
  Source {Cvs tkcon.cvs.sourceforge.net:/cvsroot/tkcon -D 2014-12-01 tkcon}
  Configure {}
  Install {Tcl}
}
#@endverbatim
## @defgroup tkdnd
#@verbatim
Package tkdnd2.7 {
  Source {Wget http://prdownloads.sourceforge.net/tkdnd/TkDND/TkDND%202.7/tkdnd2.7-src.tar.gz}
  Configure {
    # fix bogus garbage collection flag
    PatchOld [Get srcdir]/configure 8673 {    PKG_CFLAGS="$PKG_CFLAGS -DMAC_TK_COCOA -std=gnu99 -x objective-c -fobjc-gc"} {\
    PKG_CFLAGS="$PKG_CFLAGS -DMAC_TK_COCOA -std=gnu99 -x objective-c"}
    Config [Get srcdir-sys]
  }
  Make {Run make}
  Install {Run make install}
  Clean {Run make clean}
  Test {Run make test}
}
Package tkdnd2.8 {
  Source {Wget http://prdownloads.sourceforge.net/tkdnd/TkDND/TkDND%202.8/tkdnd2.8-src.tar.gz}
  Configure {
    # fix bogus garbage collection flag
    PatchOld [Get srcdir]/configure 6148 {    PKG_CFLAGS="$PKG_CFLAGS -DMAC_TK_COCOA -std=gnu99 -x objective-c -fobjc-gc"} {\
    PKG_CFLAGS="$PKG_CFLAGS -DMAC_TK_COCOA -std=gnu99 -x objective-c"}
    Config [Get srcdir-sys]
  }
  Make {Run make}
  Install {Run make install}
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup tklib
# @todo  Source {Wget http://core.tcl.tk/tklib/tarball/tklib-0.6.tar.gz?uuid=tklib-0-6}
#@verbatim
Package tklib0.6 {
  Source {Wget https://github.com/tcltk/tklib/archive/276f886e3e53339d58d410711b7d36d20ef1c138.zip}
  Configure {Config [Get srcdir-sys]}
  Make {}
  Install {Run make install-libraries}
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup tksqlite
#@verbatim
Package tksqlite0.5.11 {
  Require {Use kbskit8.6 sdx.kit tktable2.10 treectrl2.4.1}
  Source {Wget http://reddog.s35.xrea.com/software/tksqlite-0.5.11.tar.gz}
  Configure {
#    PatchOld [Get srcdir]/tksqlite.tcl 14708\
#{		Cmd::openDB [file normalize [file join $_startdir $_file]]}\
#{		Cmd::openDB [file normalize [file join $::starkit::topdir .. $_file]]}
    Kit {source $::starkit::topdir/tksqlite.tcl} Tk
  }
  Make {Kit tksqlite sqlite3.8.8.3 tktable2.10 treectrl2.4.1}
  Install {Kit tksqlite -vq-gui}
  Clean {file delete -force tksqlite.vfs}
  Test {Kit tksqlite}
}
#@endverbatim
## @defgroup tkpath
#@verbatim
Package tkpath0.3.3 {
  Source {Wget https://bitbucket.org/andrew_shadura/tkpath/get/a6704f14a201.zip}
  Configure {
	# fix ANSI ARGS define
    PatchOld [Get srcdir]/generic/tkp.h 14 {} \
{#ifndef _ANSI_ARGS_
#define _ANSI_ARGS_(X) X
#endif
}
	# fix link problem on OSX from forcing universal binary
	PatchOld [Get srcdir]/configure 6262 \
{	PKG_LIBS="$PKG_LIBS $i"} \
{	#PKG_LIBS="$PKG_LIBS $i"}
	
	PatchOld [Get srcdir]/configure 6326 \
{    PKG_CFLAGS="$PKG_CFLAGS -arch i386 -arch x86_64"} \
{    #PKG_CFLAGS="$PKG_CFLAGS -arch i386 -arch x86_64"}
    Config [Get srcdir-sys] --with-tkinclude=[Get srcdir-sys]/../tk8.6/generic
  }
  Make {
    Run make binaries libraries
  }
  Install {
    Run make install-binaries install-libraries
  }
  Clean {Run make clean}
}
#@endverbatim
## @defgroup tktable
#@verbatim
Package tktable2.10 {
  Source {Cvs tktable.cvs.sourceforge.net:/cvsroot/tktable -r tktable-2-10-0 tktable}
  Configure {
    PatchOld [Get srcdir]/generic/tkTable.h 21 {#include <tk.h>} {#include <tkInt.h>}
    Config [Get srcdir-sys]
  }
  Make {Run make binaries}
  Install {
    Run make install-binaries
    Libdir Tktable2.10
  }
  Clean {Run make clean}
}
#@endverbatim
## @defgroup tls
#@verbatim
Package tls1.6.7 {
  Source {Wget http://prdownloads.sourceforge.net/tls/tls1.6.7-src.tar.gz}
  Configure {Config [Get srcdir-sys]}
  Make {Run make}
  Install {Run make install}
  Clean {Run make clean}
  Test {Run make test}
}
#@endverbatim
## @defgroup treectrl
# @todo Error in configure: tk header default.h not found
#@verbatim
Package treectrl2.4.1 {
  Source {Wget http://prdownloads.sourceforge.net/sourceforge/tktreectrl/tktreectrl-2.4.1.tar.gz}
  Configure {
	# fix bogus garbage collection flag
    PatchOld [Get srcdir]/configure 6430 {    PKG_CFLAGS="$PKG_CFLAGS -DMAC_TK_COCOA -std=gnu99 -x objective-c -fobjc-gc"} {\
    PKG_CFLAGS="$PKG_CFLAGS -DMAC_TK_COCOA -std=gnu99 -x objective-c"}
	# fix wrong detection of Tk private headers
    PatchOld [Get srcdir]/configure 5492 {	-f "${ac_cv_c_tkh}/tkWinPort.h"; then} {""; then}
    PatchOld [Get srcdir]/configure 5495 {	-f "${ac_cv_c_tkh}/tkUnixPort.h"; then} {""; then}
    Config [Get srcdir-sys]
  }
  Make {Run make}
  Install {Run make install-binaries install-libraries}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup trofs
#@verbatim
Package trofs0.4.6 {
  Source {Wget http://math.nist.gov/~DPorter/tcltk/trofs/trofs0.4.6.tar.gz}
  Configure {
#    PatchOld [Get srcdir]/Makefile.in 148 \
{DEFS		= @DEFS@ $(PKG_CFLAGS)} \
{DEFS		= @DEFS@ $(PKG_CFLAGS) -D_USE_32BIT_TIME_T}
    Config [Get srcdir-sys]
  }
  Make {Run make binaries}
  Install {Run make install-binaries}
  Clean {Run make clean}
}
Package trofs0.4.8 {
  Source {Wget http://math.nist.gov/~DPorter/tcltk/trofs/trofs0.4.8.tar.gz}
  Configure {
    Config [Get srcdir-sys]
  }
  Make {Run make binaries}
  Install {Run make install-binaries}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup udp
#@verbatim
Package udp1.0.8 {
  Source {Cvs tcludp.cvs.sourceforge.net:/cvsroot/tcludp -r tcludp-1_0_8 tcludp}
  Configure {
    if {[Get sys] eq {unix}} {
      file attributes [Get srcdir]/tclconfig/install-sh -permissions u+x
    }
    Config [Get srcdir-sys]
  }
  Make {Run make binaries}
  Install {Run make install-binaries}
  Clean {Run make clean}
}
Package udp1.0.11 {
  Source {Wget http://prdownloads.sourceforge.net/sourceforge/tcludp/1.0.11/tcludp-1.0.11.tar.gz}
  Configure {
    Config [Get srcdir-sys]
  }
  Make {Run make binaries}
  Install {Run make install-binaries}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup vfs
#@verbatim
Package vfs1.4 {
  Source {Cvs tclvfs.cvs.sourceforge.net:/cvsroot/tclvfs -D 2012-01-10 tclvfs}
  Configure {
    PatchOld [Get srcdir]/Makefile.in 143 \
{INCLUDES	= @PKG_INCLUDES@ @TCL_INCLUDES@}\
{INCLUDES	= @TCL_INCLUDES@}
#    PatchOld [Get srcdir]/Makefile.in 147 \
#{DEFS		= @DEFS@ $(PKG_CFLAGS)
#} {DEFS		= @DEFS@ $(PKG_CFLAGS) -D_USE_32BIT_TIME_T
#} 
    Config [Get srcdir-sys] --with-tclinclude=[Get builddir-sys]/include
  }
  Make {Run make}
  Install {Run make install-binaries}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup vfs
#@verbatim
Package vfs1.4-static {
  Source {Link vfs1.4}
  Configure {
    PatchOld [Get srcdir]/Makefile.in 143 \
{INCLUDES	= @PKG_INCLUDES@ @TCL_INCLUDES@}\
{INCLUDES	= @TCL_INCLUDES@}
#    PatchOld [Get srcdir]/Makefile.in 147 \
#{DEFS		= @DEFS@ $(PKG_CFLAGS)
#} {DEFS		= @DEFS@ $(PKG_CFLAGS) -D_USE_32BIT_TIME_T
#} 
    Config [Get srcdir-sys] --disable-shared --with-tclinclude=[Get builddir-sys]/include}
  Make {Run make}
  Install {Run make install-binaries}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup vqtcl
#@verbatim
Package vqtcl4.1 {
  Source {Svn http://tclkit.googlecode.com/svn/trunk/vqtcl -r21}
}
#@endverbatim
## @defgroup vqtcl
# @bug Configure: big endian problem
#@verbatim
Package vqtcl4.1-static {
  Source {Link vqtcl4.1}
  Configure {
    PatchOld [Get srcdir]/generic/vlerq.c 42 {#if !defined(_BIG_ENDIAN) && defined(WORDS_BIGENDIAN)} {#if defined(WORDS_BIGENDIAN)}
    Config [Get srcdir-sys] --disable-shared
  }
  Make {
    set MYFLAGS "-D__[exec uname -p]__"
    Run make PKG_CFLAGS=$MYFLAGS
  }
  Install {Run make install-binaries}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup wcb
#@verbatim
Package wcb3.5 {
  Source {Wget http://www.nemethi.de/wcb/wcb3.5.tar.gz}
  Configure {}
  Install {Tcl}
}
#@endverbatim
## @defgroup wikit
#@verbatim
Package wikit.tkd {
  Source {Wget http://prdownloads.sourceforge.net/sourceforge/tclerswikidata/wikit-20090210.tkd}
}
#@endverbatim
## @defgroup xotcl
#@verbatim
Package xotcl1.6.8 {
  Source {Wget http://media.wu-wien.ac.at/download/xotcl-1.6.8.tar.gz}
  Configure {Config [Get srcdir-sys]}
  Make {Run make binaries libraries}
  Install {Run make install-binaries install-libraries}
  Clean {Run make clean}
}
#@endverbatim
## @defgroup z
#@verbatim
Package z0.1 {
  Source {Cvs kbskit.cvs.sourceforge.net:/cvsroot/z -r z_0_1 z}
}
#@endverbatim
## @defgroup zlib
#@verbatim
Package zlib1.2.8 {
  Source {Wget http://sourceforge.net/projects/libpng/files/zlib/1.2.8/zlib-1.2.8.tar.gz}
}
#@endverbatim
## @defgroup zlib
#@verbatim
Package zlib1.2.8-static {
  Source {Link zlib1.2.8}
  Configure {
    eval file copy [glob [Get srcdir]/*] .
    if {$::tcl_platform(platform) ne {windows}} {
      set MYFLAGS "[Get TCL_EXTRA_CFLAGS] [Get TCL_CFLAGS_OPTIMIZE]"
      Run env CC=[Get CC] CFLAGS=$MYFLAGS ./configure --prefix=[Get builddir-sys] --eprefix=[Get builddir-sys]
    }
  }
  Make {
    if {$::tcl_platform(platform) eq {windows}} {
      Run env BINARY_PATH=[Get builddir]/bin INCLUDE_PATH=[Get builddir]/include LIBRARY_PATH=[Get builddir]/lib make -fwin32/Makefile.gcc
    } else {
      Run make
    }
  }
  Install {
    if {$::tcl_platform(platform) eq {windows}} {
      Run env BINARY_PATH=[Get builddir]/bin INCLUDE_PATH=[Get builddir]/include LIBRARY_PATH=[Get builddir]/lib make -fwin32/Makefile.gcc install
    } else {
      file mkdir [file join [Get builddir]/share/man]
      Run make install
    }
  }
  Clean {Run make clean}
}
#@endverbatim
## @defgroup tango
#@verbatim
Package tango0.8.90 {
  Source {
    #Cvs :pserver:anoncvs@anoncvs.freedesktop.org:/cvs/tango tango-icon-theme
    Wget http://tango.freedesktop.org/releases/tango-icon-theme-0.8.90.tar.gz
  }
}
#@endverbatim
## @defgroup silkicons
#@verbatim
Package silkicons1.3 {
  Source {Wget http://www.famfamfam.com/lab/icons/silk/famfamfam_silk_icons_v013.zip}
}
#@endverbatim
## @}
}

#===============================================================================

# start application
if {[info exists argv0] && [file tail [info script]] eq [file tail $argv0]} {
  ::kbs_main $argv
}
#===============================================================================
# vim: set syntax=tcl
