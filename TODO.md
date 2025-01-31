UART cli for:
* log mgmt
* name to flash
* channel mgmt for ESP32-C5 (5GHz)
* channel mgmt for ESP32-S3 (2.4GHz)
* peer list
* mdns operations
* ipv6 neigbor table from mac address
* netif send + recv
ble & wifi coexistence
ble send single packets with: https://github.com/espressif/esp-nimble/blob/c0363f26cf6484830779292035fe309c1ec0eb94/nimble/host/src/ble_gap.c#L3024


TX[3][1]: To: ff02:0000:0000:0000:0000:0000:0000:00fb:5353, Packet[9810]: AUTHORITATIVE
    A: _airdrop._tcp.local. PTR IN 4500[10] AirDrop._airdrop._tcp.local.
    A: AirDrop._airdrop._tcp.local. SRV IN FLUSH 120[13] 0 0 8770 test.local.
    A: AirDrop._airdrop._tcp.local. TXT IN FLUSH 4500[1] 
    X: test.local. AAAA IN FLUSH 120[16] fe80:0000:0000:0000:8265:99ff:fec7:ae98

1: Interface: AWDL_DEF, Type: V6, TTL: 120
  PTR : a9c23d0015c7._airdrop._tcp
  SRV : iPhone.local:8770
  TXT : [1] flags=503(3); 
  AAAA: fe80:0000:0000:0000:f0c8:9aff:fe6a:8225

  