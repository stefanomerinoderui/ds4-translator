// SPDX-License-Identifier: GPL-2.0-only
//
// What a game built on SDL2 does with the emulated pad: opens it through
// SDL's HIDAPI driver (hidraw) or its evdev driver and asks for rumble. Prints
// what SDL made of the device, for the VM tests to compare with the fake pad's
// log of what arrived.
//
//   sdl-rumble hidapi|evdev VENDOR-ID-HEX [defaults]
//
// "defaults" leaves out the hints that force rumble on for pads SDL does not
// take for genuine ones, which is how a game (or Steam) runs it.

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	int hidapi, found = -1;

	if (argc != 3 && argc != 4)
		return 2;
	hidapi = !strcmp(argv[1], "hidapi");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, hidapi ? "1" : "0");
	if (argc == 3) {
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
	}
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_EVENTS)) {
		printf("sdl init failed: %s\n", SDL_GetError());
		return 1;
	}
	for (int attempt = 0; attempt < 30 && found < 0; attempt++) {
		SDL_PumpEvents();
		SDL_Delay(100);
		for (int i = 0; i < SDL_NumJoysticks(); i++) {
			// By vendor: SDL's HIDAPI driver names the pad its own way.
			if (SDL_JoystickGetDeviceVendor(i) == (Uint16)strtoul(argv[2], NULL, 16))
				found = i;
		}
	}
	if (found < 0) {
		printf("sdl found no joystick from vendor %s (of %d)\n", argv[2], SDL_NumJoysticks());
		return 1;
	}
	SDL_Joystick *js = SDL_JoystickOpen(found);

	if (!js) {
		printf("sdl could not open it: %s\n", SDL_GetError());
		return 1;
	}
	printf("sdl joystick: \"%s\" vendor=%04x product=%04x type=%d hidapi=%d\n",
	       SDL_JoystickName(js), SDL_JoystickGetVendor(js), SDL_JoystickGetProduct(js),
	       (int)SDL_JoystickGetType(js), hidapi);
	printf("sdl rumble: %s\n",
	       SDL_JoystickRumble(js, 0xFFFF, 0x4000, 2000) == 0 ? "accepted" : SDL_GetError());
	fflush(stdout);
	for (int i = 0; i < 15; i++) {
		SDL_PumpEvents();
		SDL_Delay(100);
	}
	SDL_JoystickClose(js);
	SDL_Quit();
	return 0;
}
