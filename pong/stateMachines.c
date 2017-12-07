#include <msp430.h>
#include "stateMachines.h"
#include "buzzer.h"
#include "pong.h"

void state_advance()		/* alternate between toggling red & green */
{

  //static enum {R=0, G=1} color = R; // initial color
  static enum {START, PLAY, WIN} state = START;
  switch (state) 
  {
      case START: 
          /* draw start screen */
          state = PLAY;
          startscreen();
          break;
      case PLAY:
          state = WIN; 
          winscreen();
          break;
      case WIN: 
          state = START;
          startscreen();
          break;
  }
}



