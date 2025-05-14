#include <Arduino.h>
#include <bootloader_random.h>

#include "luatosWrapper.h"

#define MAX_BUFFER_SIZE 8192
char luaCodeBuffer[MAX_BUFFER_SIZE];
int bufferPos = 0;
bool isReceiving = false;
#define CTRL_D 4
luatosWrapper a;



void processChar(char c) {
    std::string error = "";
    // Check for Ctrl+D (end of transmission)
    if (c == CTRL_D) {
      if (isReceiving && bufferPos > 0) {
        // Null-terminate the buffer
        luaCodeBuffer[bufferPos] = '\0';
        
        // Log the received code
        Serial.println("\n--- Received Lua Code ---");
        Serial.println(luaCodeBuffer);
        Serial.println("-------------------------");
        
        // Execute the Lua code
        Serial.println("Executing code...");
        int result = a.luatosWrapper_exec_string(L, luaCodeBuffer,error);
        // Execute the partial code
        Serial.printf("%s",error.c_str());
        
        if (result != 0) {
          Serial.println("Error executing Lua code!");
        } else {
          Serial.println("Code executed successfully");
        }
        
        // Reset the buffer
        memset(luaCodeBuffer, 0, MAX_BUFFER_SIZE);
        bufferPos = 0;
        isReceiving = false;
        
        // Ready for next code
        Serial.println("Ready to receive new code...");
      }
      return;
    }
    if (bufferPos < MAX_BUFFER_SIZE - 1) {
        luaCodeBuffer[bufferPos++] = c;
        isReceiving = true;
        
        // Echo character to debug console
        if (c >= 32 && c <= 126) {  // Printable ASCII
          Serial.write(c);
        } else if (c == '\n') {
          Serial.write('\n');
        } else if (c == '\r') {
          // Ignore carriage return
        } else {
          // Show hex for non-printable characters
          Serial.print("[0x");
          Serial.print(c, HEX);
          Serial.print("]");
        }
      } else {
        // Buffer overflow handling
        if (!isReceiving) {
          return;  // Already handled
        }
        luaCodeBuffer[bufferPos] = '\0';
        a.luatosWrapper_exec_string(L, luaCodeBuffer,error);
        // Execute the partial code
        Serial.printf("%s",error.c_str());
        
        // Reset buffer
        memset(luaCodeBuffer, 0, MAX_BUFFER_SIZE);
        bufferPos = 0;
        isReceiving = false;
      }
    }
    
        

void setup()
{
 a.begin(115200);
 
  memset(luaCodeBuffer, 0, MAX_BUFFER_SIZE);
}

void loop()
{
    while (Serial.available()) {
        char c = Serial.read();
        processChar(c);
      }
}