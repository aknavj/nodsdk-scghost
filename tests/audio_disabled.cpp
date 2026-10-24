#include "AudioPlayer.h"

#include <cstdio>

int main() {
	if(AudioPlayer::isAvailable()) {
		std::fprintf(stderr, "Disabled audio backend reports itself available\n");
		return 1;
	}

	AudioPlayer audio;
	const int16_t samples[] = {0, 0};
	audio.play(samples, 1, 2, 16000);
	if(audio.isPlaying() || audio.getProgress() != 0.0f ||
	   audio.getDuration() != 0.0f || audio.getPosition() != 0.0f) {
		std::fprintf(stderr, "Disabled audio backend changed playback state\n");
		return 1;
	}
	audio.stop();
	audio.stop();
	if(audio.isPlaying()) {
		std::fprintf(stderr, "Stopping disabled audio changed playback state\n");
		return 1;
	}

	std::puts("Disabled audio regression passed");
	return 0;
}
