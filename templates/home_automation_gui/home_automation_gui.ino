/*
  This program is a template GUI for hardware prototyping.
  This is intended to allow rapid implementation of a clean GUI display that controls hardware outputs, by quickly adapting functions in this template.

  Demonstrated example functions include a horizontal/vertical interface, global LVGL styles, linked object states, grids and sub-grids, a custom tabview, and event handlers

  Built for the Arduino Giga R1 Wifi Display.
  This template uses the LV graphics library (LVGL) to create UI elements -- LVGL Version 8.3.11 (Later versions not currently supported by Arduino).

  Get-Started Reference: https://docs.arduino.cc/tutorials/giga-display-shield/lvgl-guide/
  LVGL Documentation: https://docs.lvgl.io/8.0/index.html
*/

#include "lv_conf.h"                    // LVGL configuration file
#include <lvgl.h>                      // LVGL source
#include <Arduino_H7_Video.h>           // Arduino video output manager
#include <Arduino_GigaDisplayTouch.h>   // Arduino GIGA touch input manager

#include <WiFi.h>
#include <WiFiUdp.h>
#include <mbed_mktime.h>
#include "arduino_secrets.h" // WiFi connection details

//~~~~~~~~~~ WiFi Variables ~~~~~~~~~~~~//
  int timezone = -5;
  int status = WL_IDLE_STATUS;

  char ssid[] = SECRET_SSID;
  char pass[] = SECRET_PASS;

  int keyIndex = 0;  // your network key index number (needed only for WEP)
  unsigned int localPort = 2390;  // local port to listen for UDP packets

  constexpr auto timeServer{ "pool.ntp.org" };
  const int NTP_PACKET_SIZE = 48;  // NTP timestamp is in the first 48 bytes of the message

  byte packetBuffer[NTP_PACKET_SIZE];  // buffer to hold incoming and outgoing packets

  // A UDP instance to let us send and receive packets over UDP
  WiFiUDP Udp;

//~~~~~~~~~~ RTC  Variables ~~~~~~~~~~~~//
  constexpr unsigned long clockInterval{ 1000 };
  unsigned long clockCheck{};

  lv_obj_t * RTC_label; //clock

//~~~~~~~~~~ LVGL Declarations ~~~~~~~~~//
  // GUI orientation:
    // 800,480 -> landscape; 480, 800 -> portrait
    Arduino_H7_Video          Display(800, 480, GigaDisplayShield);   // Define output display/rotation
    Arduino_GigaDisplayTouch  TouchDetector;                          // Define Touch input

  // Define LVGL Colors
    lv_color_t white =      lv_color_hex(0xFFFFFF);
    lv_color_t off_white =  lv_color_hex(0xE7E7DA);
    lv_color_t grey =       lv_color_hex(0x707070);
    lv_color_t black =      lv_color_hex(0x000000);
    
    lv_color_t yellow     = lv_color_hex(0xF39F2B);
    lv_color_t periwinkle = lv_color_hex(0x926EED);
    lv_color_t taupe =    lv_color_hex(0x483D3F);

  // Declare LVGL Styles
    // basic styles
      static lv_style_t style_default_toplevel;  
      static lv_style_t style_taupe;
      static lv_style_t style_periwinkle;
      static lv_style_t style_noBorder;  
    // panel/grid styles
      static lv_style_t style_panel_1;
      static lv_style_t style_panel_2;
      static lv_style_t style_panel_3;
    // button styles
      static lv_style_t style_button_toggled;
      static lv_style_t style_active;
      static lv_style_t style_inactive;
    // slider styles
      static lv_style_t style_slider_knob;
    // font styles
      static lv_style_t style_header_1;
      static lv_style_t style_subtitle_label;

  // declare GUI screens and shared objects
    lv_obj_t * screen1; // "home" screen

    lv_obj_t * grid_base; // base grid (topbar + content)
    lv_obj_t * topbar;    // header bar
    lv_obj_t * buttonbar; // sub-header tab button bar 
    lv_obj_t * grid_tab1; // "lights" tab grid
    lv_obj_t * grid_tab2; // "music" tab grid
    lv_obj_t * grid_tab3; // "AC" tab grid
    lv_obj_t * grid_tab4; // "Info" tab grid

    lv_obj_t * tab1_btn;
    lv_obj_t * tab2_btn;
    lv_obj_t * tab3_btn;
    lv_obj_t * tab4_btn;

void setup(){
  Serial.begin(9600);
  Display.begin();
  TouchDetector.begin();

  //~~~~ create LVGL theme, styles and screens ~~~~~//
    createLVGLstyles(); //setup declared LVGL styles

    lv_disp_t * display = lv_disp_get_default();
    lv_theme_t * theme = lv_theme_default_init(display,                 
                                            yellow,   /* Primary and secondary palette */
                                            periwinkle,
                                            true,        /* True = Dark theme,  False = light theme. */
                                            &lv_font_montserrat_28);

    screen1 = lv_obj_create(NULL);
    lv_obj_set_size(screen1, Display.width(), Display.height());
    lv_obj_clear_flag(screen1, LV_OBJ_FLAG_SCROLLABLE);

    // create loading label
      lv_scr_load(screen1);
      lv_obj_t * label = lv_label_create(screen1);
      lv_label_set_text(label, "Joining WiFi Network...");
      lv_obj_set_style_text_font(label, &lv_font_montserrat_24, NULL);
      lv_obj_center(label);
      lv_timer_handler();

  //~~~~~~~~~~ WiFi Setup ~~~~~~~~~~~~//
    // check for the WiFi module:
    if (WiFi.status() == WL_NO_SHIELD) {
      Serial.println("Communication with WiFi module failed!");
      lv_label_set_text(label, "Communication with WiFi module failed!");
      lv_timer_handler();
      // don't continue
      while (true)
        ;
    }

    // attempt to connect to WiFi network:
    while (status != WL_CONNECTED) {
      Serial.print("Attempting to connect to SSID: ");
      Serial.println(ssid);
      // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
      status = WiFi.begin(ssid, pass);

      // wait 10 seconds for connection:
      delay(10000);
    }

    Serial.println("Connected to WiFi");
    printWifiStatus();

  //~~~~~~~~~~ RTC Setup ~~~~~~~~~~~~~//
    setNtpTime();

  //~~~~~~~~~~ Create LVGL objects ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
    lv_obj_add_style(screen1, &style_default_toplevel, 0);
    grid_base =  createGrid1(screen1); // create an LVGL Grid layout
    topbar =     createTopbar(grid_base); // create GUI Topbar Panel (Page 1)
    buttonbar =  createButtonBar(grid_base); // create button/tab topbar

    grid_tab1 =  createGrid2(grid_base); // create a subset grid for "lights" tab
    grid_tab2 =  createGrid3(grid_base); // create a subset grid for "music" tab
    grid_tab3 =  createGrid4(grid_base); // create a subset grid for "AC" tab
    grid_tab4 =  createGrid5(grid_base); // create a subset grid for "INFO" tab

    
    lv_obj_add_state(tab1_btn, LV_STATE_CHECKED);
    lv_event_send(tab1_btn, LV_EVENT_CLICKED, NULL);
}

void loop() {
  lv_timer_handler(); // LVGL updater function

  // update clock
  if (millis() > clockCheck) {
    Serial.print("System Clock:          ");
    Serial.println(getLocaltime());
    clockCheck = millis() + clockInterval;

    String str = getLocaltime();
    int str_len = str.length()+1;
    char char_array[str_len];
    str.toCharArray(char_array, str_len);
    lv_label_set_text(RTC_label, char_array);
  }

  delay(5);
}

//~~~~~~~~~~~~~~~~ LVGL object/layout creation Functions ~~~~~~~~~~~~~~~//
  lv_obj_t * createGrid1(lv_obj_t * screen){ // Create Top-Level Grid (Title Bar & Content)
    lv_obj_t * grid = lv_obj_create(screen);
    lv_obj_add_style(grid, &style_default_toplevel, 0);
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create columns
    static lv_coord_t row_dsc[] = {100, 70, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create rows

    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_size(grid, Display.width(), Display.height());
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    return grid;
  }

  lv_obj_t * createGrid2(lv_obj_t * topLevelGrid){ // Create a Sub-Grid ("lights" tab)
    lv_obj_t * grid = lv_obj_create(topLevelGrid);
    lv_obj_add_style(grid, &style_noBorder, 0);
    lv_obj_add_style(grid, &style_periwinkle, LV_STATE_SCROLLED | LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(grid, 0, 0);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create columns
    static lv_coord_t row_dsc[] = {130, 130, 130, LV_GRID_TEMPLATE_LAST}; // create rows
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_grid_cell(grid, LV_GRID_ALIGN_STRETCH, 0, 1,    //column
                      LV_GRID_ALIGN_STRETCH, 2, 1);     //row

    // create switch panels
    createSwitch(grid, 0, 0, "Office", LV_SYMBOL_EYE_OPEN);
    createSwitch(grid, 0, 1, "Kitchen", LV_SYMBOL_EYE_OPEN);
    createSwitch(grid, 1, 0, "Patio", LV_SYMBOL_EYE_OPEN);
    createSwitch(grid, 1, 1, "Living Room", LV_SYMBOL_EYE_OPEN);
    createSwitch(grid, 2, 0, "Dining Room", LV_SYMBOL_EYE_OPEN);
    createSwitch(grid, 2, 1, "Mudroom", LV_SYMBOL_EYE_OPEN);

    return grid;
  }

  lv_obj_t * createGrid3(lv_obj_t * topLevelGrid){ // Create a Sub-Grid ("music" tab)
    lv_obj_t * label;
    lv_obj_t * grid = lv_obj_create(topLevelGrid);
    lv_obj_add_style(grid, &style_noBorder, 0);
    lv_obj_add_style(grid, &style_periwinkle, LV_STATE_SCROLLED | LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(grid, 0, 0);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create columns
    static lv_coord_t row_dsc[] = {LV_GRID_FR(4), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create rows
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_grid_cell(grid, LV_GRID_ALIGN_STRETCH, 0, 1,    //column
                      LV_GRID_ALIGN_STRETCH, 2, 1);     //row

    // create labels
      label = lv_label_create(grid);
      lv_label_set_text(label, LV_SYMBOL_AUDIO "  Now Playing");
      lv_obj_add_style(label, &style_subtitle_label, 0);
      lv_obj_set_grid_cell(label, LV_GRID_ALIGN_START, 0, 1,    //column
                        LV_GRID_ALIGN_START, 0, 1);     //row    

      label = lv_label_create(grid);
      lv_label_set_text(label, "Something About Us");
      lv_obj_add_style(label, &style_header_1, 0);
      lv_obj_set_grid_cell(label, LV_GRID_ALIGN_CENTER, 0, 1,    //column
                        LV_GRID_ALIGN_CENTER, 1, 1);     //row    
      
      label = lv_label_create(grid);
      lv_label_set_text(label, "Daft Punk");
      lv_obj_add_style(label, &style_subtitle_label, 0);
      lv_obj_set_grid_cell(label, LV_GRID_ALIGN_CENTER, 0, 1,    //column
                        LV_GRID_ALIGN_CENTER, 2, 1);     //row    

      label = lv_label_create(grid);
      lv_label_set_text(label, "Discovery");
      lv_obj_add_style(label, &style_subtitle_label, 0);
      lv_obj_set_grid_cell(label, LV_GRID_ALIGN_CENTER, 0, 1,    //column
                        LV_GRID_ALIGN_START, 3, 1);     //row    


    // create buttons
      lv_obj_t * playBtn = lv_btn_create(grid);
      lv_obj_set_grid_cell(playBtn, LV_GRID_ALIGN_CENTER, 0, 1,    //column
                        LV_GRID_ALIGN_CENTER, 0, 1);     //row
      lv_obj_add_flag(playBtn, LV_OBJ_FLAG_CHECKABLE);
      lv_obj_add_style(playBtn, &style_button_toggled, LV_STATE_CHECKED);
      lv_obj_add_style(playBtn, &style_inactive, LV_STATE_DEFAULT);
      lv_obj_add_style(playBtn, &style_periwinkle, LV_STATE_PRESSED);
      lv_obj_set_size(playBtn,120,120);
      lv_obj_set_style_radius(playBtn,60,0);
      lv_obj_add_event_cb(playBtn, playButton_cb_event, LV_EVENT_CLICKED, 0);
      label = lv_label_create(playBtn);
      lv_label_set_text(label, LV_SYMBOL_PLAY);
      lv_obj_set_style_text_font(label, &lv_font_montserrat_38, LV_PART_MAIN);
      lv_obj_center(label);   

      lv_obj_t * backBtn = lv_btn_create(grid);
      lv_obj_set_grid_cell(backBtn, LV_GRID_ALIGN_CENTER, 0, 1,    //column
                         LV_GRID_ALIGN_CENTER, 0, 1);     //row
      lv_obj_set_style_translate_x(backBtn, lv_pct(-160), NULL);
      lv_obj_add_style(backBtn, &style_inactive, LV_STATE_DEFAULT);
      lv_obj_add_style(backBtn, &style_periwinkle, LV_STATE_PRESSED);
      lv_obj_set_size(backBtn, 90, 90);
      lv_obj_set_style_radius(backBtn, 45, 0);
      label = lv_label_create(backBtn);
      lv_label_set_text(label, LV_SYMBOL_PREV);
      lv_obj_set_style_text_font(label, &lv_font_montserrat_30, LV_PART_MAIN);
      lv_obj_center(label);   

      lv_obj_t * nextBtn = lv_btn_create(grid);
      lv_obj_set_grid_cell(nextBtn, LV_GRID_ALIGN_CENTER, 0, 1,    //column
                         LV_GRID_ALIGN_CENTER, 0, 1);     //row
      lv_obj_set_style_translate_x(nextBtn, lv_pct(160), NULL);
      lv_obj_add_style(nextBtn, &style_inactive, LV_STATE_DEFAULT);
      lv_obj_add_style(nextBtn, &style_periwinkle, LV_STATE_PRESSED);
      lv_obj_set_size(nextBtn, 90, 90);
      lv_obj_set_style_radius(nextBtn, 45, 0);
      label = lv_label_create(nextBtn);
      lv_label_set_text(label, LV_SYMBOL_NEXT);
      lv_obj_set_style_text_font(label, &lv_font_montserrat_30, LV_PART_MAIN);
      lv_obj_center(label);   

    return grid;
  }

  lv_obj_t * createGrid4(lv_obj_t * topLevelGrid){ // Create a Sub-Grid ("AC" tab)
    lv_obj_t * label;
    lv_obj_t * button;
    lv_obj_t * grid = lv_obj_create(topLevelGrid);
    lv_obj_add_style(grid, &style_noBorder, 0);
    lv_obj_add_style(grid, &style_periwinkle, LV_STATE_SCROLLED | LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(grid, 0, 0);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);
    static lv_coord_t col_dsc[] = {240, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create columns
    static lv_coord_t row_dsc[] = {70, 80, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create rows
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_grid_cell(grid, LV_GRID_ALIGN_STRETCH, 0, 1,    //column
                      LV_GRID_ALIGN_STRETCH, 2, 1);     //row

    // create labels
      label = lv_label_create(grid);
      lv_label_set_text(label, "Climate Control");
      lv_obj_add_style(label, &style_header_1, 0);
      lv_obj_set_grid_cell(label, LV_GRID_ALIGN_CENTER, 0, 2,    //column
                        LV_GRID_ALIGN_CENTER, 0, 1);     //row 

      label = lv_label_create(grid);
      lv_label_set_text(label, "Currently: 68 F");
      lv_obj_add_style(label, &style_subtitle_label, 0);
      lv_obj_set_grid_cell(label, LV_GRID_ALIGN_CENTER, 0, 2,    //column
                        LV_GRID_ALIGN_START, 1, 1);     //row 
      lv_obj_t * statusLabel = lv_label_create(grid);
      lv_label_set_text(statusLabel, "OFF");
      lv_obj_add_style(statusLabel, &style_subtitle_label, 0);
      lv_obj_set_grid_cell(statusLabel, LV_GRID_ALIGN_CENTER, 0, 2,    //column
                        LV_GRID_ALIGN_CENTER, 1, 1);     //row           

    // create a power button
      lv_obj_t * pwrBtn = lv_btn_create(grid);
      lv_obj_set_grid_cell(pwrBtn, LV_GRID_ALIGN_CENTER, 0, 1,    //column
                        LV_GRID_ALIGN_CENTER, 0, 2);     //row
      lv_obj_add_flag(pwrBtn, LV_OBJ_FLAG_CHECKABLE);
      lv_obj_add_style(pwrBtn, &style_button_toggled, LV_STATE_CHECKED);
      lv_obj_add_style(pwrBtn, &style_inactive, LV_STATE_DEFAULT);
      lv_obj_add_style(pwrBtn, &style_periwinkle, LV_STATE_PRESSED);
      lv_obj_set_size(pwrBtn, 120, 120);
      lv_obj_set_style_radius(pwrBtn, 60, 0);
      lv_obj_add_event_cb(pwrBtn, ACpwrButton_cb_event, LV_EVENT_CLICKED, statusLabel);
      label = lv_label_create(pwrBtn);
      lv_label_set_text(label, LV_SYMBOL_POWER);
      lv_obj_set_style_text_font(label, &lv_font_montserrat_38, LV_PART_MAIN);
      lv_obj_center(label); 

    // create a slider 
      lv_obj_t * slider = lv_slider_create(grid);
      lv_obj_set_grid_cell(slider, LV_GRID_ALIGN_CENTER, 0, 2, LV_GRID_ALIGN_CENTER, 2, 1);
      lv_obj_set_size(slider, Display.width() - 240, 100);
      lv_obj_add_style(slider, &style_periwinkle, LV_PART_KNOB | LV_STATE_PRESSED);
      lv_obj_add_style(slider, &style_periwinkle, LV_PART_INDICATOR | LV_STATE_PRESSED);
      lv_obj_add_style(slider, &style_slider_knob, LV_PART_KNOB);
      lv_obj_set_style_radius(slider, 20, LV_PART_KNOB);
      lv_obj_set_style_radius(slider, 20, LV_PART_INDICATOR);
      lv_obj_set_style_radius(slider, 20, LV_PART_MAIN);
      lv_obj_set_style_pad_left(slider, -20, LV_PART_KNOB);
      lv_obj_set_style_pad_right(slider, -20, LV_PART_KNOB);
      lv_obj_add_flag(slider, LV_OBJ_FLAG_ADV_HITTEST); // only adjustable via dragging knob; disable click-jump-to-value behavior
      
      label = lv_label_create(slider);
      lv_obj_add_style(label, &style_header_1, 0);
      lv_label_set_text(label, "68 F");
      lv_obj_center(label);

      lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, label);   
      lv_obj_add_event_cb(slider, ACslider_cb_event, LV_EVENT_RELEASED, statusLabel);   
      lv_slider_set_value(slider, 68, LV_ANIM_OFF);
      lv_event_send(slider, LV_EVENT_VALUE_CHANGED, NULL);

    // create slider buttons
      button = lv_btn_create(grid);
      lv_obj_set_grid_cell(button, LV_GRID_ALIGN_START, 0, 2, LV_GRID_ALIGN_CENTER, 2, 1);
      lv_obj_add_style(button, &style_inactive, LV_STATE_DEFAULT);
      lv_obj_add_style(button, &style_periwinkle, LV_STATE_PRESSED);
      lv_obj_set_size(button, 80, 100);
      lv_obj_set_style_radius(button, 20, 0);
      lv_obj_add_event_cb(button, ACdown_cb_event, LV_EVENT_CLICKED, slider);
      label = lv_label_create(button);
      lv_label_set_text(label, LV_SYMBOL_LEFT);
      lv_obj_set_style_text_font(label, &lv_font_montserrat_32, LV_PART_MAIN);
      lv_obj_center(label); 

      button = lv_btn_create(grid);
      lv_obj_set_grid_cell(button, LV_GRID_ALIGN_END, 0, 2, LV_GRID_ALIGN_CENTER, 2, 1);
      lv_obj_add_style(button, &style_inactive, LV_STATE_DEFAULT);
      lv_obj_add_style(button, &style_periwinkle, LV_STATE_PRESSED);
      lv_obj_set_size(button, 80, 100);
      lv_obj_set_style_radius(button, 20, 0);
      lv_obj_add_event_cb(button, ACup_cb_event, LV_EVENT_CLICKED, slider);
      label = lv_label_create(button);
      lv_label_set_text(label, LV_SYMBOL_RIGHT);
      lv_obj_set_style_text_font(label, &lv_font_montserrat_32, LV_PART_MAIN);
      lv_obj_center(label); 

    return grid;
  }  

  lv_obj_t * createGrid5(lv_obj_t * topLevelGrid){ // Create a Sub-Grid ("Info" tab)
    lv_obj_t * label;
    lv_obj_t * grid = lv_obj_create(topLevelGrid);
    lv_obj_add_style(grid, &style_noBorder, 0);
    lv_obj_add_style(grid, &style_periwinkle, LV_STATE_SCROLLED | LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(grid, 0, 0);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);
    //static lv_coord_t col_dsc[] = {240, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create columns
    //static lv_coord_t row_dsc[] = {70, 80, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create rows
    //lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_grid_cell(grid, LV_GRID_ALIGN_STRETCH, 0, 1,    //column
                      LV_GRID_ALIGN_STRETCH, 2, 1);     //row

    // create label
      label = lv_label_create(grid);
      lv_label_set_text(label, " Home-Bot 2000: Home Automation GUI\n\n"
                              " Software Version: xx.xx.xxxxxxxxxx\n"
                              " Hardware Kit Version: ~~~.~~~~~~\n\n"
                              " Produced by: xxxxxxxxxxx\n"
                              " Special Thanks: xxxxxxx");
      lv_obj_add_style(label, &style_subtitle_label, 0);
      lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
      lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);

    return grid;
  }  

  lv_obj_t * createTopbar(lv_obj_t * parent){
    lv_obj_t * topbar;
    lv_obj_t * label;
    lv_obj_t * dropdown;
    lv_obj_t * btn;

    // base container
      topbar = lv_obj_create(parent);
      lv_obj_add_style(topbar, &style_noBorder, 0);
      lv_obj_set_grid_cell(topbar, LV_GRID_ALIGN_STRETCH, 0, 1,    //column
                              LV_GRID_ALIGN_STRETCH, 0, 1);     //row

    // Header
      label = lv_label_create(topbar);
      lv_label_set_text(label, "HOME-BOT 2000");
      lv_obj_add_style(label, &style_header_1, 0);
      lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    // Wifi Symbol
      label = lv_label_create(topbar);
      lv_label_set_text(label, LV_SYMBOL_WIFI);
      lv_obj_add_style(label, &style_header_1, 0);
      lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

    // Clocktime
      RTC_label = lv_label_create(topbar);
      String str = getLocaltime();
      int str_len = str.length()+1;
      char char_array[str_len];
      str.toCharArray(char_array, str_len);
      lv_label_set_text(RTC_label, char_array);
      lv_obj_add_style(RTC_label, &style_header_1, 0);
      lv_obj_align(RTC_label, LV_ALIGN_RIGHT_MID, 0, 0);

    return topbar;
  }

  lv_obj_t * createButtonBar(lv_obj_t * parent){
    lv_obj_t * buttonGrid;
    lv_obj_t * label;
    lv_obj_t * btn;

    // button bar grid
      buttonGrid = lv_obj_create(parent);
      lv_obj_add_style(buttonGrid, &style_noBorder, 0);
      lv_obj_set_style_pad_top(buttonGrid, 5, 0);
      lv_obj_set_style_pad_bottom(buttonGrid, 5, 0);
      lv_obj_set_style_pad_column(buttonGrid, 20, 0);
      static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create 4 columns
      static lv_coord_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create row
      lv_obj_set_grid_dsc_array(buttonGrid, col_dsc, row_dsc);
      lv_obj_set_grid_cell(buttonGrid, LV_GRID_ALIGN_STRETCH, 0, 1,    //column
                              LV_GRID_ALIGN_STRETCH, 1, 1);     //row         

    // create tab buttons
      tab1_btn = createTabButton(buttonGrid, "Lights", 0);
      tab2_btn = createTabButton(buttonGrid, "Music", 1);
      tab3_btn = createTabButton(buttonGrid, "AC", 2);
      tab4_btn = createTabButton(buttonGrid, "Info", 3);

      lv_obj_add_event_cb(tab1_btn, pageTab1_cb_event, LV_EVENT_CLICKED, NULL);
      lv_obj_add_event_cb(tab2_btn, pageTab2_cb_event, LV_EVENT_CLICKED, NULL); 
      lv_obj_add_event_cb(tab3_btn, pageTab3_cb_event, LV_EVENT_CLICKED, NULL); 
      lv_obj_add_event_cb(tab4_btn, pageTab4_cb_event, LV_EVENT_CLICKED, NULL); 

    return buttonGrid;
  }

  lv_obj_t * createTabButton(lv_obj_t * parent, char * labelText, int column){
    lv_obj_t * label;
    lv_obj_t * btn;

    btn = lv_btn_create(parent);
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, column, 1,    //column
                      LV_GRID_ALIGN_STRETCH, 0, 1);     //row
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_center(btn); 

    lv_obj_add_style(btn, &style_button_toggled, LV_STATE_CHECKED);
    lv_obj_add_style(btn, &style_inactive, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn,20,0);

    label = lv_label_create(btn);
    lv_label_set_text(label, labelText);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(label);   

    return btn;
  }

  lv_obj_t * createSwitch(lv_obj_t * parent, int row, int column, char * title, char * symbol){ // Create an individual Switch Panel
    lv_obj_t * panel;
    lv_obj_t * swtch;
    lv_obj_t * label;
    lv_obj_t * icon;
    
    panel = lv_obj_create(parent);
    lv_obj_set_grid_cell(panel, LV_GRID_ALIGN_STRETCH, column, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_add_style(panel, &style_panel_3, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_50, 0);

    static lv_coord_t cols_dsc[] = {80, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create columns
    static lv_coord_t rows_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create rows
    lv_obj_set_grid_dsc_array(panel, cols_dsc, rows_dsc);

    icon = lv_label_create(panel);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_44, 0);

    label = lv_label_create(panel);
    lv_label_set_text(label, title);
    lv_obj_add_style(label, &style_header_1, 0);
    lv_obj_set_grid_cell(label, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_START, 0, 1);

    swtch = lv_switch_create(panel);
    lv_obj_set_grid_cell(swtch, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_END, 0, 1);
    lv_obj_set_size(swtch, 100, 40);
    lv_obj_add_style(swtch, &style_periwinkle, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(swtch, 2, LV_PART_INDICATOR | LV_PART_MAIN);
    lv_obj_set_style_border_color(swtch, white, LV_PART_INDICATOR | LV_PART_MAIN);
    
    lv_obj_add_event_cb(swtch, switchToggle_event, LV_EVENT_CLICKED, icon);

    return swtch; // return switch object, not entire switch panel
  }

  lv_obj_t * createSlider(lv_obj_t * parent, int row, char * title, char * symbol){ // Create an individual Slider Panel
    lv_obj_t * panel;
    lv_obj_t * slider;
    lv_obj_t * label;
    lv_obj_t * icon;
    
    panel = lv_obj_create(parent);
    lv_obj_set_grid_cell(panel, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_add_style(panel, &style_panel_3, 0);

    static lv_coord_t cols_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create columns
    static lv_coord_t rows_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST}; // create rows
    lv_obj_set_grid_dsc_array(panel, cols_dsc, rows_dsc);

    label = lv_label_create(panel);
    lv_label_set_text(label, title);
    lv_obj_add_style(label, &style_subtitle_label, 0);
    lv_obj_set_grid_cell(label, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    icon = lv_label_create(panel);
    lv_label_set_text(icon, symbol);
    lv_obj_add_style(icon, &style_subtitle_label, 0);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    // create a slider
      slider = lv_slider_create(panel);
      lv_obj_set_grid_cell(slider, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
      lv_obj_set_size(slider, Display.width() - 130, 40);
      lv_obj_add_style(slider, &style_periwinkle, LV_PART_KNOB | LV_STATE_PRESSED);
      lv_obj_add_style(slider, &style_periwinkle, LV_PART_INDICATOR | LV_STATE_PRESSED);
      lv_obj_add_flag(slider, LV_OBJ_FLAG_ADV_HITTEST); // only adjustable via dragging knob; disable click-jump-to-value behavior
      lv_slider_set_range(slider, -5 , 105);
      lv_obj_add_style(slider, &style_slider_knob, LV_PART_KNOB);

      label = lv_label_create(slider);
      lv_obj_add_style(label, &style_subtitle_label, 0);
      lv_label_set_text(label, "0");
      lv_obj_center(label);

      lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, label);      

      return slider;
  } 

//~~~~~~~~~~~~~~~~ LVGL misc/helper functions ~~~~~~~~~~~~~~~~~~~~~~~~~~//
  void createLVGLstyles(){ // setup all global LVGL styles
    // basic styles
      // style_default_toplevel // no padding; no border
        lv_style_init(&style_default_toplevel);

        lv_style_set_pad_row(&style_default_toplevel,0);
        lv_style_set_pad_column(&style_default_toplevel,0);
        lv_style_set_pad_left(&style_default_toplevel, 0);
        lv_style_set_pad_right(&style_default_toplevel, 0);
        lv_style_set_pad_top(&style_default_toplevel, 0);
        lv_style_set_pad_bottom(&style_default_toplevel, 0);

        lv_style_set_border_width(&style_default_toplevel, 0);
        lv_style_set_border_opa(&style_default_toplevel, 0);
        lv_style_set_radius(&style_default_toplevel, 0);

      //"no Border" style
        lv_style_init(&style_noBorder);
        lv_style_set_border_width(&style_noBorder, 0);
        lv_style_set_border_opa(&style_noBorder, 0);
        lv_style_set_radius(&style_noBorder, 0);

      //colors
        lv_style_init(&style_taupe);    
        lv_style_set_bg_color(&style_taupe, taupe);

        lv_style_init(&style_periwinkle);
        lv_style_set_bg_color(&style_periwinkle, periwinkle);

        lv_style_init(&style_active);
        lv_style_set_bg_color(&style_active, yellow);

        lv_style_init(&style_inactive);
        lv_style_set_bg_color(&style_inactive, grey);

    // panel/grid styles  
      // style_panel_1  // (topbar) no margins; white background; blue underline
        lv_style_init(&style_panel_1);
        // lv_style_set_text_font(&style_panel_1, &lv_font_montserrat_28);
        // lv_style_set_bg_color(&style_panel_1, light_grey);

        /*Set a background color and a radius*/
          lv_style_set_radius(&style_panel_1, 1);
          lv_style_set_bg_opa(&style_panel_1, LV_OPA_COVER);
          //lv_style_set_bg_color(&style_panel_1, dark_grey);

        /*Add border to the bottom*/
          // lv_style_set_border_color(&style_panel_1, taupe);
          // lv_style_set_border_width(&style_panel_1, 5);
          //lv_style_set_border_opa(&style_panel_1, LV_OPA_50);
          // lv_style_set_border_side(&style_panel_1, LV_BORDER_SIDE_BOTTOM); // | LV_BORDER_SIDE_RIGHT);

      // style_panel_2 // (button panel) transparent bg; 
        lv_style_init(&style_panel_2);
        lv_style_set_border_color(&style_panel_2, taupe);
        lv_style_set_border_width(&style_panel_2, 0);
        //lv_style_set_border_side(&style_panel_2, LV_BORDER_SIDE_LEFT);
        lv_style_set_bg_opa(&style_panel_2, 0);

      // style_panel_3 // (slider) transparent bg; blue border
        lv_style_init(&style_panel_3);
        lv_style_set_radius(&style_panel_3, 20);
        //lv_style_set_border_color(&style_panel_3, taupe);
        lv_style_set_border_width(&style_panel_3, 0);
        //lv_style_set_border_side(&style_panel_3, LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_BOTTOM);
        lv_style_set_bg_color(&style_panel_3, taupe);
        lv_style_set_bg_opa(&style_panel_3, 0);

    // button styles
      lv_style_init(&style_button_toggled);
      // lv_style_set_outline_width(&style_button_toggled, 3);
      // lv_style_set_outline_pad(&style_button_toggled, 5);
      // lv_style_set_outline_color(&style_button_toggled, periwinkle);
      lv_style_set_bg_color(&style_button_toggled, yellow);
    
    // slider styles
      lv_style_init(&style_slider_knob);
      lv_style_set_border_width(&style_slider_knob, 4);
      lv_style_set_border_color(&style_slider_knob, taupe);
      //lv_style_set_pad_all(&style_slider_knob, 1); /*Makes the knob larger*/
      
    // font styles
      lv_style_init(&style_header_1);
      lv_style_set_text_font(&style_header_1, &lv_font_montserrat_34);
      lv_style_set_text_color(&style_header_1, white);
      lv_style_set_align(&style_header_1, LV_ALIGN_CENTER);
      
      lv_style_init(&style_subtitle_label);
      lv_style_set_text_font(&style_subtitle_label, &lv_font_montserrat_22);
      lv_style_set_text_color(&style_subtitle_label, white);
      lv_style_set_text_align(&style_subtitle_label, LV_TEXT_ALIGN_CENTER);
      lv_style_set_align(&style_subtitle_label, LV_ALIGN_TOP_MID);     

  }

//~~~~~~~~~~~~~~~~ LVGL Event Handler functions ~~~~~~~~~~~~~~~~~~~~~~~~//
  void switchToggle_event(lv_event_t * e){ // generic switch event handler: grey out icon when "OFF"; increase panel opacity when "ON"
    lv_obj_t * swtch = lv_event_get_target(e);
    lv_obj_t * panel = lv_obj_get_parent(swtch);
    lv_obj_t * icon = (lv_obj_t * ) lv_event_get_user_data(e);

    if (lv_obj_has_state(swtch, LV_STATE_CHECKED)){
      lv_obj_set_style_bg_opa(panel, LV_OPA_100, 0);
      lv_obj_set_style_text_color(icon, yellow, 0);

    } else {
      lv_obj_set_style_bg_opa(panel, LV_OPA_50, 0);
      lv_obj_set_style_text_color(icon, white, 0);
    }
  }
  void playButton_cb_event(lv_event_t * e){ // custom switch event handler: eye open/closed symbol based on switch state
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * icon = lv_obj_get_child(btn, 0);

    if (lv_obj_has_state(btn, LV_STATE_CHECKED)){
      lv_label_set_text(icon, LV_SYMBOL_PAUSE);
    } else {
      lv_label_set_text(icon, LV_SYMBOL_PLAY);
    }
  }
  void slider_event_cb(lv_event_t * e){ // generic slider event handler: display slider value on top of slider
    int low_bound = 50;
    int upper_bound = 90;
    int buffer = 1; 
    char buf[8];
    
    lv_obj_t * slider = lv_event_get_target(e);
    lv_obj_t * sliderLabel = (lv_obj_t * ) lv_event_get_user_data(e);
    int sliderValue = (int)lv_slider_get_value(slider);

    // fix slider visual bug
    if (sliderValue < low_bound) {
      lv_slider_set_value(slider, low_bound, LV_ANIM_OFF);
      sliderValue = (int)lv_slider_get_value(slider);
    } else if (sliderValue > upper_bound) {
      lv_slider_set_value(slider, upper_bound, LV_ANIM_OFF);
      sliderValue = (int)lv_slider_get_value(slider);
    }
    lv_slider_set_range(slider, (low_bound - buffer), (upper_bound + buffer));

    lv_snprintf(buf, sizeof(buf), "%d F", (int)lv_slider_get_value(slider));
    lv_label_set_text(sliderLabel, buf);

    //lv_label_set_text_fmt(sliderLabel, "%"LV_PRId32, lv_slider_get_value(slider));

    // if (sliderValue > 45) {
    //   lv_obj_set_style_text_color(sliderLabel, lv_color_white(), 0);
    // } else {
    //   lv_obj_set_style_text_color(sliderLabel, taupe, 0);
    // }
  }
  void pageTab1_cb_event(lv_event_t * e){ // switch to tab 1
    lv_obj_clear_flag(grid_tab1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(grid_tab2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(grid_tab3, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(grid_tab4, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_state(tab1_btn, LV_STATE_CHECKED);
    lv_obj_clear_state(tab2_btn, LV_STATE_CHECKED);
    lv_obj_clear_state(tab3_btn, LV_STATE_CHECKED);
    lv_obj_clear_state(tab4_btn, LV_STATE_CHECKED);
  } 
  void pageTab2_cb_event(lv_event_t * e){ // switch to tab 1
    lv_obj_add_flag(grid_tab1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(grid_tab2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(grid_tab3, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(grid_tab4, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_state(tab1_btn, LV_STATE_CHECKED);
    lv_obj_add_state(tab2_btn, LV_STATE_CHECKED);
    lv_obj_clear_state(tab3_btn, LV_STATE_CHECKED);
    lv_obj_clear_state(tab4_btn, LV_STATE_CHECKED);
  } 
  void pageTab3_cb_event(lv_event_t * e){ // switch to tab 1
    lv_obj_add_flag(grid_tab1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(grid_tab2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(grid_tab3, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(grid_tab4, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_state(tab1_btn, LV_STATE_CHECKED);
    lv_obj_clear_state(tab2_btn, LV_STATE_CHECKED);
    lv_obj_add_state(tab3_btn, LV_STATE_CHECKED);
    lv_obj_clear_state(tab4_btn, LV_STATE_CHECKED);
  } 
  void pageTab4_cb_event(lv_event_t * e){ // switch to tab 1
    lv_obj_add_flag(grid_tab1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(grid_tab2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(grid_tab3, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(grid_tab4, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_state(tab1_btn, LV_STATE_CHECKED);
    lv_obj_clear_state(tab2_btn, LV_STATE_CHECKED);
    lv_obj_clear_state(tab3_btn, LV_STATE_CHECKED);
    lv_obj_add_state(tab4_btn, LV_STATE_CHECKED);
  } 
  void ACpwrButton_cb_event(lv_event_t * e){ // generic slider event handler: display slider value on top of slider
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * slider = lv_obj_get_child(lv_obj_get_parent(btn), 4);
    lv_obj_t * status = (lv_obj_t * ) lv_event_get_user_data(e);

    if (lv_obj_has_state(btn, LV_STATE_CHECKED)){
      //lv_label_set_text(status, LV_SYMBOL_PAUSE);
      lv_event_send(slider, LV_EVENT_RELEASED, NULL);
    } else {
      lv_label_set_text(status, "OFF");
      lv_obj_set_style_text_color(status, white, NULL);
    }
  }
  void ACslider_cb_event(lv_event_t * e){ // generic slider event handler: display slider value on top of slider
    lv_obj_t * slider = lv_event_get_target(e);
    int sliderValue = (int)lv_slider_get_value(slider);
    lv_obj_t * status = (lv_obj_t * ) lv_event_get_user_data(e);
    lv_obj_t * pwrBtn = lv_obj_get_child(lv_obj_get_parent(slider), 3);

    if (!lv_obj_has_state(pwrBtn, LV_STATE_CHECKED)){
      return;
    }

    if (sliderValue < 68){
      lv_label_set_text(status, "Cooling");
      lv_obj_set_style_text_color(status, yellow, NULL);
    } else if (sliderValue > 68){
      lv_label_set_text(status, "Heating");
      lv_obj_set_style_text_color(status, periwinkle, NULL);
    } else {
      lv_label_set_text(status, "Active Hold");
      lv_obj_set_style_text_color(status, white, NULL);
    }
  }
  void ACdown_cb_event(lv_event_t * e){ // generic slider event handler: display slider value on top of slider
    lv_obj_t * button = lv_event_get_target(e);
    lv_obj_t * slider = (lv_obj_t * ) lv_event_get_user_data(e);
    int sliderValue = (int)lv_slider_get_value(slider);

    lv_slider_set_value(slider, sliderValue-1, LV_ANIM_OFF);
    lv_event_send(slider, LV_EVENT_VALUE_CHANGED, NULL);
    lv_event_send(slider, LV_EVENT_RELEASED, NULL);
  }
  void ACup_cb_event(lv_event_t * e){ // generic slider event handler: display slider value on top of slider
    lv_obj_t * button = lv_event_get_target(e);
    lv_obj_t * slider = (lv_obj_t * ) lv_event_get_user_data(e);
    int sliderValue = (int)lv_slider_get_value(slider);

    lv_slider_set_value(slider, sliderValue+1, LV_ANIM_OFF);
    lv_event_send(slider, LV_EVENT_VALUE_CHANGED, NULL);
    lv_event_send(slider, LV_EVENT_RELEASED, NULL);
  }

//~~~~~~~~~~~~~~~~ WiFi/RTC functions ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

  void setNtpTime() {
    Udp.begin(localPort);
    sendNTPpacket(timeServer);
    delay(1000);
    parseNtpPacket();
  }

  // send an NTP request to the time server at the given address
  unsigned long sendNTPpacket(const char* address) {
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;  // LI, Version, Mode
    packetBuffer[1] = 0;           // Stratum, or type of clock
    packetBuffer[2] = 6;           // Polling Interval
    packetBuffer[3] = 0xEC;        // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    Udp.beginPacket(address, 123);  // NTP requests are to port 123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
  }

  unsigned long parseNtpPacket() {
    if (!Udp.parsePacket())
      return 0;

    Udp.read(packetBuffer, NTP_PACKET_SIZE);
    const unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    const unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    const unsigned long secsSince1900 = highWord << 16 | lowWord;
    constexpr unsigned long seventyYears = 2208988800UL;
    const unsigned long epoch = secsSince1900 - seventyYears;
    const unsigned long new_epoch = epoch + (3600 * timezone); //multiply the timezone with 3600 (1 hour)
    set_time(new_epoch);
    return epoch;
  }

  String getLocaltime() {
    char buffer[32];
    tm t;
    _rtc_localtime(time(NULL), &t, RTC_FULL_LEAP_YEAR_SUPPORT);
    strftime(buffer, 32, "%Y-%m-%d %k:%M:%S", &t);
    String buf = String(buffer);
    //buf = buf.substring(11, 16); //HH:MM only
    buf = buf.substring(11, 19); // HH:MM:SS only 
    return buf;
    //return String(buffer);
  }

  void printWifiStatus() {
    // print the SSID of the network you're attached to:
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    // print your board's IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
  }