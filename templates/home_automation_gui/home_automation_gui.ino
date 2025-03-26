/*
  This program is a template GUI for hardware prototyping.
  This is intended to allow rapid implementation of a clean GUI display that controls hardware outputs, by quickly adapting functions in this template.

  Demonstrated example functions include a horizontal/vertical interface, global LVGL styles, linked object states, grids and sub-grids, and event handlers

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

//~~~~~~~~~~ LVGL Declarations ~~~~~~~~~//
  // GUI orientation:
    // 800,480 -> landscape; 480, 800 -> portrait
    Arduino_H7_Video          Display(480, 800, GigaDisplayShield);   // Define output display/rotation
    Arduino_GigaDisplayTouch  TouchDetector;                          // Define Touch input

  // Define LVGL Colors
    lv_color_t off_white =  lv_color_hex(0xE7E7DA);
    lv_color_t grey =       lv_color_hex(0xBFBFBB);
    lv_color_t light_blue = lv_color_hex(0x0496FF);
    lv_color_t dark_blue =  lv_color_hex(0x006BA6);
    lv_color_t coral =      lv_color_hex(0xEF6F6C);
    lv_color_t magenta =    lv_color_hex(0xB37BA4);

  // Declare LVGL Styles
    // basic styles
      static lv_style_t style_dark_blue;
      static lv_style_t style_coral;  
    // panel/grid styles
      static lv_style_t style_panel_1;
      static lv_style_t style_panel_2;
      static lv_style_t style_panel_3;
      static lv_style_t style_top_level_grid;
    // button styles
      static lv_style_t style_button_toggled;
      static lv_style_t style_active;
      static lv_style_t style_inactive;
    // slider styles
      static lv_style_t style_slider_knob;
    // font styles
      static lv_style_t style_header_1;
      static lv_style_t style_subtitle_label;

  // declare GUI screens
    lv_obj_t * screen1; // "sequence control" screen
    lv_obj_t * screen2; // "manual control" screen

void setup(){
  Serial.begin(9600);
  Display.begin();
  TouchDetector.begin();

  //~~~~~~~~~~ WiFi Setup ~~~~~~~~~~~~//
    // check for the WiFi module:
    if (WiFi.status() == WL_NO_SHIELD) {
      Serial.println("Communication with WiFi module failed!");
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
  

  // create LVGL display theme and styles
    createLVGLstyles(); //setup declared LVGL styles
    lv_disp_t * display = lv_disp_get_default();
    lv_theme_t * theme = lv_theme_default_init(display,                 
                                            light_blue,   /* Primary and secondary palette */
                                            coral,
                                            false,        /* True = Dark theme,  False = light theme. */
                                            &lv_font_montserrat_28);

  // Create Screen 1 ("Sequence Control") ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    screen1 = lv_obj_create(NULL);
    lv_obj_set_size(screen1, Display.width(), Display.height());
    lv_scr_load(screen1);
    lv_obj_clear_flag(screen1, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t * grid_1 =       createGrid1(screen1); // create an LVGL Grid layout
    lv_obj_t * topbar_1 =     createTopbar_scr1(grid_1, "Sequence Mode"); // create GUI Topbar Panel (Page 1)

    lv_obj_t * grid_2 =       createGrid2(grid_1); // create a subset grid
    lv_obj_t * btnPanel =     createButtonPanel(grid_2); // create GUI Button Panel
    lv_obj_t * sliderPanel =  createSwitchPanel(grid_2); // create GUI Switch / Slider Panel

    lv_obj_add_style(screen1, &style_top_level_grid, 0);
    lv_obj_add_style(grid_1, &style_top_level_grid, 0);
    lv_obj_add_style(topbar_1, &style_panel_1, 0);
    lv_obj_add_style(grid_2, &style_dark_blue, LV_STATE_SCROLLED | LV_PART_SCROLLBAR);
    lv_obj_add_style(btnPanel, &style_panel_2, 0);

  // Create Screen 2 ("Manual Control") ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    screen2 = lv_obj_create(NULL);
    lv_obj_set_size(screen2, Display.width(), Display.height());
    lv_obj_clear_flag(screen2, LV_OBJ_FLAG_SCROLLABLE);  

    lv_obj_t * grid_3 =       createGrid1(screen2); // create an LVGL Grid layout
    lv_obj_t * topbar_2 =     createTopbar_scr2(grid_3, "Manual Mode"); // create GUI Topbar Panel (Page 1)

    lv_obj_add_style(screen2, &style_top_level_grid, 0);
    lv_obj_add_style(grid_3, &style_top_level_grid, 0);
    lv_obj_add_style(topbar_2, &style_panel_1, 0);
}

void loop() {
  lv_timer_handler(); // LVGL updater function

  // update clock
  if (millis() > clockCheck) {
    Serial.print("System Clock:          ");
    Serial.println(getLocaltime());
    clockCheck = millis() + clockInterval;
  }

  delay(5);
}

//~~~~~~~~~~~~~~~~ LVGL object/layout creation Functions ~~~~~~~~~~~~~~~//
  lv_obj_t * createGrid1(lv_obj_t * screen){ // Create Top-Level Grid (Title Bar & Content)
    lv_obj_t * grid = lv_obj_create(screen);
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create columns
    static lv_coord_t row_dsc[] = {100, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create rows

    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_size(grid, Display.width(), Display.height());
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    return grid;
  }

  lv_obj_t * createGrid2(lv_obj_t * topLevelGrid){ // Create a Sub-Grid (Screen 1 Buttons & Switch/Slider Panels)
    lv_obj_t * grid = lv_obj_create(topLevelGrid);
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create columns
    static lv_coord_t row_dsc[] = {200, 800, LV_GRID_TEMPLATE_LAST}; // create rows

    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_grid_cell(grid, LV_GRID_ALIGN_STRETCH, 0, 1,    //column
                      LV_GRID_ALIGN_STRETCH, 1, 1);     //row
    lv_obj_set_style_bg_color(grid, off_white, 0);
    lv_obj_set_style_radius(grid, 0, 0);
    //lv_obj_set_style_pad_right(grid, 20, 0);
    //lv_obj_set_style_pad_left(grid, 20, 0);
    lv_obj_set_style_pad_right(grid, 0, 0);
    lv_obj_set_style_pad_left(grid, 0, 0);
    lv_obj_set_style_pad_top(grid, 20, 0);
    lv_obj_set_style_pad_bottom(grid, 0, 0);
    lv_obj_set_style_pad_column(grid, 40, 0);

    lv_obj_set_scroll_dir(grid, LV_DIR_VER);

    return grid;
  }

  lv_obj_t * createTopbar_scr1(lv_obj_t * parent, char * header){
    lv_obj_t * topbar;
    lv_obj_t * label;
    lv_obj_t * dropdown;
    lv_obj_t * btn;

    // base container
      topbar = lv_obj_create(parent);
      lv_obj_set_grid_cell(topbar, LV_GRID_ALIGN_STRETCH, 0, 1,    //column
                              LV_GRID_ALIGN_STRETCH, 0, 1);     //row

    // header
      label = lv_label_create(topbar);
      lv_label_set_text(label, header);
      lv_obj_add_style(label, &style_header_1, 0);

    // Right Tab Button
      btn = lv_btn_create(topbar);
      lv_obj_set_size(btn,50,50);
      lv_obj_set_style_radius(btn, 10,0);
      lv_obj_align(btn, LV_ALIGN_RIGHT_MID, 0, 0);
      lv_obj_add_event_cb(btn, pageBtn_event_cb, LV_EVENT_CLICKED, NULL);
      label = lv_label_create(btn);
      lv_label_set_text(label, LV_SYMBOL_RIGHT);
      lv_obj_set_style_text_font(label, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_center(label);   

    return topbar;
  }

  lv_obj_t * createTopbar_scr2(lv_obj_t * parent, char * header){
    lv_obj_t * topbar;
    lv_obj_t * label;
    lv_obj_t * dropdown;
    lv_obj_t * btn;

    // base container
      topbar = lv_obj_create(parent);
      lv_obj_set_grid_cell(topbar, LV_GRID_ALIGN_STRETCH, 0, 1,    //column
                              LV_GRID_ALIGN_STRETCH, 0, 1);     //row

    // header
      label = lv_label_create(topbar);
      lv_label_set_text(label, header);
      lv_obj_add_style(label, &style_header_1, 0);

    // Left Tab Button
      btn = lv_btn_create(topbar);
      lv_obj_set_size(btn,50,50);
      lv_obj_set_style_radius(btn, 10,0);
      lv_obj_align(btn, LV_ALIGN_LEFT_MID, 0, 0);
      lv_obj_add_event_cb(btn, pageBtn_event_cb, LV_EVENT_CLICKED, NULL);
      label = lv_label_create(btn);
      lv_label_set_text(label, LV_SYMBOL_LEFT);
      lv_obj_set_style_text_font(label, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_center(label);   

    return topbar;
  }

  lv_obj_t * createButtonPanel(lv_obj_t * parent){ // Create Screen 1 Buttons
    lv_obj_t * panel;
    lv_obj_t * label;
    lv_obj_t * btn;
    lv_obj_t * slider;
    lv_obj_t * dropdown;

    // Base container (sub-grid)
      panel = lv_obj_create(parent);
      static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create columns
      static lv_coord_t row_dsc[] = {LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create rows

      lv_obj_set_grid_dsc_array(panel, col_dsc, row_dsc);
      lv_obj_set_grid_cell(panel, LV_GRID_ALIGN_STRETCH, 0, 1,    //column
                        LV_GRID_ALIGN_STRETCH, 0, 1);     //row
      // styling
      lv_obj_set_style_pad_row(panel, 0, 0);
      lv_obj_set_style_pad_column(panel, 0, 0);
      lv_obj_set_style_pad_left(panel, 30, 0);
      lv_obj_set_style_pad_right(panel, 30, 0);
      lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // create power button
      lv_obj_t * powerBtn = lv_btn_create(panel);
      lv_obj_set_grid_cell(powerBtn, LV_GRID_ALIGN_CENTER, 0, 1,    //column
                        LV_GRID_ALIGN_CENTER, 0, 1);     //row
      lv_obj_add_flag(powerBtn, LV_OBJ_FLAG_CHECKABLE);
      lv_obj_add_style(powerBtn, &style_button_toggled, LV_STATE_CHECKED);
      lv_obj_add_style(powerBtn, &style_inactive, LV_STATE_DEFAULT);
      lv_obj_set_size(powerBtn,120,120);
      lv_obj_set_style_radius(powerBtn,60,0);
      label = lv_label_create(powerBtn);
      lv_label_set_text(label, LV_SYMBOL_POWER);
      lv_obj_set_style_text_font(label, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_center(label);   
      label = lv_label_create(panel);
      lv_obj_set_grid_cell(label, LV_GRID_ALIGN_CENTER, 0, 1,    //column
                  LV_GRID_ALIGN_CENTER, 1, 1);     //row
      lv_label_set_text(label, "Power");
      lv_obj_add_style(label, &style_subtitle_label, 0);

    // create dropdown menu
      dropdown = lv_dropdown_create(panel);
      lv_obj_set_grid_cell(dropdown, LV_GRID_ALIGN_CENTER, 1, 1,    //column
                        LV_GRID_ALIGN_CENTER, 0, 1);     //row
      lv_obj_set_style_width(dropdown, 200, NULL);
      //lv_obj_set_style_height(dropdown, 80, NULL);
      lv_dropdown_set_options(dropdown, "1,2,3,4\n"
                        "1,2,3\n"
                        "1,2\n"
                        "1");
      lv_obj_add_style(dropdown, &style_header_1, 0);

      label = lv_label_create(panel);
      lv_obj_set_grid_cell(label, LV_GRID_ALIGN_CENTER, 1, 1,    //column
                  LV_GRID_ALIGN_CENTER, 1, 1);     //row
      lv_label_set_text(label, "Active Motors");
      lv_obj_add_style(label, &style_subtitle_label, 0);

      lv_obj_t * list = lv_dropdown_get_list(dropdown);
      lv_obj_add_style(list, &style_header_1, 0);
      lv_obj_set_style_text_align(list, LV_TEXT_ALIGN_CENTER, NULL);
      lv_obj_set_style_text_align(dropdown, LV_TEXT_ALIGN_CENTER, NULL);
      lv_obj_set_style_text_font(dropdown, &lv_font_montserrat_32,NULL);

    return panel;
  }

  lv_obj_t * createSwitchPanel(lv_obj_t * parent){ // Create Screen 1 Switches/Sliders
    lv_obj_t * panelGrid;
    lv_obj_t * label;

    // Base container (panelgrid)
      panelGrid = lv_obj_create(parent);
      static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create columns
      static lv_coord_t row_dsc[] = {60, LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create rows

      lv_obj_set_grid_dsc_array(panelGrid, col_dsc, row_dsc);
      lv_obj_set_grid_cell(panelGrid, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);

    // header
      label = lv_label_create(panelGrid);
      lv_label_set_text(label, "Parameters");
      lv_obj_add_style(label, &style_header_1, 0);
      lv_obj_set_grid_cell(label, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    // individual switch/slider panels
      // lv_obj_t * switch_wifi =      createSwitch(panelGrid, 1, "Wifi", LV_SYMBOL_WIFI);
      lv_obj_t * slider_speed =     createSlider(panelGrid, 1, "Speed",     LV_SYMBOL_CHARGE);
      lv_obj_t * slider_duration =  createSlider(panelGrid, 2, "Duration",  LV_SYMBOL_REFRESH);
      lv_obj_t * slider_overlap =   createSlider(panelGrid, 3, "Overlap",   LV_SYMBOL_LIST);
      lv_obj_t * switch_bluetooth = createSwitch(panelGrid, 4, "Bluetooth", LV_SYMBOL_BLUETOOTH);
      lv_obj_t * switch_eye =       createSwitch(panelGrid, 5, "Visible",   LV_SYMBOL_EYE_CLOSE);
      // lv_obj_t * switch_storage =   createSwitch(panelGrid, 4, "Storage", LV_SYMBOL_SD_CARD);
      // lv_obj_t * switch_shuffle =   createSwitch(panelGrid, 5, "Shuffle", LV_SYMBOL_SHUFFLE);

      lv_obj_add_event_cb(switch_eye, switchToggle_eye_event, LV_EVENT_CLICKED, 0); // custom event handler for eye open/closed switch panel
      

    return panelGrid;
  }

  lv_obj_t * createSwitch(lv_obj_t * parent, int row, char * title, char * symbol){ // Create an individual Switch Panel
    lv_obj_t * panel;
    lv_obj_t * swtch;
    lv_obj_t * label;
    lv_obj_t * icon;
    
    panel = lv_obj_create(parent);
    lv_obj_set_grid_cell(panel, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_add_style(panel, &style_panel_3, 0);

    static lv_coord_t cols_dsc[] = {80, LV_GRID_FR(1), 100, LV_GRID_TEMPLATE_LAST}; // create columns
    static lv_coord_t rows_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // create rows
    lv_obj_set_grid_dsc_array(panel, cols_dsc, rows_dsc);

    icon = lv_label_create(panel);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_44, 0);
    lv_obj_set_style_text_color(icon, grey, 0);

    label = lv_label_create(panel);
    lv_label_set_text(label, title);
    lv_obj_add_style(label, &style_header_1, 0);
    lv_obj_set_grid_cell(label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    swtch = lv_switch_create(panel);
    lv_obj_set_grid_cell(swtch, LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_size(swtch, 90, 50);
    lv_obj_add_style(swtch, &style_coral, LV_PART_INDICATOR | LV_STATE_CHECKED);
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
      lv_obj_add_style(slider, &style_coral, LV_PART_KNOB | LV_STATE_PRESSED);
      lv_obj_add_style(slider, &style_coral, LV_PART_INDICATOR | LV_STATE_PRESSED);
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
      lv_style_init(&style_dark_blue);    
      lv_style_set_bg_color(&style_dark_blue, dark_blue);

      lv_style_init(&style_coral);
      lv_style_set_bg_color(&style_coral, coral);

      lv_style_init(&style_active);
      lv_style_set_bg_color(&style_active, light_blue);

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
          lv_style_set_border_color(&style_panel_1, dark_blue);
          lv_style_set_border_width(&style_panel_1, 5);
          //lv_style_set_border_opa(&style_panel_1, LV_OPA_50);
          lv_style_set_border_side(&style_panel_1, LV_BORDER_SIDE_BOTTOM); // | LV_BORDER_SIDE_RIGHT);

      // style_panel_2 // (button panel) transparent bg; 
        lv_style_init(&style_panel_2);
        lv_style_set_border_color(&style_panel_2, dark_blue);
        lv_style_set_border_width(&style_panel_2, 0);
        //lv_style_set_border_side(&style_panel_2, LV_BORDER_SIDE_LEFT);
        lv_style_set_bg_opa(&style_panel_2, 0);

      // style_panel_3 // (slider) transparent bg; blue border
        lv_style_init(&style_panel_3);
        lv_style_set_radius(&style_panel_3, 20);
        lv_style_set_border_color(&style_panel_3, dark_blue);
        lv_style_set_border_width(&style_panel_3, 2);
        //lv_style_set_border_side(&style_panel_3, LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_BOTTOM);
        lv_style_set_bg_color(&style_panel_3, dark_blue);
        lv_style_set_bg_opa(&style_panel_3, 0);

      // style_top_level_grid // no padding; no border
        lv_style_init(&style_top_level_grid);

        lv_style_set_pad_row(&style_top_level_grid,0);
        lv_style_set_pad_column(&style_top_level_grid,0);
        lv_style_set_pad_left(&style_top_level_grid, 0);
        lv_style_set_pad_right(&style_top_level_grid, 0);
        lv_style_set_pad_top(&style_top_level_grid, 0);
        lv_style_set_pad_bottom(&style_top_level_grid, 0);

        lv_style_set_border_width(&style_top_level_grid, 0);
        lv_style_set_radius(&style_top_level_grid, 0);

    // button styles
      lv_style_init(&style_button_toggled);
      lv_style_set_outline_width(&style_button_toggled, 3);
      lv_style_set_outline_pad(&style_button_toggled, 5);
      lv_style_set_outline_color(&style_button_toggled, coral);
      lv_style_set_bg_color(&style_button_toggled, coral);
    
    // slider styles
      lv_style_init(&style_slider_knob);
      lv_style_set_border_width(&style_slider_knob, 4);
      lv_style_set_border_color(&style_slider_knob, dark_blue);
      //lv_style_set_pad_all(&style_slider_knob, 1); /*Makes the knob larger*/
      
    // font styles
      lv_style_init(&style_header_1);
      lv_style_set_text_font(&style_header_1, &lv_font_montserrat_34);
      lv_style_set_text_color(&style_header_1, dark_blue);
      lv_style_set_align(&style_header_1, LV_ALIGN_CENTER);
      
      lv_style_init(&style_subtitle_label);
      lv_style_set_text_font(&style_subtitle_label, &lv_font_montserrat_22);
      lv_style_set_text_color(&style_subtitle_label, dark_blue);
      lv_style_set_text_align(&style_subtitle_label, LV_TEXT_ALIGN_CENTER);
      lv_style_set_align(&style_subtitle_label, LV_ALIGN_TOP_MID);     

  }

//~~~~~~~~~~~~~~~~ LVGL Event Handler functions ~~~~~~~~~~~~~~~~~~~~~~~~//
  void switchToggle_event(lv_event_t * e){ // generic switch event handler: grey out icon when "OFF"; increase panel opacity when "ON"
    lv_obj_t * swtch = lv_event_get_target(e);
    lv_obj_t * panel = lv_obj_get_parent(swtch);
    lv_obj_t * icon = (lv_obj_t * ) lv_event_get_user_data(e);

    if (lv_obj_has_state(swtch, LV_STATE_CHECKED)){
      lv_obj_set_style_bg_opa(panel, 20, 0);
      lv_obj_set_style_text_color(icon, light_blue, 0);

    } else {
      lv_obj_set_style_bg_opa(panel, 0, 0);
      lv_obj_set_style_text_color(icon, grey, 0);
    }
  }

  void switchToggle_eye_event(lv_event_t * e){ // custom switch event handler: eye open/closed symbol based on switch state
    lv_obj_t * swtch = lv_event_get_target(e);
    lv_obj_t * panel = lv_obj_get_parent(swtch);
    lv_obj_t * icon = lv_obj_get_child(panel, 0);

    if (lv_obj_has_state(swtch, LV_STATE_CHECKED)){
      lv_label_set_text(icon, LV_SYMBOL_EYE_OPEN);
    } else {
      lv_label_set_text(icon, LV_SYMBOL_EYE_CLOSE);
    }
  }
  
  void powerToggle_event(lv_event_t * e){ // power switch event handler: grey out + disable linked buttons when "OFF"
    lv_obj_t * pwr_button = lv_event_get_target(e);
    lv_obj_t * other_button = (lv_obj_t * ) lv_event_get_user_data(e);

    if (lv_obj_has_state(pwr_button, LV_STATE_CHECKED)){
      lv_obj_set_style_bg_color(other_button, light_blue, 0);
      lv_obj_add_flag(other_button, LV_OBJ_FLAG_CLICKABLE);
    } else {
      lv_obj_set_style_bg_color(other_button, grey, 0);
      lv_obj_clear_flag(other_button, LV_OBJ_FLAG_CLICKABLE);
    }
  } 

  void slider_event_cb(lv_event_t * e){ // generic slider event handler: display slider value on top of slider
      lv_obj_t * slider = lv_event_get_target(e);
      lv_obj_t * sliderLabel = (lv_obj_t * ) lv_event_get_user_data(e);
      int sliderValue = (int)lv_slider_get_value(slider);

      // fix slider visual bug
      if (sliderValue < 0) {
        lv_slider_set_value(slider, 0, LV_ANIM_OFF);
        sliderValue = (int)lv_slider_get_value(slider);
      } else if (sliderValue > 100) {
        lv_slider_set_value(slider, 100, LV_ANIM_OFF);
        sliderValue = (int)lv_slider_get_value(slider);
      }

      lv_label_set_text_fmt(sliderLabel, "%"LV_PRId32, lv_slider_get_value(slider));

      if (sliderValue > 45) {
        lv_obj_set_style_text_color(sliderLabel, lv_color_white(), 0);
      } else {
        lv_obj_set_style_text_color(sliderLabel, dark_blue, 0);
      }
  }

  void pageBtn_event_cb(lv_event_t * e){  // load another screen when titlebar button is pressed
    if (lv_scr_act() == screen2){
      lv_scr_load(screen1);
    } else {
      lv_scr_load(screen2);
    }
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