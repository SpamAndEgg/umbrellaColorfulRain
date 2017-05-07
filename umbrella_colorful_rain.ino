// LED UMBRELLA "COLORFUL RAIN".
// This project code is for building an LED umbrella with WS2812 LEDs. A choseable number of LED stripes are controlled 
// to display different animations, further called modes. A button allows cycling through the 6 different modes. In the 
// original umbrella project, 8 LED stripes, each with 30 pixels, were were guled on each rib of the umbrella. Two 
// Arduino Pro Mini were used to control the 8 stripes, while one worked unstable controlling 8 stripes. The power
// supply was realized by an ordinary power bank. A video of the project can be found at https://youtu.be/2npH8hMX6MA.
//
// I tested this code also with an ATtiny85, it works but only one LED stripe could be controlled.
//
// For comments or questions please contact me at marlonalexander.fleck@gmail.com. 

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

// --------------------------------------------------------------------------------------------------------------------------
// -------------------------CONSTANTS----------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------
// Number of pixel on each stripe.
#define N_PIXEL 30
// Number of stripes controlled by one controller.
#define N_STRIPE 3
// Number of drops parallel for the raindrop animation.
#define N_DROP 4
// Define the pin for the button to cycle through the different modes.
#define BUTTON_PIN 13
// Set the number of available modes that can be cycled through with the button.
#define N_MODE 6
// The controller offset is used for the spiral and lantern animation. The animation uses an offset of illuminated time for  
// the different stripes. If multiple controllers are used for one object, an offset between the controllers can be set by
// "CONTROLLER_OFFSET". The offset is added to the stripe number (e.g. if you use 1 controller for 2 stripes, set the offset
// to 0 for the first controller, to 2 for the second controller, to 4 for the third controller and so on).
#define CONTROLLER_OFFSET 1
//
#define N_SPIRAL_PIXEL 5
#define SPIRAL_DISTANCE 10
#define SPIRAL_ANIMATION_LENGTH 20

// --------------------------------------------------------------------------------------------------------------------------
// -------------------------CLASSES------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------
class Color {
  public:
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// Declare function here while it is used in the class "Drop".
void set_drop(Color* pixel_buffer, int drop_position, uint8_t drop_length, Color color);
// Inititate the mode while it is needed by the class "Drop".
uint8_t mode = 0;

class Drop {
  public:
    uint8_t position;
    uint8_t length;
    Color color;
    uint16_t delay;
    uint16_t delay_counter;
    Color *pixel_buffer;

    Drop()
      : position(0)
      , length(0)
      , color( {
      0xFF, 0xFF, 0xFF
    })
    , delay(0)
    , delay_counter(0)
    , pixel_buffer(0)
    {}

    Drop(Color *pb) {
      // Hand this drop a pointer for the pixel buffer of the stripe the drop is on.
      pixel_buffer = pb;
      random_init();
    }

    void random_init() {
      delay_counter = random(0, 15);
      delay = random(1, 3);
      length = random(2, 5);
      position = 0;
      if (mode == 0) {
        // If the mode is 0 the raindrops will be in colors.
        uint8_t switch_color = random(0, 7);
        switch (switch_color) {
          case 1:
            color.r = 200;
            color.g = 0;
            color.b = 0;
            break;
          case 2:
            color.r = 200;
            color.g = 200;
            color.b = 0;
            break;
          case 3:
            color.r = 200;
            color.g = 0;
            color.b = 200;
            break;
          case 4:
            color.r = 0;
            color.g = 200;
            color.b = 0;
            break;
          case 5:
            color.r = 0;
            color.g = 200;
            color.b = 200;
            break;
          case 6:
            color.r = 0;
            color.g = 0;
            color.b = 200;
            break;
        }
      }
      else {
        // If the mode is not 0 (mode 1) the raindrops will be white.
        color.r = 200;
        color.g = 200;
        color.b = 200;
      }
    }



    void update_drop() {
      // If the delay counter is on, reduce it by one, otherwise update the drop position.
      if (delay_counter != 0) {
        delay_counter--;
        return;
      }
      position++;
      delay_counter = delay;
      // Initiate the drop again if it not on the LED stripe anymore
      if (position > N_PIXEL + length) {
        random_init();
      }
    }

    void paint() {
      set_drop(pixel_buffer, position, length, color);
    }
};

// --------------------------------------------------------------------------------------------------------------------------
// -------------------------INITIALIZATION-----------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------
// Set up the stripes.
// Parameter 1 = number of pixels in pixels
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
// The order of the colors is RED, GREEN, BLUE.
Adafruit_NeoPixel led_stripe[] {
  Adafruit_NeoPixel(N_PIXEL, 2, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(N_PIXEL, 5, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(N_PIXEL, 8, NEO_GRB + NEO_KHZ800),
};

// Initiate an array for the drops for every stripe.
Drop drop[N_STRIPE][N_DROP];
// Initiate the pixel buffer, in which every pixel for every stripe is stored.
Color pixel_buffer[N_STRIPE][N_PIXEL];
// Initiate the last button state.
boolean last_button_state = false;
// Initiate the frame counter for the spiral and stroboscope effect.
uint16_t frame_counter = 0;
// Initiate the animation step for the spiral effect.
uint8_t animation_counter = 0;
// The max. increment for the stroboscope effect is the square root of the number of pixels on each stripe.
int stroboscope_increment_max = sqrt(N_PIXEL)+5;
// A boolean value that tracks if the latern mode was already active the last loop iteration.
boolean is_lantern = false;
// A start pixel on the stripes for the lantern mode will be chosen randomly to avoid an overuse of the lantern pixels.
uint16_t lantern_start_pixel;

// --------------------------------------------------------------------------------------------------------------------------
// -------------------------SETUP--------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
#if defined (__AVR_ATtiny85__)
  if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif
  // Set a random seed by reading the analog signal from an unused pin.
  randomSeed(analogRead(0));
  // Set the button pin as an input.
  pinMode(BUTTON_PIN, INPUT);
  // Initiate the LED stripes.
  for (uint16_t i_stripe = 0; i_stripe < N_STRIPE; i_stripe++) {
    led_stripe[i_stripe].begin();
    // Initiate the drops for this stripe.
    for (uint16_t i_drop = 0; i_drop < N_DROP; i_drop++) {
      drop[i_stripe][i_drop] = Drop((Color*)&pixel_buffer[i_stripe]);
    }
  }

}

// --------------------------------------------------------------------------------------------------------------------------
// -------------------------LOOP---------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------
void loop() {
  // Check if the button was pressed.
  bool button_state = get_button_state();
  if (button_state == HIGH) {
    mode = (mode + 1) % N_MODE;
  }

  // Call the function for the current mode.
  switch (mode) {
    case 0: led_rain(); is_lantern = false; break;
    case 1: led_rain(); break;
    case 2: led_spiral(); break;
    case 3: led_spiral(); break;
    case 4: led_stroboscope(); break;
    case 5: 
      // Check if the lantern mode was just chosen. 
      if (!is_lantern) {
        // Initiate a random start pixel as the illuminated pixel for the first stripe. This is done so the LEDs, that might
        // have long running times in latern mode, change randomly. If you use multiple controllers, this random number will 
        // interfere with the lantern offset. In this case you can set this value to a fixed number.
        lantern_start_pixel = random(0,3);
        is_lantern = true;
      }
      led_lantern(); delay(50); break;
  }

  // Update the LED pixel buffer.
  for (uint16_t i_stripe = 0; i_stripe < N_STRIPE; i_stripe++) {
    set_pixel_buffer(pixel_buffer[i_stripe], i_stripe);
  }

  // Write the pixel buffer to the LEDs.
  for (uint16_t i_stripe = 0; i_stripe < N_STRIPE; i_stripe++) {
    led_stripe[i_stripe].show();
  }
  delay(1);
}

// --------------------------------------------------------------------------------------------------------------------------
// -------------------------FUNCTIONS----------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------

// ################# GET BUTTON STATE ########################### (CHECKED)
bool get_button_state() {
  bool button_state = digitalRead(BUTTON_PIN);
  delay(10);
  // The following checks if a "HIGH" state lasts longer, otherwise it could be bouncing effects from releasing the button.
  if (button_state != last_button_state) {
    button_state = digitalRead(BUTTON_PIN);
  }
  // The following check prevents a pushed and hold button to cycle though more than one mode.
  if (last_button_state == button_state) {
    return false;
  }
  last_button_state = button_state;
  return button_state;
}

// ################# SET PIXEL BUFFER ########################### (CHECKED)
void set_pixel_buffer(Color* pixel_buffer, uint16_t this_stripe) {
  for (uint16_t i_pixel = 0; i_pixel < N_PIXEL; i_pixel++) {
    // Prevent the pixel ilumination value to exceed the max 255.
    pixel_buffer[i_pixel].r = min(pixel_buffer[i_pixel].r, 255);
    pixel_buffer[i_pixel].g = min(pixel_buffer[i_pixel].g, 255);
    pixel_buffer[i_pixel].b = min(pixel_buffer[i_pixel].b, 255);
    // Set the pixel on this stripe.
    led_stripe[this_stripe].setPixelColor(i_pixel, led_stripe[this_stripe].Color(pixel_buffer[i_pixel].r, 
      pixel_buffer[i_pixel].g, pixel_buffer[i_pixel].b));
    // After the pixel has been set the buffer is set back to zero.
    pixel_buffer[i_pixel].r = 0;
    pixel_buffer[i_pixel].g = 0;
    pixel_buffer[i_pixel].b = 0;
  }
}

// ################# SET DROP ########################### (CHECKED)
void set_drop(Color* pixel_buffer, int drop_position, uint8_t drop_length, Color color) {
  // Here a drop is set. The drop effect is realized by reducing the brightness from the lowest pixel on to the top.
  // Special cases for when the drop exceeds the LED stripe to the top or bottom have to be consindered.

  // First, the visible part on the stripe is defined.
  // The top pixel of the drop that can be seen (If all are seen, this is 0)
  uint8_t dropindex_top;
  // The bottom pixel of the drop that can be seen (If all are seen, this is the drop length)
  uint8_t dropindex_bot;
  if (drop_position < drop_length) {
    // Case: The drop exceeds the LED stripe at the top, thus only the visible pixels at the drop bottom have to be shown.
    dropindex_top = 0;
    dropindex_bot = drop_position;
  }
  else if (drop_position > N_PIXEL) {
    // Case: The drop exceeds the LED stripe at the bottom, thus only the upper drop pixels are visible.
    dropindex_top = drop_position - N_PIXEL;
    dropindex_bot = drop_length;
  }
  else {
    // Case: The whole drop can be displayed.
    dropindex_top = 0;
    dropindex_bot = drop_length;
  }
  // Go through every drop pixel and adjust the brightness. The bottom pixel will have full brightness.
  for (uint16_t i_droppixel = dropindex_top; i_droppixel < dropindex_bot; i_droppixel++) {
    float brightness_multiplier = 0;
    // If bottom pixel set to full brightness.
    if (i_droppixel == 0) {
      brightness_multiplier = 1;
    }
    // If not the bottom pixel lower brightness, the more up the pixel the lower the brightness.
    else {
      brightness_multiplier = 0.8 / sq(i_droppixel) / drop_length;
    }

    uint8_t R_adjust = color.r * brightness_multiplier;
    uint8_t G_adjust = color.g * brightness_multiplier;
    uint8_t B_adjust = color.b * brightness_multiplier;
    // Update the pixel buffer.
    pixel_buffer[drop_position - i_droppixel - 1].r += R_adjust;
    pixel_buffer[drop_position - i_droppixel - 1].g += G_adjust;
    pixel_buffer[drop_position - i_droppixel - 1].b += B_adjust;
  }
}

// ################# LED RAIN ########################### (CHECKED)
void led_rain() {
  for (uint16_t i_stripe = 0; i_stripe < N_STRIPE; i_stripe++) {
    for (uint16_t i_drop = 0; i_drop < N_DROP; i_drop++) {
      drop[i_stripe][i_drop].paint();
      drop[i_stripe][i_drop].update_drop();
    }
  }
}

// ################# SET SPIRAL ###########################
void set_spiral(uint16_t this_stripe, uint16_t this_point_position) {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  if (mode == 2) {
    // In mode 2 the spiral effect will be in rainbow colors.

    // The rainbow color spectrum is obtained in 5 different sectors, each with its own RGB function to determin the
    // right RGB values. Thus the stripe is split into 5 sectors.
    int stripe_sector = this_point_position * 5 / N_PIXEL;
    // Calculate where within one sector the point lies.
    int sector_position = (this_point_position + 1) % (N_PIXEL / 6);

    switch (stripe_sector) {
      case 0:
        red = 255;
        green = sector_position * 255 / N_PIXEL * 5;
        blue = 0;
        break;
      case 1:
        red = 255 - sector_position * 200 / N_PIXEL;
        green = 255;
        blue = 0;
        break;
      case 2:
        red = 0;
        green = 255;
        blue = sector_position * 200 / N_PIXEL * 5;
        break;
      case 3:
        red = 0;
        green = 255 - sector_position * 200 / N_PIXEL;
        blue = 255;
      case 4:
        red = sector_position * 200 / N_PIXEL * 5;
        green = 0;
        blue = 255;
        break;
      //case 5:
      //  red = 255;
      //  green = 0;
      //  blue = 255;
      //  break;
      default:
        red = 0xff;
        green = 0xff;
        blue = 0xff;
    }
  }
  else {
    // If the current mode is not 2, the spiral will be given out in the color red.
    red = 0xff;
    green = 0x00;
    blue = 0x00;
  }
  // Set the spiral animation as a drop with varrying length
  set_drop(pixel_buffer[this_stripe], this_point_position, 3 - 
    cos((animation_counter % SPIRAL_ANIMATION_LENGTH) * 2 * PI / SPIRAL_ANIMATION_LENGTH) * 2, {red, green , blue});
}

// ################# LED SPIRAL ###########################
void led_spiral() {
  for (uint16_t i_stripe = 0; i_stripe < N_STRIPE; i_stripe++) {
    // The bouncy effect of the spiral is realized by only showing it 1/4 of the time with an offset between stripes
    // and, if used and set up, different controllers.
    if (!((frame_counter + i_stripe + CONTROLLER_OFFSET) % 4)) {
      // Set up the spiral effect points to occur with an offset defined by "SPIRAL_DISTANCE" on one stripe. Each point
      // will expand to the top and collapse again.
      for (uint16_t i_point = 0; i_point < N_PIXEL / SPIRAL_DISTANCE; i_point++) {
        // The effect of a downwards spiral is realized by adding the frame counter to the start pixel of the effect.
        set_spiral(i_stripe, (frame_counter + i_point * SPIRAL_DISTANCE) % N_PIXEL);
      }
    }
  }
  // Update the animation step.
  animation_counter++;
  // If end of animation is reached, the frame counter is updated.
  if (!(animation_counter % (SPIRAL_ANIMATION_LENGTH - 1))) {
    frame_counter++;
    // Reset the animation counter for a new animation.
    animation_counter = 0;
  }
}

// ################# LED STROBOSCOPE #########################
// In the stroboscope animation a few LEDs on each stripe chosen randomly are set to maximum white.
void led_stroboscope() {
  for (uint16_t i_stripe = 0; i_stripe < N_STRIPE; i_stripe++) {
    // Set every second stripe to black. This strengenth the stroboscope effect
    if (frame_counter % 2) {
      frame_counter++;
      break;
    }
    uint16_t pixel_counter = 0;
    for (uint16_t i_pixel = 0; i_pixel < N_PIXEL; i_pixel++) {
      // A random increment for the next pixel to be illuminated is computed.
      pixel_counter += random(0, stroboscope_increment_max);
      // If the next pixel to illuminate is not on the LED stripe anymore the loop is exited.
      if (pixel_counter >= N_PIXEL) {
        break;
      }
      pixel_buffer[i_stripe][pixel_counter].r = 200;
      pixel_buffer[i_stripe][pixel_counter].g = 200;
      pixel_buffer[i_stripe][pixel_counter].b = 200;
    }
  }
}

// ################# LED LANTERN ###########################
// This mode will illuminate every third LED in a yellowish light as a steady light source.
void led_lantern() {
  for (uint16_t i_stripe = 0; i_stripe < N_STRIPE; i_stripe++) {
    // The illuminated LEDs will have an offset of 1 from stripe to stripe.
    for (uint16_t i_pixel = (i_stripe + lantern_start_pixel + CONTROLLER_OFFSET) % 3; i_pixel < N_PIXEL; i_pixel = i_pixel+ 3) {
      pixel_buffer[i_stripe][i_pixel].r = 200;
      pixel_buffer[i_stripe][i_pixel].g = 200;
      pixel_buffer[i_stripe][i_pixel].b = 0;
    }
  }
}












