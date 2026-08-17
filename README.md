# VitaBtFix

Allows newer AirPods to play audio on a jailbroken PS Vita. Other Bluetooth headphones with the same compatibility issue may work too.

The Vita can pair with some newer Bluetooth headphones and control their volume, but audio remains silent. VitaBtFix corrects the A2DP timestamp behavior in Sony's Bluetooth stack that causes this issue.

> **VitaBtFix is currently experimental.** 

## Compatibility

| Device        | Vita firmware | Status      |
| ------------- | ------------- | ----------- |
| AirPods Pro 2 | 3.65          | ✅ Works |
| AirPods Pro 3 | 3.65          | ✅ Works |
| AirPods 4 | 3.65          | ✅ Works |

Other Vita firmware versions and Bluetooth headphones are currently untested. Reports are welcome.

When reporting compatibility, include your Vita firmware, headphone model, and whether audio works.

## Install

1. Download `vitabtfix.skprx` from the [latest release](https://github.com/gabew100/VitaBtFix/releases).
2. Copy `vitabtfix.skprx` to `ur0:tai/`.
3. Add the following line under `*KERNEL` in `ur0:tai/config.txt`:

```text
ur0:tai/vitabtfix.skprx
```

4. Reboot the Vita.
5. Pair your headphones and play something.

## Troubleshooting

If VitaBtFix does not work:

* Make sure `vitabtfix.skprx` is listed under `*KERNEL` in `ur0:tai/config.txt`.
* Make sure the path in `config.txt` matches where you copied the plugin.
* Reboot the Vita after installing or changing the plugin.
* Check `ux0:data/vitabtfix/log.txt` for a line similar to:

```text
patch: +… -> +512 ok
```

* If you see:

```text
no +8000 timestamp add
```

VitaBtFix could not recognize the required code pattern and will leave the Bluetooth module untouched.

If the Vita has trouble booting after installation, hold **L** while booting to skip kernel plugins, then remove or rename `vitabtfix.skprx`.

When reporting a problem, include your Vita firmware version and headphone model. If possible, also attach `ux0:data/vitabtfix/log.txt`.

## Disable

Hold **L** while booting to skip kernel plugins, then delete or rename:

```text
ur0:tai/vitabtfix.skprx
```

Remove its line from `ur0:tai/config.txt`, then reboot.

## Build

Build using [VitaSDK](https://vitasdk.org/) in Docker:

```bash
docker run --rm -v "/path/to/Vita BT:/src" -w /src vitasdk/vitasdk \
  bash -lc 'export VITASDK=/usr/local/vitasdk; export PATH="$VITASDK/bin:$PATH"; mkdir -p build-docker; cd build-docker; cmake .. && make -j2'
```

Output:

```text
build-docker/vitabtfix.skprx
```

## Limitations

VitaBtFix does not add support for:

* AAC-only or LE Audio-only headphones
* Bluetooth pairing problems
* Connection dropouts
* Bluetooth audio latency

The PS Vita uses Bluetooth 2.1+EDR and SBC for Bluetooth audio.

## License

VitaBtFix is licensed under the [MIT License](LICENSE).
