# device-coap-c

EdgeX device service for CoAP-based REST protocol

This device service allows a 3rd party sensor application to push data into EdgeX via CoAP. Like HTTP, CoAP provides REST based access to resources, but CoAP is more compact for use in constrained IoT devices.

The device-coap-c service (_device-coap_ for short) is modeled after the HTTP based [device-rest-go](https://github.com/edgexfoundry/device-rest-go) service, and runs over UDP. The current implementation is meant for one-way communication from a device into EdgeX by posting readings asynchronously. Synchronous interaction initiated by EdgeX can be added in the future.

device-coap uses DTLS for secure communication to devices. It is written in C, and relies on the well known [libcoap](https://libcoap.net/) library.


## Resources

device-coap creates a parameterized CoAP resource to which data may be posted:

```
   /a1r/{deviceName}/{resourceName}
```

- `a1r` is short for "API v1 resource", as defined by device-rest-go.
- `deviceName` refers to a `device` managed by the CoAP device service. For example, `res/configuration.toml` pre-defines a device named 'd1'.
- `resourceName` refers to a `deviceResource` defined in the device profile, as described in the sub-section below.

Payload data posted to one of these resources is type validated, and the resulting value then is sent into EdgeX via the Device SDK's asynchronous `post_readings` capability.

## Profiles

[example-datatype.yaml](./res/example-datatype.yaml) defines  generic resources for data types. The table below shows the available resource names and correspondence with CoAP attributes. 

For example, the 'int' resource name means that EdgeX provides a CoAP resource, `/a1r/{deviceName}/int`. This resource accepts an integer encoded as text, like `42`.

| resourceName | Type   | EdgeX MediaType<br>CoAP Content-Format|
|---------|--------|---------------------------------------|
| int     | Int32  | text/plain                            |
| float   | Float64| text/plain                            |
| json    | String | application/json                      |

>_Note:_ You must define the Content-Format option in the CoAP POST request. See the _Testing_ section below for example use.


## Configuration

This section describes properties in [configuration.toml](./res/configuration.toml) as used by device-coap. See the _Configuration and Registry_ section of the EdgeX documentation for background.

### Driver

Below are the recognized properties for the Driver section, followed by an example.

| Key         | Value                                                                             |
|-------------|-----------------------------------------------------------------------------------|
| CoapBindAddr| Address on which CoAP server listens for devices                                  |
| SecurityMode| DTLS client-server security type. Does not support raw public key or certificates.|
| PskKey      | Pre-shared key. Accepts only a single key, ignored in NoSec mode.                 |


```
[Driver]
  # Supports IPv4 or IPv6 if provided by network infrastructure. Use '0.0.0.0'
  # for any IPv4 interface, or '::' for any IPv6 interface.
  CoapBindAddr = '0.0.0.0'
  # Choose 'PSK' or 'NoSec'
  SecurityMode = 'PSK'
  # Key is up to 16 arbitrary bytes; must be base64 encoded here
  PskKey = 'ME42aURHZ3Uva0Y0eG9lZw=='
```

### DeviceList
The `DeviceList` section pre-defines the 'd1' device.

```toml
# Pre-define Devices
[[DeviceList]]
  Name = 'd1'
  Profile = 'Coap-Device'
  Description = 'Coap Data Generator Device'
  Labels = [ "coap", "rest" ]
  [DeviceList.Protocols]
    [DeviceList.Protocols.other]
```

## Docker Integration

### Building

You can build a Docker image with the command below from the top level directory of a device-coap checkout.

```
   $ make docker
```

### Compose

Below is an example entry for a docker-compose template with the rest of the EdgeX setup. The CoAP server listens on the default secure port, 5684. It also listens on any interface since the CoAP message likely arrives from an external network.

```
  device-coap:
    image: kb2ma/docker-device-coap-c:0.2-dev
    ports:
      - "127.0.0.1:49750:49750"
      - "0.0.0.0:5684:5684/udp"
    container_name: kb2ma-device-coap
    hostname: kb2ma-device-coap
    networks:
      - edgex-network
    environment:
      <<: *common-variables
      Service_Host: kb2ma-device-coap
    depends_on:
      - metadata
      - data
```

## Testing/Simulation

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

## Development

This section describes how to build and run a device-coap executable independent from Docker, for development or debugging.

### Building

device-coap depends on libcoap and tinydtls. See [build_deps.sh](scripts/build_deps.sh) to download and build them. As with any C based EdgeX device project, device-coap also depends on the EdgeX [C SDK](https://github.com/edgexfoundry/device-sdk-c/blob/master) for its SDK library and headers. Finally, see [build.sh](scripts/build.sh) and [build_debug.sh](scripts/build_debug.sh) to build device-coap itself. These scripts may be invoked via `make build` and `make build-debug` respectively.

### Running

Simply run the generated executable. The example below was built with the `build_debug.sh` script.

```
   $ build/debug/device-coap -f configuration-native.toml
```

>_Note:_ Service configuration in `configuration-native.toml` is customized for a separate device-coap executable. Uses '172.17.0.1' for the `Service->Host` parameter. Uses 'localhost' for the `Host` parameter in the `Registry` and `Clients` sections.

Run with `-h` to see all command line options.