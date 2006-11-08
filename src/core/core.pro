# File generated by kdevelop's qmake manager. 
# ------------------------------------------- 
# Subdir relative project main directory: ./src/core
# Target is a library:  traversocore

include(../libbase.pri)

PRECOMPILED_HEADER = precompile.h 

LIBS += -ltraversocommands \
        -ltraversoaudiobackend
        
INCLUDEPATH += ../../src/traverso \
		../../src/traverso/songcanvas \
               ../../src/traverso/build \
               ../../src/core \
               ../../src/commands \
               ../../src/engine \
               ../../src/plugins/LV2 \
               ../../src/plugins \
               . 
QMAKE_LIBDIR = ../../lib 
TARGET = traversocore 
DESTDIR = ../../lib 

TEMPLATE = lib 

SOURCES	= AudioClip.cpp \
	AudioClipList.cpp \
	AudioClipManager.cpp \
	AudioSource.cpp \
	AudioSourceManager.cpp \
	Command.cpp \
	Config.cpp \
	ContextItem.cpp \
	ContextPointer.cpp \
	Curve.cpp \
	CurveNode.cpp \
	Debugger.cpp \
	DiskIO.cpp \
	Export.cpp \
	FadeCurve.cpp \
	FileHelpers.cpp \
	IEAction.cpp \
	Information.cpp \
	InputEngine.cpp \
	Mixer.cpp \
	MtaRegion.cpp \
	MtaRegionList.cpp \
	Peak.cpp \
	PrivateReadSource.cpp \
	Project.cpp \
	ProjectManager.cpp \
	ReadSource.cpp \
	RingBuffer.cpp \
	Song.cpp \
	Track.cpp \
	Utils.cpp \
	WriteSource.cpp \
	gdither.cpp \
	SnapList.cpp

HEADERS	= precompile.h \
	AudioClip.h \
	AudioClipList.h \
	AudioClipManager.h \
	AudioSource.h \
	AudioSourceManager.h \
	Command.h \
	Config.h \
	ContextItem.h \
	ContextPointer.h \
	CurveNode.h \
	Curve.h \
	Debugger.h \
	DiskIO.h \
	Export.h \
	FadeCurve.h \
	FileHelpers.h \
	IEAction.h \
	Information.h \
	InputEngine.h \
	Mixer.h \
	MtaRegion.h \
	MtaRegionList.h \
	Peak.h \
	PrivateReadSource.h \
	Project.h \
	ProjectManager.h \
	ReadSource.h \
	RingBuffer.h \
	RingBufferNPT.h \
	Song.h \
	Track.h \
	Utils.h \
	WriteSource.h \
	libtraversocore.h \
	gdither.h \
	gdither_types.h \
	gdither_types_internal.h \
	noise.h \
	FastDelegate.h \
	SnapList.h
	
macx {
	QMAKE_LIBDIR += /usr/local/qt/lib
}

