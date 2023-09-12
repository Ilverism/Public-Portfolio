# *****************************************************************************
# ***************************  Python Source Code  ****************************
# *****************************************************************************
#
# DESCRIPTION
#   This program generates a playable version of the game 2048 on a locally
#   hosted server via the bottle library. The game can be controlled via
#   joystick and keyboard inputs.
#   The player's score and high score is also displayed on an external LCD
#   with support from the LCD1602.py file.
#   Multithreading is used to poll for joystick inputs on the client side.
#   JavaScript, CSS, and HTML are used for handling incoming inputs, updating
#   the game state, and displaying those updates in the browser for the player
#   to see.
#      
#   The program relies on the following external connections and peripherals:
#
#       JOYSTICK                            LCD
#       GND → GND                           GND → GND
#       +5V → +5V (VCC)                     +5V → +5V
#       VRX → A1  (ADS7830 ADC)             SDA → SDA1
#       VRY → A0  (ADS7830 ADC)             SCL → SCL1
#       SW  → GPIO18
#
#       ADS7830 ADC
#       SDA → SDA1
#       SCL → SCL1
#
# *****************************************************************************

#Imports
import RPi.GPIO as GPIO
import time
import random
import threading
import requests
import json
import LCD1602
from ADCDevice import *
from bottle import route, run, request, redirect, response


#Global Constants
DISPLAY_SCORE_MAX = 9999
IIC_BUS_NUMBER = 1
GPIO_JOYSTICK_Z_PIN = 12
JOYSTICK_THRESHOLD_MIN = 50
JOYSTICK_THRESHOLD_MAX = 250
DELAY_MS_100 = 0.10
GRID_WIDTH = 4
ADC_CHANNEL_X = 0
ADC_CHANNEL_Y = 1


#-----------------------------------------------------------------------------
# DESCRIPTION
# Initializes a 4x4 game grid with two random tiles (either 2 or 4) placed on
# the grid.
#
# RETURN:
#     grid - A 4x4 grid with two random tiles (either 2 or 4)
#-----------------------------------------------------------------------------
def new_game():
    
    #Create the 4x4 grid
    grid = [[0 for _ in range(GRID_WIDTH)] for _ in range(GRID_WIDTH)]
    
    #Add the initial 2 random tiles to the grid
    add_tile(grid)
    add_tile(grid)
    
    return grid

#-----------------------------------------------------------------------------
# DESCRIPTION
# Adds a new tile (either 2 or 4) to the grid in a random empty position.
#
# INPUT PARAMETERS:
#     grid - A 4x4 grid
#-----------------------------------------------------------------------------
def add_tile(grid):
    
    #Get random position for the new tile (target space must be empty)
    empty_tiles = [(i, j) for i in range(GRID_WIDTH) for j in range(GRID_WIDTH) if grid[i][j] == 0]
    i, j = random.choice(empty_tiles)
    
    #Add a 2 (or rarely a 4) to the target location
    grid[i][j] = 2 if random.random() < 0.9 else 4

#-----------------------------------------------------------------------------
# DESCRIPTION
# Merges tiles across a given line. It combines pairs of adjacent identical tiles
# and returns the merged line and the score gained from the merge.
#
# INPUT PARAMETERS:
#     line - A list of integers representing a row or column of the grid
#
# RETURN:
#     merged_line - A list of integers representing the merged line
#     newAddedScore - An integer representing the score gained from the merge
#-----------------------------------------------------------------------------
def merge_tiles(line):
    merged_line = [x for x in line if x != 0]
    newAddedScore = 0
    for i in range(len(merged_line) - 1):
        if merged_line[i] == merged_line[i + 1]:
            merged_value = merged_line[i] * 2
            merged_line[i] = merged_value
            merged_line[i + 1] = 0
            newAddedScore += merged_value
    merged_line = [x for x in merged_line if x != 0]
    return merged_line + [0] * (GRID_WIDTH - len(merged_line)), newAddedScore

#-----------------------------------------------------------------------------
# DESCRIPTION
# Moves and merges all the tiles to the left.
#
# INPUT PARAMETERS:
#     grid - A 4x4 grid
#
# RETURN:
#     new_grid - A 4x4 grid with the tiles moved and merged to the left
#     score - An integer representing the score gained from the merge
#-----------------------------------------------------------------------------
def move_left(grid):
        
    new_grid = []
    score = 0

    for row in grid:
        merged_row, row_score = merge_tiles(row)
        new_grid.append(merged_row)
        score += row_score

    return new_grid, score

#-----------------------------------------------------------------------------
# DESCRIPTION
# Moves and merges all the tiles to the right.
#
# INPUT PARAMETERS:
#     grid - A 4x4 grid
#
# RETURN:
#     new_grid - A 4x4 grid with the tiles moved and merged to the right
#     score - An integer representing the score gained from the merge
#-----------------------------------------------------------------------------
def move_right(grid):

    new_grid = []
    score = 0

    for row in grid:
        reversed_row = row[::-1]
        merged_row, row_score = merge_tiles(reversed_row)
        new_grid.append(merged_row[::-1])
        score += row_score

    return new_grid, score

#-----------------------------------------------------------------------------
# DESCRIPTION
# Moves and merges all the tiles upwards.
#
# INPUT PARAMETERS:
#     grid - A 4x4 grid
#
# RETURN:
#     new_grid - A 4x4 grid with the tiles moved and merged upwards
#     score - An integer representing the score gained from the merge
#-----------------------------------------------------------------------------
def move_up(grid):
    
    transposed_grid = list(map(list, zip(*grid)))
    new_transposed_grid, score = move_left(transposed_grid)
    return list(map(list, zip(*new_transposed_grid))), score

#-----------------------------------------------------------------------------
# DESCRIPTION
# Moves and merges all the tiles downwards.
#
# INPUT PARAMETERS:
#     grid - A 4x4 grid
#
# RETURN:
#     new_grid - A 4x4 grid with the tiles moved and merged downwards
#     score - An integer representing the score gained from the merge
#-----------------------------------------------------------------------------
def move_down(grid):
    
    transposed_grid = list(map(list, zip(*grid)))
    new_transposed_grid, score = move_right(transposed_grid)
    return list(map(list, zip(*new_transposed_grid))), score

#-----------------------------------------------------------------------------
# DESCRIPTION
#   Moves all the tiles in the grid in the specified direction and merges
#   tiles with the same value.
#
# INPUT PARAMETERS:
#   grid - a 4x4 list of integers representing the current game state
#   direction - a string representing the direction to move the tiles
#   ("up", "down", "left", or "right")
#
# RETURN:
#   The updated grid as a 4x4 list of integers with merged tiles and moved
#   tiles in the specified direction. If the grid is not updated, the function
#   returns the original grid.
#-----------------------------------------------------------------------------
def move(grid, direction):
    isGridUpdated = False
    newAddedScore = 0

    if   direction == 'a':
        new_grid, newAddedScore = move_left(grid)
    elif direction == 'd':
        new_grid, newAddedScore = move_right(grid)
    elif direction == 'w':
        new_grid, newAddedScore = move_up(grid)
    elif direction == 's':
        new_grid, newAddedScore = move_down(grid)

    isGridUpdated = any(old_row != new_row for old_row, new_row in zip(grid, new_grid))

    return new_grid, newAddedScore, isGridUpdated

#-----------------------------------------------------------------------------
# DESCRIPTION
#   This function checks if the game is over by verifying if there are any
#   empty tiles or adjacent identical tiles left on the grid.
#
# INPUT PARAMETERS:
#   grid - A 4x4 grid
#
# RETURN:
#   isGameOver - A boolean indicating whether the game is over or not
#-----------------------------------------------------------------------------
def is_game_over(grid):
    
    #Locate empty tiles (game is not over if one is found)
    for row in grid:
        for x, y in zip(row[:-1], row[1:]):
            if ((x == y) or (x == 0) or (y == 0)):
                return False
    for row in zip(*grid):
        for x, y in zip(row[:-1], row[1:]):
            if ((x == y) or (x == 0) or (y == 0)):
                return False
            
    #No empty tiles found, game is over
    return True

#-----------------------------------------------------------------------------
# DESCRIPTION
#   This function generates the HTML and JavaScript code for the game
#   interface, including the game grid, score display, game over message, and
#   input handling.
#
# RETURN:
#   html_string - A string containing the complete HTML and JavaScript code
#   for the game interface
#-----------------------------------------------------------------------------
@route('/')
def game_2048():
    game_over = is_game_over(GAME_STATE['grid'])

    grid_html = '<table>'
    for row in GAME_STATE['grid']:
        grid_html += '<tr>'
        for cell in row:
            grid_html += f'<td class="tile-{cell}">{cell if cell != 0 else ""}</td>'
        grid_html += '</tr>'
    grid_html += '</table>'

    # Add score and high score display
    score_html = f'<div>Score: {GAME_STATE["score"]}<br>High Score: {GAME_STATE["high_score"]}</div>'

    # Add game over message and instructions to try again
    game_over_html = f'<div id="game-over" style="display: {"block" if game_over else "none"};">Game Over!<br>Press [Space] on the keyboard or the Z-Axis on the joystick to try again</div>'
    
    #Define the grid and tiles (CSS)
    css = '''
    <style>
        table {
            border-collapse: collapse;
            font-family: Arial, sans-serif;
        }
        td {
            border: 1px solid #ccc;
            height: 50px;
            width: 50px;
            text-align: center;
            vertical-align: middle;
            font-size: 24px;
        }
        .tile-0 { background-color: #f4f4f4; }
        .tile-2 { background-color: #eee4da; color: #776e65; }
        .tile-4 { background-color: #ede0c8; color: #776e65; }
        .tile-8 { background-color: #f2b179; color: #f9f6f2; }
        .tile-16 { background-color: #f59563; color: #f9f6f2; }
        .tile-32 { background-color: #f67c5f; color: #f9f6f2; }
        .tile-64 { background-color: #f65e3b; color: #f9f6f2; }
        .tile-128 { background-color: #edcf72; color: #f9f6f2; }
        .tile-256 { background-color: #edcc61; color: #f9f6f2; }
        .tile-512 { background-color: #edc850; color: #f9f6f2; }
        .tile-1024 { background-color: #edc53f; color: #f9f6f2; }
        .tile-2048 { background-color: #edc22e; color: #f9f6f2; }
    </style>
    '''

    #Define input functions (JavaScript)
    js = '''
    <script>
        const XHR_READY_STATE_DONE = 4;
        const XHR_STATUS_OK        = 200;
        const OLD_GRID_STRING_DEFAULT = "";
        const TIMEOUT_DELAY_200_MS = 200;

        //
        // Create Event Listener to detect when WASD or Space have been pressed
        //
        document.addEventListener('keydown', function(event) {
        
            if(['w', 'a', 's', 'd'].includes(event.key.toLowerCase())) {
            
                event.preventDefault();
                let xhr = new XMLHttpRequest();
                xhr.open('POST', '/move');
                xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
                xhr.onload = function() {
                
                    if (xhr.status === XHR_STATUS_OK) {
                        location.reload();                        
                        }

                    };

                xhr.send('direction=' + event.key.toLowerCase());
            
                }

            // Handle spacebar event when game is over
            if (event.code === 'Space' && document.getElementById('game-over').style.display === 'block') {
            
                event.preventDefault();
                let xhr = new XMLHttpRequest();
                xhr.open('POST', '/restart');
                xhr.onload = function() {
                
                    if (xhr.status === XHR_STATUS_OK) {
                        location.reload();
                        }
                
                    };

                xhr.send();
                
                }
            
            });

    //-----------------------------------------------------------------------------
    // DESCRIPTION
    //   This function is used to continuously poll for the joystick input.
    //   The function ensures that a valid input is given, and will ensure that the
    //   new input actually caused a change in the game state before reloading the
    //   page.
    //-----------------------------------------------------------------------------   
    function pollJoystickInput() {
    
        let xhr = new XMLHttpRequest();
        xhr.open('GET', '/joystick_move');

        xhr.onreadystatechange = function() {
        
            if (xhr.readyState == XHR_READY_STATE_DONE && xhr.status == XHR_STATUS_OK) {
            
                var myArr = JSON.parse(xhr.responseText);
                
                let grid = JSON.stringify(myArr["grid"]);
                let oldGridString = localStorage.getItem('oldGridString') || OLD_GRID_STRING_DEFAULT;
                
                //Check if the new grid caused an update before reloading
                if (oldGridString != grid) {
                    localStorage.setItem('oldGridString', grid);
                    location.reload();
                    }

                setTimeout(pollJoystickInput, TIMEOUT_DELAY_200_MS);
            
                }
            
            }

            xhr.send();
        
        }
    pollJoystickInput();
    </script>
    '''

    return css + game_over_html + grid_html + score_html + js

#-----------------------------------------------------------------------------
# DESCRIPTION
#   Displays the updated score/high score on the LCD.
#   Limits the values to 4 digits (9999).
#-----------------------------------------------------------------------------
def lcd_score_update():
    
    LCD1602.clear()
    
    #Display LCD Score
    LCD1602.write(
        LCD1602.LCD_CHAR_POSITION_1,
        LCD1602.LCD_LINE_NUM_1,
        "Score: "+str(min(DISPLAY_SCORE_MAX, GAME_STATE['score']))
        )
    
    #Display LCD High Score
    LCD1602.write(
        LCD1602.LCD_CHAR_POSITION_1,
        LCD1602.LCD_LINE_NUM_2,
        "High Score: "+str(min(DISPLAY_SCORE_MAX, GAME_STATE['high_score']))
        )

#-----------------------------------------------------------------------------
# DESCRIPTION
#   Restarts the game by generating a new grid with the 'new_game' function
#   and resetting the score to 0.
#-----------------------------------------------------------------------------
@route('/restart', method='POST')
def restart_game():
    GAME_STATE['grid'] = new_game()
    GAME_STATE['score'] = 0
    lcd_score_update()
    
#-----------------------------------------------------------------------------
# DESCRIPTION
#   Handles requests for tile movement, subsequently updating the game state.
#   After the movement is processed, the grid, score, and high score will
#   be appropriately updated.
#
# INPUT PARAMETERS:
#   direction ('w' 'a' 's' or 'd')
#-----------------------------------------------------------------------------
@route('/move', method='POST')
def move_tiles(direction=None):

    #Fetch direction if none was provided already
    if not direction:
        direction = request.forms.get('direction')
    
    #Check if direction is valid
    if direction in ('w', 'a', 's', 'd'):

        #Update game data
        GAME_STATE['grid'], addedScore, moved = move(GAME_STATE['grid'], direction)
        
        if moved:
            add_tile(GAME_STATE['grid'])

            GAME_STATE['score'] += addedScore
            
            #Update High Score
            if GAME_STATE['score'] > GAME_STATE['high_score']:
                GAME_STATE['high_score'] = GAME_STATE['score']
            
            lcd_score_update()
                    
    if not direction: 
        redirect('/')

#-----------------------------------------------------------------------------
# DESCRIPTION
#   Sets up the ADC device and GPIO to add joystick compatibility.
#   Activates the pin connected to the joystick's z-axis so it can be used
#   as a button.
#
# RETURN:
#   adc
#-----------------------------------------------------------------------------
def io_setup():
    
    adc = ADCDevice()
    if (adc.detectI2C(0x48)):
        adc = PCF8591()
    if (adc.detectI2C(0x4B)):
        adc = ADS7830()
    else:
        print("No correct I2C address found");
        exit(-1)
    
    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(GPIO_JOYSTICK_Z_PIN, GPIO.IN, GPIO.PUD_UP)

    return adc

#-----------------------------------------------------------------------------
# DESCRIPTION
#   Closes the provided adc and cleans up the GPIO.
#
# INPUT PARAMETERS:
#   adc
#-----------------------------------------------------------------------------
def destroy(adc):
    adc.close()
    GPIO.cleanup()
    LCD1602.clear()
    LCD1602.display_off()

#-----------------------------------------------------------------------------
# DESCRIPTION
#   Closes the provided adc and cleans up the GPIO.
#-----------------------------------------------------------------------------
def handle_joystick_input():

    #Define 'static' variable to record previous direction inputs
    if not hasattr(handle_joystick_input, "directionPrev"):
        handle_joystick_input.directionPrev = None

    #Define 'static' variable for the joystick ADC
    if not hasattr(handle_joystick_input, "joystickADC"):
        handle_joystick_input.joystickADC = io_setup()

    while True:
        
        direction = None
        
        xAxisVal = handle_joystick_input.joystickADC.analogRead(ADC_CHANNEL_X)
        yAxisVal = handle_joystick_input.joystickADC.analogRead(ADC_CHANNEL_Y)

        #Push Z Axis Button (Use to start/restart the game?)
        if (not GPIO.input(GPIO_JOYSTICK_Z_PIN)):
            if is_game_over(GAME_STATE['grid']):
                requests.post('http://localhost:8080/restart')
        
        #Horizontal Axis
        elif (xAxisVal <= JOYSTICK_THRESHOLD_MIN):
            direction = 'a'
        elif (xAxisVal >= JOYSTICK_THRESHOLD_MAX):
            direction = 'd'

        #Vertical Axis
        elif (yAxisVal <= JOYSTICK_THRESHOLD_MIN):
            direction = 'w'
        elif (yAxisVal >= JOYSTICK_THRESHOLD_MAX):
            direction = 's'
            
        #Send an update if the joystick is in a different direction than it was previously
        if direction:
            if (direction!=handle_joystick_input.directionPrev):
                response = requests.get(f'http://localhost:8080/joystick_move?direction={direction}')
        
        handle_joystick_input.directionPrev = direction
        
        time.sleep(DELAY_MS_100)

#-----------------------------------------------------------------------------
# DESCRIPTION
#   Moves tiles and delivers the updated game data when in
#
# RETURN:
#   Dump (string) of the incoming JSON data
#-----------------------------------------------------------------------------
@route('/joystick_move', method=['GET', 'POST'])
def joystick_move_tiles():
    
    direction = request.forms.get('direction') or request.query.get('direction')
    
    if (direction != None):
        move_tiles(direction)
        
    game_data_cur = {
        'grid': GAME_STATE['grid'],
        'score': GAME_STATE['score'],
        'high_score': GAME_STATE['high_score'],
        'game_over': is_game_over(GAME_STATE['grid'])
        }
    response.content_type = 'application/json'
    return json.dumps(game_data_cur)


if (__name__ == '__main__'):
    
    print("Program initializing...")

    #Initialize Global Game Data
    GAME_STATE = {
        'grid' : new_game(),
        'score' : 0,
        'high_score': 0
        }

    try:
        
        LCD1602.init(LCD1602.LCD_IIC_ADDRESS, IIC_BUS_NUMBER)
        LCD1602.clear()

        handle_joystick_input.should_stop = False
        joystick_thread = threading.Thread(target=handle_joystick_input)
        joystick_thread.daemon = True
        joystick_thread.start()
        
        run(host='localhost', port=8080)

    except KeyboardInterrupt:
        pass
    
    print("Ending program...")
    handle_joystick_input.should_stop = True
    joystick_thread.join()
    destroy()
    os.kill(os.getpid(), signal.SIGTERM)
