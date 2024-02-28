/*
  my_plugin.c - An embedded CNC Controller with rs274/ngc (g-code) support

  "Dummy" user plugin init, called if no user plugin is provided.

  Part of grblHAL

  Copyright (c) 2020-2023 Terje Io

  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ADD_MY_PLUGIN

/*
  my_plugin.c - some MQTT fun

  Part of grblHAL

  Public domain.
*/

#include "driver.h"

#if MQTT_ENABLE

#include <string.h>

#include <stdio.h>

#include "networking/networking.h"

static bool mqtt_connected = false;
static const char *client_id = NULL;
static coolant_set_state_ptr coolant_set_state_;
static on_state_change_ptr on_state_change;
static on_report_options_ptr on_report_options;
static on_program_completed_ptr on_program_completed;
static on_mqtt_client_connected_ptr on_client_connected;
static on_mqtt_message_received_ptr on_message_received;

static const char *msg_alarm = "Alarm %d! - %s";
static const char *msg_job_complete = "job completed!";
static const char *msg_coolant_on = "turn on water cooler!";
static const char *msg_coolant_off = "turn off water cooler!";

static void onStateChanged (sys_state_t state)
{
    
}

void onProgramCompleted (program_flow_t program_flow, bool check_mode)
{
    if(!check_mode && mqtt_connected)
        mqtt_publish_message("grblHALxx", msg_job_complete, strlen(msg_job_complete), 1, false);

    if(on_program_completed)
        on_program_completed(program_flow, check_mode);
}

static void onCoolantSetState (coolant_state_t state)
{
    static coolant_state_t last_state = {0};

    coolant_set_state_(state);

    if(state.flood != last_state.flood && mqtt_connected) {
        if(state.flood)
            mqtt_publish_message("grblHALxx", msg_coolant_on, strlen(msg_coolant_on), 1, false);
        else
            mqtt_publish_message("grblHALxx", msg_coolant_off, strlen(msg_coolant_off), 1, false);
    }

    last_state = state;
}

static void onMQTTconnected (bool connected)
{
    if(on_client_connected)
        on_client_connected(connected);

    if((mqtt_connected = connected)) {
        printf("trying to subscibe \n");
        mqtt_subscribe_topic("grblHAL", 1, NULL);
        client_id = networking_get_info()->mqtt_client_id;
    } else
        client_id = NULL;
}

static bool onMQTTmessage (const char *topic, const void *payload, size_t payload_length)
{

    // hal.stream.write(topic);
    // hal.stream.write(ASCII_EOL);
    // hal.stream.write((char *)payload);
    // hal.stream.write(ASCII_EOL);
  char newLimit[9];
  strncpy(newLimit, (char *)payload, 9);
  printf("old min: %f \n", sys.work_envelope.min.values[0]);
  printf("homed mask: %d \n", sys.homed.mask);
  float newLimitFloat = -atof(newLimit);
  
  if (newLimitFloat < -1800 && newLimitFloat > -2150 && (sys.homed.mask & X_AXIS_BIT)) {
    sys.work_envelope.min.values[0] = newLimitFloat;
  }
  
  printf("new min: %f \n", sys.work_envelope.min.values[0]);
  return on_message_received == NULL || on_message_received(topic, payload, payload_length);
}

static void onReportOptions (bool newopt)
{
    on_report_options(newopt);

    if(!newopt)
        hal.stream.write("[PLUGIN:MCM Dynamic Limit V0.2]" ASCII_EOL);
}

void my_plugin_init (void)
{
  on_report_options = grbl.on_report_options;
  grbl.on_report_options = onReportOptions;

  on_state_change = grbl.on_state_change;
  grbl.on_state_change = onStateChanged;

  coolant_set_state_ = hal.coolant.set_state;
  hal.coolant.set_state = onCoolantSetState;

  on_program_completed = grbl.on_program_completed;
  grbl.on_program_completed = onProgramCompleted;

  on_client_connected = mqtt_events.on_client_connected;
  mqtt_events.on_client_connected = onMQTTconnected;

  on_message_received = mqtt_events.on_message_received;
  mqtt_events.on_message_received = onMQTTmessage;

}

#endif


#endif
