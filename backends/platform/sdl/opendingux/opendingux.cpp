/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#define FORBIDDEN_SYMBOL_EXCEPTION_system

#include "common/scummsys.h"

#include "backends/platform/sdl/opendingux/opendingux.h"
#include "backends/events/opendingux/opendinguxsdl-events.h"

#include "common/config-manager.h"

#include "backends/fs/posix/posix-fs-factory.h"
#include "backends/fs/posix/posix-fs.h"
#include "backends/saves/default/default-saves.h"

#define SCUMM_DIR	"~/.scummvm"
#define CONFIG_FILE	"~/.scummvmrc"
#define SAVE_PATH	"~/.scummvm/saves"
#define LOG_FILE	"~/.scummvm/scummvm.log"

void OSystem_SDL_Opendingux::init() {

	_fsFactory = new POSIXFilesystemFactory();
	if (!Posix::assureDirectoryExists(SCUMM_DIR)) {
		system("mkdir " SCUMM_DIR);
	}

	// Invoke parent implementation of this method
	OSystem_SDL::init();
}

void OSystem_SDL_Opendingux::initBackend() {
	ConfMan.registerDefault("fullscreen", true);
	ConfMan.registerDefault("aspect_ratio", true);
	ConfMan.registerDefault("themepath", "./themes");
	ConfMan.registerDefault("extrapath", "./engine-data");
	ConfMan.registerDefault("gui_theme", "builtin");
	ConfMan.registerDefault("scale_factor", "1");

	ConfMan.setBool("fullscreen", true);
	ConfMan.setInt("joystick_num", 0);

	if (!ConfMan.hasKey("aspect_ratio")) {
		ConfMan.setBool("aspect_ratio", true);
	}
	if (!ConfMan.hasKey("themepath")) {
		ConfMan.set("themepath", "./themes");
	}
	if (!ConfMan.hasKey("extrapath")) {
		ConfMan.set("extrapath", "./engine-data");
	}
	if (!ConfMan.hasKey("savepath")) {
		ConfMan.set("savepath", SAVE_PATH);
	}
	if (!ConfMan.hasKey("gui_theme")) {
		ConfMan.set("gui_theme", "builtin");
	}
	if (!ConfMan.hasKey("scale_factor")) {
                ConfMan.set("scale_factor", "1");
        }

	// Create the savefile manager
	if (_savefileManager == 0) {
		_savefileManager = new DefaultSaveFileManager(SAVE_PATH);
	}

	// Create the events manager
	if (_eventSource == 0)
		_eventSource = new ODSdlEventSource();

	// Call parent implementation of this method
	OSystem_SDL::initBackend();
}

Common::String OSystem_SDL_Opendingux::getDefaultConfigFileName() {
	return CONFIG_FILE;

}

Common::String OSystem_SDL_Opendingux::getDefaultLogFileName() {
	return LOG_FILE;
}

bool OSystem_SDL_Opendingux::hasFeature(Feature f) {
	if (f == kFeatureFullscreenMode)
		return false;
	if (f == kFeatureAspectRatioCorrection)
		return false;

	return OSystem_SDL::hasFeature(f);
}

void OSystem_SDL_Opendingux::setFeatureState(Feature f, bool enable) {
	OSystem_SDL::setFeatureState(f, enable);
	}

bool OSystem_SDL_Opendingux::getFeatureState(Feature f) {
	return OSystem_SDL::getFeatureState(f);
}
