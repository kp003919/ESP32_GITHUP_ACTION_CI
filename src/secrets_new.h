#include <pgmspace.h>
 
// ====== secrets_new.h ======
#define SECRET_NEW
 
// WiFi credentials
#define WIFI_SSID     "VM1080293"
#define WIFI_PASSWORD "Omidmuhsin2015"


// MQTT client ID
#define Client_ID "ESP32_ID_1"

 
// AWS IoT endpoint
// Copy from AWS IoT Core console
// Replace with your AWS IoT endpoint
// Example: "your-endpoint-ats.iot.your-region.amazonaws.com"
const char AWS_IOT_ENDPOINT[] = "you endpoint"; 
 
// Copy contents from AmazonRootCA1.pem here ▼  
// CA certificate
// Use triple quotes R"EOF(  )EOF" to handle multi-line strings
// Store in PROGMEM to save RAM
// Using raw string literals for multi-line strings
// Copy contents from AmazonRootCA1.pem here ▼
   // CA certificate for my local Mosquitto broker (public, but kept with the other secrets)
   static const char MQTT_CA_CERT[] PROGMEM = R"EOF(
  -----BEGIN CERTIFICATE-----
MIIDCDCCAfCgAwIBAgIUdng7DocmkTZV6YU1t3gexhBgb7MwDQYJKoZIhvcNAQEL
BQAwHDEaMBgGA1UEAwwRQ3VyZSBJb1QgTG9jYWwgQ0EwHhcNMjYxMDI2MDk0NTIy
WhcNMzYxMDIzMDk0NTIyWjAcMRowGAYDVQQDDBFDdXJlIElvVCBMb2NhbCBDQTCC
ASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANO9Z5yzZz+8Idxg+y9CrSce
9j2P73NJ8iZ3qWAWcodydO0RMXfljiH+Em8EfDmRuRsWP4+zYBeDdyRALm1X0pOW
8yMtofxKVe/VQqCi5lsJced24MWzlUK75Axgpr04HxMSUaTmZDOiL7sKtTPrwM6z
tCc1CJdOxjN/a9jA/JS8X+8iVDXymhC61CEp3rkO9lMJnnS4Jp4ziLOfYVBwfOgk
cWqIG8icn3VdaDBCC/BKFHyhHNDeiBHpTZ8qCNJlCqebLgqXxxA6Wzg7yg7MsdYx
qd6fYaaajct2mBN98wQHp+gyiV3IFHSb+N8qiDzkekmmPJyrU5Dno9GgqUGNoi8C
AwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0O
BBYEFKgfsx9/SktlIzd65b+Euj1gPs50MA0GCSqGSIb3DQEBCwUAA4IBAQBl4osZ
cM4sPujhj3z0xOxS2+e04akTYQ2yKPo+hQYW9pO8O26Zr9AqDtLe7ieOtpvjsB7S
cXKS/eWjxmFTrXwm+uJx/arHLC8BrbGEvGLdxTgvV8CBPiG2GO2EU5wY5ve1dPyF
RWUpggT6ADuvC3ZA+xNFSSM69kt2OWZ+JauZYWDqBEifg53RnFpkaAVhasRR4MSC
dThlJ432j1w4FuaekfGhUBgNfFn+gEe7RR21O0xhBGshuD9vzSQXUxcHFVkPgZam
3iEcowEaAJyIAV8izAj2LAmySbzhXHMajlUs3bpeDR00msk+3uwiFQC29H2XG+o3
cINHkbWU8gkTGuCU
-----END CERTIFICATE-----
   )EOF";
 
// Copy contents from XXXXXXXX-certificate.pem.crt here ▼
static const char AWS_CERT_CRT[] PROGMEM = R"KEY(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----


 
)KEY";
 
 
// Copy contents from  XXXXXXXX-private.pem.key here ▼
static const char AWS_CERT_PRIVATE[] PROGMEM = R"KEY(
-----BEGIN RSA PRIVATE KEY-----

-----END RSA PRIVATE KEY-----

 
)KEY";
