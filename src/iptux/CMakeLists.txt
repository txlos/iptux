pkg_check_modules (IPTUX REQUIRED glib-2.0>=2.32.0 gthread-2.0>=2.14.0 gtk+-3.0>=3.12 jsoncpp>=1.0)
pkg_search_module(GLIB REQUIRED glib-2.0)

include_directories(${GLIB_INCLUDE_DIRS})
message(${GLIB_INCLUDE_DIRS})
include_directories("${PROJECT_SOURCE_DIR}/src" "${PROJECT_BINARY_DIR}/src" "${PROJECT_BINARY_DIR}/src/iptux" "${IPTUX_INCLUDE_DIRS}")
link_libraries("${IPTUX_LIBRARIES}")
link_libraries("${Intl_LIBRARIES}")
link_libraries(${GLIB_LDFLAGS})

if(GST_FOUND)
	include_directories("${GST_INCLUDE_DIRS}")
	link_libraries("${GST_LIBRARIES}")
endif(GST_FOUND)

add_custom_command(OUTPUT IptuxResource.h
		MAIN_DEPENDENCY resources/iptux.gresource.xml
		COMMAND glib-compile-resources --generate-header --manual-register --target=IptuxResource.h
			--sourcedir=${CMAKE_CURRENT_SOURCE_DIR}/resources ${CMAKE_CURRENT_SOURCE_DIR}/resources/iptux.gresource.xml
		)

add_custom_command(OUTPUT IptuxResource.cpp
		MAIN_DEPENDENCY resources/iptux.gresource.xml
		COMMAND glib-compile-resources --generate-source --manual-register --target=IptuxResource.cpp
			--sourcedir=${CMAKE_CURRENT_SOURCE_DIR}/resources ${CMAKE_CURRENT_SOURCE_DIR}/resources/iptux.gresource.xml
		)

add_library(iptux
	STATIC
	AnalogFS.cpp
	Application.cpp
	callback.cpp
	Command.cpp
        UiCoreThread.cpp
	DataSettings.cpp
	DetectPal.cpp
	dialog.cpp
	DialogBase.cpp
	DialogGroup.cpp
	DialogPeer.cpp
    global.cpp
	HelpDialog.cpp
	IptuxConfig.cpp
	LogSystem.cpp
	MainWindow.cpp
		Models.cpp
	output.cpp
		UiProgramData.cpp
	RecvFile.cpp
	RecvFileData.cpp
	RevisePal.cpp
	SendFile.cpp
	SendFileData.cpp
	ShareFile.cpp
	SoundSystem.cpp
	StatusIcon.cpp
	support.cpp
	TcpData.cpp
	UdpData.cpp
	utils.cpp
	wrapper.cpp
	WindowConfig.cpp
    TransWindow.cpp TransWindow.h
	TransFileModel.cpp TransFileModel.h
	TransAbstract.cpp TransAbstract.h
	UiModels.h UiModels.cpp
	IptuxResource.h IptuxResource.cpp
    UiHelper.h UiHelper.cpp
    ProgramData.cpp ProgramData.h
    CoreThread.cpp CoreThread.h
    Platform.h Platform.cpp Platform-win32.h Platform-win32.cpp PlatformLinux.h PlatformLinux.cpp
    Event.cpp Event.h Exception.cpp Exception.h)



