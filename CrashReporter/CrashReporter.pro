TARGET = NatronCrashReporter
QT       += core network gui

CONFIG += console
CONFIG -= app_bundle
CONFIG += moc
CONFIG += qt

TEMPLATE = app

CONFIG(debug, debug|release){
    DEFINES *= DEBUG
} else {
    DEFINES *= NDEBUG
}

macx {
  # Set the pbuilder version to 46, which corresponds to Xcode >= 3.x
  # (else qmake generates an old pbproj on Snow Leopard)
  QMAKE_PBUILDER_VERSION = 46

  QMAKE_MACOSX_DEPLOYMENT_VERSION = $$split(QMAKE_MACOSX_DEPLOYMENT_TARGET, ".")
  QMAKE_MACOSX_DEPLOYMENT_MAJOR_VERSION = $$first(QMAKE_MACOSX_DEPLOYMENT_VERSION)
  QMAKE_MACOSX_DEPLOYMENT_MINOR_VERSION = $$last(QMAKE_MACOSX_DEPLOYMENT_VERSION)
  universal {
    message("Compiling for universal OSX $${QMAKE_MACOSX_DEPLOYMENT_MAJOR_VERSION}.$$QMAKE_MACOSX_DEPLOYMENT_MINOR_VERSION")
    contains(QMAKE_MACOSX_DEPLOYMENT_MAJOR_VERSION, 10) {
      contains(QMAKE_MACOSX_DEPLOYMENT_TARGET, 4)|contains(QMAKE_MACOSX_DEPLOYMENT_MINOR_VERSION, 5) {
        # OSX 10.4 (Tiger) and 10.5 (Leopard) are x86/ppc
        message("Compiling for universal ppc/i386")
        CONFIG += x86 ppc
      }
      contains(QMAKE_MACOSX_DEPLOYMENT_MINOR_VERSION, 6) {
        message("Compiling for universal i386/x86_64")
        # OSX 10.6 (Snow Leopard) may run on Intel 32 or 64 bits architectures
        CONFIG += x86 x86_64
      }
      # later OSX instances only run on x86_64, universal builds are useless
      # (unless a later OSX supports ARM)
    }
  }

  #link against the CoreFoundation framework for the StandardPaths functionnality
  LIBS += -framework CoreServices
}

SOURCES += \
    CrashDialog.cpp \
    main.cpp

HEADERS += \
    CrashDialog.h

BREAKPAD_PATH = ../google-breakpad/src
INCLUDEPATH += $$BREAKPAD_PATH

SOURCES += \
        $$BREAKPAD_PATH/client/minidump_file_writer.cc \
        $$BREAKPAD_PATH/common/md5.cc \
        $$BREAKPAD_PATH/common/string_conversion.cc \
        $$BREAKPAD_PATH/common/convert_UTF.c \

# mac os x
mac {
        # hack to make minidump_generator.cc compile as it uses
        # esp instead of __esp
        # DEFINES += __DARWIN_UNIX03=0 -- looks like we doesn't need it anymore

        SOURCES += \
                $$BREAKPAD_PATH/client/mac/handler/breakpad_nlist_64.cc \
                $$BREAKPAD_PATH/client/mac/handler/minidump_generator.cc \
                $$BREAKPAD_PATH/client/mac/handler/dynamic_images.cc \
                $$BREAKPAD_PATH/client/mac/crash_generation/crash_generation_server.cc \
                $$BREAKPAD_PATH/common/mac/string_utilities.cc \
                $$BREAKPAD_PATH/common/mac/file_id.cc \
                $$BREAKPAD_PATH/common/mac/macho_id.cc \
                $$BREAKPAD_PATH/common/mac/macho_utilities.cc \
                $$BREAKPAD_PATH/common/mac/macho_walker.cc \
                $$BREAKPAD_PATH/common/mac/bootstrap_compat.cc

        OBJECTIVE_SOURCES += \
                $$BREAKPAD_PATH/common/mac/MachIPC.mm
}

# other *nix
unix:!mac {
        SOURCES += \
                $$BREAKPAD_PATH/client/linux/handler/minidump_generator.cc \
                $$BREAKPAD_PATH/client/linux/handler/linux_thread.cc \
                $$BREAKPAD_PATH/client/linux/crash_generation/crash_generation_server.cc \
                $$BREAKPAD_PATH/common/linux/guid_creator.cc \
                $$BREAKPAD_PATH/common/linux/file_id.cc
}

win32 {
        SOURCES += \
                $BREAKPAD_PATH/client/windows/crash_generation/client_info.cc \
                $$BREAKPAD_PATH/client/windows/crash_generation/crash_generation_server.cc \
                $$BREAKPAD_PATH/client/windows/crash_generation/minidump_generator.cc \
                $$BREAKPAD_PATH/common/windows/guid_string.cc
}

RESOURCES += \
    ../Gui/GuiResources.qrc

INSTALLS += target