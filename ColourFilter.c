// build commands: gcc -std=gnu11 -O2 -Wall -Wextra -o "Colour Filter" ColourFilter.c $(pkg-config --cflags --libs gtk+-3.0 ayatana-appindicator3-0.1 x11 xrandr libconfig)

#include <gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <sys/time.h>
#include <libconfig.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <fcntl.h>
#include <sys/file.h>

void LoadConfig();
void ScreenInformation();
void StartInterface();
void SetLight(double *Values);
void OpenSettingsWindow(GtkWidget *widget);
void SliderChanged(GtkRange *range, gpointer Element);
void DropdownChanged(GtkWidget *widget, gpointer user_data);
void HelpFunction(GtkWidget *widget);
void DisableOrEnable(GtkWidget *widget);
void OKFunction();
void ResetEverything();
void ConfirmReset();
void SaveConfig();
void CreateConfigFile();
void SetLinearRamp();
void Exit(GtkWidget *widget);
void EaseController(int Switch);
gboolean EaseOnFunction(gpointer user_data);
gboolean EaseOffFunction(gpointer user_data);
gboolean CheckTime(gpointer user_data);
gboolean WaitFunction(gpointer user_data);

// Variables and pointers for screen information
Display *DisplayPointer;
Window WindowReference;
XRRScreenResources *ScreenResources;
XRRCrtcGamma *NewGammaRamp;
XRRCrtcGamma *DefaultGamma;
GtkWidget *SettingsWindow;
GtkWidget *HelpWindow;
GtkWidget *ConfirmWindow;
// Colour values: red, green, blue, brightness.
double ColourValues[4] = {1, 0.7, 0.5, 1.0};
// 0 = filter off, 1 = filter on timer, 2 = filter always on
int FilterMode = 1;
int GammaSize;
int DisplayID;
int StartHourInt = 20;
int StartMinuteInt = 0;
int EndHourInt = 7;
int EndMinuteInt = 0;
int OnOff = 0;
int Easing = 0;
int EaseSteps = 50;
double StepTime = 0.1;
int CheckSeconds = 20;
int Counter = 1;
int Waiting = 0;
guint TimerID = 0;
char ActiveFilePath[1024];
char ConfigFileName[1024];


int main(int argc, char **argv)
{
	// Create lock file, check if an instance is already open
	int LockFile = open("/tmp/colourfilter.lock", O_CREAT | O_RDWR, 0666);
    if (LockFile < 0) return 1;
	// TODO error messages
    if (flock(LockFile, LOCK_EX | LOCK_NB) < 0) 
    {
		fprintf(stderr, "ColourFilter is already running\n");
		exit(1);
	}
	
	// Get the folder path
	ssize_t PathLength = readlink("/proc/self/exe", ActiveFilePath, sizeof(ActiveFilePath) - 1);
    ActiveFilePath[PathLength] = '\0';
	char *LastSlash = strrchr(ActiveFilePath, '/');
	*(LastSlash + 1) = '\0';
	
	LoadConfig();
	gtk_init(&argc, &argv);
	ScreenInformation();
	StartInterface();
	return 0;
}


void LoadConfig()
{
	// Load saved settings from config file, if none exists then make new config file
	config_t Config;
	config_init(&Config);
	strcpy(ConfigFileName, ActiveFilePath);
    strcat(ConfigFileName, "config.cfg");
	
	if (!config_read_file(&Config, ConfigFileName)) CreateConfigFile();
	else
	{
		config_lookup_float(&Config, "RedValue", &ColourValues[0]);
		config_lookup_float(&Config, "GreenValue", &ColourValues[1]);
		config_lookup_float(&Config, "BlueValue", &ColourValues[2]);
		config_lookup_float(&Config, "Brightness", &ColourValues[3]);
		
		config_lookup_int(&Config, "StartHourInt", &StartHourInt);
		config_lookup_int(&Config, "StartMinuteInt", &StartMinuteInt);
		config_lookup_int(&Config, "EndHourInt", &EndHourInt);
		config_lookup_int(&Config, "EndMinuteInt", &EndMinuteInt);
		
		config_lookup_int(&Config, "FilterMode", &FilterMode);
		config_lookup_float(&Config, "StepTime", &StepTime);
		config_lookup_int(&Config, "EaseSteps", &EaseSteps);
		config_lookup_int(&Config, "CheckSeconds", &CheckSeconds);		
	}
		
	config_destroy(&Config);
}


// Get the display information
void ScreenInformation()
{
	// Check the display is valid, exit if not 
    DisplayPointer = XOpenDisplay(NULL);
    if (DisplayPointer == NULL) 
    {
		fprintf(stderr, "Display pointer was invalid\n");
        exit(2);
    }

    WindowReference = DefaultRootWindow(DisplayPointer);
    ScreenResources = XRRGetScreenResources(DisplayPointer, WindowReference);
    if (ScreenResources == NULL) 
    {
		fprintf(stderr, "Could not get screen resources\n");
        exit(3);
    }

	// Set the display, allocate memory for gamma reset, set gamma default to current settings
    DisplayID = ScreenResources->crtcs[0];
    GammaSize = XRRGetCrtcGammaSize(DisplayPointer, DisplayID);
    NewGammaRamp = XRRAllocGamma(GammaSize);
    DefaultGamma = XRRAllocGamma(GammaSize);
    XRRCrtcGamma *TempGamma = XRRGetCrtcGamma(DisplayPointer, DisplayID);
    memcpy(DefaultGamma->red, TempGamma->red, sizeof(unsigned short) * GammaSize);
	memcpy(DefaultGamma->green, TempGamma->green, sizeof(unsigned short) * GammaSize);
	memcpy(DefaultGamma->blue, TempGamma->blue, sizeof(unsigned short) * GammaSize);

	// Free the temporary gamma
	XFree(TempGamma);
}


void StartInterface()
{
	// Tray icon and menu pointers
    AppIndicator *Indicator;
    GtkWidget *Menu;
    GtkWidget *Item;
    
	strcat(ActiveFilePath, "CFIcon.png");
    
    // Make tray icon
    Indicator = app_indicator_new("Colour Filter", ActiveFilePath, APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_status(Indicator, APP_INDICATOR_STATUS_ACTIVE);

	// Make menu and menu items
    Menu = gtk_menu_new();

    Item = gtk_menu_item_new_with_label("Settings");
    g_signal_connect(Item, "activate", G_CALLBACK(OpenSettingsWindow), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(Menu), Item);

    Item = gtk_menu_item_new_with_label("Exit");
    g_signal_connect(Item, "activate", G_CALLBACK(Exit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(Menu), Item);

    gtk_widget_show_all(Menu);
    app_indicator_set_menu(Indicator, GTK_MENU(Menu));
    g_timeout_add_seconds(CheckSeconds, CheckTime, NULL);
    gtk_main();
}


void OpenSettingsWindow(GtkWidget *widget) 
{
    if (GTK_IS_WIDGET(SettingsWindow)) return;
	
    SettingsWindow = gtk_dialog_new(); 
	gtk_window_set_title(GTK_WINDOW(SettingsWindow), "Colour Filter Settings");
	gtk_window_set_modal(GTK_WINDOW(SettingsWindow), FALSE);
		
    GtkWidget *ContentArea = gtk_dialog_get_content_area(GTK_DIALOG(SettingsWindow));
    gtk_container_set_border_width(GTK_CONTAINER(ContentArea), 12);
    
    GtkWidget *BlueSlider, *GreenSlider, *RedSlider, *BrightnessSlider, *BlueSliderLabel, *GreenSliderLabel, *RedSliderLabel, *BrightnessLabel;
    
    // Create labels for the sliders
    BlueSliderLabel = gtk_label_new("\nBlue Light Level");
    GreenSliderLabel = gtk_label_new("\nGreen Light Level");
    RedSliderLabel = gtk_label_new("Red Light Level");
    BrightnessLabel = gtk_label_new("\nBrightness");

    // Blue Gamma Slider
    BlueSlider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01);
    gtk_scale_set_draw_value(GTK_SCALE(BlueSlider), FALSE);
    gtk_range_set_value(GTK_RANGE(BlueSlider), ColourValues[2]);
    g_signal_connect(BlueSlider, "value-changed", G_CALLBACK(SliderChanged), GINT_TO_POINTER(2));

    // Green Gamma Slider
    GreenSlider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01);
    gtk_scale_set_draw_value(GTK_SCALE(GreenSlider), FALSE);
    gtk_range_set_value(GTK_RANGE(GreenSlider), ColourValues[1]); 
    g_signal_connect(GreenSlider, "value-changed", G_CALLBACK(SliderChanged), GINT_TO_POINTER(1));
    
    // Red Gamma Slider
    RedSlider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01);
    gtk_scale_set_draw_value(GTK_SCALE(RedSlider), FALSE);
    gtk_range_set_value(GTK_RANGE(RedSlider), ColourValues[0]); 
    g_signal_connect(RedSlider, "value-changed", G_CALLBACK(SliderChanged), GINT_TO_POINTER(0));   

	// Brightness Slider
	BrightnessSlider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.2, 1.0, 0.01);
    gtk_scale_set_draw_value(GTK_SCALE(BrightnessSlider), FALSE);
    gtk_range_set_value(GTK_RANGE(BrightnessSlider), ColourValues[3]);
    g_signal_connect(BrightnessSlider, "value-changed", G_CALLBACK(SliderChanged), GINT_TO_POINTER(3));
		
    // Create a vertical box and add labels and sliders to it
    GtkWidget *VerticalBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(ContentArea), VerticalBox);
    int Spacing = 0;
    
    gtk_box_pack_start(GTK_BOX(VerticalBox), RedSliderLabel, TRUE, TRUE, Spacing);
    gtk_box_pack_start(GTK_BOX(VerticalBox), RedSlider, TRUE, TRUE, Spacing);
    gtk_box_pack_start(GTK_BOX(VerticalBox), GreenSliderLabel, TRUE, TRUE, Spacing);
    gtk_box_pack_start(GTK_BOX(VerticalBox), GreenSlider, TRUE, TRUE, Spacing);    
    gtk_box_pack_start(GTK_BOX(VerticalBox), BlueSliderLabel, TRUE, TRUE, Spacing);
    gtk_box_pack_start(GTK_BOX(VerticalBox), BlueSlider, TRUE, TRUE, Spacing);
    gtk_box_pack_start(GTK_BOX(VerticalBox), BrightnessLabel, TRUE, TRUE, Spacing);
    gtk_box_pack_start(GTK_BOX(VerticalBox), BrightnessSlider, TRUE, TRUE, Spacing);
       
    int SliderWidth = 300;
    gtk_widget_set_size_request(BlueSlider, SliderWidth, -1);
	gtk_widget_set_size_request(GreenSlider, SliderWidth, -1);
	gtk_widget_set_size_request(RedSlider, SliderWidth, -1);
	gtk_widget_set_size_request(BrightnessSlider, SliderWidth, -1);
	
	
	// Create combo boxes for selecting hours and minutes
	GtkWidget *StartHour, *StartMinute, *EndHour, *EndMinute;

	StartHour = gtk_combo_box_text_new();
	StartMinute = gtk_combo_box_text_new();
	EndHour = gtk_combo_box_text_new();
	EndMinute = gtk_combo_box_text_new();

	// Populate the combo boxes with values 00 to 23 for hours and 00 to 59 for minutes
	for (int i = 0; i <= 23; i++) 
	{
		char hour[3];
		sprintf(hour, "%02d", i);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(StartHour), hour);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(EndHour), hour);
	}

	for (int i = 0; i <= 59; i++) 
	{
		char minute[3];
		sprintf(minute, "%02d", i);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(StartMinute), minute);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(EndMinute), minute);
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(StartHour), StartHourInt);
	gtk_combo_box_set_active(GTK_COMBO_BOX(StartMinute), StartMinuteInt);
	gtk_combo_box_set_active(GTK_COMBO_BOX(EndHour), EndHourInt);
	gtk_combo_box_set_active(GTK_COMBO_BOX(EndMinute), EndMinuteInt);
	
	
    g_signal_connect(StartHour, "changed", G_CALLBACK(DropdownChanged), &StartHourInt);
    g_signal_connect(StartMinute, "changed", G_CALLBACK(DropdownChanged), &StartMinuteInt);
    g_signal_connect(EndHour, "changed", G_CALLBACK(DropdownChanged), &EndHourInt);
    g_signal_connect(EndMinute, "changed", G_CALLBACK(DropdownChanged), &EndMinuteInt);
	
	GtkWidget *DropdownLabels = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget *StartTimeLabel = gtk_label_new("\n        Start Time");
    gtk_box_pack_start(GTK_BOX(DropdownLabels), StartTimeLabel, FALSE, FALSE, 0);
	GtkWidget *Spacer0 = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(DropdownLabels), Spacer0, TRUE, TRUE, 0);
	GtkWidget *EndTimeLabel = gtk_label_new("\nEnd Time         ");
    gtk_box_pack_start(GTK_BOX(DropdownLabels), EndTimeLabel, FALSE, FALSE, 0);	
    gtk_box_pack_start(GTK_BOX(VerticalBox), DropdownLabels, FALSE, FALSE, 0);	
	
    GtkWidget *DropdownBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget *Spacer1 = gtk_label_new(NULL);
	gtk_widget_set_size_request(Spacer1, 40, -1);
	
	gtk_box_pack_start(GTK_BOX(VerticalBox), DropdownBox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(DropdownBox), StartHour, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(DropdownBox), StartMinute, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(DropdownBox), Spacer1, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(DropdownBox), EndHour, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(DropdownBox), EndMinute, FALSE, FALSE, 0);

	GtkWidget *Spacer4 = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(VerticalBox), Spacer4, TRUE, TRUE, 0);

	// Make the buttons
	GtkWidget *HelpButton = gtk_button_new_with_label("Help");
    GtkWidget *OKButton = gtk_button_new_with_label("OK");	
    GtkWidget *DisableButton = gtk_button_new_with_label("Def");
    if (FilterMode == 0) gtk_button_set_label(GTK_BUTTON(DisableButton), "Off");
	else if (FilterMode == 1) gtk_button_set_label(GTK_BUTTON(DisableButton), "Time");
	else if (FilterMode == 2) gtk_button_set_label(GTK_BUTTON(DisableButton), "On");
    
    // Connect buttons to functions
    g_signal_connect(HelpButton, "clicked", G_CALLBACK(HelpFunction), NULL);
    g_signal_connect(DisableButton, "clicked", G_CALLBACK(DisableOrEnable), NULL);
    g_signal_connect(OKButton, "clicked", G_CALLBACK(OKFunction), NULL);

    GtkWidget *ButtonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *Spacer2 = gtk_label_new(NULL);
    GtkWidget *Spacer3 = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(VerticalBox), ButtonBox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(ButtonBox), HelpButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ButtonBox), Spacer2, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(ButtonBox), DisableButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ButtonBox), Spacer3, TRUE, TRUE, 0);	
	gtk_box_pack_start(GTK_BOX(ButtonBox), OKButton, FALSE, FALSE, 0);
    
    int ButtonSize = 80;
    gtk_widget_set_size_request(HelpButton, ButtonSize, -1);   
    gtk_widget_set_size_request(DisableButton, ButtonSize, -1);      
    gtk_widget_set_size_request(OKButton, ButtonSize, -1);       

    gtk_widget_show_all(SettingsWindow);
}


// Set the gamma based on input values
void SetLight(double *Values) 
{
	// Adjust the settings for the gamma ramp
	// Each gamma ramp element is the default setting, multiplied by the user setting for that colour, multiplied by the user setting for brightness
    for (int i = 0; i < GammaSize; i++) 
    {
		NewGammaRamp->red[i] = DefaultGamma->red[i] * Values[0] * Values[3];
		NewGammaRamp->green[i] = DefaultGamma->green[i] * Values[1] * Values[3];
		NewGammaRamp->blue[i] = DefaultGamma->blue[i] * Values[2] * Values[3];
    }
	// Apply gamma ramp to screen
    XRRSetCrtcGamma(DisplayPointer, DisplayID, NewGammaRamp);
    XSync(DisplayPointer, False);
}


// Get dropdown menu selection
void DropdownChanged(GtkWidget *widget, gpointer user_data)
{
	int *Pointer = user_data;
	*Pointer = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
}


// Periodically check the time to see if the filter should be activated
// Should always return TRUE or the timer will stop
gboolean CheckTime(gpointer user_data)
{
	if (Waiting) return TRUE;
	
	// If the setting is 'Off', but the filter itself is not off, then call EaseController(0) to fade out the filter
	if (FilterMode == 0)
	{
		if (OnOff != 0) EaseController(0);
		return TRUE;
	}
	
	// If the setting is 'On', but the filter itself is not on, call EaseController(1) to fade in the filter
	if (FilterMode == 2)
	{
		if (OnOff != 1) EaseController(1);
		return TRUE;
	}
	
	// This part of the function will only be reached if the filter mode is set to '1' or 'Time'
	// Get the current time, convert all times to a single integer value for easier calculations
	time_t UnixTime;
	struct tm *TimeInfo;
	time(&UnixTime);
    TimeInfo = localtime(&UnixTime);
    int StartTimeSingle = StartHourInt * 60 + StartMinuteInt;
    int EndTimeSingle = EndHourInt * 60 + EndMinuteInt;
    int CurrentTimeSingle = TimeInfo->tm_hour * 60 + TimeInfo->tm_min;
    
    // printf("Current Time: %i, Start Time: %i, End Time: %i\n", CurrentTimeSingle, StartTimeSingle, EndTimeSingle);
	
	// Activate the filter.
	// First condition: If both the start time and the end time are on the same day, check whether the current time is after the start time and also before the end time.
	// Second condition: If the start time and the end time are on different days, check if it is before the end time.
	if ((EndTimeSingle > StartTimeSingle && CurrentTimeSingle >= StartTimeSingle && CurrentTimeSingle < EndTimeSingle) || (EndTimeSingle < StartTimeSingle && (CurrentTimeSingle < EndTimeSingle || CurrentTimeSingle >= StartTimeSingle)))
	{
		EaseController(1);
	}
	
	// Deactivate the filter.
	// First condition: If the start and end times fall on the same day, check if the current time is before the start or after the end time.
	// Second condition: If the start time and end time are on separate days, check that the current time is after the end time and before the start time.	
	// This conditional is redundant since one of those conditions must be true if the above conditions are not, but it is included to show the logic.
	else if ((EndTimeSingle >= StartTimeSingle && (CurrentTimeSingle < StartTimeSingle || CurrentTimeSingle >= EndTimeSingle)) || (EndTimeSingle <= StartTimeSingle && CurrentTimeSingle >= EndTimeSingle && CurrentTimeSingle < StartTimeSingle))
	{
		EaseController(0);
	}
	return TRUE;
}


// Control the fade in and fade out of the filter
void EaseController(int Switch)
{	
	// If it is already easing then return and wait to send again
	if (Easing) return;
	
	// printf("It's checking the onoff switch\n");
	
	// If the on/off setting is the same as requested then the function does not need to run
	if (OnOff == Switch) return;
	
	// clamp the value between 1ms and 1 minute per step
	guint StepMS = (guint)(StepTime * 1000);
	if (StepMS < 1) StepMS = 1;
	else if (StepMS > 60000) StepMS = 60000;
		
	Counter = 1;
	Easing = 1;
	OnOff = Switch;

	if (Switch == 1) g_timeout_add(StepMS, EaseOnFunction, NULL);
	if (Switch == 0) g_timeout_add(StepMS, EaseOffFunction, NULL);	
}
	


gboolean EaseOnFunction(gpointer user_data)
{
	// This is a failsafe for settings adjustments during easing causing undefined behaviour
	if (!Easing) return FALSE;
	
    double ValuesTemp[4];
    
    // For each colour setting, subtract a fraction of the difference between the default setting and the user setting
    // The precise fraction is determined by the number of ease steps
    for (int i = 0; i < 4; i++)
    {
		ValuesTemp[i] = 1 - (1 - ColourValues[i]) / EaseSteps * Counter;
	}
	
	SetLight(ValuesTemp);
    Counter++;

    if (Counter > EaseSteps) 
    {
		Easing = 0;
		return FALSE;
	}
    
    return TRUE;
}


gboolean EaseOffFunction(gpointer user_data)
{
	// This is a failsafe for settings adjustments during easing causing undefined behaviour
	if (!Easing) return FALSE;
	
	double ValuesTemp[4];
	
    // For each colour setting, add a fraction of the difference between the user setting and the default setting
    // The precise fraction is determined by the number of ease steps	
	for (int i = 0; i < 4; i++)
	{
		ValuesTemp[i] = ColourValues[i] + (1 - ColourValues[i]) / EaseSteps * Counter;
	}

	SetLight(ValuesTemp);
    Counter++;

    if (Counter > EaseSteps) 
    {
		Easing = 0;
		return FALSE;
	}

    return TRUE;
}


// Set the colour values in real time based on adjustments to the sliders
void SliderChanged(GtkRange *range, gpointer Element) 
{
	int i = (int)(uintptr_t)Element;
	Easing = 0;
    ColourValues[i] = gtk_range_get_value(range);
    
    // Stop user from turning all colours off completely, leaving the screen blank
    // This was done rather than clamping values to allow maximum flexibility
    if (ColourValues[0] < 0.2 && ColourValues[1] < 0.2 && ColourValues[2] < 0.2)
    {
		ColourValues[i] = 0.2;
		gtk_range_set_value(GTK_RANGE(range), 0.2);
	}
	
	// Preview the new settings for 5 seconds
    OnOff = 1;
    SetLight(ColourValues);
    if (FilterMode == 0 || FilterMode == 1)
	{
		Waiting = 1;
		
		if (TimerID != 0)
		{
			g_source_remove(TimerID);
			TimerID = 0;
		}	
		TimerID = g_timeout_add_seconds(5, WaitFunction, NULL);
    }
}


gboolean WaitFunction(gpointer user_data)
{
	if (FilterMode == 0) EaseController(0);
	Waiting = 0;
	TimerID = 0;
	return FALSE;
}


// Open the help window
void HelpFunction(GtkWidget *widget)
{
	if (GTK_IS_WIDGET(HelpWindow)) return;
	
	HelpWindow = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(HelpWindow), "Help Window");
	gtk_window_set_default_size(GTK_WINDOW(HelpWindow), 500, -1);
	gtk_window_set_modal(GTK_WINDOW(HelpWindow), FALSE);
	GtkWidget *ContentArea = gtk_dialog_get_content_area(GTK_DIALOG(HelpWindow));
	
    GtkWidget *ContainerBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(ContentArea), ContainerBox);
	
	GtkWidget *TextBox = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(TextBox), GTK_WRAP_WORD);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(TextBox), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(TextBox), FALSE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(TextBox), 10);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(TextBox), 10);
	gtk_text_view_set_top_margin(GTK_TEXT_VIEW(TextBox), 10);
	gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(TextBox), 10);

	GtkTextBuffer *TextContent = gtk_text_view_get_buffer(GTK_TEXT_VIEW(TextBox));
	gtk_text_buffer_set_text(TextContent,
        "Adjust the sliders to set your desired colour intensity. If the filter is off, it will temporarily activate to preview your settings. "
        "Set the filter start and end times from the corresponding dropdown menus.\n\n"
        "Press the middle button to change the filter mode. The filter can be set to always on, always off, or 'Time' mode which activates and deactivates the filter based on the time of day. "
        "The config file found in the install folder contains more user settings. \n\n"
        "Right click the tray icon and select 'Exit' to close the program and reset all colour values. "
        "In case of undesired behaviour, click the 'Reset' button below to reset all settings to default values and close the program.", -1);

    gtk_box_pack_start(GTK_BOX(ContainerBox), TextBox, TRUE, TRUE, 0);
    gtk_widget_set_margin_bottom(TextBox, 10);
    
	GtkWidget *ButtonsBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(ContainerBox), ButtonsBox, FALSE, FALSE, 0);
	gtk_widget_set_margin_start(ButtonsBox, 10);
	gtk_widget_set_margin_end(ButtonsBox, 10);
	
	GtkWidget *ResetButton = gtk_button_new_with_label("Reset");
	g_signal_connect(ResetButton, "clicked", G_CALLBACK(ConfirmReset), NULL);
	gtk_box_pack_start(GTK_BOX(ButtonsBox), ResetButton, FALSE, FALSE, 0);

	GtkWidget *Spacer5 = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(ButtonsBox), Spacer5, TRUE, TRUE, 0);

	GtkWidget *BackButton = gtk_button_new_with_label("Back");
	g_signal_connect_swapped(BackButton, "clicked", G_CALLBACK(gtk_widget_destroy), HelpWindow);
	gtk_box_pack_start(GTK_BOX(ButtonsBox), BackButton, FALSE, FALSE, 0);	
	
	gtk_widget_set_size_request(BackButton, 80, -1);   
    gtk_widget_set_size_request(ResetButton, 80, -1);

    gtk_widget_show_all(HelpWindow);
}


// This function resets all settings to default, including the config file
// If the process is not exited cleanly it can cause issues upon restart, this reset fixes any potential problems
void ResetEverything()
{
	Easing = 0;
	FilterMode = 0;
	CreateConfigFile();
	SetLinearRamp();
	gtk_widget_destroy(ConfirmWindow);
	gtk_widget_destroy(HelpWindow);
	Exit(SettingsWindow);
}


void DisableOrEnable(GtkWidget *widget)
{
	if (FilterMode == 0)
	{
		gtk_button_set_label(GTK_BUTTON(widget), "Time");
		FilterMode = 1;
	}
	else if (FilterMode == 1)
	{
		gtk_button_set_label(GTK_BUTTON(widget), "On");
		FilterMode = 2;
		EaseController(1);
	}
	else
	{
		gtk_button_set_label(GTK_BUTTON(widget), "Off");
		FilterMode = 0;
		EaseController(0);
	}
}


void OKFunction()
{
	SaveConfig();
	gtk_widget_destroy(SettingsWindow);
}


// Window to confirm reset function
void ConfirmReset()
{
	if (GTK_IS_WIDGET(ConfirmWindow)) return;
	
	ConfirmWindow = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(ConfirmWindow), "Confirm Reset");
	gtk_window_set_modal(GTK_WINDOW(ConfirmWindow), FALSE);
	GtkWidget *ContentArea = gtk_dialog_get_content_area(GTK_DIALOG(ConfirmWindow));
	GtkWidget *ConfirmLabel = gtk_label_new("\n  Reset all settings to the default values?  \n");
	gtk_container_add(GTK_CONTAINER(ContentArea), ConfirmLabel);
	
	GtkWidget *ButtonsBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(ContentArea), ButtonsBox, FALSE, FALSE, 0);
	gtk_widget_set_margin_start(ButtonsBox, 10);
	gtk_widget_set_margin_end(ButtonsBox, 10);
	
	GtkWidget *ConfirmButton = gtk_button_new_with_label("Confirm");
	g_signal_connect(ConfirmButton, "clicked", G_CALLBACK(ResetEverything), NULL);
	gtk_box_pack_start(GTK_BOX(ButtonsBox), ConfirmButton, FALSE, FALSE, 0);

	GtkWidget *Spacer6 = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(ButtonsBox), Spacer6, TRUE, TRUE, 0);

	GtkWidget *CancelButton = gtk_button_new_with_label("Cancel");
	g_signal_connect_swapped(CancelButton, "clicked", G_CALLBACK(gtk_widget_destroy), ConfirmWindow);
	gtk_box_pack_start(GTK_BOX(ButtonsBox), CancelButton, FALSE, FALSE, 0);	
	
	gtk_widget_set_size_request(ConfirmButton, 80, -1);   
    gtk_widget_set_size_request(CancelButton, 80, -1);

    gtk_widget_show_all(ConfirmWindow);
}


void SaveConfig()
{
	config_t Config;
    config_init(&Config);
    if (!config_read_file(&Config, ConfigFileName)) CreateConfigFile();
    
    config_setting_t *ConfigRoot = config_root_setting(&Config);
    config_setting_t *ConfigSetting;
	
	ConfigSetting = config_setting_get_member(ConfigRoot, "RedValue");
	config_setting_set_float(ConfigSetting, ColourValues[0]);
	
	ConfigSetting = config_setting_get_member(ConfigRoot, "GreenValue");
	config_setting_set_float(ConfigSetting, ColourValues[1]);
	
	ConfigSetting = config_setting_get_member(ConfigRoot, "BlueValue");
	config_setting_set_float(ConfigSetting, ColourValues[2]);
	
	ConfigSetting = config_setting_get_member(ConfigRoot, "Brightness");
	config_setting_set_float(ConfigSetting, ColourValues[3]);
	
	ConfigSetting = config_setting_get_member(ConfigRoot, "StartHourInt");
	config_setting_set_int(ConfigSetting, StartHourInt);
    
	ConfigSetting = config_setting_get_member(ConfigRoot, "StartMinuteInt");
	config_setting_set_int(ConfigSetting, StartMinuteInt);
	
	ConfigSetting = config_setting_get_member(ConfigRoot, "EndHourInt");
	config_setting_set_int(ConfigSetting, EndHourInt);
    
	ConfigSetting = config_setting_get_member(ConfigRoot, "EndMinuteInt");
	config_setting_set_int(ConfigSetting, EndMinuteInt);
	
	ConfigSetting = config_setting_get_member(ConfigRoot, "FilterMode");
	config_setting_set_int(ConfigSetting, FilterMode);
	
	ConfigSetting = config_setting_get_member(ConfigRoot, "StepTime");
	config_setting_set_float(ConfigSetting, StepTime);		
	
	ConfigSetting = config_setting_get_member(ConfigRoot, "EaseSteps");
	config_setting_set_int(ConfigSetting, EaseSteps);
	
	ConfigSetting = config_setting_get_member(ConfigRoot, "CheckSeconds");
	config_setting_set_int(ConfigSetting, CheckSeconds);
	
	config_write_file(&Config, ConfigFileName);
	config_destroy(&Config);
}


void CreateConfigFile() 
{
    config_t Config;
    config_setting_t *root;

    config_init(&Config);

    root = config_root_setting(&Config);

    config_setting_t *ConfigSetting;

    ConfigSetting = config_setting_add(root, "RedValue", CONFIG_TYPE_FLOAT);
    config_setting_set_float(ConfigSetting, 1.0f);

    ConfigSetting = config_setting_add(root, "GreenValue", CONFIG_TYPE_FLOAT);
    config_setting_set_float(ConfigSetting, 0.6f);

    ConfigSetting = config_setting_add(root, "BlueValue", CONFIG_TYPE_FLOAT);
    config_setting_set_float(ConfigSetting, 0.4f);

    ConfigSetting = config_setting_add(root, "Brightness", CONFIG_TYPE_FLOAT);
    config_setting_set_float(ConfigSetting, 1.0f);

    ConfigSetting = config_setting_add(root, "StartHourInt", CONFIG_TYPE_INT);
    config_setting_set_int(ConfigSetting, 20);

    ConfigSetting = config_setting_add(root, "StartMinuteInt", CONFIG_TYPE_INT);
    config_setting_set_int(ConfigSetting, 0);

    ConfigSetting = config_setting_add(root, "EndHourInt", CONFIG_TYPE_INT);
    config_setting_set_int(ConfigSetting, 7);

    ConfigSetting = config_setting_add(root, "EndMinuteInt", CONFIG_TYPE_INT);
    config_setting_set_int(ConfigSetting, 0);
    
    ConfigSetting = config_setting_add(root, "FilterMode", CONFIG_TYPE_INT);
    config_setting_set_int(ConfigSetting, 1);
    
    ConfigSetting = config_setting_add(root, "StepTime", CONFIG_TYPE_FLOAT);
    config_setting_set_float(ConfigSetting, 0.1);
    
    ConfigSetting = config_setting_add(root, "EaseSteps", CONFIG_TYPE_INT);
    config_setting_set_int(ConfigSetting, 50);   
    
    ConfigSetting = config_setting_add(root, "CheckSeconds", CONFIG_TYPE_INT);
    config_setting_set_int(ConfigSetting, 20);    

    // Write to a new .cfg file
    if (!config_write_file(&Config, ConfigFileName)) 
    {
        fprintf(stderr, "Failed to write default config: %s\n", config_error_text(&Config));
    }

    config_destroy(&Config);
}
	

// Set the colour ramp to a linear gradient. Effectively a default setting to clear the filter in case of errors.
void SetLinearRamp()
{
	int MaxColourValue = 65535;
	
    for (int i = 1; i < GammaSize; i++) 
    {
		DefaultGamma->red[i] = (MaxColourValue * i) / (GammaSize - 1);
		DefaultGamma->green[i] = (MaxColourValue * i) / (GammaSize - 1);
		DefaultGamma->blue[i] = (MaxColourValue * i) / (GammaSize - 1);
    }
}
	

// Reset to default, free memory, exit program
void Exit(GtkWidget *widget) 
{
	double ResetColours[4] = {1, 1, 1, 1};
    SetLight(ResetColours);
    XRRFreeGamma(NewGammaRamp);
    XRRFreeGamma(DefaultGamma);
    XRRFreeScreenResources(ScreenResources);
    XCloseDisplay(DisplayPointer);
    gtk_main_quit();
}
