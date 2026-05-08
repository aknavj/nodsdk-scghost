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

#include "AudioPlayer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <algorithm>

/*
 */
AudioPlayer::AudioPlayer() {
	// Ignore SIGPIPE so writes to closed pipes don't kill us
	signal(SIGPIPE, SIG_IGN);
}

/*
 */
AudioPlayer::~AudioPlayer() {
	stop();
}

/*
 */
float AudioPlayer::getDuration() const {
	if(sample_rate == 0) return 0.0f;
	return (float)frame_count / (float)sample_rate;
}

/*
 */
float AudioPlayer::getPosition() const {
	return progress.load() * getDuration();
}

/*
 */
void AudioPlayer::play(const int16_t *samples, uint32_t frame_count, uint32_t num_channels, uint32_t sample_rate) {
	stop();

	if(!samples || frame_count == 0 || num_channels == 0 || sample_rate == 0) return;

	this->frame_count = frame_count;
	this->channels	  = num_channels;
	this->sample_rate = sample_rate;
	data_size		  = (size_t)frame_count * num_channels * sizeof(int16_t);
	data			  = (int16_t *)malloc(data_size);
	if(!data) return;
	memcpy(data, samples, data_size);

	stop_requested = false;
	is_playing	   = true;
	progress	   = 0.0f;

	thread = std::thread(&AudioPlayer::play_thread, this);
}

/*
 */
void AudioPlayer::stop() {
	stop_requested = true;

	// Kill child process if running
	if(child_pid > 0) {
		kill(child_pid, SIGKILL);
		waitpid(child_pid, nullptr, 0);
		child_pid = -1;
	}

	if(thread.joinable()) thread.join();

	cleanup();
}

/*
 */
void AudioPlayer::cleanup() {
	free(data);
	data	   = nullptr;
	data_size  = 0;
	is_playing = false;
}

/*
 */
void AudioPlayer::play_thread() {
	int pipefd[2];
	if(pipe(pipefd) < 0) {
		printf("[AudioPlayer] pipe() failed\n");
		is_playing = false;
		return;
	}

	pid_t pid = fork();
	if(pid < 0) {
		printf("[AudioPlayer] fork() failed\n");
		close(pipefd[0]);
		close(pipefd[1]);
		is_playing = false;
		return;
	}

	if(pid == 0) {
		// Child process
		close(pipefd[1]); // close write end
		dup2(pipefd[0], STDIN_FILENO);
		close(pipefd[0]);

		char ch_str[8], rate_str[16];
		snprintf(ch_str, sizeof(ch_str), "%u", channels);
		snprintf(rate_str, sizeof(rate_str), "%u", sample_rate);

		execlp("aplay", "aplay", "-f", "S16_LE", "-c", ch_str, "-r", rate_str, "-q", // quiet — no status messages
			   "-", // read from stdin
			   (char *)nullptr);

		// If exec fails, try paplay as fallback
		_exit(1);
	}

	// Parent process
	close(pipefd[0]); // close read end
	child_pid = pid;

	// Write PCM data in small chunks to allow cancellation
	const size_t chunk_size = 4096;
	size_t written			= 0;

	while(written < data_size && !stop_requested.load()) {
		size_t to_write = std::min(chunk_size, data_size - written);
		ssize_t w		= write(pipefd[1], (const uint8_t *)data + written, to_write);
		if(w <= 0) break; // pipe broken (child killed) or error
		written += (size_t)w;
		progress = (float)written / (float)data_size;
	}

	close(pipefd[1]);

	// Wait for child to finish playing its buffer
	if(!stop_requested.load()) {
		int status;
		waitpid(pid, &status, 0);
	}

	child_pid  = -1;
	progress   = 1.0f;
	is_playing = false;
}
