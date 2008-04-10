TEMPLATE = lib
TARGET = NXOpenGLRenderingEngine
DESTDIR = ../../../../../../lib/


CONFIG += stl \
 opengl \
 dll \
 rtti \
 plugin \
 debug_and_release \
 build_all

CONFIG(debug,debug|release) {
	TARGET = $$join(TARGET,,,_d)
}

QT += opengl


HEADERS += ../../../../../../include/Nanorex/Interface/NXAtomData.h \
 ../../../../../../include/Nanorex/Interface/NXBondData.h \
 ../../../../../../include/Nanorex/Interface/NXEntityManager.h \
 ../../../../../Plugins/RenderingEngines/OpenGL/NXOpenGLRendererPlugin.h \
 ../../../../../Plugins/RenderingEngines/OpenGL/NXOpenGLRenderingEngine.h \
 ../../../../../../include/Nanorex/Interface/NXRendererPlugin.h \
 ../../../../../../include/Nanorex/Interface/NXRenderingEngine.h \
 ../../../../../../include/Nanorex/Interface/NXSceneGraph.h \
 ../../../../../Plugins/RenderingEngines/OpenGL/NXOpenGLSceneGraph.h \
 ../../../../../Plugins/RenderingEngines/OpenGL/NXOpenGLMaterial.h \
 ../../../../../Plugins/RenderingEngines/OpenGL/NXOpenGLCamera.h \
 ../../../../../Plugins/RenderingEngines/OpenGL/NXOpenGLCamera_sm.h \
 ../../../../../Plugins/RenderingEngines/OpenGL/trackball.h \
 ../../../../../Plugins/RenderingEngines/OpenGL/statemap.h


INCLUDEPATH += ../../../../../../include \
 ../../../../../../src \
 ../../../../../../src/Plugins/RenderingEngines/OpenGL/GLT \
 $(OPENBABEL_INCPATH) \
 $(HDF5_SIMRESULTS_INCPATH) \
 ../../../../../../src/Plugins/RenderingEngines/OpenGL


SOURCES += ../../../../../Plugins/RenderingEngines/OpenGL/NXOpenGLRenderingEngine.cpp \
 ../../../../../Plugins/RenderingEngines/OpenGL/NXOpenGLCamera.cpp \
 ../../../../../Plugins/RenderingEngines/OpenGL/NXOpenGLCamera_sm.cpp \
 ../../../../../Plugins/RenderingEngines/OpenGL/trackball.c

QMAKE_CXXFLAGS_DEBUG += -DNX_DEBUG \
 -g \
 -O0 \
 -fno-inline


TARGETDEPS += ../../../../../../lib/libNXOpenGLSceneGraph.a \
  ../../../../../../lib/libGLT.a \
  ../../../../../../lib/libNanorexInterface.so \
  ../../../../../../lib/libNanorexUtility.so

unix {
    QMAKE_CLEAN += $${DESTDIR}$${TARGET}.so $${DESTDIR}lib$${TARGET}.so
# Remove the "lib" from the start of the library
    QMAKE_POST_LINK = echo $(DESTDIR)$(TARGET) | sed -e \'s/\\(.*\\)lib\\(.*\\)\\(\\.so\\)/\1\2\3/\' | xargs cp $(DESTDIR)$(TARGET)
}

macx {
    QMAKE_CLEAN += $${DESTDIR}$${TARGET}.dylib
    TARGETDEPS ~= s/.so/.dylib/g
    QMAKE_POST_LINK ~= s/.so/.dylib/g
}

win32 {
	CONFIG -= dll
	CONFIG += staticlib
    QMAKE_CLEAN += $${DESTDIR}$${TARGET}.dll
    TARGETDEPS ~= s/.so/.a/g
# qmake puts these library declarations too early in the g++ command on win32
	LIBS += -lopengl32 -lglu32 -lgdi32 -luser32
}


PROJECTLIBS = -lNXOpenGLSceneGraph \
  -lGLT \
  -lNanorexInterface \
  -lNanorexUtility \

CONFIG(debug,debug|release) {
	PROJECTLIBS ~= s/(.+)/\1_d/g
}

# message($$PROJECTLIBS)

LIBS += -L../../../../../../lib \
  $$PROJECTLIBS \
  -L$(OPENBABEL_LIBPATH) \
  -lopenbabel

