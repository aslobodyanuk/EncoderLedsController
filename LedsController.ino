#include <FastLED.h>
#include <OneButton.h>
#include <EEPROMVar.h>
#include <EEPROMex.h>
#include <Encoder.h>

//Data types for leds controller

#define SETTINGS_MODE_MAX_VALUE 4

enum SettingsMode {
	Brightness = 1,
	Color,
	ColorAnimation,
	RainbowAnimation
};

enum ColorLoopState {
	white,
	minusRed,
	plusRed,
	minusGreen,
	plusGreen,
	minusBlue,
	plusBlue
};

//End of data types

#define ENCODER_BUTTON_PIN 2
#define LEDS_COUNT 15
#define LEDS_DATA_PIN 9

#define ENCODER_BRIGHTNESS_MULTIPLIER 4
#define ENCODER_BRIGNTNESS_SPEED 10
#define ENCODER_COLOR_SPEED 20

#define SETTINGS_MENU_BRIGHTNESS_IF_ZERO 20
#define SETTINGS_MENU_DISPLAY_TIME_MILLIS 500
#define SETTINGS_FADE_OUT_SPEED_MILLIS 75

#define SMOOTH_FADE_DURATION 800

#define COLOR_ANIMATION_SPEED 10
#define ENCODER_COLOR_ANIMATION_SPEED 3
#define MAX_COLOR_ANIMATION_TIME 60
#define MIN_COLOR_ANIMATION_TIME 1

#define RAINBOW_ANIMATION_SPEED 20
#define RAINBOW_ANIMATION_PLUS_FACTOR 3

//Encoder variables
long _encoderPosition = 0;
long _previousEncoderPosition = 0;

long _tempEncoderPosition = 0;
bool _tempEncoderReset = false;

//Color settings
bool _isWhite = true;
byte _r = 255;
byte _g = 255;
byte _b = 255;
byte _ledsBrightness = 20;

//Settings menu variables
bool _isDisplayingCurrentSetting = false;
long _currentSettingDisplayExecutionTime;
bool _isDisplayingFade = false;
long _currentSettingFadeExecutionTime;
int _currentFadePlusLed = 0;
bool _isResetNeeded = true;

//Major board settings
bool _isEnabled = false;
bool _isInSettingsMode = false;
SettingsMode _currentMode = Brightness;
ColorLoopState _colorLoopState = plusGreen;

Encoder _encoderWheel(3, 4);
OneButton _encoderButton(ENCODER_BUTTON_PIN, true);
CRGB _ledsData[LEDS_COUNT];

//Smooth fade in and fade out variables
int _smoothFadeMillisTime;

bool _executeSmoothStart = false;
long _smoothStartLastExec;
byte _smoothStartTempLedsBrightness = 0;

bool _executeSmoothStop = false;
long _smoothStopLastExec;
byte _smoothStopTempLedsBrightness = 0;

//Color animation variables
int _colorAnimationTime = 60;
int _tempColorAnimationTime;
long _colorAnimationLastExec;
ColorLoopState _colorAnimationLoopState = plusGreen;
CRGB _colorAnimationColor = CRGB(255, 0, 0);

//Rainbow animation variables
CRGBPalette16 _rainbowPallete = RainbowColors_p;
TBlendType    _currentBlending = LINEARBLEND;
uint8_t _rainbowAnimationIndex = 0;
long _rainbowAnimationLastExecTime;

void setup() {
	Serial.begin(9600);

	pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);  
	_encoderButton.attachLongPressStart(encoderButtonLongPressed);
	_encoderButton.attachClick(encoderButtonClick);
	
	pinMode(LEDS_DATA_PIN, OUTPUT);
	FastLED.addLeds<WS2812B, LEDS_DATA_PIN, RGB>(_ledsData, LEDS_COUNT);

	initializeMemory();

	FastLED.setBrightness(0);
	setColorForAllLeds(_r, _g, _b);

	_currentSettingDisplayExecutionTime = millis();
	_currentSettingFadeExecutionTime = millis();
	_smoothStartLastExec = millis();

	_colorAnimationLastExec = millis();	
	_tempColorAnimationTime = _colorAnimationTime;

	_rainbowAnimationLastExecTime = millis();

	_smoothFadeMillisTime = SMOOTH_FADE_DURATION / _ledsBrightness;	
}

void loop()
{
	_encoderButton.tick();
	processDisplayOfCurrentSetting();
	processSmoothFade();

	_previousEncoderPosition = _encoderPosition;
	_encoderPosition = _encoderWheel.read();
	
	if (_encoderPosition < _previousEncoderPosition)
	{
		_tempEncoderPosition--;
	}
	if (_encoderPosition > _previousEncoderPosition)
	{
		_tempEncoderPosition++;
	}

	if (_isEnabled == true)
	{
		processColorAnimation();
		processRainbowAnimaton();
	}

	if (_isEnabled == true && _isInSettingsMode == true)
	{
		switch (_currentMode)
		{
			case Brightness:
			{
				processBrightness();
				break;
			}
			case Color:
			{
				processColor();
				break;
			}
			case ColorAnimation:
			{
				hanleSettingsForColorAnimation();
				break;
			}
			case RainbowAnimation:
			{
				processRainbowAnimaton();
				break;
			}
			default:
				break;
		}
	}	
}

void initializeMemory()
{
	_r = EEPROM.readByte(0);
	_g = EEPROM.readByte(1);
	_b = EEPROM.readByte(2);
	_ledsBrightness = EEPROM.readByte(3);
	byte colorLoopStateInt = EEPROM.readByte(4);
	_colorAnimationTime = EEPROM.readInt(6);
	_colorLoopState = (ColorLoopState)colorLoopStateInt;
	byte settingMode = EEPROM.readByte(5);
	_currentMode = (SettingsMode)settingMode;
	if (_r == 255 && _g == 255 && _b == 255)
	{
		_isWhite = true;
	}
	else
	{
		_isWhite = false;
	}
	
	Serial.println(_r);
	Serial.println(_g);
	Serial.println(_b);
	Serial.println(_colorLoopState);
	Serial.println(_ledsBrightness);
	Serial.println(_isWhite);
	Serial.println(_colorAnimationTime);
	Serial.println(settingMode);
}

void writeToMemory()
{
	EEPROM.writeByte(0, _r);
	EEPROM.writeByte(1, _g);
	EEPROM.writeByte(2, _b);
	EEPROM.writeByte(3, _ledsBrightness);
	int colorLoopState = _colorLoopState;
	EEPROM.writeByte(4, colorLoopState);
	int settingModeState = _currentMode;
	EEPROM.writeByte(5, settingModeState);
	EEPROM.writeInt(6, _colorAnimationTime);
}

void processColor()
{
	if (_tempEncoderPosition >= 4)
	{
		_tempEncoderPosition = 0;
		_tempEncoderReset = true;

		colorLoop(true);
	}

	if (_tempEncoderPosition <= -4)
	{
		_tempEncoderPosition = 0;
		_tempEncoderReset = true;

		colorLoop(false);
	}

	if (_tempEncoderReset)
	{
		Serial.print(_r);
		Serial.print(" ");
		Serial.print(_g);
		Serial.print(" ");
		Serial.print(_b);
		Serial.println(" ");

		writeToMemory();
		setColorForAllLeds(_r, _g, _b);
		_tempEncoderReset = false;
	}
}

void colorLoop(bool direction)
{
	if (_isWhite == true)
	{
		_r = 255;
		_g = 0;
		_b = 0;
		_isWhite = false;
		return;
	}
	if (direction == true)
	{		
		if (_colorLoopState == plusGreen) {
			if (_g + ENCODER_COLOR_SPEED > 255)
			{
				_g = 255;
			}
			else
			{
				_g = _g + ENCODER_COLOR_SPEED;
			}
			if (_g == 255) {
				_colorLoopState = minusRed;
			}
		}		
		if (_colorLoopState == minusRed) {
			if (_r - ENCODER_COLOR_SPEED < 0)
			{
				_r = 0;
			}
			else
			{
				_r = _r - ENCODER_COLOR_SPEED;
			}
			if (_r == 0) {
				_colorLoopState = plusBlue;
			}
		}
		if (_colorLoopState == plusBlue) {
			if (_b + ENCODER_COLOR_SPEED > 255)
			{
				_b = 255;
			}
			else
			{
				_b = _b + ENCODER_COLOR_SPEED;
			}
			if (_b == 255) {
				_colorLoopState = minusGreen;
			}
		}
		if (_colorLoopState == minusGreen) {
			if (_g - ENCODER_COLOR_SPEED < 0)
			{
				_g = 0;
			}
			else
			{
				_g = _g - ENCODER_COLOR_SPEED;
			}
			if (_g == 0) {
				_colorLoopState = plusRed;
			}
		}
		if (_colorLoopState == plusRed) {
			if (_r + ENCODER_COLOR_SPEED > 255)
			{
				_r = 255;
			}
			else
			{
				_r = _r + ENCODER_COLOR_SPEED;
			}
			if (_r == 255) {
				_colorLoopState = minusBlue;
			}
		}
		if (_colorLoopState == minusBlue) {
			if (_b - ENCODER_COLOR_SPEED < 0)
			{
				_b = 0;
			}
			else
			{
				_b = _b - ENCODER_COLOR_SPEED;
			}
			if (_b == 0) {
				_colorLoopState = white;
				_isWhite = true;
			}
		}
		if (_colorLoopState == white)
		{
			_r = 255;
			_g = 255;
			_b = 255;
			_colorLoopState = plusGreen;
			_isWhite = true;
		}
	}
	else
	{
		if (_colorLoopState == minusBlue) {
			if (_b + ENCODER_COLOR_SPEED > 255)
			{
				_b = 255;
			}
			else
			{
				_b = _b + ENCODER_COLOR_SPEED;
			}
			if (_b == 255) {
				_colorLoopState = plusRed;
			}
		}
		if (_colorLoopState == plusBlue) {			
			if (_b - ENCODER_COLOR_SPEED < 0)
			{
				_b = 0;
			}
			else
			{
				_b = _b - ENCODER_COLOR_SPEED;
			}
			if (_b == 0) {
				_colorLoopState = minusRed;
			}
		}
		if (_colorLoopState == minusRed) {			
			if (_r + ENCODER_COLOR_SPEED > 255)
			{
				_r = 255;
			}
			else
			{
				_r = _r + ENCODER_COLOR_SPEED;
			}
			if (_r == 255) {
				_colorLoopState = plusGreen;
			}
		}
		if (_colorLoopState == plusGreen) {
			if (_g - ENCODER_COLOR_SPEED < 0)
			{
				_g = 0;
			}
			else
			{
				_g = _g - ENCODER_COLOR_SPEED;
			}
			if (_g == 0) {
				_colorLoopState = white;
			}
		}		
		if (_colorLoopState == plusRed) {
			if (_r - ENCODER_COLOR_SPEED < 0)
			{
				_r = 0;
			}
			else
			{
				_r = _r - ENCODER_COLOR_SPEED;
			}
			if (_r == 0) {
				_colorLoopState = minusGreen;
			}
		}
		if (_colorLoopState == minusGreen) {
			if (_g + ENCODER_COLOR_SPEED > 255)
			{
				_g = 255;
			}
			else
			{
				_g = _g + ENCODER_COLOR_SPEED;
			}
			if (_g == 255) {
				_colorLoopState = plusBlue;
			}
		}
		if (_colorLoopState == white)
		{
			_r = 255;
			_g = 255;
			_b = 255;
			_colorLoopState = minusBlue;
			_isWhite = true;
		}
	}
}

void hanleSettingsForColorAnimation()
{
	if (_tempEncoderPosition >= 4)
	{
		_tempEncoderPosition = 0;
		_tempEncoderReset = true;

		if (_colorAnimationTime < MAX_COLOR_ANIMATION_TIME - ENCODER_COLOR_ANIMATION_SPEED)
		{
			_tempColorAnimationTime = _tempColorAnimationTime + ENCODER_COLOR_ANIMATION_SPEED;
		}
		else
		{
			_tempColorAnimationTime = MAX_COLOR_ANIMATION_TIME;
		}
	}

	if (_tempEncoderPosition <= -4)
	{
		_tempEncoderPosition = 0;
		_tempEncoderReset = true;

		if (_tempColorAnimationTime > MIN_COLOR_ANIMATION_TIME)
		{
			_tempColorAnimationTime = _tempColorAnimationTime - ENCODER_COLOR_ANIMATION_SPEED;
		}
		else
		{
			_tempColorAnimationTime = 1;
		}
	}

	if (_tempEncoderReset)
	{
		Serial.println(_colorAnimationTime);
		writeToMemory();
		_colorAnimationTime = _tempColorAnimationTime;
		_tempEncoderReset = false;
	}
}

void processColorAnimation()
{
	if (_currentMode == ColorAnimation)
	{
		if (millis() - _colorAnimationLastExec > _colorAnimationTime)
		{
			_colorAnimationColor = localColorLoop(true, _colorAnimationColor);
			setColorForAllLeds(_colorAnimationColor.r, _colorAnimationColor.g, _colorAnimationColor.b);
			_colorAnimationLastExec = millis();
		}
	}
}

CRGB localColorLoop(bool direction, CRGB currentColor)
{
	if (direction == true)
	{
		if (_colorAnimationLoopState == plusGreen) {
			if (currentColor.g + COLOR_ANIMATION_SPEED > 255)
			{
				currentColor.g = 255;
			}
			else
			{
				currentColor.g = currentColor.g + COLOR_ANIMATION_SPEED;
			}
			if (currentColor.g == 255) {
				_colorAnimationLoopState = minusRed;
			}
		}
		if (_colorAnimationLoopState == minusRed) {
			if (currentColor.r - COLOR_ANIMATION_SPEED < 0)
			{
				currentColor.r = 0;
			}
			else
			{
				currentColor.r = currentColor.r - COLOR_ANIMATION_SPEED;
			}
			if (currentColor.r == 0) {
				_colorAnimationLoopState = plusBlue;
			}
		}
		if (_colorAnimationLoopState == plusBlue) {
			if (currentColor.b + COLOR_ANIMATION_SPEED > 255)
			{
				currentColor.b = 255;
			}
			else
			{
				currentColor.b = currentColor.b + COLOR_ANIMATION_SPEED;
			}
			if (currentColor.b == 255) {
				_colorAnimationLoopState = minusGreen;
			}
		}
		if (_colorAnimationLoopState == minusGreen) {
			if (currentColor.g - COLOR_ANIMATION_SPEED < 0)
			{
				currentColor.g = 0;
			}
			else
			{
				currentColor.g = currentColor.g - COLOR_ANIMATION_SPEED;
			}
			if (currentColor.g == 0) {
				_colorAnimationLoopState = plusRed;
			}
		}
		if (_colorAnimationLoopState == plusRed) {
			if (currentColor.r + COLOR_ANIMATION_SPEED > 255)
			{
				currentColor.r = 255;
			}
			else
			{
				currentColor.r = currentColor.r + COLOR_ANIMATION_SPEED;
			}
			if (currentColor.r == 255) {
				_colorAnimationLoopState = minusBlue;
			}
		}
		if (_colorAnimationLoopState == minusBlue) {
			if (currentColor.b - COLOR_ANIMATION_SPEED < 0)
			{
				currentColor.b = 0;
			}
			else
			{
				currentColor.b = currentColor.b - COLOR_ANIMATION_SPEED;
			}
			if (currentColor.b == 0) {
				_colorAnimationLoopState = plusGreen;
				_isWhite = true;
			}
		}
	}
	else
	{
		if (_colorAnimationLoopState == minusBlue) {
			if (currentColor.b + COLOR_ANIMATION_SPEED > 255)
			{
				currentColor.b = 255;
			}
			else
			{
				currentColor.b = currentColor.b + COLOR_ANIMATION_SPEED;
			}
			if (currentColor.b == 255) {
				_colorAnimationLoopState = plusRed;
			}
		}
		if (_colorAnimationLoopState == plusBlue) {
			if (currentColor.b - COLOR_ANIMATION_SPEED < 0)
			{
				currentColor.b = 0;
			}
			else
			{
				currentColor.b = currentColor.b - COLOR_ANIMATION_SPEED;
			}
			if (currentColor.b == 0) {
				_colorAnimationLoopState = minusRed;
			}
		}
		if (_colorAnimationLoopState == minusRed) {
			if (currentColor.r + COLOR_ANIMATION_SPEED > 255)
			{
				currentColor.r = 255;
			}
			else
			{
				currentColor.r = currentColor.r + COLOR_ANIMATION_SPEED;
			}
			if (currentColor.r == 255) {
				_colorAnimationLoopState = plusGreen;
			}
		}
		if (_colorAnimationLoopState == plusGreen) {
			if (currentColor.g - COLOR_ANIMATION_SPEED < 0)
			{
				currentColor.g = 0;
			}
			else
			{
				currentColor.g = currentColor.g - COLOR_ANIMATION_SPEED;
			}
			if (currentColor.g == 0) {
				_colorAnimationLoopState = minusBlue;
			}
		}
		if (_colorAnimationLoopState == plusRed) {
			if (currentColor.r - COLOR_ANIMATION_SPEED < 0)
			{
				currentColor.r = 0;
			}
			else
			{
				currentColor.r = currentColor.r - COLOR_ANIMATION_SPEED;
			}
			if (currentColor.r == 0) {
				_colorAnimationLoopState = minusGreen;
			}
		}
		if (_colorAnimationLoopState == minusGreen) {
			if (currentColor.g + COLOR_ANIMATION_SPEED > 255)
			{
				currentColor.g = 255;
			}
			else
			{
				currentColor.g = currentColor.g + COLOR_ANIMATION_SPEED;
			}
			if (currentColor.g == 255) {
				_colorAnimationLoopState = plusBlue;
			}
		}
	}
	return CRGB(currentColor.r, currentColor.g, currentColor.b);
}

void encoderButtonLongPressed()
{
	if (_isEnabled == true)
	{
		_isInSettingsMode = !_isInSettingsMode;
		
		if (_isInSettingsMode == true)
			showCurrentSetting(0);
		else
			_executeSmoothStart = true;
	}
}

void encoderButtonClick()
{
	if (_isEnabled == true)
	{
		if (_isInSettingsMode == true)
		{
			int numMode = _currentMode;
			numMode++;

			if (numMode > SETTINGS_MODE_MAX_VALUE)
			{
				numMode = 1;
			}
			_currentMode = (SettingsMode)numMode;

			writeToMemory();
			_isResetNeeded = true;
			showCurrentSetting(0);
		}
		else
		{
			_executeSmoothStart = false;
			_smoothStopTempLedsBrightness = _ledsBrightness;
			_executeSmoothStop = true;
			_isEnabled = false;
		}
	}
	else
	{
		_executeSmoothStop = false;
		_smoothStartTempLedsBrightness = 0;
		_executeSmoothStart = true;
		_isEnabled = true;
	}
}

void processSmoothFade()
{
	if (_executeSmoothStart == true)
	{
		if (millis() - _smoothStartLastExec > _smoothFadeMillisTime)
		{
			_smoothStartTempLedsBrightness++;
			if (_smoothStartTempLedsBrightness <= _ledsBrightness)
			{
				FastLED.setBrightness(_smoothStartTempLedsBrightness);
				FastLED.show();
			}
			else
			{
				_smoothStartTempLedsBrightness = 0;
				_executeSmoothStart = false;
			}
			if (_smoothStartTempLedsBrightness == 255)
			{
				_smoothStartTempLedsBrightness = 0;
				_executeSmoothStart = false;
			}
			_smoothStartLastExec = millis();
		}
	}			
	
	if (_executeSmoothStop == true)
	{
		if (millis() - _smoothStopLastExec > _smoothFadeMillisTime)
		{			
			_smoothStopTempLedsBrightness--;
			if (_smoothStopTempLedsBrightness >= 0 && _smoothStopTempLedsBrightness <= _ledsBrightness)
			{
				FastLED.setBrightness(_smoothStopTempLedsBrightness);
				FastLED.show();
			}
			if (_smoothStopTempLedsBrightness == 0)
			{				
				_executeSmoothStop = false;
			}
			_smoothStopLastExec = millis();
		}
	}
}

void processDisplayOfCurrentSetting()
{
	if (_isResetNeeded == true)
	{
		_currentSettingDisplayExecutionTime = millis();
		_isDisplayingFade = false;
		_isResetNeeded = false;
	}
	if (_isDisplayingCurrentSetting == true && _isDisplayingFade == false && millis() - _currentSettingDisplayExecutionTime > SETTINGS_MENU_DISPLAY_TIME_MILLIS)
	{
		_isDisplayingFade = true;
		_currentFadePlusLed = 0;
	}
	if (_isDisplayingFade == true && millis() - _currentSettingFadeExecutionTime > SETTINGS_FADE_OUT_SPEED_MILLIS)
	{
		if (_currentFadePlusLed > LEDS_COUNT / 2 + 1)
		{
			_isDisplayingCurrentSetting = false;
			_isDisplayingFade = false;
			setColorForAllLeds(_r, _g, _b);
		}
		else
		{
			showCurrentSetting(_currentFadePlusLed);
			_currentFadePlusLed++;
			_currentSettingFadeExecutionTime = millis();
		}
	}
}

void showCurrentSetting(int plusLeds)
{
	bool ignoreGeneralColors = _currentMode == RainbowAnimation;
	CRGB currentColor = CRGB(_r, _g, _b);
	if (_currentMode == ColorAnimation)
		currentColor = _colorAnimationColor;

	_isDisplayingCurrentSetting = true;
	_currentSettingDisplayExecutionTime = millis();

	if (_ledsBrightness <= ENCODER_BRIGNTNESS_SPEED * ENCODER_BRIGHTNESS_MULTIPLIER)
	{
		_ledsBrightness = SETTINGS_MENU_BRIGHTNESS_IF_ZERO;
		FastLED.setBrightness(_ledsBrightness);
	}

	int currentModeNum = _currentMode;
	int currentLedCount = currentModeNum + plusLeds;
	for (int counter = 0; counter < LEDS_COUNT; counter++)
	{
		if (currentLedCount > 0)
		{
			_ledsData[counter].red = currentColor.r;
			_ledsData[counter].green = currentColor.g;
			_ledsData[counter].blue = currentColor.b;
			currentLedCount--;
		}
		else
		{
			_ledsData[counter].red = 0;
			_ledsData[counter].green = 0;
			_ledsData[counter].blue = 0;
		}
	}

	currentLedCount = currentModeNum + plusLeds;
	for (int counter = LEDS_COUNT - 1; counter > 0; counter--)
	{
		if (currentLedCount > 0)
		{
			_ledsData[counter].red = currentColor.r;
			_ledsData[counter].green = currentColor.g;
			_ledsData[counter].blue = currentColor.b;
			currentLedCount--;
		}
		else
		{
			break;
		}
	}

	if (ignoreGeneralColors == false)
		FastLED.show();
}

void setColorForAllLeds(byte r, byte g, byte b) {
	if (_isDisplayingCurrentSetting)
	{
		for (int counter = 0; counter < LEDS_COUNT; counter++) {
			if (isLedBlack(_ledsData[counter]))
				continue;

			_ledsData[counter].red = r;
			_ledsData[counter].green = g;
			_ledsData[counter].blue = b;
		}
	}
	else
	{
		for (int counter = 0; counter < LEDS_COUNT; counter++) {
			_ledsData[counter].red = r;
			_ledsData[counter].green = g;
			_ledsData[counter].blue = b;
		}
	}
	FastLED.show();
}

bool isLedBlack(CRGB ledValue)
{
	if (ledValue.r == 0 && ledValue.g == 0 && ledValue.b == 0)
		return true;
	else
		return false;
}

void processBrightness()
{
	if (_tempEncoderPosition >= 4)
	{
		_tempEncoderPosition = 0;
		_tempEncoderReset = true;

		if (_ledsBrightness < 255 - ENCODER_BRIGNTNESS_SPEED)
		{
			_ledsBrightness = _ledsBrightness + ENCODER_BRIGNTNESS_SPEED;
		}
		else
		{
			_ledsBrightness = 255;
		}
	}

	if (_tempEncoderPosition <= -4)
	{
		_tempEncoderPosition = 0;
		_tempEncoderReset = true;

		if (_ledsBrightness > ENCODER_BRIGNTNESS_SPEED)
		{
			_ledsBrightness = _ledsBrightness - ENCODER_BRIGNTNESS_SPEED;
		}
		else
		{
			_ledsBrightness = 0;
		}
	}

	if (_tempEncoderReset)
	{
		if (_ledsBrightness < ENCODER_BRIGNTNESS_SPEED * ENCODER_BRIGHTNESS_MULTIPLIER)
		{
			_smoothFadeMillisTime = SMOOTH_FADE_DURATION / _ledsBrightness;
			FastLED.setBrightness(0);
			FastLED.show();
			writeToMemory();
			Serial.print("[B][0] ");
		}

		if (_ledsBrightness >= ENCODER_BRIGNTNESS_SPEED * ENCODER_BRIGHTNESS_MULTIPLIER)
		{
			_smoothFadeMillisTime = SMOOTH_FADE_DURATION / _ledsBrightness;
			FastLED.setBrightness(_ledsBrightness);
			FastLED.show();
			writeToMemory();
			Serial.print("[B][*] ");
		}

		Serial.println(_ledsBrightness);
		_tempEncoderReset = false;
	}
}

void processRainbowAnimaton()
{
	if (_currentMode == RainbowAnimation)
	{
		if (millis() - _rainbowAnimationLastExecTime > RAINBOW_ANIMATION_SPEED)
		{
			fillLEDsFromPaletteColors(_rainbowAnimationIndex);
			_rainbowAnimationIndex++;
			FastLED.show();
			_rainbowAnimationLastExecTime = millis();
		}
	}
}

void fillLEDsFromPaletteColors(uint8_t colorIndex)
{
	for (int counter = 0; counter < LEDS_COUNT; counter++) {
		if (_isDisplayingCurrentSetting == true && isLedBlack(_ledsData[counter]))
			continue;

		CRGB colorFromPallete = ColorFromPalette(_rainbowPallete, colorIndex, 255, _currentBlending);
		_ledsData[counter] = CRGB(colorFromPallete.r, colorFromPallete.g, colorFromPallete.b);
		colorIndex += RAINBOW_ANIMATION_PLUS_FACTOR;
	}
}