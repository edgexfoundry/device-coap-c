# device-coap-c
[![Build Status](https://jenkins.edgexfoundry.org/view/EdgeX%20Foundry%20Project/job/edgexfoundry/job/device-coap-c/job/main/badge/icon)](https://jenkins.edgexfoundry.org/view/EdgeX%20Foundry%20Project/job/edgexfoundry/job/device-coap-c/job/main/) [![GitHub Latest Dev Tag)](https://img.shields.io/github/v/tag/edgexfoundry/device-coap-c?include_prereleases&sort=semver&label=latest-dev)](https://github.com/edgexfoundry/device-coap-c/tags) ![GitHub Latest Stable Tag)](https://img.shields.io/github/v/tag/edgexfoundry/device-coap-c?sort=semver&label=latest-stable) [![GitHub License](https://img.shields.io/github/license/edgexfoundry/device-coap-c)](https://choosealicense.com/licenses/apache-2.0/) [![GitHub Pull Requests](https://img.shields.io/github/issues-pr-raw/edgexfoundry/device-coap-c)](https://github.com/edgexfoundry/device-coap-c/pulls) [![GitHub Contributors](https://img.shields.io/github/contributors/edgexfoundry/device-coap-c)](https://github.com/edgexfoundry/device-coap-c/contributors) [![GitHub Committers](https://img.shields.io/badge/team-committers-green)](https://github.com/orgs/edgexfoundry/teams/device-coap-c-committers/members) [![GitHub Commit Activity](https://img.shields.io/github/commit-activity/m/edgexfoundry/device-coap-c)](https://github.com/edgexfoundry/device-coap-c/commits)


EdgeX device service for CoAP-based REST protocol

This device service supports both CoAP client and CoAP server. CoAP server allows a 3rd party sensor application to push data into EdgeX via CoAP. CoAP client allows EdgeX to read sensor data through auto-events and send a command to 3rd party sensor application. Like HTTP, CoAP provides REST based access to resources, but CoAP is more compact for use in constrained IoT devices.

The device-coap-c service (_device-coap_ for short) is modeled after the HTTP based [device-rest-go](https://github.com/edgexfoundry/device-rest-go) service, and runs over UDP.

device-coap uses DTLS for secure communication to devices. It is written in C, and relies on the well known [libcoap](https://libcoap.net/) library.

For background on device-coap-c, CoAP and low power wireless, see the [presentation](https://zoom.us/rec/share/N2Uh7C9qScsj32bs0T8aNF4VPPOuFSypnhQp3g2LmSFfOA16giRq9gwqpGvNb1HX.kknLyNV7Rj72mPms?startTime=1602514686000) to the Device Working Group.

## Resources

device-coap creates a parameterized CoAP resource. CoAP server may post data to these resources. CoAP client may initiate auto-events to end device's on these resources. CoAP client may send command to end device's coap-server on these resources. 

```
   /a1r/{deviceName}/{resourceName}
```

- `a1r` is short for "API v1 resource", as defined by device-rest-go.
- `deviceName` refers to a `device` managed by the CoAP device service. For example, `res/devices/devices.json` pre-defines a device named 'd1'.
- `resourceName` refers to a `deviceResource` defined in the device profile, as described in the sub-section below.

Payload data posted to one of these resources is type validated, and the resulting value then is sent into EdgeX via the Device SDK's asynchronous `post_readings` capability.

## Profiles

[example-datatype.json](./res/profiles/example-datatype.json) defines  generic resources for data types. The table below shows the available resource names and correspondence with CoAP attributes. 

For example, the 'int' resource name means that EdgeX provides a CoAP resource, `/a1r/{deviceName}/int`. This resource accepts an integer encoded as text, like `42`.

| resourceName | Type   | CoAP Content-Format|
|---------|--------|---------------------------------------|
| int     | Int32  | text/plain                            |
| float   | Float64| text/plain                            |
| json    | String | application/json                      |

>_Note:_ You must define the Content-Format option in the CoAP POST request. See the _Testing_ section below for example use.


## Configuration

This section describes properties in [configuration.toml](./res/configuration.toml) as used by device-coap. See the _Configuration and Registry_ section of the EdgeX documentation for background.

### Driver

Below are the recognized properties for the Driver section, followed by an example. These values are read only when starting the device-coap service. So if you change a property, you must restart the service for the change to take effect.


| Key         | Value                                                                             |
|-------------|-----------------------------------------------------------------------------------|
| CoapBindAddr| Address on which CoAP server listens for devices                                  |
| SecurityMode| DTLS client-server security type. Does not support raw public key or certificates.|


```
[Driver]
  # Supports IPv4 or IPv6 if provided by network infrastructure. Use '0.0.0.0'
  # for any IPv4 interface, or '::' for any IPv6 interface.
  CoapBindAddr = '0.0.0.0'
  # Choose 'PSK' or 'NoSec'
  SecurityMode = 'PSK'
```

### Secrets

If configured for PSK mode, a key must be stored in the service's secret store:
| Secret name | Value                                                                             |
|-------------|-----------------------------------------------------------------------------------|
| PskKey      | Pre-shared key. Accepts only a single key, ignored in NoSec mode.                 |

For example if using insecure mode (secrets in configuration file):

```
  [Writable.InsecureSecrets]
    [Writable.InsecureSecrets.psk]
      path = "psk"
      [Writable.InsecureSecrets.psk.Secrets]
        # Key is up to 16 arbitrary bytes; must be base64 encoded here
        PskKey = 'ME42aURHZ3Uva0Y0eG9lZw=='
```

## Devices
### Devices for CoAP Server 

A pre-defined device 'd1' is supplied. At present no properties for the `other` protocol are defined for a device.

```json
{
  "name": "d1",
  "profileName": "example-datatype",
  "description": "Example generic data type device",
  "labels": [ "coap", "rest" ],
  "protocols": { "other": { } }
}
```

### Devices for CoAP Client

A predefined device 'd2' is supplied. 

```json
{
    "name": "d2",
    "profileName": "example-datatype",
    "description": "Example generic data type device",
    "protocols":
    {
        "COAP": 
        {
            "ED_ADDR": "127.0.0.1",
            "ED_SecurityMode": "PSK",
            "ED_PskKey": "hello123"
        }
    },
    "autoEvents":
    [   
        { "sourceName": "int", "onChange": false, "interval": "30s" }
    ]   
}
```

| Key             | Value                                                        |
| --------------- | ------------------------------------------------------------ |
| ED_ADDR         | Address on which CoAP client initiates request to end device |
| ED_SecurityMode | DTLS client-server security type. Does not support raw public key or certificates. Possible values are PSK/NoSec |
| ED_PskKey       | Pre-shared key. Accepts only a single key, ignored in NoSec mode. |

- Auto-events are supported for the resources mentioned in the profile for example `int` resource. 

## Docker Integration

### Building

You can build a Docker image with the command below from the top level directory of a device-coap checkout.

```
   $ make docker
```

### Compose

Below is an example entry for a docker-compose template with the rest of the EdgeX setup. The CoAP server listens on the default secure port, 5684. It also listens on any interface since the CoAP message likely arrives from an external network. However, it is more secure to use the address for the specific interface for CoAP messaging in your setup.

```
  device-coap:
    image: edgexfoundry/device-coap:0.2-dev
    ports:
      - "127.0.0.1:59988:59988"
      - "0.0.0.0:5684:5684/udp"
    container_name: edgex-device-coap
    hostname: edgex-device-coap
    networks:
      - edgex-network
    environment:
      <<: *common-variables
      Service_Host: edgex-device-coap
    depends_on:
      - metadata
      - data
```

## Testing/Simulation for CoAP Server

You can use simulated data to test this service with libcoap's `coap-client` command line tool. The examples below are organized by the SecurityMode defined in the configuration.

**NoSec**

```
   $ coap-client -m post -t 0 -e 1001 coap://127.0.0.1/a1r/d1/int
```
**PSK**

```
   $ coap-client -m post -u r17 -k 0N6iDGgu/kF4xoeg -t 0 -e 1001 coaps://127.0.0.1/a1r/d1/int
```

  * For DTLS PSK, a CoAP client must include a user identity via the `-u` option as well as the same key the server uses. Presently, the device-coap server does not evaluate the identity, only the key. Also, `coap-client` reads the key as a literal string, so characters must be readable from the command line. Finally, notice the protocol in the address is `coaps`. This protocol uses UDP port 5684 rather than 5683 for protocol `coap`.
  * POSTing a text integer value will set the  `Value` of the `Reading` in EdgeX to the string representation of the value as an `Int32`. The POSTed value is verified to be a valid `Int32` value.
  * A 400 error will be returned if the POSTed value fails the `Int32` type verification.

### Zephyr CoAP client

Also see my Zephyr based [edgex-coap-peer](https://github.com/kb2ma/edgex-coap-peer) repository for a simple CoAP client usable on an IoT device. The client posts integer data for the example profile above, to `/a1r/d1/int`.

### RIOT CoAP client

Also see my RIOT based [riot-edgex-coap-client](https://github.com/kb2ma/riot-edgex-coap-client) repository for a more realistic CoAP client. The client posts a temperature measurement from a sensor every 60 seconds to `/a1r/d1/float`.

## Testing/Simulation for CoAP Client

You can use simulated data to test the CoAP client functionality of this device service using libcoap server. Resources must be handled properly in the coap-server to test CoAP client. The examples below are organized by the ED_SecurityMode defined in devices.json.

**NoSec**

```
$ ./coap-server -A 127.0.0.1
```

**PSK**

```
$ ./coap-server -A 127.0.0.1 -k hello123
```

  * For DTLS PSK, a coap-server must include same key the CoAP client uses. The coap-server reads key as a literal string, so characters must be readable from the command line. 

## Development

This section describes how to build and run a device-coap executable independent from Docker, for development or debugging.

### Building

device-coap depends on libcoap and tinydtls. The [build_deps.sh](scripts/build_deps.sh) script provides a template to build these libraries that you can adapt for use at the command line. `build_deps.sh` is intended for use by the Docker build, so first review [Dockerfile.alpine](scripts/Dockerfile.alpine). Notice that it creates a `/device-coap` directory as a workspace, and then runs `build_deps.sh`. Also keep in mind that a Docker build has full privileges over its container filesystem as it runs.


As with any C based EdgeX device project, device-coap also depends on the EdgeX [C SDK](https://github.com/edgexfoundry/device-sdk-c/blob/master) for its SDK library and headers. Finally, see [build.sh](scripts/build.sh) and [build_debug.sh](scripts/build_debug.sh) to build device-coap itself. These scripts may be invoked via `make build` and `make build-debug` respectively.

### Running

Simply run the generated executable. The example below was built with the `build_debug.sh` script.

```
   $ build/debug/device-coap -f configuration-native.toml
```

>_Note:_ `configuration-native.toml` adapts the contents of `configuration.toml` for use with a separate device-coap executable.

Run with `-h` to see all command line options.
