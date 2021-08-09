# hikmqtt
Broker Alerts from Multiple HikVision Devices to MQTT

Configure the configuration file for the broker to receive and post events.
I, personally, listen for commands on 'hikctrl' and post notifications on 'hikAlarm'.

## Config file

```
mqtt_server =
{
  address     = "10.10.10.10";
  port        = 1883;
  username    = "admin";
  password    = "12345";

  subscribe   = "BTRGB/hikctrl"
  publish     = "BTRGB/hikalarm";
};

devices = (
  { id = "1", address = "10.10.10.1"; username = "admin"; password = "pass123"; },
  { id = "2", address = "10.10.10.2"; username = "admin"; password = "pass123"; },
  { id = "3", address = "10.10.10.3"; username = "admin"; password = "pass123"; },
  { id = "4", address = "10.10.10.4"; username = "admin"; password = "pass123"; }
);
```

* 'subscribe' is where we wait and listen for commands.
* 'publish' is where we publish alarms and responses to commands.
* The 'id', in 'devices' above, is used to send requests to the specified device and is known as 'devId' in the commands below.

## Available commands:

### update_preset_names

Call this command to update the local copy of preset names for the given device and channel. You should do this before searching for a preset by name.

```mqtt
{ "command": "update_preset_names" , "devId": 0, "args": { "channel": 1 }}
```

### get_preset_byname
```
{ "command": "get_preset_byname" , "devId": 0, "args": { "channel": 1, "preset": "Night mode" }}
```

### get_ptz_pos
```
{ "command": "get_ptz_pos" , "devId": 0, "args": { "channel": 1, "preset": 2 }}
```

### get_preset_details
```
{ "command": "get_preset_details" , "devId": 0, "args": { "channel": 1, "preset": 1 }}
```
### delete_preset
```
```
### set_preset
```
```
### set_ptz_pos
```
```
### call_preset
```
```
### start_record
```
```
### stop_record
```
```
### dvr_reboot
```
```

