#include <stdio.h>
#include <dlfcn.h>
#include <time.h>
#include <stdlib.h>
#include <SDL.h>

 /* Since the joystick doesn't generate new events, if an analog trigger is hold in one direction,
  * and one does want the mouse to move continually, we have to generate events in fixed intervals. */
#define POLL_PER_SECOND 25

/* Should the calculted relative movement be quadrated */
const int quadratic = 1;

/* a smaller number increases the sinsitivity.
 * Valid Numbers range from 1 to 32000.
 * Normal is about 3200 which results to maximal 10 px linear or 100 px quadratic movement per POLL */
const int sensitivity_divisor = 3200;

/* funtion pointers to the Original functions */
int (* real_SDL_pollEvent) (SDL_Event *);
int (* real_SDL_Init) (Uint32);
// Uint8* (* real_SDL_GetKeyState) (int *);
SDLMod (* real_SDL_GetModState) (void);

/* ms runtime, when out hooked version last provided a valid emulated Mousemovement. */
static int last_update = 0;
static int __relx = 0;
static int __rely = 0;

SDL_Joystick * j = NULL;
int axes = 0;
int buttons = 0;

/* 1 iff ctrl_key should be emulated as pressed */
int ctrl_modifier = 0;

/* handle to libSDL.so */
void * handle;

__attribute__ ((constructor)) static void __wrapperInit(){
	handle = dlopen("libSDL.so", RTLD_LAZY);
	real_SDL_pollEvent = (int(*)(SDL_Event*)) dlsym(handle, "SDL_PollEvent");
	real_SDL_Init = (int(*)(Uint32)) dlsym(handle, "SDL_Init");
//	real_SDL_GetKeyState = (Uint8 * (*)(int*)) dlsym(handle, "SDL_GetKeyState");
	real_SDL_GetModState = (SDLMod (*)(void)) dlsym(handle, "SDL_GetModState");
	printf("Hook initialized\n");
}

__attribute__ ((destructor)) static void __wrapperDeinit(){
	dlclose(handle);
}

int SDL_Init(Uint32 flags){
	flags |= SDL_INIT_JOYSTICK;
	int result = real_SDL_Init(flags);
	int joy_cnt = SDL_NumJoysticks();
	printf("%d Joystick(s) found\n", joy_cnt);
	if (joy_cnt){
		int i = 0;
		SDL_JoystickEventState(SDL_ENABLE);
		j = SDL_JoystickOpen(0);
		axes = SDL_JoystickNumAxes(j);
		buttons = SDL_JoystickNumButtons(j);
		printf("%s - %d Axes, %d Buttons: ", SDL_JoystickName(i), axes, buttons);
		if ((axes >= 2) && (buttons >= 4)){
			printf("Should work\n");
		} else {
			printf("Failure: Not enought Axes(2) or Buttons(4)\n");
			SDL_JoystickEventState(SDL_IGNORE);
			SDL_JoystickClose(j);
			j = NULL;
		}
	}
	return result;
}

/* function which emulates the mouse movement, by overwriting the passed event, with mouse data */
static void __fake_mouse(SDL_Event * event, int x, int y){
	event->type = SDL_MOUSEMOTION;
	int mx, my;
	SDL_GetMouseState(&mx, &my);
	int tgtx;
	int tgty;
	if (quadratic){
		tgtx = mx+x*abs(x);
		tgty = my+y*abs(y);
	} else {
		tgtx = mx+x;
		tgty = my+y;
	}
	event->motion.x = tgtx;
	event->motion.y = tgty;
	SDL_WarpMouse(tgtx, tgty);
	last_update = SDL_GetTicks();
}

//This is not needed in the current version, but might be useful for other games / etc.
//Uint8* SDL_GetKeyState(int * nkeys){
//	Uint8* keys = real_SDL_GetKeyState(nkeys);
//	return keys;
//}

SDLMod SDL_GetModState(void){
	SDLMod current_state = real_SDL_GetModState();
	if (ctrl_modifier){
		current_state |= KMOD_LCTRL;
	}
	return current_state;
}

int SDL_PollEvent(SDL_Event * event){
	int diff = SDL_GetTicks() - last_update;
	int result = real_SDL_pollEvent(event);
	if (!result) {
		if ((__relx == 0) && (__rely == 0)) return 0;
		if (diff < (1000 / POLL_PER_SECOND)) { // already updated
			return 0;
		} else {
			__fake_mouse(event, __relx, __rely);
		}
		return 0;
	}
	switch (event->type){
		default:
			printf("unhandled event\n");
			break;
		case SDL_MOUSEMOTION: break;
		case SDL_MOUSEBUTTONDOWN: break;
		case SDL_MOUSEBUTTONUP:	break;
		case SDL_QUIT:
			if (j){
				/* If we receive a QUIT event, it is the perfect time to close down the joystick handling.
				 * the rest gets cleaned up by the real program */
				SDL_JoystickClose(j);
				SDL_JoystickEventState(SDL_IGNORE);
			}
			break;
		case SDL_KEYDOWN: break;
		case SDL_KEYUP: break;	  
		case SDL_VIDEORESIZE: break;
		case SDL_ACTIVEEVENT: break;
		case SDL_JOYAXISMOTION:{
			/* 0 left analogstick horizontal
			   1 left analogstick vertical */
			__relx = SDL_JoystickGetAxis(j, 0) / sensitivity_divisor;
			__rely = SDL_JoystickGetAxis(j, 1) / sensitivity_divisor;
			/* recursive call THIS function to get remaining events.
			 * If the real one is called here, joystick event which occured at the same time might get lost. */
			SDL_PollEvent(event);
			break;
		}
		case SDL_JOYBUTTONDOWN: // no break
		case SDL_JOYBUTTONUP:{

			int button_id = event->jbutton.button;
			int pressed = (event->type == SDL_JOYBUTTONDOWN);

			switch (button_id){
				default:
					printf("Unmapped button %d pressed\n", event->jbutton.button);
					break;
				case 0: // no breaks
				case 1:
				case 2:
				case 3:
					/* Mapping of Buttons of Gamepad to Mousebuttons. Mousebutton 4 and 5 are Scrollevents */
					event->button.button = (int []){SDL_BUTTON_LEFT, SDL_BUTTON_RIGHT, 4, 5}[button_id];
					int x,y;
					SDL_GetMouseState(&x, &y);
					event->type = pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
					event->button.state = pressed ? SDL_PRESSED : SDL_RELEASED;
					event->button.x = x;
					event->button.y = y;
					break;
				case 4:
					/* emulation of the ctrl_key */
					ctrl_modifier = pressed;
					event->type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
					event->key.keysym.mod = KMOD_CTRL;
					event->key.keysym.sym = SDLK_LCTRL | SDLK_RCTRL;
					break;
				case 5:
					/* emulation of the delete key */
					event->type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
					event->key.keysym.sym = SDLK_DELETE;
					break;
			}
		}
	}
	return result;
}

