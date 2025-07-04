#include "html_common.h"
#include <Arduino.h>

// Load template from PROGMEM
String loadTemplate(const char* templateData) {
  return String(FPSTR(templateData));
}

// Process template replacing placeholders with actual values
String processTemplate(const String& templateStr, TemplateProcessor processor) {
  String result = templateStr;
  
  // Find all placeholders in format %PLACEHOLDER%
  int startIndex = 0;
  while ((startIndex = result.indexOf('%', startIndex)) != -1) {
    int endIndex = result.indexOf('%', startIndex + 1);
    if (endIndex == -1) break;
    
    String placeholder = result.substring(startIndex + 1, endIndex);
    String value = processor(placeholder);
    
    if (value.length() > 0) {
      result = result.substring(0, startIndex) + value + result.substring(endIndex + 1);
      startIndex += value.length();
    } else {
      startIndex = endIndex + 1;
    }
  }
  
  return result;
}