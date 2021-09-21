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

#include "backends/platform/opendingux/opendingux.h"
#include "backends/events/opendingux/opendinguxsdl-events.h"

void OSystem_SDL_Opendingux::initBackend() {
	ConfMan.registerDefault("fullscreen", true);
	ConfMan.registerDefault("aspect_ratio", true);
	ConfMan.registerDefault("themepath", "./themes");
	ConfMan.registerDefault("extrapath", "./engine-data");
	ConfMan.registerDefault("gui_theme", "builtin");

	// Create the events manager
	if (_eventSource == 0)
		_eventSource = new ODSdlEventSource();

	// Call parent implementation of this method
	OSystem_POSIX::initBackend();
}

bool OSystem_SDL_Opendingux::hasFeature(Feature f) {
	if (f == kFeatureFullscreenMode)
		return false;
	if (f == kFeatureAspectRatioCorrection)
		return false;

	return OSystem_SDL::hasFeature(f);
}
