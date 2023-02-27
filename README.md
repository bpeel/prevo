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
data. These must be separately built from the dictionary's XML data
using a program called prevodb, which is available here:

 https://github.com/bpeel/prevodb/

The dictionary data is available in these two repos:

 https://github.com/revuloj/revo-fonto  
 https://github.com/revuloj/voko-grundo

You will need to clone both of them in order to build the database.

You can build prevodb with the usual automake commands:

    ./autogen.sh
    make -j4

Note that it will need the developer packages for expat and glib in
order to build.

Now assuming you have the source for ReVo in `$HOME/revo-fonto` and
`$HOME/voko-grundo`, prevodb in `$HOME/prevodb` and PReVo in
`$HOME/prevo`, you can use the following command to build the database
and put it in the right location to build the application:

    $HOME/prevodb/src/prevodb -i $HOME/revo-fonto \
                              -i $HOME/voko-grundo \
                              -o $HOME/prevo/app/src/main

You will likely see some warnings about inconsistencies in the XML
sources. It is safe to ignore these.

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

The message for each tag contains the git hashes used for the ReVo
sources and the prevodb program. This information can be used to build
a copy of a release using exactly the same data. You can see this
information for example with:

    git show 0.12
