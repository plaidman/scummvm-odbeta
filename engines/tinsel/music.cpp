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

// FIXME: This code is taken from MADE and may need more work (e.g. setVolume).

// MIDI and digital music class

#include "audio/audiostream.h"
#include "audio/mididrv.h"
#include "audio/midiparser.h"
// Miles Audio for Discworld 1
#include "audio/miles.h"
// Discworld Noir
#include "audio/decoders/mp3.h"

#include "backends/audiocd/audiocd.h"

#include "common/config-manager.h"
#include "common/file.h"
#include "common/memstream.h"

#include "tinsel/adpcm.h"
#include "tinsel/config.h"
#include "tinsel/sound.h"
#include "tinsel/music.h"
#include "tinsel/handle.h"
#include "tinsel/sysvar.h"

enum {
	MUSIC_JUMP = -1,
	MUSIC_END = -2,

	BLMAGIC  = -3458,

	DIM_SPEED = 8
};

namespace Tinsel {

static const int enhancedAudioGRAVersion[] = {
	 1,   2,   1,   1,   3,   3,   4,   4,   5,   6, //   1-10
	 1,   7,   8,   9,  10,   3,  11,  11,  12,  13, //  11-20
	13,  13,  13,  13,  14,  13,  13,  15,  16,  17, //  21-30
	15,  18,  19,  20, 338,  21,  21,  22,  22,  23, //  31-40
	24,  25,  26,  27,  28,  29,  30,  31,  32,  33, //  41-50
	34,  35,  35,  36,  37,  38,  39,  39,  39,  39, //  51-60
	40,  39,  41,  41,  42,  43,  42,  44,  45,  41, //  61-70
	46,  48,  47,  48,  49,  50,  51,  52,  53,  54, //  71-80
	55,  56,  57,  58,  59,  60,  61,  62,  63,  61, //  81-90
	64,  65,  66,  67,  68,  69,  70,  68,  71,  72, //  91-100
	73,  74,  75,  12,  76,  77,  78,  79,  80,   4, // 101-110
	81,  82,  83,  82,  81,  84,  85,  86,  87,  88, // 111-120
	89,  90,  88,   2,   2,   2,   2,   2,   2,  60, // 121-130
	91,  92,  93,  94,  94,  95,  96,  52,   4,  97, // 131-140
	98,  99,  99                                     // 141-143
};

static const int enhancedAudioSCNVersion[] = {
	 301, 302,   2,    1,   1, 301, 302,   3,   3,   4, //   1-10
	   4,   5,   6,    1,   7,   8,   9,  10,   8,  11, //  11-20
	  11,  12,  13,   13,  13,  13,  13,  14,  13,  13, //  21-30
	  15,  16,  17,   15,  18,  19,  20, 338,  21,  21, //  31-40
	 341, 342,  22,   22,  23,  24,  25,  26,  27,  28, //  41-50
	  29,  30,  31,   32,  33,  34,  35,  35,  36,  37, //  51-60
	  38,  39,  39,   39,  39,  40,  39,  41,  41,  42, //  61-70
	  43,  42,  44,   45,  41,  46,  48,  47,  48,  49, //  71-80
	  50,  51,  52,   53,  54,  55,  56,  57,  58,  59, //  81-90
	  60,  61,  62,   63,  61,  64,  65,  66,  67,  68, //  91-100
	  69,  70,  68,   71,  72,  73,  74,  75,  12,  76, // 101-110
	  77,  78,  79,   80,   4,   4,  82,  83,  77,   4, // 111-120
	  84,  85,  86, 3124,  88,  89,  90,  88,   2,   2, // 121-130
	   2,   2,   2,    2,   2,   2,   2,   2,   2,   2, // 131-140
	3142,  91,  92,   93,  94,  94,  95,  96,  52,   4, // 141-150
	  97,  98,  99,   99                                // 151-154
};

int Music::GetTrackNumber(SCNHANDLE hMidi) {
	for (int i = 0; i < ARRAYSIZE(_midiOffsets); i++) {
		if (_midiOffsets[i] == hMidi)
			return i;
	}

	return -1;
}

SCNHANDLE Music::GetTrackOffset(int trackNumber) {
	assert(trackNumber < ARRAYSIZE(_midiOffsets));
	return _midiOffsets[trackNumber];
}

/**
 * Plays the specified MIDI sequence through the sound driver.
 * @param dwFileOffset		File offset of MIDI sequence data
 * @param bLoop				Whether to loop the sequence
 */
bool Music::PlayMidiSequence(uint32 dwFileOffset, bool bLoop) {
	_currentMidi = dwFileOffset;
	_currentLoop = bLoop;

	bool mute = false;
	if (ConfMan.hasKey("mute"))
		mute = ConfMan.getBool("mute");

	SetMidiVolume(mute ? 0 : _vm->_config->_musicVolume);

	// the index and length of the last tune loaded
	uint32 dwSeqLen = 0;	// length of the sequence

	// Support for external music from the music enhancement project
	if (_vm->getFeatures() & GF_ENHANCED_AUDIO_SUPPORT) {
		int trackNumber = GetTrackNumber(dwFileOffset);
		// Track 8 has been removed in the German CD re-release "Neon Edition"
		if ((_vm->getFeatures() & GF_ALT_MIDI) && trackNumber >= 8)
			trackNumber++;

		int track = 0;
		if (trackNumber >= 0) {
			if (_vm->getFeatures() & GF_SCNFILES)
				track = enhancedAudioSCNVersion[trackNumber];
			else
				track = enhancedAudioGRAVersion[trackNumber];

			if (track > 0) {
				StopMidi();

				// StopMidi resets these fields, so set them again
				_currentMidi = dwFileOffset;
				_currentLoop = bLoop;

				// try to play track, but don't fall back to a true CD
				g_system->getAudioCDManager()->play(track, bLoop ? -1 : 1, 0, 0, true);

				// Check if an enhanced audio track is being played.
				// If it is, stop here and don't load a MIDI track
				if (g_system->getAudioCDManager()->isPlaying()) {
					return true;
				}
			}
		} else {
			warning("Unknown MIDI offset %d", dwFileOffset);
		}
	}

	if (dwFileOffset == 0)
		return true;

	Common::File midiStream;

	// open MIDI sequence file in binary mode
	if (!midiStream.open(MIDI_FILE))
		error(CANNOT_FIND_FILE, MIDI_FILE);

	// move to correct position in the file
	midiStream.seek(dwFileOffset, SEEK_SET);

	if (TinselV1Mac) {
		// The Macintosh version of DW1 uses raw PCM for music
		dwSeqLen = midiStream.readUint32BE();
		_vm->_sound->playDW1MacMusic(midiStream, dwSeqLen);
	} else if (TinselV1Saturn) {
		// TODO: Music format for the Saturn version
	} else {
		dwSeqLen = midiStream.readUint32LE();

		// make sure buffer is large enough for this sequence
		assert(dwSeqLen > 0 && dwSeqLen <= _midiBuffer.size);

		// stop any currently playing tune
		_vm->_midiMusic->stop();

		// read the sequence. This needs to be read again before playSEQ() is
		// called even if the music is restarting, as playSEQ() reads the file
		// name off the buffer itself. However, that function adds SMF headers
		// to the buffer, thus if it's read again, the SMF headers will be read
		// and the filename will always be 'MThd'.
		if (midiStream.read(_midiBuffer.pDat, dwSeqLen) != dwSeqLen)
			error(FILE_IS_CORRUPT, MIDI_FILE);

		// WORKAROUND for bug #4393 "DW1: No intro music at first start on Wii",
		// which actually affects all ports, since it's specific to the GRA version.
		//
		// The GRA version does not seem to set the channel volume at all for the first
		// intro track, thus we need to do that here. We only initialize the channels
		// used in that sequence. And we are using 127 as default channel volume.
		//
		// Only in the GRA version dwFileOffset can be "38888", just to be sure, we
		// check for the SCN files feature flag not being set though.
		if (_vm->getGameID() == GID_DW1 && dwFileOffset == 38888 && !(_vm->getFeatures() & GF_SCNFILES)) {
			_vm->_midiMusic->send(0x7F07B0 |  3);
			_vm->_midiMusic->send(0x7F07B0 |  5);
			_vm->_midiMusic->send(0x7F07B0 |  8);
			_vm->_midiMusic->send(0x7F07B0 | 10);
			_vm->_midiMusic->send(0x7F07B0 | 13);
		}

		_vm->_midiMusic->playMIDI(dwSeqLen, bLoop);
	}

	midiStream.close();

	return true;
}

/**
 * Returns TRUE if a Midi tune is currently playing.
 */
bool Music::MidiPlaying() {
	if (_vm->getFeatures() & GF_ENHANCED_AUDIO_SUPPORT) {
		if (g_system->getAudioCDManager()->isPlaying())
			return true;
	}
	return _vm->_midiMusic->isPlaying();
}

/**
 * Stops any currently playing midi.
 */
bool Music::StopMidi() {
	_currentMidi = 0;
	_currentLoop = false;

	if (_vm->getFeatures() & GF_ENHANCED_AUDIO_SUPPORT) {
		g_system->getAudioCDManager()->stop();
	}

	_vm->_midiMusic->stop();
	return true;
}


/**
 * Gets the volume of the MIDI music.
 */
int Music::GetMidiVolume() {
	return _vm->_config->_musicVolume;
}

/**
 * Sets the volume of the MIDI music.
 * @param vol			New volume - 0..MAXMIDIVOL
 */
void Music::SetMidiVolume(int vol) {
	assert(vol >= 0 && vol <= Audio::Mixer::kMaxChannelVolume);
	_vm->_midiMusic->setVolume(vol);
}

/**
 * Opens and inits all MIDI sequence files.
 */
void Music::OpenMidiFiles() {
	Common::File midiStream;

	if (TinselV0) {
		// The early demo version of DW1 doesn't have MIDI
	} else if (TinselV2) {
		// DW2 uses a different music mechanism
	} else if (TinselV1Mac) {
		// open MIDI sequence file in binary mode
		if (!midiStream.open(MIDI_FILE))
			error(CANNOT_FIND_FILE, MIDI_FILE);

		uint32 curTrack = 1;
		uint32 songLength = 0;
		int32 fileSize = midiStream.size();

		// Init
		for (int i = 0; i < ARRAYSIZE(_midiOffsets); i++)
			_midiOffsets[i] = 0;

		midiStream.skip(4);	// skip file header

		while (!midiStream.eos() && !midiStream.err() && midiStream.pos() != fileSize) {
			assert(curTrack < ARRAYSIZE(_midiOffsets));
			_midiOffsets[curTrack] = midiStream.pos();
			//debug("%d: %d", curTrack, g_midiOffsets[curTrack]);

			songLength = midiStream.readUint32BE();
			midiStream.skip(songLength);

			curTrack++;
		}

		midiStream.close();
	} else if (TinselV1Saturn) {
		// The Saturn version has digital music with a different track structure
		// TODO
		warning("Music support for Discworld 1 Saturn");
	} else {
		if (_midiBuffer.pDat)
			// already allocated
			return;

		// open MIDI sequence file in binary mode
		if (!midiStream.open(MIDI_FILE))
			error(CANNOT_FIND_FILE, MIDI_FILE);

		// get length of the largest sequence
		_midiBuffer.size = midiStream.readUint32LE();
		if (midiStream.eos() || midiStream.err())
			error(FILE_IS_CORRUPT, MIDI_FILE);

		if (_midiBuffer.size) {
			// allocate a buffer big enough for the largest MIDI sequence
			if ((_midiBuffer.pDat = (uint8 *)malloc(_midiBuffer.size)) != NULL) {
				// clear out the buffer
				memset(_midiBuffer.pDat, 0, _midiBuffer.size);
			}
		}

		// Now scan through the contents of the MIDI file to find the offset
		// of each individual track, in order to create a mapping from MIDI
		// offset to track number, for the enhanced MIDI soundtrack.
		// The first song is always at position 4. The subsequent ones are
		// calculated dynamically.
		uint32 curOffset = 4;
		uint32 curTrack = 0;
		uint32 songLength = 0;

		// Init
		for (int i = 0; i < ARRAYSIZE(_midiOffsets); i++)
			_midiOffsets[i] = 0;

		while (!midiStream.eos() && !midiStream.err()) {
			if (curOffset + (4 * curTrack) >= (uint32)midiStream.size())
				break;

			assert(curTrack < ARRAYSIZE(_midiOffsets));
			_midiOffsets[curTrack] = curOffset + (4 * curTrack);
			//debug("%d: %d", curTrack, midiOffsets[curTrack]);

			songLength = midiStream.readUint32LE();
			curOffset += songLength;
			midiStream.skip(songLength);

			curTrack++;
		}

		midiStream.close();
	}
}

void Music::DeleteMidiBuffer() {
	free(_midiBuffer.pDat);
	_midiBuffer.pDat = nullptr;
}

void Music::CurrentMidiFacts(SCNHANDLE* pMidi, bool* pLoop) {
	*pMidi = _currentMidi;
	*pLoop = _currentLoop;
}

void Music::RestoreMidiFacts(SCNHANDLE	Midi, bool Loop) {
	StopMidi();

	_currentMidi = Midi;
	_currentLoop = Loop;

	bool mute = false;
	if (ConfMan.hasKey("mute"))
		mute = ConfMan.getBool("mute");

	PlayMidiSequence(_currentMidi, true);
	SetMidiVolume(mute ? 0 : _vm->_config->_musicVolume);
}

#if 0
// Dumps all of the game's music in external XMIDI *.xmi files
void dumpMusic() {
	Common::File midiFile;
	Common::DumpFile outFile;
	char outName[20];
	midiFile.open(MIDI_FILE);
	int outFileSize = 0;
	char buffer[20000];

	const int total = 155;	// maximum (SCN version)

	for (int i = 0; i < total; i++) {
		if (midiOffsets[i] == 0)
			break;

		sprintf(outName, "track%03d.xmi", i + 1);
		outFile.open(outName);

		if (i < total - 1)
			outFileSize = midiOffsets[i + 1] - midiOffsets[i] - 4;
		else
			outFileSize = midiFile.size() - midiOffsets[i] - 4;

		midiFile.seek(midiOffsets[i] + 4, SEEK_SET);

		assert(outFileSize < 20000);
		midiFile.read(buffer, outFileSize);
		outFile.write(buffer, outFileSize);

		outFile.close();
	}

	midiFile.close();
}
#endif

MidiMusicPlayer::MidiMusicPlayer(TinselEngine *vm) {
	_driver = nullptr;
	_milesAudioMode = false;
	bool milesAudioEnabled = false;

	if (vm->getPlatform() == Common::kPlatformDOS) {
		// Enable Miles Audio for DOS platform only...
		switch (vm->getGameID()) {
		case GID_DW1:
			if (!vm->getIsADGFDemo()) {
				// ...for Discworld 1
				milesAudioEnabled = true;
			} else {
				if (vm->isV1CD()) {
					// ...and for Discworld 1 CD Demo
					milesAudioEnabled = true;
				}
			}
			break;
		default:
			break;
		}
	}

	if (milesAudioEnabled) {
		// Discworld 1 (DOS) uses Miles Audio 3
		// use our own Miles Audio drivers
		//
		// It seems that there are multiple versions of Discworld 1
		//
		// Version 1:
		// Has SAMPLE.AD for AdLib and SAMPLE.OPL for OPL-3
		// Timbre files are inside a subdirectory of the CD called "/drivers". Main game files are in
		// another subdirectory, which means the user has to copy those files over.
		// Installer script copies all drivers directly to harddrive without name changes
		//
		// Version 2:
		// Has FAT.OPL only (gets copied by the installer into MIDPAK.AD or MIDPAK.OPL)
		// Timbre file is inside subdirectory "drivers" right in the main game directory.
		// Installer copies FAT.OPL to MIDPAK.AD all the time, even when user selected AWE32
		//
		// Neither have timbre data for MT32

		::MidiDriver::DeviceHandle dev = ::MidiDriver::detectDevice(MDT_MIDI | MDT_ADLIB | MDT_PREFER_GM);
		::MusicType musicType = ::MidiDriver::getMusicType(dev);
		Common::File fileClass;

		switch (musicType) {
		case MT_ADLIB:
			if (fileClass.exists("FAT.OPL")) {
				// Version 2: fat.opl, may be in drivers-subdirectory
				_driver = Audio::MidiDriver_Miles_AdLib_create("", "FAT.OPL");
			} else {
				if (fileClass.exists("MIDPAK.AD")) {
					// Version 2: drivers got installed and fat.opl got copied over by the user
					_driver = Audio::MidiDriver_Miles_AdLib_create("MIDPAK.AD", "");
				} else {
					if ((fileClass.exists("SAMPLE.AD")) || (fileClass.exists("SAMPLE.OPL"))) {
						// Version 1: sample.ad / sample.opl, have to be copied over by the user for this version
						_driver = Audio::MidiDriver_Miles_AdLib_create("SAMPLE.AD", "SAMPLE.OPL");
					} else {
						error("MILES-ADLIB: timbre file not found (may be called FAT.OPL, MIDPAK.AD, SAMPLE.AD or SAMPLE.OPL, may be in a subdirectory)");
					}
				}
			}
			break;
		case MT_MT32:
			// Discworld 1 doesn't have a MT32 timbre file
			_driver = Audio::MidiDriver_Miles_MT32_create("");
			break;
		case MT_GM:
			if (ConfMan.getBool("native_mt32")) {
				_driver = Audio::MidiDriver_Miles_MT32_create("");
				musicType = MT_MT32;
			}
			break;
		default:
			break;
		}
		if (!_driver) {
			// nothing got created yet? -> create default driver
			MidiPlayer::createDriver();
		} else {
			_milesAudioMode = true;
		}

	} else {
		MidiPlayer::createDriver();
	}

	int ret = _driver->open();
	if (ret == 0) {
		if (_nativeMT32)
			_driver->sendMT32Reset();
		else
			_driver->sendGMReset();

		_driver->setTimerCallback(this, &timerCallback);
	}
}

void MidiMusicPlayer::setVolume(int volume) {
	_vm->_mixer->setVolumeForSoundType(Audio::Mixer::kMusicSoundType, volume);

	Audio::MidiPlayer::setVolume(volume);
}

void MidiMusicPlayer::send(uint32 b) {
	if (_milesAudioMode) {
		_driver->send(b);
		return;
	}

	Audio::MidiPlayer::send(b);

	byte channel = (byte)(b & 0x0F);
	if (_channelsTable[channel]) {
		if ((b & 0xFFF0) == 0x0079B0) {
			// We've just Reset All Controllers, so we need to
			// re-adjust the volume. Otherwise, volume is reset to
			// default whenever the music changes.
			_channelsTable[channel]->send(0x000007B0 | (((_channelsVolume[channel] * _masterVolume) / 255) << 16) | channel);
		}
	}
}

void MidiMusicPlayer::playMIDI(uint32 size, bool loop) {
	Common::StackLock lock(_mutex);

	if (_isPlaying)
		return;

	stop();

	if (TinselV1PSX)
		playSEQ(size, loop);
	else
		playXMIDI(size, loop);
}

void MidiMusicPlayer::playXMIDI(uint32 size, bool loop) {
	// It seems like not all music (the main menu music, for instance) set
	// all the instruments explicitly. That means the music will sound
	// different, depending on which music played before it. This appears
	// to be a genuine glitch in the original. For consistency, reset all
	// instruments to the default one (piano).

	for (int i = 0; i < 16; i++) {
		_driver->send(0xC0 | i, 0, 0);
	}

	// Load XMID resource data

	MidiParser *parser = MidiParser::createParser_XMIDI();
	if (parser->loadMusic(_vm->_music->GetMidiBuffer(), size)) {
		parser->setTrack(0);
		parser->setMidiDriver(this);
		parser->setTimerRate(getBaseTempo());
		parser->property(MidiParser::mpCenterPitchWheelOnUnload, 1);
		parser->property(MidiParser::mpSendSustainOffOnNotesOff, 1);

		_parser = parser;

		_isLooping = loop;
		_isPlaying = true;
	} else {
		delete parser;
	}
}

void MidiMusicPlayer::playSEQ(uint32 size, bool loop) {
	uint8 *midiData = _vm->_music->GetMidiBuffer();
	// MIDI.DAT holds the file names in DW1 PSX
	Common::String baseName((char *)midiData, size);
	Common::String seqName = baseName + ".SEQ";

	// TODO: Load the instrument bank (<baseName>.VB and <baseName>.VH)

	Common::File seqFile;
	if (!seqFile.open(seqName))
		error("Failed to open SEQ file '%s'", seqName.c_str());

	if (seqFile.readUint32LE() != MKTAG('S', 'E', 'Q', 'p'))
		error("Failed to find SEQp tag");

	// Make sure we don't have a SEP file (with multiple SEQ's inside)
	if (seqFile.readUint32BE() != 1)
		error("Can only play SEQ files, not SEP");

	uint16 ppqn = seqFile.readUint16BE();
	uint32 tempo = seqFile.readUint16BE() << 8;
	tempo |= seqFile.readByte();
	/* uint16 beat = */ seqFile.readUint16BE();

	// SEQ is directly based on SMF and we'll use that to our advantage here
	// and convert to SMF and then use the SMF MidiParser.

	// Calculate the SMF size we'll need
	uint32 dataSize = seqFile.size() - 15;
	uint32 actualSize = dataSize + 7 + 22;

	// Resize the buffer if necessary
	midiData = _vm->_music->ResizeMidiBuffer(actualSize);

	// Now construct the header
	WRITE_BE_UINT32(midiData, MKTAG('M', 'T', 'h', 'd'));
	WRITE_BE_UINT32(midiData + 4, 6); // header size
	WRITE_BE_UINT16(midiData + 8, 0); // type 0
	WRITE_BE_UINT16(midiData + 10, 1); // one track
	WRITE_BE_UINT16(midiData + 12, ppqn);
	WRITE_BE_UINT32(midiData + 14, MKTAG('M', 'T', 'r', 'k'));
	WRITE_BE_UINT32(midiData + 18, dataSize + 7); // SEQ data size + tempo change event size

	// Add in a fake tempo change event
	WRITE_BE_UINT32(midiData + 22, 0x00FF5103); // no delta, meta event, tempo change, param size = 3
	WRITE_BE_UINT16(midiData + 26, tempo >> 8);
	midiData[28] = tempo & 0xFF;

	// Now copy in the rest of the events
	seqFile.read(midiData + 29, dataSize);
	seqFile.close();

	MidiParser *parser = MidiParser::createParser_SMF();
	if (parser->loadMusic(midiData, actualSize)) {
		parser->setTrack(0);
		parser->setMidiDriver(this);
		parser->setTimerRate(getBaseTempo());
		parser->property(MidiParser::mpCenterPitchWheelOnUnload, 1);
		parser->property(MidiParser::mpSendSustainOffOnNotesOff, 1);

		_parser = parser;

		_isLooping = loop;
		_isPlaying = true;
	} else {
		delete parser;
	}
}

void MidiMusicPlayer::pause() {
	setVolume(-1);
	_isPlaying = false;
}

void MidiMusicPlayer::resume() {
	setVolume(_vm->_music->GetMidiVolume());
	_isPlaying = true;
}

PCMMusicPlayer::PCMMusicPlayer() {
	_silenceSamples = 0;

	_curChunk = 0;
	_state = S_IDLE;
	_mState = S_IDLE;
	_scriptNum = -1;
	_scriptIndex = 0;
	_forcePlay = false;

	_volume = 255;
	_dimmed = false;
	_dimmedTinsel = false;
	_dimIteration = 0;
	_dimmedVolume = 0;
	_dimPosition = 0;

	_fadeOutVolume = 0;
	_fadeOutIteration = 0;

	_hScript = _hSegment = 0;

	_end = true;

	_vm->_mixer->playStream(Audio::Mixer::kMusicSoundType,
			&_handle, this, -1, _volume, 0, DisposeAfterUse::NO, true);
}

PCMMusicPlayer::~PCMMusicPlayer() {
	_vm->_mixer->stopHandle(_handle);
	delete _curChunk;
}

void PCMMusicPlayer::startPlay(int id) {
	if (_filename.empty())
		return;

	debugC(DEBUG_DETAILED, kTinselDebugMusic, "Playing PCM music %s, index %d", _filename.c_str(), id);

	Common::StackLock slock(_mutex);

	stop();

	_scriptNum = id;
	_scriptIndex = 1;
	_state = S_NEW;

	play();
}

void PCMMusicPlayer::stopPlay() {
	Common::StackLock slock(_mutex);

	stop();
}

int PCMMusicPlayer::readBuffer(int16 *buffer, const int numSamples) {
	Common::StackLock slock(_mutex);

	if (!_curChunk && ((_state == S_IDLE) || (_state == S_STOP)))
		return 0;

	int samplesLeft = numSamples;

	while (samplesLeft > 0) {
		if (_silenceSamples > 0) {
			int n = MIN(_silenceSamples, samplesLeft);

			memset(buffer, 0, n);

			buffer += n;
			_silenceSamples -= n;
			samplesLeft -= n;

		} else if (_curChunk &&
		          ((_state == S_MID) || (_state == S_NEXT) || (_state == S_NEW))) {
			int n = _curChunk->readBuffer(buffer, samplesLeft);

			buffer += n;
			samplesLeft -= n;

			if (_curChunk->endOfData()) {
				_state = S_END1;

				delete _curChunk;
				_curChunk = 0;
			}
		} else {

			if (!getNextChunk())
				break;
		}
	}

	return (numSamples - samplesLeft);
}

bool PCMMusicPlayer::isStereo() const {
	if (TinselV3) {
		return true;
	} else {
		return false;
	}
}

int PCMMusicPlayer::getRate() const {
	if (TinselV3) {
		if (_curChunk) {
			return _curChunk->getRate();
		} else {
			return 0;
		}
	} else {
		return 22050;
	}
}

bool PCMMusicPlayer::isPlaying() const {
	return ((_state != S_IDLE) && (_state != S_STOP));
}

bool PCMMusicPlayer::isDimmed() const {
	return _dimmed;
}

void PCMMusicPlayer::getTunePlaying(void *voidPtr, int length) {
	Common::StackLock lock(_mutex);

	debugC(DEBUG_DETAILED, kTinselDebugMusic, "getTunePlaying");

	assert(length == (3 * sizeof(int32)));

	int32 *p = (int32 *) voidPtr;

	_mState = _state;

	p[0] = (int32) _mState;
	p[1] = _scriptNum;
	p[2] = _scriptIndex;
}

void PCMMusicPlayer::restoreThatTune(void *voidPtr) {
	Common::StackLock lock(_mutex);

	debugC(DEBUG_DETAILED, kTinselDebugMusic, "restoreThatTune");

	int32 *p = (int32 *) voidPtr;

	_mState = (State) p[0];
	_scriptNum = p[1];
	_scriptIndex = p[2];

	if (_mState != S_IDLE)
		_state = S_NEW;

	delete _curChunk;
	_curChunk = 0;

	_end = false;
}

void PCMMusicPlayer::setMusicSceneDetails(SCNHANDLE hScript,
		SCNHANDLE hSegment, const char *fileName) {

	Common::StackLock lock(_mutex);

	stop();

	debugC(DEBUG_INTERMEDIATE, kTinselDebugMusic, "Setting music scene details: %s", fileName);

	_hScript = hScript;
	_hSegment = hSegment;
	_filename = fileName;

	// Start scene with music not dimmed
	_dimmed = false;
	_dimmedTinsel = false;
	_dimIteration = 0;
	setVol(255);
}

void PCMMusicPlayer::setVolume(int volume) {
	assert((volume >= 0) && (volume <= 100));

	_dimmed = false;
	setVol((volume * 255) / 100);
}

void PCMMusicPlayer::setVol(uint8 volume) {
	_volume = volume;

	_vm->_mixer->setChannelVolume(_handle, _volume);
}

bool PCMMusicPlayer::getMusicTinselDimmed() const {
	return _dimmedTinsel;
}

void PCMMusicPlayer::dim(bool bTinselDim) {
	if (_dimmed || (_volume == 0) ||
			(_state == S_IDLE) || !_curChunk || (SysVar(SV_MUSICDIMFACTOR) == 0))
		return;

	_dimmed = true;
	if (bTinselDim)
		_dimmedTinsel = true;

	_dimmedVolume = _volume - (_volume / SysVar(SV_MUSICDIMFACTOR));

	// Iterate down, negative iteration
	if (!_dimIteration)
		_dimPosition = _volume;
	_dimIteration = (_dimmedVolume - _volume)/DIM_SPEED;

	debugC(DEBUG_DETAILED, kTinselDebugMusic, "Dimming music from %d to %d, steps %d", _dimPosition, _dimmedVolume, _dimIteration);

	// And SFX
	if (SysVar(SYS_SceneFxDimFactor))
		_vm->_sound->setSFXVolumes(255 - 255/SysVar(SYS_SceneFxDimFactor));
}

void PCMMusicPlayer::unDim(bool bTinselUnDim) {
	if (!_dimmed || (_dimmedTinsel && !bTinselUnDim))
		return; // not dimmed

	_dimmed = _dimmedTinsel = false;

	if ((_volume == 0) || (_state == S_IDLE) || !_curChunk)
		return;

	// Iterate up, positive iteration
	if (!_dimIteration)
		_dimPosition = _dimmedVolume;
	_dimIteration = (_volume - _dimmedVolume)/DIM_SPEED;

	debugC(DEBUG_DETAILED, kTinselDebugMusic, "UnDimming music from %d to %d, steps %d", _dimPosition, _volume, _dimIteration);

	// And SFX
	_vm->_sound->setSFXVolumes(255);
}

void PCMMusicPlayer::dimIteration() {
	if (_dimIteration != 0)
	{
		_dimPosition += _dimIteration;
		if (_dimPosition >= _volume)
		{
			_dimPosition = _volume;
			_dimIteration = 0;
		}
		else if (_dimPosition <= _dimmedVolume)
		{
			_dimPosition = _dimmedVolume;
			_dimIteration = 0;
		}

		_vm->_mixer->setChannelVolume(_handle, _dimPosition);
	}
}

void PCMMusicPlayer::startFadeOut(int ticks) {
	if ((_volume == 0) || (_state == S_IDLE) || !_curChunk)
		return;

	debugC(DEBUG_INTERMEDIATE, kTinselDebugMusic, "Fading out music...");

	if (_dimmed) {
		// Start from dimmed volume and go from there
		_dimmed = false;
		_fadeOutVolume = _volume - _volume/SysVar(SV_MUSICDIMFACTOR);
	} else
		_fadeOutVolume = _volume;

	assert(ticks != 0);
	_fadeOutIteration = _fadeOutVolume / ticks;

	fadeOutIteration();
}

void PCMMusicPlayer::fadeOutIteration() {
	if ((_volume == 0) || (_state == S_IDLE) || !_curChunk)
		return;

	_fadeOutVolume = CLIP<int>(_fadeOutVolume -= _fadeOutIteration, 0, 255);

	_vm->_mixer->setChannelVolume(_handle, _fadeOutVolume);
}

Common::MemoryReadStream *readSampleData(const Common::String &filename, uint32 sampleOffset, uint32 sampleLength) {
	Common::File file;
	if (!file.open(filename))
		error(CANNOT_FIND_FILE, filename.c_str());

	file.seek(sampleOffset);
	if (file.eos() || file.err() || (uint32)file.pos() != sampleOffset)
		error(FILE_IS_CORRUPT, filename.c_str());

	byte *buffer = (byte *) malloc(sampleLength);
	assert(buffer);

	// read all of the sample
	if (file.read(buffer, sampleLength) != sampleLength)
		error(FILE_IS_CORRUPT, filename.c_str());

	return new Common::MemoryReadStream(buffer, sampleLength, DisposeAfterUse::YES);
}

struct MusicSegment {
	uint32 numChannels;
	uint32 bitsPerSec;
	uint32 bitsPerSample;
	uint32 sampleLength;
	uint32 sampleOffset;
};

void PCMMusicPlayer::loadADPCMMusicFromSegment(int segmentNum) {
	MusicSegment *musicSegments = (MusicSegment *)_vm->_handle->LockMem(_hSegment);

	assert(FROM_32(musicSegments[segmentNum].numChannels) == 1);
	assert(FROM_32(musicSegments[segmentNum].bitsPerSample) == 16);

	uint32 sampleOffset = FROM_32(musicSegments[segmentNum].sampleOffset);
	uint32 sampleLength = FROM_32(musicSegments[segmentNum].sampleLength);
	uint32 sampleCLength = (((sampleLength + 63) & ~63)*33)/64;

	debugC(DEBUG_DETAILED, kTinselDebugMusic, "Creating ADPCM music chunk with size %d, "
			"offset %d (script %d.%d)",
			sampleCLength, sampleOffset, _scriptNum, _scriptIndex - 1);

	Common::SeekableReadStream *sampleStream = readSampleData(_filename, sampleOffset, sampleCLength);

	delete _curChunk;
	_curChunk = new Tinsel8_ADPCMStream(sampleStream, DisposeAfterUse::YES, sampleCLength, 22050, 1, 32);
}

struct MusicSegmentNoir {
	uint32 sampleLength;
	uint32 sampleOffset;
};

void PCMMusicPlayer::loadMP3MusicFromSegment(int segmentNum) {
#ifdef USE_MAD
	MusicSegmentNoir *musicSegments = (MusicSegmentNoir *)_vm->_handle->LockMem(_hSegment);

	Common::SeekableReadStream *sampleStream = readSampleData(_filename, musicSegments[segmentNum].sampleOffset,
			musicSegments[segmentNum].sampleLength);

	delete _curChunk;
	_curChunk = Audio::makeMP3Stream(sampleStream, DisposeAfterUse::YES);
#else
	delete _curChunk;
	_curChunk = 0;
#endif
}

void PCMMusicPlayer::loadMusicFromSegment(int segmentNum) {
	if (TinselV3) {
		loadMP3MusicFromSegment(segmentNum);
	} else {
		loadADPCMMusicFromSegment(segmentNum);
	}
}

bool PCMMusicPlayer::getNextChunk() {
	int32 *script, *scriptBuffer;
	int id;
	int snum;

	switch (_state) {
	case S_NEW:
	case S_NEXT:
		_forcePlay = false;

		script = scriptBuffer = (int32 *)_vm->_handle->LockMem(_hScript);

		// Set parameters for this chunk of music
		id = _scriptNum;
		while (id--)
			script = scriptBuffer + READ_32(script);
		snum = FROM_32(script[_scriptIndex++]);

		if (snum == MUSIC_JUMP || snum == MUSIC_END) {
			// Let usual code sort it out!
			_scriptIndex--;    // Undo increment
			_forcePlay = true; // Force a Play
			_state = S_END1;   // 'Goto' S_END1
			break;
		}

		loadMusicFromSegment(snum);

		_state = S_MID;
		return true;

	case S_END1:
		debugC(DEBUG_DETAILED, kTinselDebugMusic, "Music reached state S_END1 (script %d.%d)",
				_scriptNum, _scriptIndex);

		script = scriptBuffer = (int32 *)_vm->_handle->LockMem(_hScript);

		id = _scriptNum;
		while (id--)
			script = scriptBuffer + READ_32(script);
		snum = FROM_32(script[_scriptIndex]);

		if (snum == MUSIC_END) {
			_state = S_END2;
		} else {
			if (snum == MUSIC_JUMP)
				_scriptIndex = FROM_32(script[_scriptIndex+1]);

			_state = _forcePlay ? S_NEW : S_NEXT;
			_forcePlay = false;
		}

		return true;

	case S_END2:
		debugC(DEBUG_DETAILED, kTinselDebugMusic, "Music reached state S_END2 (script %d.%d)",
				_scriptNum, _scriptIndex);

		_silenceSamples = 11025; // Half a second of silence
		return true;

	case S_END3:
		debugC(DEBUG_DETAILED, kTinselDebugMusic, "Music reached state S_END3 (script %d.%d)",
				_scriptNum, _scriptIndex);

		stop();
		_state = S_IDLE;
		return false;

	case S_IDLE:
		return false;

	default:
		break;
	}

	return true;
}

void PCMMusicPlayer::play() {
	if (_curChunk)
		return;
	if (_scriptNum == -1)
		return;

	_end = false;

	getNextChunk();
}

void PCMMusicPlayer::stop() {
	delete _curChunk;
	_curChunk = 0;
	_scriptNum = -1;
	_state = S_IDLE;
	_mState = S_IDLE;

	_end = true;
}

} // End of namespace Tinsel
