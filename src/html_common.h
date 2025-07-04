#ifndef HTML_COMMON_H
#define HTML_COMMON_H

#include <Arduino.h>

// Template processor function type
typedef String (*TemplateProcessor)(const String& var);

// Common HTML elements used by all pages
const char HTML_HEADER[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<title>ESP32 UART Bridge</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta charset="UTF-8">
<link rel="icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><text x='0' y='14' font-size='16'>🔗</text></svg>">
)rawliteral";

const char HTML_STYLES[] PROGMEM = R"rawliteral(
<style>
body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }
.container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
h1 { color: #333; text-align: center; }
.section { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
label { display: block; margin: 10px 0 5px 0; font-weight: bold; }
input, select { width: 100%; padding: 8px; margin: 5px 0; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; }
button { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; margin: 5px; }
button:hover { background-color: #45a049; }
.status { padding: 10px; margin: 10px 0; border-radius: 4px; background-color: #e7f3ff; border-left: 4px solid #2196F3; }
table { width: 100%; border-collapse: collapse; margin: 10px 0; border: 2px solid #333; table-layout: fixed; }
th, td { border: 1px solid #ddd; padding: 8px; text-align: left; word-wrap: break-word; }
th { background-color: #f2f2f2; }
td:first-child { width: 45%; }
td:last-child { width: 55%; text-align: right; }
table tr:nth-child(even) { background-color: #f1f3f4; }
table tr:nth-child(odd) { background-color: #ffffff; }
.log-container { height: 200px; overflow-y: auto; border: 1px solid #ddd; padding: 10px; background-color: #f8f9fa; font-family: monospace; font-size: 12px; }
.log-entry { margin: 2px 0; }
.warning { background-color: #fff3cd; border-left: 4px solid #ffc107; padding: 10px; margin: 10px 0; }
.success { background-color: #d4edda; border-left: 4px solid #28a745; padding: 10px; margin: 10px 0; }
pre { background-color: #f8f9fa; padding: 10px; border-radius: 4px; overflow-x: auto; }
@media (max-width: 480px) {
  .container { padding: 15px; margin: 10px; }
  td:first-child { width: 40%; font-size: 14px; }
  td:last-child { width: 60%; font-size: 14px; }
  button { padding: 12px 16px !important; font-size: 16px !important; min-height: 44px; }
  h1 { font-size: 24px; } h3 { font-size: 18px; }
}
</style>
)rawliteral";

const char HTML_BODY_START[] PROGMEM = R"rawliteral(
</head><body>
<div class="container">
<h1>🔗 ESP32 UART Bridge</h1>
)rawliteral";

const char HTML_FOOTER[] PROGMEM = R"rawliteral(
</div></body></html>
)rawliteral";

// Success page template (simple, standalone page)
const char HTML_SUCCESS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>Connected</title></head>
<body><h1>Successfully Connected!</h1>
<p>You can now configure your UART Bridge.</p>
<script>setTimeout(function(){window.location='/';}, 2000);</script>
</body></html>
)rawliteral";

// Simple response templates
const char HTML_SAVE_SUCCESS[] PROGMEM = R"rawliteral(
<html><head><title>Configuration Saved</title></head><body>
<h1>Configuration Saved</h1>
<p>Settings updated successfully!</p>
<p><a href='/'>Back to Settings</a></p>
</body></html>
)rawliteral";

const char HTML_RESET_SUCCESS[] PROGMEM = R"rawliteral(
<html><head><title>Statistics Reset</title></head><body>
<h1>Statistics and Logs Reset</h1>
<p>Traffic statistics and logs have been reset.</p>
<p><a href='/'>Back to Settings</a></p>
</body></html>
)rawliteral";

// Template processing functions
String processTemplate(const String& templateStr, TemplateProcessor processor);
String loadTemplate(const char* templateData);

#endif // HTML_COMMON_H