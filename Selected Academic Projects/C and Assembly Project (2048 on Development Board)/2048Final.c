//----------------------------------------------
//
//  The purpose of this project is to generate a
// 2048 game on the Dragon12 board. The game will 
// be played using the joystick and the Z-Axis
// button. The game will be displayed via the putty
// terminal in a 4x4 grid.
//
//----------------------------------------------

/*
INCLUDED PERIPHERALS:
1.  7-Segment Displays
2.  Joystick
3.  SW5 Button
4.  Speaker
5.  RGB LED
6.  SCI0
7.  Timer
8.  LEDs
9.  Light Sensor
10. Potentiometer
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <hidef.h>      /* common defines and macros */
#include <mc9s12dg256.h>     /* derivative information */
#pragma LINK_INFO DERIVATIVE "mc9s12dg256b"

#include "queue.h"
#include "main_asm.h" /* interface to the assembly module */
#include "Dragon12.h"



//Enums
enum DIRECTION { Unknown, Up, Down, Left, Right };

//Macros
#define SCI_BAUD_DEFAULT    9600

#define AD_JOYSTICK_MIN    0
#define AD_JOYSTICK_MAX 1023
#define AD_JOYSTICK_MID  512

#define AD_CHANNEL_JOYSTICK_X   3 //Pin A11
#define AD_CHANNEL_JOYSTICK_Y   3 //Pin A3

#define PTH_ZAXIS_BITMASK 0x08    //Pin PH3

#define JOYSTICK_THRESHOLD_SIZE 250
#define JOYSTICK_THRESHOLD_MIN  (AD_JOYSTICK_MIN + JOYSTICK_THRESHOLD_SIZE)
#define JOYSTICK_THRESHOLD_MAX  (AD_JOYSTICK_MAX - JOYSTICK_THRESHOLD_SIZE)

#define AD_CHANNEL_LIGHT_SENSOR 4
#define AD_LIGHT_SENSOR_THRESHOLD 50

#define AD_CHANNEL_POTENTIOMETER 7
#define AD_POTENTIOMETER_MAXIMUM 1023
#define AD_POTENTIOMETER_THRESHOLD_DARK   (AD_POTENTIOMETER_MAXIMUM * 0.25)
#define AD_POTENTIOMETER_THRESHOLD_LIGHT  (AD_POTENTIOMETER_MAXIMUM * 0.75)

#define INTERRUPT_FLAG_TOGGLE_MASK 0x01

#define SCORE_DISPLAY_MAX 9999

#define MOVE_VALID     1
#define MOVE_INVALID  -1

#define MOTOR_SPEED_ENABLED       500
#define MOTOR_SPEED_DISABLED        0

#define TIMER_ENABLE  0x80



//Gameplay Macros
#define CELL_INDEX_ROW    0
#define CELL_INDEX_COL    1

#define GRID_LENGTH     4
#define GRID_SIZE       (GRID_LENGTH * GRID_LENGTH)

#define GRID_INDEX_MAX  (GRID_LENGTH - 1)

#define LED_FLASH_MAX   20


//ANSI Macros
#define ANSI_RESET          "\033[0m"

#define ANSI_TERMINAL_CLEAR "\033[2J"
#define ANSI_CURSOR_RESET   "\033[H"

#define ANSI_TEXT_COL_BLACK "\033[30m"
#define ANSI_TEXT_COL_WHITE "\033[37m"
#define ANSI_TEXT_COL_GRAY  "\033[38;5;16m"
#define ANSI_BACK_COL_WHITE "\033[47m"


//Delay Macros
#define DELAY_MS_1      1
#define DELAY_MS_20    20
#define DELAY_MS_50    50
#define DELAY_MS_100  100
#define DELAY_MS_200  200
#define DELAY_MS_500  500
#define DELAY_S_1    1000

//Functional Macros
#define MAX(a, b)   (((a) > (b)) ? (a) : (b))
#define MIN(a, b)   (((a) < (b)) ? (a) : (b))

//Global Flags
int G_Note = 0;
bool G_SW4Pressed = _FALSE_;
bool G_SW5Pressed = _FALSE_;
bool G_ZAxPressed = _FALSE_;

// Define note, pitch, & frequency.
#define NOTE_c      2867      //  261.63 Hz
#define NOTE_d      2554      //  293.66 Hz
#define NOTE_e      2276      //  329.63 Hz
#define NOTE_f      2148      //  349.23 Hz
#define NOTE_g      1914      //  392.00 Hz
#define NOTE_a      1705      //  440.00 Hz
#define NOTE_b      1519      //  493.88 Hz
#define NOTE_C      1434      //  523.25 Hz
#define NOTE_D      1277      //  587.33 Hz
#define NOTE_E      1138      //  659.26 Hz
#define NOTE_F      1074      //  698.46 Hz
#define NOTE_G       957      //  783.99 Hz
#define NOTE_A       853      //  880.00 Hz
#define NOTE_B       760      //  987.77 Hz
#define NOTE_CC      717      // 1046.50 Hz
#define NOTE_DD      639      // 1174.66 Hz

#define NOTE_REST 0

#define NOTE_WHOLE    512
#define NOTE_HALF     256
#define NOTE_QUARTER  128
#define NOTE_EIGHTH    64
#define NOTE_SIXTEENTH 32



//-----------------------------------
//  print
//
//  return - void
//  parameters
//    char* - stringIn
//
//  Outputs a string of characters to
//  via SCI0.
//-----------------------------------
void print(char* stringIn) {

  int i;
  char charCur;

  for (i = 0; (charCur = stringIn[i]) != '\0'; i++) {
    outchar0(charCur);
  }
  outchar0('\0');

}

//-----------------------------------------------------------------
// intr_sw_or_z_pressed
// 
// return - none
// parameters - none
// 
// Sets a flag indicating that the Joystick's Z-Axis has been
// pressed.
//-----------------------------------------------------------------
void interrupt 25 intr_sw_or_z_pressed() {

  uint8 clearFlag = 0x00;

  //SW4 Pressed, Force Game Over for Debugging
  if (PIFH & PORTH_SW4_BITMASK) {

    G_SW4Pressed = _TRUE_;
    clearFlag |= PORTH_SW4_BITMASK;

    }

  //SW5 Pressed, Set Audio Toggle Flag
  if (PIFH & PORTH_SW5_BITMASK) {

    //Disable main loop
    G_SW5Pressed = _TRUE_;
    clearFlag |= PORTH_SW5_BITMASK;

    }

  //Joystick Pressed, Set ... Flag
  if (PIFH & PTH_ZAXIS_BITMASK) {

    G_ZAxPressed = _TRUE_;
    clearFlag |= PTH_ZAXIS_BITMASK;

    }

  //Clear Interrupt Flag
  //PIFH = clearFlag;
  PIFH = 0xFF;

  }

//-----------------------------------------------------------------
// intr_play_tone
//
// return - none
// parameters - none
//
// Runs the tone function using the pitch currently stored in the
// global 'G_Note' variable.
//-----------------------------------------------------------------
void interrupt 13 intr_play_tone() {
  tone(G_Note);
  }


//-----------------------------------------------------------------
// print_int
//
// return - none
// parameters
//  int - number
//
// Runs the tone function using the pitch currently stored in the
// global 'G_Note' variable.
//-----------------------------------------------------------------
void print_int(int number) {

  int i, len = 0, n = number;
  
  // Buffer to store the converted integer string
  // (max 11 characters for a 32-bit int and 1 for null terminator)
  char buffer[12]; 
  
  // Check if the number is negative, if so print '-' and make the number positive
  if (n < 0) {
    outchar0('-');
    n = -n;
    }

  // Calculate the length of the integer
  do {
    len++;
    n /= 10;
  } while (n > 0);

  // Convert the integer to a string (in reverse order)
  n = number;
  for (i = 0; i < len; i++) {
    buffer[len - i - 1] = (char)('0' + (n % 10));
    n /= 10;
  }

  // Null-terminate the string
  buffer[len] = '\0';

  // Print the string
  for (i = 0; buffer[i] != '\0'; i++) {
    outchar0(buffer[i]);
  }
  outchar0('\0');
}


//-----------------------------------
// return - grid of 4x4 ints
//
// return - grid (of 4x4 ints)
// parameters - none
//
// Allocates a grid of integers and
// returns it. Each entry in the
// grid has a default value of 0.
//-----------------------------------
int** generate_grid() {
  int i, j;
  int** grid = (int**)malloc(GRID_LENGTH * sizeof(int*));

  for (i = 0; i < GRID_LENGTH ; i++) {
    grid[i] = (int*)malloc(GRID_LENGTH * sizeof(int));
    }

  for (i = 0; i < GRID_LENGTH; i++) {
    for (j = 0; j < GRID_LENGTH; j++) {
      grid[i][j] = 0;
    }
  }

  return grid;
}


//-----------------------------------
//  clear_grid
//
//  return - void
//  parameters
//    int** - grid (Grid of 4x4 ints)
//
//  Clears each entry in the target
//  grid to 0.
//-----------------------------------
void clear_grid(int** grid) {
  int i, j;
  for (i = 0; i < GRID_LENGTH; i++) {
    for (j = 0; j < GRID_LENGTH; j++) {
      grid[i][j] = 0;
    }
  }
}

//-----------------------------------
//  clear_grid_lose
//
//  return - void
//  parameters
//    int** - grid (Grid of 4x4 ints)
//
//  DEBUGGING /
//  DEMONSTRATION FUNCTION!
//
//  Clears the grid to a state where
//  the player will lose after their
//  next move.
//-----------------------------------
void clear_grid_lose(int** grid) {
  int i, j;
  int itr = 0;
  for (i = 0; i < GRID_LENGTH; i++) {
    for (j = 0; j < GRID_LENGTH; j++) {
      grid[i][j] = itr;
      itr++;
    }
  }
}

//-----------------------------------
//  print_tile_color
//
//  return - void
//  parameters
//    int - value (of a tile)
//
//  Prints an ANSI code to change the
//  terminal color based on the value
//  of a tile.
//-----------------------------------
void print_tile_color(int value) {
  switch (value) {
  case 2:     print("\033[48;5;230m"); break; // Light Yellow
  case 4:     print("\033[48;5;229m"); break; // Yellow
  case 8:     print("\033[48;5;214m"); break; // Orange
  case 16:    print("\033[48;5;208m"); break; // Dark Orange
  case 32:    print("\033[48;5;196m"); break; // Red
  case 64:    print("\033[48;5;202m"); break; // Dark Red
  case 128:   print("\033[48;5;154m"); break; // Light Green
  case 256:   print("\033[48;5;118m"); break; // Green
  case 512:   print("\033[48;5;47m");  break; // Dark Green
  case 1024:  print("\033[48;5;45m");  break; // Light Blue
  case 2048:  print("\033[48;5;21m");  break; // Blue
  case 4096:  print("\033[48;5;57m");  break; // Dark Blue
  default:    print("\033[48;5;240m"); break; // Gray
  }
  print(ANSI_TEXT_COL_GRAY);
}

//-----------------------------------
//  reset_color
//
//  return - void
//  parameters - none
//
//  Resets the terminal to default
//  color.
//-----------------------------------
void reset_color() {
  print(ANSI_RESET); // Reset color
}

//-----------------------------------
//  clear_putty
// 
//  return - void
//  parameters - none
//
//  Clears the terminal and resets
//  the position of the cursor.
//-----------------------------------
void clear_putty() {

  print(ANSI_TERMINAL_CLEAR);
  print(ANSI_CURSOR_RESET);

  }

//-----------------------------------
//  print_grid_light
// 
//  return - void
//  parameters 
//    int** - grid (Grid of 4x4 ints)
//
//  Prints the grid using the light
//  mode colors.
//-----------------------------------
void print_grid_light(int** grid) {

  int i, j;
  
  reset_color();
  print(ANSI_BACK_COL_WHITE);
  clear_putty();
  
  for (i = 0; i < GRID_LENGTH; i++) {
    for (j = 0; j < GRID_LENGTH; j++) {
    
      print_tile_color(grid[i][j]);

      // Pad output with spaces, up to 4 characters
      if      (grid[i][j] <   10) { print("   "); }
      else if (grid[i][j] <  100) { print("  ");  }
      else if (grid[i][j] < 1000) { print(" ");   }
      
      print_int(grid[i][j]);
      reset_color();
      
      print(ANSI_BACK_COL_WHITE);
      print("    ");
      
    }

    print("\n\r");
    print("\n\r");
    
  }
  
  print("\n\r");
  
  //Black Text
  print(ANSI_TEXT_COL_BLACK);
  
}

//-----------------------------------
//  print_grid_dark
// 
//  return - void
//  parameters 
//    int** - grid (Grid of 4x4 ints)
//
//  Prints the grid using the dark
//  mode colors.
//-----------------------------------
void print_grid_dark(int** grid) {

  int i, j;
  
  reset_color();
  print(ANSI_TEXT_COL_WHITE);
  clear_putty();
  
  for (i = 0; i < GRID_LENGTH; i++) {
    for (j = 0; j < GRID_LENGTH; j++) {
    
      print_tile_color(grid[i][j]);

      // Pad output with spaces, up to 4 characters
      if      (grid[i][j] <   10) { print("   "); }
      else if (grid[i][j] <  100) { print("  ");  }
      else if (grid[i][j] < 1000) { print(" ");   }
      
      print_int(grid[i][j]);
      reset_color();
      
      print("    ");
      
    }
    print("\n\r");
    print("\n\r");
    
  }
  
  print("\n\r");
  
  //White Text
  print(ANSI_TEXT_COL_WHITE);
  
}


//-----------------------------------
//  print_grid
// 
//  return - void
//  parameters 
//    int** - grid (Grid of 4x4 ints)
//
//  Prints the grid by calling either
//  the print_grid_light or
//  print_grid_dark functions.
//  Determines which one to use based
//  on the values of the light sensor
//  and potentiometer.
//-----------------------------------
void print_grid(int** grid) {

  static int valPotentiometerPrev = 0;
  int valPotentiometer;
  bool use_light_mode = _FALSE_;


  //Check for potentiometer override
  valPotentiometer = ad0conv(AD_CHANNEL_POTENTIOMETER);

  //Force Dark Mode
  if (valPotentiometer < AD_POTENTIOMETER_THRESHOLD_DARK) {
    
    use_light_mode = _FALSE_;
    
    if (valPotentiometerPrev >= AD_POTENTIOMETER_THRESHOLD_DARK) {
      print("Enabled Dark Mode override");
      print("\n\r");
      ms_delay(DELAY_S_1);
      }    
    
    }
  
  //Force Light Mode
  else if (valPotentiometer > AD_POTENTIOMETER_THRESHOLD_LIGHT) {
    
    use_light_mode = _TRUE_;
    
    if (valPotentiometerPrev <= AD_POTENTIOMETER_THRESHOLD_DARK) {
      print("Enabled Light Mode override");
      print("\n\r");
      ms_delay(DELAY_S_1);
      }    

    }

  //Use light sensor value
  else {
  
    use_light_mode = (ad0conv(AD_CHANNEL_LIGHT_SENSOR) > AD_LIGHT_SENSOR_THRESHOLD);
    
    if (
      (valPotentiometerPrev < AD_POTENTIOMETER_THRESHOLD_DARK) ||
      (valPotentiometerPrev > AD_POTENTIOMETER_THRESHOLD_LIGHT)
      ) {
      
      print("Disabled Light/Dark Mode override");
      print("\n\r");
      ms_delay(DELAY_S_1);
      }
    
    }


  //Light Mode Enabled
  if (use_light_mode) { print_grid_light(grid); }

  //Dark Mode Enabled
  else { print_grid_dark(grid); }


  valPotentiometerPrev = valPotentiometer;

  }





//-----------------------------------
//  joystick_get_x_axis
// 
//  return - int
//  parameters - none
//
//  Reads the value of the joystick's
//  X-Axis via ADC 1.
//-----------------------------------
int joystick_get_x_axis() {

  return ad1conv(AD_CHANNEL_JOYSTICK_X);

  }

//-----------------------------------
//  joystick_get_x_axis
// 
//  return - int
//  parameters - none
//
//  Reads the value of the joystick's
//  Y-Axis via ADC 0.
//-----------------------------------
int joystick_get_y_axis() {

  return ad0conv(AD_CHANNEL_JOYSTICK_Y);

  }

//-----------------------------------
//  play_sound
// 
//  return - void
//  parameters
//      int - note (pitch value)
//      int - duration
//
//  Plays a given note/pitch for a
//  given duration (in ms).
//  Interrupts are disabled while the
//  sound plays to avoid conflicts
//  with other parts of the program.
//-----------------------------------
void play_sound(int note, int duration_ms) {

  DisableInterrupts;

  sound_init();
  sound_on();
  
  G_Note = note;
  ms_delay(duration_ms);

  sound_off();

  EnableInterrupts;
 
  }

//-----------------------------------
//  spawn_new_tile
// 
//  return - void
//  parameters
//    int** - grid (Grid of 4x4 ints)
//
//  Attempts to create a 2 or a 4
//  tile at a random free location in
//  the grid.
//-----------------------------------
void spawn_new_tile(int** grid) {
  int available_cells[GRID_SIZE][2];  //<-- Stores the coordinates of all available empty cells
  int count = 0, i, j, index, value;

  // Find all available empty cells
  for (i = 0; i < GRID_LENGTH; i++) {
    for (j = 0; j < GRID_LENGTH; j++) {
      if (grid[i][j] == 0) {
        available_cells[count][CELL_INDEX_ROW] = i;
        available_cells[count][CELL_INDEX_COL] = j;
        count++;
      }
    }
  }

  if (count > 0) {
    // Choose a random empty cell
    index = rand() % count;
    i = available_cells[index][CELL_INDEX_ROW];
    j = available_cells[index][CELL_INDEX_COL];

    // Spawn a new tile with a value of 2 (90% chance) or 4 (10% chance)
    value = (rand() % 10 == 0) ? 4 : 2;
    grid[i][j] = value;
  }
}

//-----------------------------------
//  slide_row_left
// 
//  return - int
//  parameters
//      int* - row (of tiles)
//
//  Slides the given row to the left,
//  attempting to merge tiles.
//  Returns either the score, if the
//  move was successful and some
//  tiles were merged, or a default
//  value indicating otherwise.
//-----------------------------------
int slide_row_left(int* row) {
  int i, j;
  int score = 0;
  int empty_index = -1;  
  bool moved = _FALSE_;
  bool merged[GRID_LENGTH] = {_FALSE_, _FALSE_, _FALSE_, _FALSE_};

  for (i = 0; i < GRID_LENGTH; i++) {
    if (row[i] != 0) {
      for (j = i + 1; j < GRID_LENGTH; j++) {
        if (row[j] != 0) {
          if (row[i] == row[j] && (merged[i]==_FALSE_)) {

            row[i] *= 2;
            row[j] = 0;
            merged[i] = _TRUE_;
            moved = _TRUE_;
            score += row[i];

          }
          break;
        }
      }
    }
  }

  // Slide non-empty tiles to the left 
  for (i = 0; i < GRID_LENGTH; i++) {
    if (row[i] == 0) {
      if (empty_index == -1) {
        empty_index = i;
      }
    } else {
      if (empty_index != -1) {
        row[empty_index] = row[i];
        row[i] = 0;
        i = empty_index;
        empty_index = -1;
        moved = _TRUE_;
      }
    }
  }

  return (moved ? score : MOVE_INVALID);
}

//-----------------------------------
//  slide_grid_left
// 
//  return - int
//  parameters
//    int** - grid (Grid of 4x4 ints)
//
//  Slides the grid to the left,
//  attempting to merge tiles.
//  Returns either the score, if the
//  move was successful and some
//  tiles were merged, or a default
//  value indicating otherwise.
//-----------------------------------
int slide_grid_left(int** grid) {
  int moved = 0;
  int score, totalScore = 0;
  int i;

  for (i = 0; i < GRID_LENGTH; i++) {
    score = slide_row_left(grid[i]);
    if (score >= 0) {
      moved = _TRUE_;
      totalScore += score;
    }
  }

  return (moved ? totalScore : MOVE_INVALID);
}

//-----------------------------------
//  slide_grid_right
// 
//  return - int
//  parameters
//    int** - grid (Grid of 4x4 ints)
//
//  Slides the grid to the right,
//  attempting to merge tiles.
//  Returns either the score, if the
//  move was successful and some
//  tiles were merged, or a default
//  value indicating otherwise.
//-----------------------------------
int slide_grid_right(int** grid) {
  int moved = 0;
  int score, totalScore = 0;
  int i, j;

  for (i = 0; i < GRID_LENGTH; i++) {
    // Reverse the row before sliding it to the left
    int temp_row[GRID_LENGTH];
    for (j = 0; j < GRID_LENGTH; j++) {
      temp_row[j] = grid[i][GRID_INDEX_MAX - j];
    }

    // Slide the reversed row to the left
    score = slide_row_left(temp_row);
    if (score >= 0) {
      moved = _TRUE_;
      totalScore += score;
      }

    // Reverse the row back and store it in the grid
    for (j = 0; j < GRID_LENGTH; j++) {
      grid[i][GRID_INDEX_MAX - j] = temp_row[j];
    }
  }
  return (moved ? totalScore : MOVE_INVALID);
}

//-----------------------------------
//  slide_grid_up
// 
//  return - int
//  parameters
//    int** - grid (Grid of 4x4 ints)
//
//  Slides the grid up,
//  attempting to merge tiles.
//  Returns either the score, if the
//  move was successful and some
//  tiles were merged, or a default
//  value indicating otherwise.
//-----------------------------------
int slide_grid_up(int** grid) {
  int moved = 0;
  int score, totalScore = 0;
  int i, j;
  int temp_row[GRID_LENGTH];

  for (j = 0; j < GRID_LENGTH; j++) {
    for (i = 0; i < GRID_LENGTH; i++) {
      temp_row[i] = grid[i][j];
    }
    
    score = slide_row_left(temp_row);
    if (score >= 0) {
      moved = _TRUE_;
      totalScore += score;
      }

    for (i = 0; i < GRID_LENGTH; i++) {
      grid[i][j] = temp_row[i];
    }
  }

  return (moved ? totalScore : MOVE_INVALID);
}

//-----------------------------------
//  slide_grid_down
// 
//  return - int
//  parameters
//    int** - grid (Grid of 4x4 ints)
//
//  Slides the grid down,
//  attempting to merge tiles.
//  Returns either the score, if the
//  move was successful and some
//  tiles were merged, or a default
//  value indicating otherwise.
//-----------------------------------
int slide_grid_down(int** grid) {
  int moved = 0;
  int score, totalScore = 0;
  int i, j;
  int temp_row[GRID_LENGTH];

  for (j = 0; j < GRID_LENGTH; j++) {
    for (i = 0; i < GRID_LENGTH; i++) {
      temp_row[i] = grid[GRID_INDEX_MAX - i][j];
    }

    score = slide_row_left(temp_row);
    if (score >= 0) {
      moved = _TRUE_;
      totalScore += score;
      }

    for (i = 0; i < GRID_LENGTH;  i++) {
      grid[GRID_INDEX_MAX - i][j] = temp_row[i];
    }
  }

  return (moved ? totalScore : MOVE_INVALID);
}

//-----------------------------------
//  move_tiles
// 
//  return - int
//  parameters
//    int** - grid (Grid of 4x4 ints)
//    enum DIRECTION - direction
//
//  Attempts to slide the grid in a
//  given direction.
//  Returns the score to be added as
//  a result of the move.
//  Displays the direction of the
//  move in the terminal. (Note that
//  this is NOT a debugging feature,
//  as it provides necessary feedback
//  for the user in the event of a
//  misinput or invalid move.)
//-----------------------------------
int move_tiles(int** grid, enum DIRECTION direction) {

  int movedScore = 0;

  switch (direction) {
    case Up:
      print("Slide Up\n\r");
      ms_delay(DELAY_MS_50);
      movedScore = slide_grid_up(grid);
      break;
    case Down:
      print("Slide Down\n\r");
      ms_delay(DELAY_MS_50);
      movedScore = slide_grid_down(grid);
      break;
    case Left:
      print("Slide Left\n\r");
      ms_delay(DELAY_MS_50);
      movedScore = slide_grid_left(grid);
      break;
    case Right:
      print("Slide Right\n\r");
      ms_delay(DELAY_MS_50);
      movedScore = slide_grid_right(grid);
      break;
    default:
      break;
    }

  return movedScore;

}

//-----------------------------------
//  game_over
// 
//  return - bool
//  parameters
//    int** - grid (Grid of 4x4 ints)
//
//  Checks and returns if the game
//  is over (due to there being no
//  remaining valid moves).
//-----------------------------------
bool game_over(int** grid) {
  int i, j;

  for (i = 0; i < GRID_LENGTH; i++) {
    for (j = 0; j < GRID_LENGTH; j++) {

      //Empty space detected
      if (grid[i][j] == 0) {
        return _FALSE_;
      }

      //Horizontal move available
      if (i < GRID_INDEX_MAX && grid[i][j] == grid[i + 1][j]) {
        return _FALSE_;
      }

      //Vertical move available
      if (j < GRID_INDEX_MAX && grid[i][j] == grid[i][j + 1]) {
        return _FALSE_;
      }
    }
  }

  //No available moves found
  return _TRUE_;
}

//-----------------------------------
//  extract_digit
// 
//  return - int
//  parameters
//    int - num
//    int - position
//
//  Extracts and returns a digit at
//  'position' from 'num'
//   (To be used when displaying
//   values on the 7-segment).
//-----------------------------------
int extract_digit(int num, int position) {
  
  int extracted_digit;
  int i;

  //Remove digits to the right of the target position
  for (i = 1; i < position; i++) {
    num /= 10;
    }

  //Extract the digit at the target position
  extracted_digit = (num % 10);
  return extracted_digit;

  }

//-----------------------------------
//  score_display_7seg
// 
//  return - void
//  parameters
//    int - score
//
//  Displays the 'score' value on the
//  7-segment.
//  The value is clamped with a max
//  value of 9999.
//-----------------------------------
void score_display_7seg(int score) {

  score = MIN(SCORE_DISPLAY_MAX, score);

  seg7dec(extract_digit(score, 1), DIG0);
  ms_delay(DELAY_MS_1);
  seg7dec(extract_digit(score, 2), DIG1);
  ms_delay(DELAY_MS_1);
  seg7dec(extract_digit(score, 3), DIG2);
  ms_delay(DELAY_MS_1);
  seg7dec(extract_digit(score, 4), DIG3);
  ms_delay(DELAY_MS_1);
  
  }

//-----------------------------------
//  game_over_leds
// 
//  return - void
//  parameters - none
//
//  Flashes the LEDs on and off.
//-----------------------------------
void game_over_leds() {

  static flashTime;

  flashTime++;

  if (flashTime < LED_FLASH_MAX / 2) {
    leds_on(0xFF);
    }
  else {
    flashTime %= LED_FLASH_MAX;
    leds_off();
    }

  }

//-----------------------------------
//  game_loop
// 
//  return - void
//  parameters
//    int** - tileGrid
//       (Grid of 4x4 ints)
//
//  Provides the main functionality
//  for the game loop. Includes
//  functionality for input,
//  displaying the game, displaying
//  the score, playing and toggling
//  audio, etc.
//-----------------------------------
void game_loop(int** tileGrid) {

  static bool soundEnabled = _TRUE_;

  static enum Direction joystickDirectionPrev = Unknown;
  enum Direction joystickDirection = Unknown;

  static int highScore = 0;
  static int score = 0;

  int movedScore = 0;
  int joystickX = AD_JOYSTICK_MID;
  int joystickY = AD_JOYSTICK_MID;

  //Read Joystick's X-Axis
  joystickX = joystick_get_x_axis();
  
  //Read Joystick's Y-Axis
  joystickY = joystick_get_y_axis();

  //Get Current Joystick Direction
  if      (joystickX < JOYSTICK_THRESHOLD_MIN) { joystickDirection = Left;  }
  else if (joystickX > JOYSTICK_THRESHOLD_MAX) { joystickDirection = Right; }
  else if (joystickY < JOYSTICK_THRESHOLD_MIN) { joystickDirection = Up;    }
  else if (joystickY > JOYSTICK_THRESHOLD_MAX) { joystickDirection = Down;  }

  //Display Score
  score_display_7seg(score);

  //Force Game Over [FOR DEBUGGING + DEMONSTRATION]
  if (G_SW4Pressed) {

    clear_grid_lose(tileGrid);
    print_grid(tileGrid);
    G_SW4Pressed = _FALSE_;

    }

  //Toggle Sound
  if (G_SW5Pressed) {

    G_SW5Pressed = _FALSE_;

    soundEnabled = !(soundEnabled);
    if (soundEnabled) { print("Sound Enabled\n\r"); }
    else { print("Sound Disabled\n\r"); }

    //Toggle LED to indicate sound is enabled/disabled
    motor4(soundEnabled ? MOTOR_SPEED_ENABLED : MOTOR_SPEED_DISABLED);

    }

  //Check if the joystick was moved to a new position
  if ((joystickDirection != Unknown) && (joystickDirection != joystickDirectionPrev)) {

    seg7s_off();

    //Move the tiles
    movedScore = move_tiles(tileGrid, joystickDirection);

    //Check if the move was valid
    if (movedScore != MOVE_INVALID) {

      //Increase incoming score (if any), play sound if enabled
      if (movedScore > 0) {

        if (soundEnabled) {
          play_sound(NOTE_G, NOTE_SIXTEENTH);
          play_sound(NOTE_D, NOTE_SIXTEENTH);
          play_sound(NOTE_B, NOTE_EIGHTH);
          }
        score += movedScore;

        }

      spawn_new_tile(tileGrid);
      print_grid(tileGrid);

      //PRINT SCORE
      print("\n\r");
      print("\n\r");
      print("Total Score: ");
      print_int(score);
      print("\n\r");
      print("Added Score: ");
      print_int(movedScore);
      print("\n\r");
      print("\n\r");

      if (game_over(tileGrid)) {

        if (score > highScore) {
          highScore = score;
          print("New high score!\n\r");
          }
        score = 0;

        clear_lcd();
        set_lcd_addr(LCD_LINE1_ADDR);
        type_lcd("High Score: ");
        set_lcd_addr(LCD_LINE2_ADDR);
        write_long_lcd(highScore);

        //Play Game Over Sound
        if (soundEnabled) {
          play_sound(NOTE_D, NOTE_HALF);
          play_sound(NOTE_C, NOTE_QUARTER);
          play_sound(NOTE_b, NOTE_QUARTER);
          play_sound(NOTE_g, NOTE_WHOLE);
          }

        //Display Game Over Message
        print("Game Over!");
        print("\n\r");
        print("\n\rPress the Joystick's Z-Axis to continue");
        
        //Flash Game Over LEDs & Wait for Z-Axis Press
        G_ZAxPressed = _FALSE_;
        seg7_disable();
        led_enable();
        while (_TRUE_) {

          game_over_leds();
          ms_delay(DELAY_MS_20);
        
          if (G_ZAxPressed) {
            G_ZAxPressed = _FALSE_;
            break;
            }

          }
        led_disable();
        seg7_enable();

        //Reset Game Grid
        clear_grid(tileGrid);
        clear_putty();
        spawn_new_tile(tileGrid);
        spawn_new_tile(tileGrid);
        print_grid(tileGrid);
      
        }
    
      }

    }

  //Record Previous Joystick Values
  joystickDirectionPrev = joystickDirection;

  }


void main(void) {

  //Initialize Variables
  bool doProgramLoop = _TRUE_;
  bool doGameLoop = _TRUE_;

  //Create a grid of 4x4 ints
  int** tileGrid = generate_grid();

  //Initialize the random seed
  TSCR1 = TIMER_ENABLE;           //Enable the timer  
  srand((unsigned)(TCNT));        //Seed the random number generator

  //Spawn the initial two tiles
  spawn_new_tile(tileGrid);
  spawn_new_tile(tileGrid);


  PLL_init();
  
  SCI0_init(SCI_BAUD_DEFAULT);

  led_disable();
  seg7_enable();
  lcd_init();
  clear_lcd();
  
  ad0_enable();
  ad1_enable();

  SW_enable();

  //Initialize Motors to display red on the RGB LED
  motor4_init();
  motor5_init();
  motor6_init();
  motor4(MOTOR_SPEED_ENABLED);
  motor5(MOTOR_SPEED_DISABLED);
  motor6(MOTOR_SPEED_DISABLED);


  //Enable external interrupts 
  EnableInterrupts;


  DDRH = 0x00;

  //Clear Old Flags       
  PIFJ = 0x00;
  PIFP = 0x00;
  PIFH = 0x00;

  //Enable Port H interrupt on falling edge
  PPSH = 0x00;
  PIEH = 0xFF;


  //Start of Program
  print_grid(tileGrid);
  while (doGameLoop) {
  
    //Do Game Loop
    game_loop(tileGrid);

    }

  //End of Program
  print("\n\r\n\rThank you for using the program");

  seg7_disable();
  clear_lcd();

  }
