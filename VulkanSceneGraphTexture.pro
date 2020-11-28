!qtConfig(vulkan): error("This example requires Qt built with Vulkan support")

QT += qml quick
CONFIG += qmltypes
QML_IMPORT_NAME = VulkanSceneGraphTexture
QML_IMPORT_MAJOR_VERSION = 1

HEADERS += VulkanSceneGraphTexture.h
SOURCES += VulkanSceneGraphTexture.cpp main.cpp
#INCLUDEPATH += ${VULKAN_SDK}/include
INCLUDEPATH += /home/tad/VULKAN/1.2.154.0/x86_64/include
LIBS += -lvulkan -L/home/tad/VULKAN/1.2.154.0/x86_64/lib
RESOURCES += VulkanSceneGraphTexture.qrc

target.path = .
INSTALLS += target
