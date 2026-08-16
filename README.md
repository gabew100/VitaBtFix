# VitaBtFix

Lets newer Bluetooth headphones play sound on a jailbroken PS Vita.

The Vita already pairs and can control volume. Some newer headsets still stay silent. Sony’s Bluetooth stack stamps each audio packet with the wrong clock. This plugin fixes that.

## Compatible

| Device | Working |
|---|---|
| AirPods Pro 2 | Yes |

## Install

1. Copy `vitabtfix.skprx` to `ur0:tai/`.
2. Under `*KERNEL` in `ur0:tai/config.txt`:

```
ur0:tai/vitabtfix.skprx
```

3. Reboot.
4. Pair the headphones and play something.

If nothing changes, check `ux0:data/vitabtfix/log.txt` for `patch: +… -> +512 ok`. If you see `no +8000 timestamp add`, this firmware’s Bluetooth module is different and the plugin will not help.

## Config

Written on first boot to `ux0:data/vitabtfix/config.txt`:

```
debug=0
target_volume=100
force_start_audio=1
force_avrcp_volume=1
# mac=AA:BB:CC:DD:EE:FF
```

- `target_volume` is absolute volume, 0–127.
- Empty `mac=` matches any device named AirPods or with Apple vendor ID `0x004C`. Add a `mac=` line to target a specific headset.
- `debug=1` logs a bit more to `ux0:data/vitabtfix/log.txt`.

## Disable

Hold **L** at boot to skip kernel plugins, then delete or rename `ur0:tai/vitabtfix.skprx` and reboot.

## Build

[VitaSDK](https://vitasdk.org/) in Docker:

```
docker run --rm -v "/path/to/Vita BT:/src" -w /src vitasdk/vitasdk \
  bash -lc 'export VITASDK=/usr/local/vitasdk; export PATH="$VITASDK/bin:$PATH"; mkdir -p build-docker; cd build-docker; cmake .. && make -j2'
```

Output: `build-docker/vitabtfix.skprx`.

## What it does not do

AAC / LE Audio only headsets, pairing problems, dropouts, or delay. The Vita is Bluetooth 2.1+EDR and SBC only.
