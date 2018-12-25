# ESP8266 + MQTT + Spring Boot
This codebase is a part of an IoT project which also involves a [Spring Boot Application](https://github.com/adnirjhar/orchid-backend) and a [frontend application](https://github.com/adnirjhar/orchid-frontend) built with Angular 4. Its built using [PlatformIO](https://platformio.org/), an open source ecosystem for IoT development. One should be able to run this project on any ESP8266 or equivalent devices. To test the implementations one must also setup the [backend](https://github.com/adnirjhar/orchid-backend) and the [frontend](https://github.com/adnirjhar/orchid-frontend) projects. 

To learn more about what this project does, please visit [Medium article](https://medium.com/@nitam/google-dialogflow-spring-boot-angular-mqtt-esp8266-anything-42d8e19dedec)

### Installing prerequisites
  - [Download](https://code.visualstudio.com/) and install appropriate VS Code IDE for your development environment.
  - Open VSCode Extension Manager.
  - Search for official `platformio-ide` extension.
    ![](https://platformio.org/images/platformio-ide-vscode-pkg-installer.eb69eca2.png)
  - Install PlatformIO IDE.
    
### Clone
```sh
$ git clone git@github.com:adnirjhar/orchid-esp8266.git
$ cd orchid-esp8266
```
### Run
  - Open the `orchid-esp8266` project folder with VSCode and PlatformIO extension should pick up the settings and initialize the project.
  - Build the code.                                                             
    ![](https://i.ibb.co/Mf3VdG7/1234-1.png)
  - Connect a working ESP8266 device. Deploy the code.
    ![](https://i.ibb.co/g43F86y/1234-2.png)
### Implementations
##### Authentication
  - Takes in multiple SSID - passwords. Connects to the nearest Wifi.
  - Logs into the server using the `login` REST api from backend with the provided credentials with code and collects the `JSESSIONID` on successful login.
  - As all the APIs need authentication, the `JSESSIONID` is added to the headers to authenticate all the next requests its going to make.

##### Fetch MQTT settings for the system and connect
  - Calls the `getESPConfig` REST api to fetch the MQTT settings like hostname,port,common channel name, etc. and connects to the MQTT Broker.
  - Starts listening to a specific channel specified for that ESP Device.
##### Receiving & sending messages
  - Upon fetching the MQTT settings, it also receives a specific MQTT channel address specific for that device to subscribe.
  - When a message is received through that channel, it parses the text to get the target pin and sets the target value (HIGH/LOW) to it.
  - If a Sensor pin (INPUT Pin) is triggered HIGH, the device sends a message to the server through the common channel, which is forwarded to the frontend application (if user is logged in) using Websockets.

To learn more about the entire project, please visit [Medium article](https://medium.com/@nitam/google-dialogflow-spring-boot-angular-mqtt-esp8266-anything-42d8e19dedec)


