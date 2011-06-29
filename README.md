TeXworks
========

This is an unofficial fork of [TeXworks][texworks], a cross-platform editor for
TeX and friends. The main purpose of this fork is to experiment with new
methods of building and packaging using [CMake][cmake] and CPack.

Please see the official TeXworks project page for official information.

What this Fork Does
-------------------

This fork contains a CMake-based build system that is designed to compile
TeXworks and optionally package it into a standalone Application for
redistribution. The CMake system has been tested using the [Homebrew][homebrew]
to supply dependencies required by TeXworks. Theoretically, dependencies could
also be supplied by other package managers such as [MacPorts][macports] or
[Fink][fink] or by stand alone binaries. This theory has not been tested and
the build system could require some modification in order to work with other
component sources ([pull requests][make-a-pull] are welcomed if someone has a
patch that improves the build system).

The build system is extensively documented with comments which are available
for your viewing pleasure at [Sharpie.github.com/TeXworks](http://Sharpie.github.com/TeXworks)
thanks to Ryan Tomayko's [Rocco][rocco] tool.

How to Build
------------
*Using Homebrew components*

### Build One for Yourself

```bash
# Install Homebrew
ruby -e "$(curl -fsSL https://gist.github.com/raw/323731/install_homebrew.rb)"

# Use Homebrew to install core tools
brew install git
brew install pkg-config
brew install cmake

# Use Homebrew to install TeXworks dependencies
# Qt will take a while, ~30 min on a 8-core Mac Pro. Go walk the dog.
brew install qt
brew install hunspell
brew install poppler --with-qt4 --enable-xpdf-headers
brew install lua # Optional---enables TeXworks support for Lua scripts


# Now, you should be able to build TeXworks
git clone git://github.com/Sharpie/TeXworks.git
cd TeXworks

mkdir build
cd build

cmake ..
make install -j <number of cores on your machine>
```

### Package One for a Friend

```bash
# Same Homebrew setup steps as above, except you may want to use the customized
# Poppler formula provided in this repository as it cuts down on the number of
# unused dependencies that will be packaged into the Application.
git clone git://github.com/Sharpie/TeXworks.git
cd TeXworks
brew install CMake/packaging/mac/poppler.rb --with-qt4 --enable-xpdf-headers

mkdir build
cd build

cmake ..
make package -j <number of cores on your machine>

# The above step creates a nice Drag N' Drop installer. This step prettifies
# it.
./set_dmg_layout.scpt
```

Good Luck!

  [texworks]:     http://code.google.com/p/texworks
  [cmake]:        http://cmake.org
  [homebrew]:     https://mxcl.github.com/homebrew/
  [macports]:     http://macports.org
  [fink]:         http://finkproject.org
  [make-a-pull]:  https://github.com/Sharpie/TeXworks/pulls
  [rocco]:        https://github.com/rtomayko/rocco
