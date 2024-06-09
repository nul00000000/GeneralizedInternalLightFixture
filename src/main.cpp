#define FASTLED_ALL_PINS_HARDWARE_SPI
#define FASTLED_ESP32_SPI_BUS HSPI

#include <Arduino.h>
#include <Wifi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <FastLED.h>

#define LED_PIN 12
#define NUM_LEDS 298
#define NUM_NETS 3

#define MAX_COLOR_CYCLE 25

const char* ssid[NUM_NETS] = {
	"certified hood classic",
	"espconn"
};
const char* password[NUM_NETS] = {
	"886-crimson-921",
	"espcode69"
};

String html;

CRGB leds[NUM_LEDS];

// 0 - off
// 1 - static color
// 2 - rainbow
// 3 - color cycle
// 4 - direct pixel control
uint8_t mode = 0;

bool needsUpdate = false;

WiFiClient* client;

String stripName;

uint64_t lastSendTime = 0;

uint32_t dly = 4000;

//rainbow variables
float hue;
float speed;
float ledOffset;

//color cycle
float hues[MAX_COLOR_CYCLE];
float sats[MAX_COLOR_CYCLE];
float bris[MAX_COLOR_CYCLE];
float timeForUnit = 0;
uint8_t cycleLength = 0;
uint8_t currentIndex = 0;

int handshake() {
	Serial.println("Handshaking");
	client = new WiFiClient();
	// if(client->connect("monke.gay", 8585)) {
	if(client->connect("192.168.1.199", 8585)) {
		client->println(stripName);
		client->flush();

		//register and connect strip
		HTTPClient site;
		site.begin("https://monke.gay/gilf/action/connect/");
		// site.begin("http://192.168.137.1:8484/connect/");
		String body = "{\"name\":\"" + stripName + "\",\"numLeds\":" + NUM_LEDS + "}";
		int code = site.POST((uint8_t*)body.c_str(), body.length());//this works :smile:
		WiFiClient* stream = site.getStreamPtr();
		html = stream->readString();
		Serial.println("Handshake succeeded: " + html);
		return 1;
	} else {
		Serial.println("Handshake failed: " + client->getWriteError());
		return 0;
	}
}

void setup() {
	Serial.begin(115200);

	FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);

	// // Connect to Wi-Fi network with SSID and password
	bool connected = false;
	for(int i = 0; i < NUM_NETS && !connected; i++) {
		Serial.print("Connecting to ");
		Serial.print(ssid[i]);
		WiFi.begin(ssid[i], password[i]);
		for(int j = 0; j < 10; j++) {
			if(WiFi.status() != WL_CONNECTED) {
				delay(500);
				Serial.print(".");
			} else {
				connected = true;
			}
		}

	}
	if(!connected) {
		Serial.println("Could not connect to network");
	}
	// Print local IP address and start web server
	Serial.println("");
	Serial.println("WiFi connected.");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	stripName = (String)"esp" + micros();

	while(!handshake()) {
		delayMicroseconds(1000000);
	}

	Serial.println(stripName);
	Serial.println(dly);

	FastLED.setBrightness(70);
}

int lastA = 0;

void HSVtoRGB(float h, float S, float V, float& R, float& G, float& B) {
    if(S>100 || S<0 || V>100 || V<0){
		R = 30;
		G = 0;
		B = 30;
        return;
    }
    float s = S/100;
    float v = V/100;
    float C = s*v;
	float H = fmod(h, 360.0f);
    float X = C*(1-abs(fmod(H/60.0, 2)-1));
    float m = v-C;
    float r,g,b;
    if(H >= 0 && H < 60){
        r = C,g = X,b = 0;
    }
    else if(H >= 60 && H < 120){
        r = X,g = C,b = 0;
    }
    else if(H >= 120 && H < 180){
        r = 0,g = C,b = X;
    }
    else if(H >= 180 && H < 240){
        r = 0,g = X,b = C;
    }
    else if(H >= 240 && H < 300){
        r = X,g = 0,b = C;
    }
    else{
        r = C,g = 0,b = X;
    }
    R = (r+m)*255;
    G = (g+m)*255;
    B = (b+m)*255;
}

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

float parseFloat(WiFiClient* client) {
	uint8_t buf[4];
	for(int i = 0; i < 4; i++) {
		buf[3 - i] = client->read();
	}
	return * (float*)buf;
}

void handleCommand() {
	if(mode == 0) {
		Serial.println("Off Set");
		FastLED.clear();
		needsUpdate = false;
	} else if(mode == 1) {
		if(client->available() >= 3) {
			uint8_t r = client->read();
			uint8_t g = client->read();
			uint8_t b = client->read();
			for(int i = 0; i < NUM_LEDS; i++) {
				leds[i] = CRGB(r, g, b);
			}
			Serial.println("Static Set: " + (String) r + " " + (String) g + " " + (String) b);
			needsUpdate = false;
		} else {
			needsUpdate = true;
		}
	} else if(mode == 2) {
		if(client->available() >= 8) {
			speed = parseFloat(client);
			ledOffset = parseFloat(client);
			needsUpdate = false;
			Serial.println("Rainbow Set: " + (String) speed + " " + (String) ledOffset);
		} else {
			needsUpdate = true;
		}
	} else if(mode == 3) {
		if(client->available() >= MAX_COLOR_CYCLE * 3 + 9) {
			// timeForUnit = ((float) client->read() * 60.0f / 256.0f);
			// ledOffset = ((float) client->read() * 360.0f / 256.0f - 180.0f) / 100.0f;
			timeForUnit = parseFloat(client);
			ledOffset = parseFloat(client);
			cycleLength = (uint8_t) client->read();
			for(int i = 0; i < MAX_COLOR_CYCLE; i++) {
				hues[i] = (float) client->read() * 360.0f / 255.0f;
				sats[i] = (float) client->read() * 100.0f / 255.0f;
				bris[i] = (float) client->read() * 100.0f / 255.0f;
			}
			Serial.println("Sequence: " + (String) speed + " " + (String) ledOffset + " " + (String) cycleLength);
			for(int i = 0; i < NUM_LEDS; i++) {
					float r = 0;
					float g = 0;
					float b = 0;
					HSVtoRGB(hues[0], sats[0], sats[0], r, g, b);
					leds[i] = CRGB(r, g, b);
				}
			needsUpdate = false;
		} else {
			needsUpdate = true;
		}
	} else if(mode == 4) {
		if(client->available() >= NUM_LEDS * 3) {
			Serial.println("Direct Set");
			for(int i = 0; i < NUM_LEDS; i++) {
				leds[i] = CRGB((uint8_t) client->read(), (uint8_t) client->read(), (uint8_t) client->read());
			}
			needsUpdate = false;
		} else {
			needsUpdate = true;
		}
	}
}

float getHueInCycle(double time) {
	float t = fmod(time, timeForUnit * cycleLength);
	int index = (int) floor(t / timeForUnit);
	double sub = t / timeForUnit - index;
	if(index >= cycleLength - 1) {
		return hues[index] * (1 - sub) + hues[0] * sub;
	} else {
		return hues[index] * (1 - sub) + hues[index + 1] * sub;
	}
}

float getSatInCycle(double time) {
	float t = fmod(time, timeForUnit * cycleLength);
	int index = (int) floor(t / timeForUnit);
	double sub = t / timeForUnit - index;
	if(index >= cycleLength - 1) {
		return sats[index] * (1 - sub) + sats[0] * sub;
	} else {
		return sats[index] * (1 - sub) + sats[index + 1] * sub;
	}
}

float getBriInCycle(double time) {
	float t = fmod(time, timeForUnit * cycleLength);
	int index = (int) floor(t / timeForUnit);
	double sub = t / timeForUnit - index;
	if(index >= cycleLength - 1) {
		return bris[index] * (1 - sub) + bris[0] * sub;
	} else {
		return bris[index] * (1 - sub) + bris[index + 1] * sub;
	}
}

void loop() {
	if(client->connected()) {
		if(millis() > lastSendTime + 1000) {
			lastSendTime = millis();
			client->println("alive");
		}
		int a = client->available();
		if(needsUpdate) {
			handleCommand();
		} else if(a >= 1) {
			uint8_t m = client->read();
			mode = m;
			handleCommand();
			FastLED.show();
			if(dly == 0) {
				dly = 1;
			}
		}
		lastA = a;
	} else {
		if(millis() > lastSendTime + 1000) {
			lastSendTime = millis();
			handshake();
		}
	}
	if(mode == 2 && !needsUpdate) {
		for(int i = 0; i < NUM_LEDS; i++) {
			float r = 0;
			float g = 0;
			float b = 0;
			HSVtoRGB(hue + ledOffset * i, 100.0f, 100.0f, r, g, b);
			leds[i] = CRGB((uint8_t) r, (uint8_t) g, (uint8_t) b);
		}
		hue += speed;
		if(hue > 360.0f) {
			hue -= 360.0f;
		} else if(hue < 0.0f) {
			hue += 360.0f;
		}
		FastLED.show();
	} else if(mode == 3 && !needsUpdate) {
		double time = micros() / 1000000.0;
		// double v = (sin((time - lastStart) / (PI * 2) * timeForUnit) + 1.0) / 2.0;
		for(int i = 0; i < NUM_LEDS; i++) {
			float r = 0;
			float g = 0;
			float b = 0;
			HSVtoRGB(getHueInCycle(time + ledOffset * i), getSatInCycle(time + ledOffset * i), getBriInCycle(time + ledOffset * i), r, g, b);
			leds[i] = CRGB((uint8_t) r, (uint8_t) g, (uint8_t) b);
		}
		FastLED.show();
	}
	delayMicroseconds(dly);
}