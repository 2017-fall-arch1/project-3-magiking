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

#define GREEN_LED BIT6


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
//Region fence = {{10,30}, {SHORT_EDGE_PIXELS-10, LONG_EDGE_PIXELS-10}}; /**< Create a fence region */

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
                    //do thing for left side score.
                }
                int velocity = ml_ball->velocity.axes[axis] =  -ml_ball->velocity.axes[axis];
                newPos_ball.axes[axis] += (2*velocity);
            }
            if(ballBoundary.botRight.axes[axis] > fence->botRight.axes[axis])  
            {
                if (axis == 0) 
                {
                    //do thing for right side score.
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
                int velocity = ml_ball->velocity.axes[axis] =  -ml_ball->velocity.axes[axis];
                newPos_ball.axes[axis] += (2*velocity);
                break;
            }
	   
        } /**< for axis */
        ml_ball->layer->posNext = newPos_ball;
    } /**< for ml_ball */
}


u_int bgColor = COLOR_BLACK;     /**< The background color */
int redrawScreen = 1;           /**< Boolean for whether screen needs to be redrawn */

Region fieldFence;		/**< fence around playing field  */

/**
 * Does something if switch is pressed.
 */
void
movePaddles(){
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

/** Initializes everything, enables interrupts and green LED, 
 *  and handles the rendering for the screen
 */
void main()
{
  P1DIR |= GREEN_LED;		/**< Green led on when CPU on */		
  P1OUT |= GREEN_LED;

  configureClocks();
  lcd_init();
  shapeInit();
  p2sw_init(15);                /** initialize all switches */

  shapeInit();

  layerInit(&layerBall);
  layerDraw(&layerBall);


  layerGetBounds(&fieldLayer, &fieldFence);


  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);	              /**< GIE (enable interrupts) */


  for(;;) { 
    while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */ 
      movePaddles();
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
      or_sr(0x10);	      /**< CPU OFF */
    }
    P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
    redrawScreen = 0;
    movLayerDraw(&ml_ball, &layerBall);
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
