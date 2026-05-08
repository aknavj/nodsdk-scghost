/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

StaRev is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#ifndef __AUDIO_PLAYER_H__
#define __AUDIO_PLAYER_H__

#include <cstdint>
#include <atomic>
#include <thread>

/*
 */
class AudioPlayer {

	public:
		AudioPlayer();
		~AudioPlayer();

		// play PCM16 audio; data copied internally. stops any current playback first.
		void play(const int16_t *samples, uint32_t frame_count, uint32_t num_channels, uint32_t sample_rate);

		// stop current playback immediately.
		void stop();

		bool isPlaying() const { return is_playing.load(); }
		float getProgress() const { return progress.load(); }

		// total duration in seconds of current clip.
		float getDuration() const;

		// current playback position in seconds.
		float getPosition() const;

	private:
		void play_thread();
		void cleanup();

		std::thread thread;
		std::atomic<bool> is_playing{false};
		std::atomic<bool> stop_requested{false};
		std::atomic<float> progress{0.0f};

		// audio buffer (owned copy)
		int16_t *data		 = nullptr;
		size_t data_size	 = 0; // bytes
		uint32_t frame_count = 0;
		uint32_t channels	 = 0;
		uint32_t sample_rate = 0;

		// child process for aplay
		pid_t child_pid = -1;
};

#endif // __AUDIO_PLAYER_H__
