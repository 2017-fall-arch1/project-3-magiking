/** \file shapemotion.c
 *  \brief This is a simple shape motion demo.
 *  This demo creates two layers containing shapes.
 *  One layer contains a rectangle and the other a circle.
 *  While the CPU is running the green LED is on, and
 *  when the screen does not need to be redrawn the CPU
 *  is turned off along with the green LED.
 */  
#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <p2switches.h>
#include <shape.h>
#include <abCircle.h>
#include "pong.h"
#include "buzzer.h"
#include "stateMachines.h"

#define GREEN_LED BIT6

#define MAX_SCORE 5

#define DELAY 500000

/* Sounds for collisions */
#define L_P_PERIOD 3500
#define L_F_PERIOD 1000
#define R_P_PERIOD 4000
#define R_F_PERIOD 1500

static int pl_score = 0;
static char pl_score_string[1];
static int pr_score = 0;
static char pr_score_string[1];
static char winner[] = "player 2";

AbRect ball    = {abRectGetBounds, abRectCheck, {4,4}}; /**< 10x10 rectangle */
AbRect paddle2 = {abRectGetBounds, abRectCheck, {4,14}}; /**< 10x10 rectangle */
AbRect paddle1 = {abRectGetBounds, abRectCheck, {4,14}}; /**< 10x10 rectangle */
AbRArrow rightArrow = {abRArrowGetBounds, abRArrowCheck, 30};

AbRectOutline fieldOutline = {	/* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2 - 10, screenHeight/2 - 10}
};
  

Layer fieldLayer = {		/* playing field as a layer */
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2},/**< initial position */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_WHITE,
  0,
};


Layer layerPr = {		/**< Layer with right paddle 2*/
  (AbShape *)&paddle2,
  {(screenWidth)-10, (screenHeight/2)}, /**< middle right*/
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_WHITE,
  &fieldLayer,
};


Layer layerPl = {		/**< Layer with left paddle1*/
  (AbShape *)&paddle1,
  {10, screenHeight/2}, /**< middle left */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_WHITE,
  &layerPr,
};

Layer layerBall = {		/**< Layer with the ball */
  (AbShape *)&ball,
  {(screenWidth/2)+10, (screenHeight/2)+5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_WHITE,
  &layerPl,
};

/** Moving Layer
 *  Linked list of layer references
 *  Velocity represents one iteration of change (direction & magnitude)
 */
typedef struct MovLayer_s {
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

/* initial value of {0,0} will be overwritten */
MovLayer ml_ball = { &layerBall, {4,4}, 0 }; 

/* paddle mov layers */
MovLayer ml_plU = { &layerPl, {0,-5}, 0 }; 
MovLayer ml_plD = { &layerPl, {0,5}, 0 }; 

MovLayer ml_prU = { &layerPr, {0,-5}, 0 }; 
MovLayer ml_prD = { &layerPr, {0,5}, 0 }; 

void movLayerDraw(MovLayer *movLayers, Layer *layers)
{
  int row, col;
  MovLayer *movLayer;

  and_sr(~8);			/**< disable interrupts (GIE off) */
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  or_sr(8);			/**< disable interrupts (GIE on) */


  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Region bounds;
    layerGetBounds(movLayer->layer, &bounds);
    lcd_setArea(bounds.topLeft.axes[0], bounds.topLeft.axes[1], 
		bounds.botRight.axes[0], bounds.botRight.axes[1]);
    for (row = bounds.topLeft.axes[1]; row <= bounds.botRight.axes[1]; row++) {
      for (col = bounds.topLeft.axes[0]; col <= bounds.botRight.axes[0]; col++) {
	Vec2 pixelPos = {col, row};
	u_int color = bgColor;
	Layer *probeLayer;
	for (probeLayer = layers; probeLayer; 
	     probeLayer = probeLayer->next) { /* probe all layers, in order */
	  if (abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)) {
	    color = probeLayer->color;
	    break; 
	  } /* if probe check */
	} // for checking all layers at col, row
	lcd_writeColor(color); 
      } // for col
    } // for row
  } // for moving layer being updated
}	  


/*
 * Draws the score on the playing screen
 */
void
scoreDraw()
{
    /* left */
    getScoreChar(pl_score, pl_score_string);
    drawChar5x7(20, 1, pl_score_string[0], COLOR_YELLOW, COLOR_BLACK);
    /* right */
    getScoreChar(pr_score, pr_score_string);
    drawChar5x7(screenWidth-20, 1, pr_score_string[0], COLOR_YELLOW, COLOR_BLACK);
}

/*
 * sets s to char representation of n
 * from k&r itoa
 */
void
getScoreChar(int n, char s[])
{
   int i, sign;

   if ((sign = n) <0) /* record sign */
       n = -n;
   i=0;
   do{
       s[i++] = n % 10 + '0';
   } while ((n /= 10) >0);
   if (sign < 0)
       s[i++] = '-';
   s[i] = '\0';
}


/** Advances the paddle within a fence
 *  
 *  \param ml The moving shape to be advanced
 *  \param fence The region which will serve as a boundary for ml
 */
void
mlPaddleAdvance(MovLayer *ml, Region *fence)
{
    Vec2 newPos;
    u_char axis;
    Region shapeBoundary;
    for (; ml; ml = ml->next) 
    {
        vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
        abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
        for (axis = 0; axis < 2; axis ++) 
        {
            if ((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]) ||
                    (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]) ) 
            { 
                int velocity = REV * (ml->velocity.axes[axis]);
                newPos.axes[axis] += (velocity);   /*< don't bounce, just don't move past bound*/ 
            }	/**< if outside of fence */
        } /**< for axis */
        ml->layer->posNext = newPos;
    } /**< for ml */

}

/** Advances the ball within a fence
 *  
 *  \param ml The moving shape to be advanced
 *  \param fence The region which will serve as a boundary for ml
 */
void mlBallAdvance(MovLayer *ml_ball, MovLayer *ml_plU, MovLayer *ml_prU, Region *fence)
{
    Vec2 newPos_ball;
    Vec2 newPos_pl;
    Vec2 newPos_pr;

    u_char axis;

    Region ballBoundary;
    Region plBoundary;
    Region prBoundary;

    for (; ml_ball; ml_ball = ml_ball->next) {
        vec2Add(&newPos_ball, &ml_ball->layer->posNext, &ml_ball->velocity);
        abShapeGetBounds(ml_ball->layer->abShape, &newPos_ball, &ballBoundary);

        vec2Add(&newPos_pl, &ml_plU->layer->posNext, &ml_plU->velocity);
        abShapeGetBounds(ml_plU->layer->abShape, &newPos_pl, &plBoundary);           /** get left paddle boundaries */

        vec2Add(&newPos_pr, &ml_prU->layer->posNext, &ml_prU->velocity);
        abShapeGetBounds(ml_prU->layer->abShape, &newPos_pr, &prBoundary);           /** get left paddle boundaries */
        for (axis = 0; axis < 2; axis ++) 
        {
            /** left_fence should act as a bool here. If we go into the inner section of this if, then it hit some part of the fence */
            if ( (ballBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis])) 
            {
                if (axis == 0)         /**< only care about left/right wall*/
                {
                    //if win
		    if (++pr_score == MAX_SCORE)
		    {
			/* draw pr winner */
			winner[7] = '2';
                        winscreen();
			int velocity = ml_ball->velocity.axes[axis] =  -ml_ball->velocity.axes[axis];
			newPos_ball.axes[axis] += (2*velocity);
			break;
		    }

                    buzzer_set_period(L_F_PERIOD);
                    __delay_cycles(DELAY); //leave the buzzer on
                }
                int velocity = ml_ball->velocity.axes[axis] =  -ml_ball->velocity.axes[axis];
                newPos_ball.axes[axis] += (2*velocity);
            }
            if(ballBoundary.botRight.axes[axis] > fence->botRight.axes[axis])  
            {
                if (axis == 0) 
                {
                    //do thing for right side score.
		    if (++pl_score == MAX_SCORE)
		    {
			/* draw pr winner */
			winner[7] = '1';
                        winscreen();
			int velocity = ml_ball->velocity.axes[axis] =  -ml_ball->velocity.axes[axis];
			newPos_ball.axes[axis] += (2*velocity);
			break;
		    }
                    buzzer_set_period(R_F_PERIOD);
                    __delay_cycles(DELAY); //leave the buzzer on
                }
                int velocity = ml_ball->velocity.axes[axis] =  -ml_ball->velocity.axes[axis];
                newPos_ball.axes[axis] += (2*velocity);
            }	//**< if outside of fence 


            
            
            //**< left paddle, check right side  
            else if (
                    (ballBoundary.topLeft.axes[0] < plBoundary.botRight.axes[0]) && //hitting the side of paddle
                    (   //*ball top y val < left paddle top */    /*ball top y val  > left paddle bottom */ //top of ball between y valls of paddle 
                        (ballBoundary.topLeft.axes[1] > plBoundary.topLeft.axes[1] && ballBoundary.topLeft.axes[1] < plBoundary.botRight.axes[1]) ||
                        //*ball bottom y val < left paddle top */   /*ball bottom y val  > left paddle bottom */  
                        (ballBoundary.botRight.axes[1] > plBoundary.topLeft.axes[1] && ballBoundary.botRight.axes[1] < plBoundary.botRight.axes[1])  
                    ) 
               )
            {
                // do a thing for left side paddle collision
                buzzer_set_period(L_P_PERIOD);
                __delay_cycles(DELAY); //leave the buzzer on
                int velocity = ml_ball->velocity.axes[axis] =  -ml_ball->velocity.axes[axis];
                newPos_ball.axes[axis] += (2*velocity);
                break;
            }
                        
            //**< right paddle, check left side 
            else if (
                    (ballBoundary.botRight.axes[0] > prBoundary.topLeft.axes[0]) && //hitting the side of paddle
                    (   //*ball top y val < right paddle top */    /*ball top y val  > right paddle bottom */ //top of ball between y valls of paddle 
                        (ballBoundary.topLeft.axes[1] > prBoundary.topLeft.axes[1] && ballBoundary.topLeft.axes[1] < prBoundary.botRight.axes[1]) ||
                        //*ball bottom y val < right paddle top */   /*ball bottom y val  > right paddle bottom */  
                        (ballBoundary.botRight.axes[1] > prBoundary.topLeft.axes[1] && ballBoundary.botRight.axes[1] < prBoundary.botRight.axes[1])  
                    ) 
               )
            {
                // do a thing for left side paddle collision
                buzzer_set_period(R_P_PERIOD);
                __delay_cycles(DELAY); //leave the buzzer on
                int velocity = ml_ball->velocity.axes[axis] =  -ml_ball->velocity.axes[axis];
                newPos_ball.axes[axis] += (2*velocity);
                break;
            }
	   
        } /**< for axis */
        ml_ball->layer->posNext = newPos_ball;
    } /**< for ml_ball */
}


Region fieldFence;		/**< fence around playing field  */

/**
 * Reads switch inputs and move's paddles accordingly
 */
void
movePaddlesC(){
    unsigned int sw = p2sw_read();
    if(!(BIT0 & sw)){
        movLayerDraw(&ml_plU, &layerPl);  /** So, of course you have to draw the layer before you can advance the screen */
        mlPaddleAdvance(&ml_plU, &fieldFence);
    }
    if(!(BIT1 & sw)){
        movLayerDraw(&ml_plD, &layerPl);  
        mlPaddleAdvance(&ml_plD, &fieldFence);
    }
    if(!(BIT2 & sw)){
        movLayerDraw(&ml_prU, &layerPl);  
        mlPaddleAdvance(&ml_prU, &fieldFence);
    }
    if(!(BIT3 & sw)){
        movLayerDraw(&ml_prD, &layerPl);  
        mlPaddleAdvance(&ml_prD, &fieldFence);
    }
    
}

/**
 * Draw start screen
 */
void
startscreen()
{
    clearScreen(COLOR_BLUE);
    drawString5x7(screenWidth/2 -10 , 25, "PONG", COLOR_BLACK, COLOR_BLUE);
    drawString5x7(20, 35, "Press any button", COLOR_BLACK, COLOR_BLUE);
    //drawString5x7(5, 35, "Press any button to start", COLOR_BLACK, COLOR_BLUE);
    for(;;)
    {
	
	if(!(BIT0 & P2IN)){
	    ml_ball.velocity.axes[0] = 4;
	    ml_ball.velocity.axes[0] = 4;
	    //state_advance();
	    break;
	}
	if(!(BIT1 & P2IN)){
	    ml_ball.velocity.axes[0] = 4;
	    ml_ball.velocity.axes[0] = 4;
	    //state_advance();
	    break;
	}
	if(!(BIT2 & P2IN)){
	    ml_ball.velocity.axes[0] = 4;
	    ml_ball.velocity.axes[0] = 4;
	    //state_advance();
	    break;
	}
	if(!(BIT3 & P2IN)){
	    ml_ball.velocity.axes[0] = 4;
	    ml_ball.velocity.axes[0] = 4;
	    //state_advance();
	    break;
    	} 
    }
    //maybe color screen black again?
}

/**
 * Draw win screen
 * Shows winning player's name
 */
void
winscreen()
{
    clearScreen(COLOR_BLUE);
    drawString5x7(screenWidth/2 -10 , 25, "PONG", COLOR_BLACK, COLOR_BLUE);
    drawString5x7(20, 35, "The winner is:", COLOR_BLACK, COLOR_BLUE);
    drawString5x7(20, 45, winner, COLOR_BLACK, COLOR_BLUE);
    for(;;){	
	if(!(( BIT0 | BIT1 ) & P2IN)){
	    pl_score = pr_score = 0;
	    break;
	}
    }
    clearScreen(COLOR_BLACK); /* clear screen and redraw shapes */
    layerDraw(&layerBall);
    scoreDraw();
}
u_int bgColor = COLOR_BLACK;     /**< The background color */
int redrawScreen = 1;           /**< Boolean for whether screen needs to be redrawn */


/** Initializes everything, enables interrupts and green LED, 
 *  and handles the rendering for the screen
 */
void main()
{
  P1DIR |= GREEN_LED;		/**< Green led on when CPU on */		
  P1OUT |= GREEN_LED;

  configureClocks();
  lcd_init();
  buzzer_init();
  shapeInit();
  p2sw_init(15);                /** initialize all switches */

  shapeInit();

  startscreen();

  layerInit(&layerBall);
  layerDraw(&layerBall);

  scoreDraw();

  layerGetBounds(&fieldLayer, &fieldFence);


  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);	              /**< GIE (enable interrupts) */


  for(;;) { 
    while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */ 
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
      or_sr(0x10);	      /**< CPU OFF */
    }
    P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
    redrawScreen = 0;
    movLayerDraw(&ml_ball, &layerBall);
    scoreDraw();
    movePaddlesC();
    buzzer_set_period(0);
  }
}

/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler()
{
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  count ++;
  if (count == 15) {
    mlBallAdvance(&ml_ball, &ml_plU, &ml_prU, &fieldFence);
    redrawScreen = 1;
    count = 0;
  } 
  P1OUT &= ~GREEN_LED;		    /**< Green LED off when cpu off */
}
