#include <Arduino.h>
#include <Wire.h>
#include <Servo.h>
#include <tcs3200.h>
#include <DFRobot_VL53L0X.h>
#include <ctype.h>

#define SERVO_BASE_PIN p15
#define SERVO_RAISE_PIN p16
#define SERVO_LENGHT_PIN p17
#define SERVO_CLAW_PIN p19

#define ANGLE_NUM 180
#define MAX_DIST 800
#define MAX_NUM_PIECES 8
#define COLOUR_DIST 100

#define RED_PLACE 0
#define GREEN_PLACE 1
#define BLUE_PLACE 2


DFRobot_VL53L0X dist_sensor;

int distance_points_polar[ANGLE_NUM];
int distance_min[MAX_NUM_PIECES] = {2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000};
uint8_t min_base_pos[MAX_NUM_PIECES] = {0, 0, 0, 0, 0, 0, 0, 0}; 
uint8_t num_pieces = 0;

uint8_t piece_base[3] = {182, 182, 182};
uint8_t piece_length[3] = {182, 182, 182};
uint8_t piece_raise[3] = {182, 182, 182};

int distance_max = 0;
uint8_t max_base_pos = 0;

Servo servo_base, servo_raise, servo_lenght, servo_claw;

uint8_t base_last_angle, raise_last_angle, length_last_angle;

int red_value, green_value, blue_value;
tcs3200 tcs(p22, p26, p27, p28, p21); // (S0, S1, S2, S3, output pin)

enum colour {white, red, green, blue, black};

colour piece_colour = white;

enum fsm_state {
    begin_state, 
    scanning_state,
    printing_state,
    get_state,
    to_colour_state,
    colour_state,
    place_state,
    adjust_base_state,
    adjust_raise_state,
    adjust_length_state,
    stand_by_state,
    error_state
};

enum menu_fsm_state {
    menu_begin,
    menu_final_place,
    menu_red_place,
    menu_green_place,
    menu_blue_place,
    menu_select_num,
    menu_adjust,
    menu_colour,
    menu_get_piece,
    menu_place
};

fsm_state fsm = begin_state, next_fsm = begin_state;
menu_fsm_state menu_fsm = menu_begin, menu_next_fsm = menu_begin;
menu_fsm_state menu_flag_adjust = menu_begin;


char incomingByte = 'n';


/////////////////////////////////////////   MISCELANEOUS FUNCTIONS  /////////////////////////////////////////

colour check_colour(int r, int g, int b)
{
    if(r > g && r > b)
        return red;

    if(g > r && g > b)
        return green;

    if(b > r && b > g)
        return blue;

    return black;
}

void claw_bite(uint8_t bite)
{
    if(bite)
        servo_claw.write(50);
    else
        servo_claw.write(0);

    return;
}

void starting_place()
{
    servo_base.write(90);
    servo_raise.write(90);
    servo_lenght.write(85);

    base_last_angle = 90;
    raise_last_angle = 90;
    length_last_angle = 85;

    delay(15);
}


void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    //join i2c bus (address optional for master)
    Wire.begin();
    //Set I2C sub-device address
    dist_sensor.begin(0x50);
    //Set to Back-to-back mode and high precision mode
    dist_sensor.setMode(dist_sensor.eContinuous,dist_sensor.eHigh);
    //Laser rangefinder begins to work
    dist_sensor.start();

    servo_base.attach(SERVO_BASE_PIN);
    servo_raise.attach(SERVO_RAISE_PIN);
    servo_lenght.attach(SERVO_LENGHT_PIN);
    servo_claw.attach(SERVO_CLAW_PIN);
    
    servo_base.write(90);
    servo_raise.write(90);
    servo_lenght.write(85);
    servo_claw.write(0);

    base_last_angle = 90;
    raise_last_angle = 90;
    length_last_angle = 85;

    Serial.println("\n\n\n****________**** COLOUR SORTING SERVO ARM ****________****\n\n\n");
    //Serial.print("Select number of pieces to be picked up: ");

}

void loop()
{

    if (Serial.available() > 0)
    {
        incomingByte = Serial.read();

        if(incomingByte == '|')
        {
            Serial.print("\nFSM State: ");Serial.print(fsm);Serial.print("    MENU FSM: ");Serial.print(menu_fsm);Serial.print("\n");
            Serial.print("Servo read values\tBase: ");Serial.print(base_last_angle);Serial.print("  Raise: ");Serial.print(raise_last_angle);Serial.print("  Length: ");Serial.print(length_last_angle);Serial.print("\n");
            incomingByte = ' ';
        }
    }

    switch (menu_fsm)
    {
    case menu_begin:
        if(piece_base[0] == 182 || piece_base[1] == 182 || piece_base[2] == 182)
        {
            Serial.print("\n\nPlaces for the final state have not yet been selected.\n Press:\n- 'r' for the red piece\n- 'g' for the green piece\n- 'b' for the blue piece\n- 'k' to go to the next piece\n\n");
            menu_next_fsm = menu_final_place;
        }
        else if (incomingByte == 'r')
        {
            menu_next_fsm = menu_final_place;
        }
        else if(incomingByte != 'n')
        {   
            menu_next_fsm = menu_select_num;
        }
        break;
    
    case menu_final_place:
        if(incomingByte == 'r')
        {
            Serial.print("\nTo adjust the RED piece final position select:\n- 'b' for base motor\n- 'r' for raise motor\n- 'l' for length\n\nWhen finished, select 'q' to return.\n");
            menu_next_fsm = menu_red_place;
            incomingByte = ' ';
        }
        else if (incomingByte == 'g')
        {
            Serial.print("\nTo adjust the GREEN piece final position select:\n- 'b' for base motor\n- 'r' for raise motor\n- 'l' for length\n\nWhen finished, select 'q' to return.\n");
            menu_next_fsm = menu_green_place;
            incomingByte = ' ';
            starting_place();
        }
        else if (incomingByte == 'b')
        {
            Serial.print("\nTo adjust the BLUE piece final position select:\n- 'b' for base motor\n- 'r' for raise motor\n- 'l' for length\n\nWhen finished, select 'q' to return.\n");
            menu_next_fsm = menu_blue_place;
            incomingByte = ' ';
            starting_place();
        }
        else if(incomingByte == 'k')
        {
            Serial.print("\n\nPlacement locations:\n - RED piece --> base:"); Serial.print(piece_base[RED_PLACE]); Serial.print(" | raise: ");Serial.print(piece_raise[RED_PLACE]);Serial.print(" | length: ");Serial.print(piece_length[RED_PLACE]);
            Serial.print("\n - GREEN piece --> base:");Serial.print(piece_base[GREEN_PLACE]); Serial.print(" | raise: ");Serial.print(piece_raise[GREEN_PLACE]);Serial.print(" | length: ");Serial.print(piece_length[GREEN_PLACE]);
            Serial.print("\n - BLUE piece --> base:");Serial.print(piece_base[BLUE_PLACE]); Serial.print(" | raise: ");Serial.print(piece_raise[BLUE_PLACE]);Serial.print(" | length: ");Serial.print(piece_length[BLUE_PLACE]);
            menu_next_fsm = menu_begin;
            incomingByte = ' ';
            starting_place();
        }
        

        break;
    
    case menu_red_place:
        if(incomingByte == 'k')
        {
            menu_next_fsm = menu_final_place;
            incomingByte = 'g';
            piece_base[RED_PLACE] = base_last_angle;
            piece_raise[RED_PLACE] = raise_last_angle;
            piece_length[RED_PLACE] = length_last_angle;
        }
        else if (incomingByte == 'b')
        {
            Serial.print("\nAdjust BASE Servo     '+' to increase angle //// '-' to decrease angle  //// 'q' to quit\n");
            menu_flag_adjust = menu_red_place;
            next_fsm = adjust_base_state;
            menu_next_fsm = menu_adjust;
        }
        else if (incomingByte == 'r')
        {
            Serial.print("\nAdjust RAISE Servo     '+' to increase angle //// '-' to decrease angle  //// 'q' to quit\n");
            menu_flag_adjust = menu_red_place;
            next_fsm = adjust_raise_state;
            menu_next_fsm = menu_adjust;
        }
        else if (incomingByte == 'l')
        {
            Serial.print("\nAdjust LENGTH Servo     '+' to increase angle //// '-' to decrease angle  //// 'q' to quit\n");
            menu_flag_adjust = menu_red_place;
            next_fsm = adjust_length_state;
            menu_next_fsm = menu_adjust;
        }
        break;
    
    case menu_green_place:
        if(incomingByte == 'k')
        {
            menu_next_fsm = menu_final_place;
            incomingByte = 'b';
            piece_base[GREEN_PLACE] = base_last_angle;
            piece_raise[GREEN_PLACE] = raise_last_angle;
            piece_length[GREEN_PLACE] = length_last_angle;
            Serial.print("\nPress 'k' to continue\n");
        }
        else if (incomingByte == 'b')
        {
            Serial.print("\nAdjust BASE Servo     '+' to increase angle //// '-' to decrease angle  //// 'q' to quit\n");
            menu_flag_adjust = menu_green_place;
            next_fsm = adjust_base_state;
            menu_next_fsm = menu_adjust;
        }
        else if (incomingByte == 'r')
        {
            Serial.print("\nAdjust RAISE Servo     '+' to increase angle //// '-' to decrease angle  //// 'q' to quit\n");
            menu_flag_adjust = menu_green_place;
            next_fsm = adjust_raise_state;
            menu_next_fsm = menu_adjust;
        }
        else if (incomingByte == 'l')
        {
            Serial.print("\nAdjust LENGTH Servo     '+' to increase angle //// '-' to decrease angle  //// 'q' to quit\n");
            menu_flag_adjust = menu_green_place;
            next_fsm = adjust_length_state;
            menu_next_fsm = menu_adjust;
        }
        break;

    case menu_blue_place:
        if(incomingByte == 'k')
        {
            menu_next_fsm = menu_final_place;
            incomingByte = ' ';
            piece_base[BLUE_PLACE] = base_last_angle;
            piece_raise[BLUE_PLACE] = raise_last_angle;
            piece_length[BLUE_PLACE] = length_last_angle;
        }
        else if (incomingByte == 'b')
        {
            Serial.print("\nAdjust BASE Servo     '+' to increase angle //// '-' to decrease angle  //// 'q' to quit\n");
            menu_flag_adjust = menu_blue_place;
            next_fsm = adjust_base_state;
            menu_next_fsm = menu_adjust;
        }
        else if (incomingByte == 'r')
        {
            Serial.print("\nAdjust RAISE Servo     '+' to increase angle //// '-' to decrease angle  //// 'q' to quit\n");
            menu_flag_adjust = menu_blue_place;
            next_fsm = adjust_raise_state;
            menu_next_fsm = menu_adjust;
        }
        else if (incomingByte == 'l')
        {
            Serial.print("\nAdjust LENGTH Servo     '+' to increase angle //// '-' to decrease angle  //// 'q' to quit\n");
            menu_flag_adjust = menu_blue_place;
            next_fsm = adjust_length_state;
            menu_next_fsm = menu_adjust;
        }
        break;

    case menu_select_num:
        if(!isdigit(incomingByte))
        {
            Serial.print("\nInput a valid integer (between 1 and ");Serial.print(MAX_NUM_PIECES); Serial.println(")!\n");
            Serial.print("\n\nSelect number of pieces to be picked up: ");
            incomingByte = 'n';
            menu_next_fsm = menu_begin;
        }
        else
        {
            char read_str[2];
            read_str[0] = incomingByte;
            read_str[1] = '\0';
            num_pieces = atoi(read_str);

            if((num_pieces > 0) && (num_pieces <= MAX_NUM_PIECES))
            {
                Serial.print("\nTo adjust servo motor position select:\n- 'b' for base motor\n- 'r' for raise motor\n- 'l' for length\n\nOtherwise, select 'p'\n");
                menu_next_fsm = menu_get_piece;
                next_fsm = scanning_state;
                Serial.println(num_pieces);
            }
            else
            {
                Serial.print("\nInput a valid integer (between 1 and ");Serial.print(MAX_NUM_PIECES); Serial.println(")!\n");
                Serial.print("\n\nSelect number of pieces to be picked up: ");
                incomingByte = 'n';
                menu_next_fsm = menu_begin;
            }
        }
        break;
    
    case menu_get_piece:

        if(incomingByte == 'p')
        {
            next_fsm = to_colour_state;
            Serial.print("\nTo adjust servo motor position select:\n- 'b' for base motor\n- 'r' for raise motor\n- 'l' for length\n\nOtherwise, select 'c' to check colour.\n");
            menu_next_fsm = menu_colour;
        }
        else if (incomingByte == 'b')
        {
            Serial.print("\nAdjust BASE Servo     '+' to increase angle //// '-' to decrease angle  //// 'q' to quit\n");
            menu_flag_adjust = menu_get_piece;
            next_fsm = adjust_base_state;
            menu_next_fsm = menu_adjust;
        }
        else if (incomingByte == 'r')
        {
            Serial.print("\nAdjust RAISE Servo     '+' to increase angle //// '-' to decrease angle  //// 'q' to quit\n");
            menu_flag_adjust = menu_get_piece;
            next_fsm = adjust_raise_state;
            menu_next_fsm = menu_adjust;
        }
        else if (incomingByte == 'l')
        {
            Serial.print("\nAdjust LENGTH Servo     '+' to increase angle //// '-' to decrease angle  //// 'q' to quit\n");
            menu_flag_adjust = menu_get_piece;
            next_fsm = adjust_length_state;
            menu_next_fsm = menu_adjust;
        }
        
        break;
    
    case menu_colour:
        if(incomingByte == 'c')
        {
            next_fsm = colour_state;
        }
        else if (incomingByte == 'k' && piece_colour != white)
        {
            menu_next_fsm = menu_place;
            next_fsm = place_state;
            Serial.print("\nPlacing piece...\nPress 'k' to return\n");
            incomingByte = ' ';
        }
        else if (incomingByte == 'b')
        {
            Serial.print("\nAdjust BASE Servo     '+' to increase angle //// '-' to decrease angle  //// 'q' to quit\n");
            menu_flag_adjust = menu_colour;
            next_fsm = adjust_base_state;
            menu_next_fsm = menu_adjust;
        }
        else if (incomingByte == 'r')
        {
            Serial.print("\nAdjust RAISE Servo     '+' to increase angle //// '-' to decrease angle  //// 'q' to quit\n");
            menu_flag_adjust = menu_colour;
            next_fsm = adjust_raise_state;
            menu_next_fsm = menu_adjust;
        }
        else if (incomingByte == 'l')
        {
            Serial.print("\nAdjust LENGTH Servo     '+' to increase angle //// '-' to decrease angle  //// 'q' to quit\n");
            menu_flag_adjust = menu_colour;
            next_fsm = adjust_length_state;
            menu_next_fsm = menu_adjust;
        }

        break;

    case menu_place:
        if(incomingByte == 'k')
        {
            incomingByte = ' ';
            menu_next_fsm = menu_begin;
            starting_place();
            Serial.print("\n\n\n****________**** COLOUR SORTING SERVO ARM ****________****\n\n\nSelect number of pieces to be picked up: ");
        }
        break;
    
    case menu_adjust:
        if(incomingByte == 'q')
        {
            menu_next_fsm = menu_flag_adjust;
            incomingByte = ' ';
        }
        break;
    default:
        Serial.println("//////////////////////  MENU FSM DEFAULT STATE ERROR  //////////////////////");
        break;
    }

    switch (fsm)
    {
    case begin_state:
        break;

    case scanning_state:
    // scan for pieces
        //Serial.println("\nScanning for pieces...");
        for(uint8_t pos = 30; pos < ANGLE_NUM; pos++)
        {
            servo_base.write(pos);
            base_last_angle = pos;
            delay(10);

            // read dist_sensor
            while (1)
            {
                if((dist_sensor.getDistance() != 0) && (dist_sensor.getDistance() > 10))
                {
                    break;
                }
            }
            
            distance_points_polar[pos] = dist_sensor.getDistance();

            // if minimum, update distance_min and save position of piece
            for(uint8_t i = 0; i < num_pieces; i++) //potencialmente completamente horrível
            {    
                if(distance_points_polar[pos] < distance_min[i])
                {
                    distance_min[i] = distance_points_polar[pos];
                    min_base_pos[i] = pos;
                }
            }

            if(distance_points_polar[pos] > distance_max)
            {
                distance_max = distance_points_polar[pos];
                max_base_pos = pos;
            }

            //Serial.print("Distance: ");Serial.println(distance_points_polar[pos]);
        }
        next_fsm = printing_state;

        break;
    
    case printing_state:
        //Serial.println("\nPrinting scan...");
        
        // for(uint16_t i = 0; i < 180; i++)
        // {
        //     Serial.print(distance_points_polar[i]); Serial.print(" ");
        // }

        Serial.print("\n\n\n");
        for(uint16_t i = 0; i < num_pieces; i++)
        {
            Serial.print("\nMin: ");Serial.print(distance_min[i]);Serial.print("  Angle: ");Serial.println(min_base_pos[i]);
        }
        Serial.print("Max: "); Serial.println(distance_max);

        next_fsm = get_state;
        
        break;

    case get_state:
        //get piece

        for(uint8_t i = 0; i < num_pieces; i++)
        {
            claw_bite(0);
            delay(15);
            servo_base.write(min_base_pos[i]);
            base_last_angle = min_base_pos[i];
            delay(200);
            servo_lenght.write(180*distance_min[i]/150 - 20);
            length_last_angle = 180*distance_min[i]/150 - 20;
            delay(15);
            servo_raise.write(90);
            raise_last_angle = 90;
            delay(15);
        }


        break;
    
    case to_colour_state:
        
        for(uint8_t i = 0; i < num_pieces; i++)
        {
            claw_bite(1);
            delay(300);
            servo_base.write(0);
            base_last_angle = 0;
            delay(200);
            servo_lenght.write(180*COLOUR_DIST/150 + 30);
            length_last_angle = 180*COLOUR_DIST/150 + 30;
            delay(15);
            servo_raise.write(180);
            raise_last_angle = 180;
            delay(15);
        }
        break;
    
    case colour_state:
        //check colour
        red_value = tcs.colorRead('r');   //reads color value for red_value
        green_value = tcs.colorRead('g');   //reads color value for green_value
        blue_value = tcs.colorRead('b');    //reads color value for blue_value

        piece_colour = check_colour(red_value, green_value, blue_value);

        if(incomingByte == 'c')
        {
            switch (piece_colour)
            {
            case red:
                Serial.println("Red piece picked up");
                break;

            case green:
                Serial.println("Green piece picked up");
                break;

            case blue:
                Serial.println("Blue piece picked up");
                break;

            case black:
                Serial.println("Black piece picked up");
                break;

            case white:
                Serial.println("Error: Colour sensor error");
                break;

            default:
                break;
            }
            incomingByte = ' ';
        }

        break;
    
    case place_state:

        servo_raise.write(110);
        delay(200);
        servo_lenght.write(90);
        raise_last_angle = 90;
        length_last_angle = 90;
        delay(200);

        // Serial.print("PIECE COLOUR:  ");Serial.println(piece_colour);
        
        switch (piece_colour)
        {
        case red:
            servo_base.write(piece_base[RED_PLACE]);
            delay(200);
            servo_raise.write(piece_raise[RED_PLACE]);
            delay(200);
            servo_lenght.write(piece_length[RED_PLACE]);
            delay(200);
            claw_bite(0);

            base_last_angle = piece_base[RED_PLACE];
            raise_last_angle = piece_raise[RED_PLACE];
            length_last_angle = piece_length[RED_PLACE];
            delay(1000);
            starting_place();
            next_fsm = begin_state;
            break;
        
        case green:
            servo_base.write(piece_base[GREEN_PLACE]);
            delay(200);
            servo_raise.write(piece_raise[GREEN_PLACE]);
            delay(200);
            servo_lenght.write(piece_length[GREEN_PLACE]);
            delay(200);
            claw_bite(0);

            base_last_angle = piece_base[GREEN_PLACE];
            raise_last_angle = piece_raise[GREEN_PLACE];
            length_last_angle = piece_length[GREEN_PLACE];
            delay(1000);
            starting_place();
            next_fsm = begin_state;
            break;
        
        case blue:
            servo_base.write(piece_base[BLUE_PLACE]);
            delay(200);
            servo_raise.write(piece_raise[BLUE_PLACE]);
            delay(200);
            servo_lenght.write(piece_length[BLUE_PLACE]);
            delay(200);
            claw_bite(0);

            base_last_angle = piece_base[BLUE_PLACE];
            raise_last_angle = piece_raise[BLUE_PLACE];
            length_last_angle = piece_length[BLUE_PLACE];
            delay(1000);
            starting_place();
            next_fsm = begin_state;
            break;
        
        default:
            Serial.print("Other colours are not supported.\n");
            break;
        }
        
        break;

    case adjust_base_state:
        if(incomingByte == '+')
        {
            if(base_last_angle < 180)
            {
                base_last_angle++;
                servo_base.write(base_last_angle);
                delay(15);
                incomingByte = ' ';
            }
        }
        
        if(incomingByte == '-')
        {
            if(base_last_angle > 0)
            {
                base_last_angle--;
                servo_base.write(base_last_angle);
                delay(15);
                incomingByte = ' ';
            }
        }
        
        if(incomingByte == 'q')
        {
            next_fsm = stand_by_state;
            incomingByte = ' ';
        }

        break;
    case adjust_raise_state:
        if(incomingByte == '+')
            {
                if(raise_last_angle < 180)
                {
                    raise_last_angle++;
                    servo_raise.write(raise_last_angle);
                    delay(15);
                    incomingByte = ' ';
                }
            }

            if(incomingByte == '-')
            {
                if(raise_last_angle > 0)
                {
                    raise_last_angle--;
                    servo_raise.write(raise_last_angle);
                    delay(15);
                    incomingByte = ' ';
                }
            }

            if(incomingByte == 'q')
            {
                next_fsm = stand_by_state;
                incomingByte = ' ';
            }
        break;
    case adjust_length_state:
        if(incomingByte == '+')
        {
            if(length_last_angle < 180)
            {
                length_last_angle++;
                servo_lenght.write(length_last_angle);
                delay(15);
                incomingByte = ' ';
            }
        }
        
        if(incomingByte == '-')
        {
            if(length_last_angle > 0)
            {
                length_last_angle--;
                servo_lenght.write(length_last_angle);
                delay(15);
                incomingByte = ' ';
            }
        }
        
        if(incomingByte == 'q')
        {
            next_fsm = stand_by_state;
            incomingByte = ' ';
        }
        
        break;

    case stand_by_state:
        break;

    case error_state:
        Serial.println("//////////////////////  FSM ERROR STATE  //////////////////////");
        while(1)
            ;
        break;

    default:
        Serial.println("//////////////////////  FSM DEFAULT STATE ERROR  //////////////////////");
        while(1)
            ;
        break;
    }

    if (next_fsm != fsm)
    {
        fsm = next_fsm;
    }

    if(menu_next_fsm != menu_fsm)
    {
        menu_fsm = menu_next_fsm;
    }
}