#include <pgmspace.h>
#include <Arduino.h>

//Definitionen fuer Datum und Uhrzeit
// enum {clh, clmin}; //clh ist 0 fuer Uhrzeit-Stunden, clmin ist 1 fuer Uhrzeit Minuten
// enum {dtYear, dtMonth, dtDay}; //Tag = 2; Monat = 1; Jahr = 0
const String WeekDays[7]={"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
// //Allgemeine Definitionen
enum {subwl = 27767, subnw = 30574, subcn = 20035, subpd = 17488, subps = 21328, sublf = 17996, sublm = 19788, submq = 29037}; //Zuordnung der Submit-Bereiche einer Ganzzahl
enum {touchUp_wsgn = 33, touchDown_gn = 32, touchRight_wsbr = 15, touchLeft_br = 4, touchF1_bl = 13, touchF2_wsbl = 12, touchF3_or = 14, touchF4_wsor = 27, RGB_Red = 22, RGB_Green = 16, RGB_Blue = 17, Display_Beleuchtung = 21};
enum {indexAGM = 0, indexLED = 1, indexProgStart = 2, indexNWK = 3};
const String Un_Checked[2]{"","Checked"};
const String varSelected[2]{"", " selected=\"\""};
// const String De_Aktiviert[2]{"Deaktiviert","Aktiviert"};
const String Ein_Aus[2]{"Aus","Ein"};

struct NWConfig {
  //Einstellungen NW-Einstellungen WLAN
  char WLAN_AP_Aktiv = 1;
  char WLAN_SSID[40] = "Terrasse";
  char WLAN_Password[70] = "";
  //Einstellungen NW-Einstellungen MQTT
  char MQTT_Server[50] = "192.168.178.5";
  uint16_t MQTT_Port = 1883;
  char MQTT_Username[20] = "MQTT_User";
  char MQTT_Password[70] = "12345";
  char MQTT_fprint[70] = "";
  char MQTT_rootpath[100] = "/Garten/Terrasse";
  //Einstellungen NW-Einstellungen Netzwerk
  char NW_StatischeIP = 0;
  char NW_NetzName[20] = "Terrasse";
  char NW_IPAdresse[17] = "192.168.178.10";
  char NW_SubMask[17] = "255.255.255.0";
  char NW_Gateway[17] = "192.168.178.1";
  char NW_DNS[17] = "192.168.178.1";
  char NW_NTPServer[55] = "fritz.box";
  char NW_NTPOffset = 0;
};


const char html_header[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Terrasse</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body bgcolor=\"#BBFFFF\">
Uhrzeit: %s | Datum: %s, %02d.%02d.%d | Status: 
<br />
<hr><h3>
<a href=\>Einstellungen</a> 

</h3><hr>
)rawliteral";

const char html_NWconfig[] PROGMEM = R"rawliteral(
<h1>Terrasse Einstellungen</h1><hr>
<h2>WLAN</h2>
<form method="post" action="/POST">
<TABLE>
  <TR>
    <TD WIDTH="120" VALIGN="TOP">
    Access Poin: </TD>
    <TD WIDTH="300" VALIGN="TOP">
    <input name="wlAP" value="1" type="checkbox" %s> <br /><br /></TD>
  </TR>
  <TR>
    <TD VALIGN="TOP">
      SSID: </TD>
  <TD>
    <input name="wlSSID" type="text" value="%s" minlength="2" maxlength="30" size="15" required="1"><br /><br /></TD>
  <TD>
    <br /><br /></TD>
  </TR>
  <TR>
    <TD VALIGN="TOP">
      Passwort: </TD>
  <TD>
    <input name="wlPassword" type="password" minlength="8" maxlength="60" size="35"><br /><br /></TD>
  </TR>
</TABLE>
  <br>
  <input value="Submit" type="submit">
</form>
<hr>
<h2>Netzwerk</h2><br />
<form method="post" action="/POST">
<TABLE>
  <TR>
    <TD WIDTH="200" VALIGN="TOP">
      Statische IP-Adresse verwenden: </TD>
    <TD WIDTH="200" VALIGN="TOP">
    <input name="nwSIP" value="" type="checkbox" %s> <br /><br /></TD>
    <TD WIDTH="200" VALIGN="TOP">
    </TD>
  </TR>
  <TR>
    <TD VALIGN="TOP">
      IP-Adresse: </TD>
  <TD>
      <input name="nwIP" type="text" minlength="7" maxlength="15" size="15" value="%s" pattern="^((\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.){3}(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$"><br /><br /></TD>
    <TD VALIGN="TOP">
       </TD>
  </TR>
  <TR>
    <TD VALIGN="TOP">
      Netzwerkname: </TD>
  <TD>
    <input name="nwNetzName" type="text" minlength="3" maxlength="15"  value="%s" required="1"> <br /><br /></TD>
    <TD VALIGN="TOP">
      </TD>
  </TR>
  <TR>
    <TD VALIGN="TOP">
      Subnetmask: </TD>
  <TD>  
    <input name="nwSubnet" type="text" minlength="7" maxlength="15" size="15" value="%s" pattern="^((\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.){3}(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$"><br /><br /></TD>
    <TD VALIGN="TOP">
       </TD>
  </TR>
  <TR>
    <TD VALIGN="TOP">
      Gateway: </TD>
  <TD>
    <input name="nwGateway" type="text" minlength="7" maxlength="15" size="15" value="%s" pattern="^((\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.){3}(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$"><br /><br /></TD>
    <TD VALIGN="TOP">
      Wird nur bei einem externen NTP-Server benoetigt!</TD>
  </TR>
  <TR>
    <TD VALIGN="TOP">
      DNS-Server: </TD>
  <TD>
    <input name="nwDNS" type="text" minlength="7" maxlength="15" size="15" value="%s" pattern="^((\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.){3}(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$"><br /><br /></TD>
    <TD VALIGN="TOP">
      Wird nur bei einem externen NTP-Server benoetigt!</TD>
  </TR>
  <TR>
    <TD VALIGN="TOP">
      Zeitserver (z.B. fritz.box): </TD>
  <TD>
    <input name="nwNTPServer" type="text" minlength="3" maxlength="50"  value="%s" required="1"> <br /><br /></TD>
    <TD VALIGN="TOP">
      </TD>
  </TR>
  <TR>
    <TD VALIGN="TOP">
      Zeitoffset (in Stunden): </TD>
    <TD>
      <select name="nwNTPOffset" autofocus="autofocus">
      <option%s>-2</option>
      <option%s>-1</option>
      <option%s>0</option>
      <option%s>+1</option>
      <option%s>+2</option>
      </select> 
    <br /><br /></TD>
    <TD VALIGN="TOP">
      </TD>
  </TR>

</TABLE>
    <br>
  <input value="Submit" type="submit">
  </form>
<hr>
<h2>MQTT</h2><br>
<form method="post" action="/POST">
<table>
  <tbody><tr>
    <td valign="TOP">
      MQTT Server: </td>
  <td>
      <input name="mqServer" type="text" minlength="7" maxlength="15" size="45" value="%s"><br><br></td>
    <td valign="TOP">
       </td>
  </tr>
  <tr>
    <td valign="TOP">
      MQTT Port: </td>
  <td>
    <input name="mqPort" type="number" minlength="3" maxlength="5" size="8" value="%u" required="1" pattern="[0-9]{5}"> <br><br></td>
    <td valign="TOP">
      </td>
  </tr>
  <tr>
    <td valign="TOP">
      Benutzername: </td>
  <td>  
    <input name="mqUser" type="text" minlength="6" maxlength="15" size="19" value="%s"><br><br></td>
    <td valign="TOP">
       </td>
  </tr>
  <tr>
    <td valign="TOP">
      Passwort: </td>
  <td>
    <input name="mqPassword" type="password" minlength="8" maxlength="60" size="35" value="xxxxxx"><br><br></td>
    <td valign="TOP">
  </tr>
  <tr>
    <td valign="TOP">
      MQTT Hauptpfad: </td>
  <td>
    <input name="mqRootpath" type="text" minlength="5" maxlength="100" size="35" value="%s"><br><br></td>
    <td valign="TOP">
  </tr>
  <tr>
    <td valign="TOP">
      Zertifikat: </td>
  <td>
    <input name="mqFPrint" type="password" minlength="8" maxlength="60" size="35" value="xxxxxx"><br><br></td>
    <td valign="TOP">
  </tr>

</tbody></table>
    <br>
  <input value="Submit" type="submit">
  </form>

</body>
</html>
)rawliteral";
