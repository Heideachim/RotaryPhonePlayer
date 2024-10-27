#undef USE_DISPLAY

#ifdef USE_DISPLAY
  #include <Adafruit_GFX.h>
  #include <Adafruit_SH110X.h>
  #include <fonts/FreeMonoBold18pt7b.h>
#endif


#ifdef USE_DISPLAY
  Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);

  void init_display(void) {

    delay(250); // wait for the OLED to power up
    display.begin(0x3C, true); // Address 0x3C default  
    delay(200);
    display.display();  // show adafruit logo
    display.clearDisplay();
    display.setRotation(1);
    display.setFont(&FreeMonoBold18pt7b);

  // text display tests
    display.setTextSize(3);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(1,64);
    display.print("ah");
    display.display(); // actually display all of the above  
  }

  void display_number(int number) {
    display.clearDisplay();
    display.setCursor(5, 64);
    display.print(number);
    display.display(); // actually display all of the above  
  }
#else
    void init_display() {}
    void display_number(int number) {}
#endif

