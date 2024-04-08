PReVo
=====

PReVo is an android application containing a portable version of the
Reta Vortaro which is an open source dictionary for Esperanto. It
contains all of the dictionary data in the package so the application
does not need to access the internet.

The official website for the program is here:

 http://www.busydoingnothing.co.uk/prevo/

It is also available in the Google play store here:

 https://play.google.com/store/apps/details?id=uk.co.busydoingnothing.prevo

or with F-Droid here:

 https://f-droid.org/en/packages/uk.co.busydoingnothing.prevo/

Building
--------

This git repo does not include the assets containing the dictionary
data. Instead they are built from the ReVo data and the prevodb
program which are in git submodules. In order to get these modules, be
sure to run the following git command:

```bash
git submodule update --init
```

The prevodb program will be built as part of the app build in order to
generate the dictionary data, so you need to make sure you have a
compiler for the host machine installed. It will also need the
developer packages for expat and glib.

Assuming you have the Android SDK installed correctly, you can build
the app either with Android Studio or the command line as follows.

Debug mode:

    cd $HOME/prevo
    ./gradlew assembleDebug

Release mode:

    cd $HOME/prevo
    ./gradlew assembleRelease

You should then have the final package in either
`app/build/outputs/apk/debug/` or `app/build/outputs/apk/release/`
depending on the build type.

Building a specific release
---------------------------

The releases are all tagged and signed in the git repo using the
following public key:

 http://www.busydoingnothing.co.uk/neilroberts.asc

The git submodules were added in version 0.25 so the signed tag
contains the commit hash of the dependencies used as well. For older
versions, the message for each tag contains the git hashes used for
the ReVo sources and the prevodb program. This information can be used
to build a copy of a release using exactly the same data. You can see
this information for example with:

    git show 0.12
