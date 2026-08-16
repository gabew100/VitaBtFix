# Online research findings — AirPods Pro 2 silent on Vita

Research date: 2026-08-15. Companion to `HANDOFF.md`. Read this before writing any more plugin code.

## TL;DR

The symptom is **not Vita-specific**. "AirPods Pro 2 pair, connect, volume works, zero audio" is the single
most-reported AirPods-on-non-Apple failure mode, and it reproduces on Windows 11 with the exact same tell:
the device shows up as **`AirPods Pro - Find My`**. The reported fix is not just a reset — it is a reset
*plus pairing the non-Apple host FIRST*, before any Apple device re-claims the buds.

Two of the handoff's three "still plausible" hypotheses can now be closed. One new, more specific
hypothesis replaces them.

## Closed hypotheses

### CLOSED — "Apple mute / AAP handshake gates the DAC" (handoff hypothesis 2)

Wrong. The Apple Accessory Protocol (AAP) has been fully reverse-engineered by the LibrePods project.
It runs over L2CAP PSM `0x1001`, unencrypted, and covers battery level, ANC mode, ear-detection events,
gestures, and conversational awareness. **It does not carry any audio-enable, unmute, or routing command.**
A2DP audio works on Linux/Android/Windows with no AAP traffic whatsoever. Stop pursuing this.

### CLOSED — "Ear detection is muting them"

Automatic Ear Detection is inactive on non-Apple hosts. AirPods play continuously regardless of whether
they are in your ears. Not the bug. (LibrePods exists specifically to *add* this feature back on
Linux/Android, which confirms it is absent by default.)

### WEAKENED — "Vita's SBC is too primitive for H2" (handoff hypothesis 1, as stated)

SBC alone is demonstrably sufficient to drive AirPods Pro 2:

- The **Nintendo Switch is SBC-only** and drives AirPods Pro 2 fine.
- On Linux, **SBC and SBC-XQ work reliably** with AirPods Pro 2. **AAC is the flaky one** — BlueZ issue
  #1093 is specifically "AirPods Pro 2 + AAC = connected but no sound", with the workaround being
  *switch to SBC*.

So plain SBC is not the problem — **provided the Vita actually lands on the SBC endpoint.** That caveat is
the new hypothesis below.

## New primary technical hypothesis: wrong stream endpoint (SEP)

AirPods Pro 2 advertise **multiple A2DP stream endpoints** — at minimum SBC (codec id `0x00`) and
MPEG-2/4 AAC (codec id `0x02`). Apple's hardware prefers AAC.

A Bluetooth 2.1-era A2DP source written when nearly every sink advertised exactly one SBC endpoint may walk
the AVDTP `DISCOVER` response naively — take SEID #0, `SET_CONFIGURATION`, `OPEN`, `START` — without
verifying the codec id of the endpoint it picked. If AirPods list AAC first, the Vita configures an **AAC**
endpoint and then pumps **SBC** frames into it.

This predicts *exactly* the observed logs:

| Observation | Explained? |
|---|---|
| `StartAudio(mac,mac,8,0)` returns success | yes — AVDTP negotiation "succeeded", just on the wrong SEP |
| Thousands of 2048-byte buffers accepted, `nz~1900`, `peak=255` | yes — source side is healthy |
| Zero errors anywhere in the stack | yes — nothing is malformed from the source's point of view |
| Speakers mute (OS believes it is routing to BT) | yes |
| Total silence in the buds | yes — H2 decodes SBC bytes as AAC, gets garbage, outputs nothing |
| Gen-1 AirPods reportedly work | plausible — different SEP list / ordering on W1 |

Corroborated by the Linux bug pattern: AirPods Pro 2 on the AAC path go silent while the transport looks
alive; the SBC path is fine.

**This is cheaply falsifiable off-Vita** (see Tier 2 below) and, if true, converts the fix from "hook the
codec somehow" into a well-defined patch: force SceBt to select the SBC SEP.

## The identity / pairing-order finding — TESTED AND DEAD (2026-08-15)

**Result: the Vita showed a clean `AirPods Pro` after a full de-register and reset, and the buds were still
silent.** The identity fix worked and changed nothing. Per the falsification criterion set out below, this
hypothesis is closed. Do not send anyone down this path again.

Kept below for the record, because the *symptom match* on Windows remains the reason it was worth testing —
and because it means the widely-reported Windows fix does not generalise to the Vita, which is itself a
useful signal that the Vita failure is a different mechanism.



Multiple Windows 11 reports match this project's symptom precisely, including the name:

> When paired to iPhone first, they appear as "AirPods Pro - find my", which uses an incompatible Bluetooth
> profile with Windows. When paired to Windows first, they appear as "AirPods Pro" (standard profile).

The working procedure reported on Windows:

1. Forget the AirPods on **every** Apple device (this de-registers from Find My / Activation Lock).
2. Turn **Find My network OFF** for the AirPods: iPhone → Settings → Bluetooth → ⓘ → More Info →
   Find My Network.
3. Hard reset the case: lid open, hold the setup button ~15 s, amber → flashing white.
4. **Pair the non-Apple device FIRST**, before any Apple device touches them again.
5. Verify the name reads `AirPods Pro` with **no `- Find My` suffix**.
6. If re-adding to an iPhone later, strip the owner prefix ("Gabe's AirPods Pro" → "AirPods Pro"), which is
   also reported to matter.

**Why a plain reset is not enough:** any iPhone signed into the same Apple ID will auto-pop and re-claim the
buds within seconds of the lid opening, re-registering them and restoring the broken profile set. The iPhone
must be fully Bluetooth-off (Settings, not Control Center) or out of range during step 4.

**Vita-side prerequisite:** delete the existing pairing in Settings → Bluetooth Devices → Delete, or the Vita
reuses the cached name and link key and you will never see the new identity.

## Vita-side context

- Officially supported profiles: **A2DP, AVRCP, HSP, HID** (HFP/PBAP on the 3G model). Radio is
  **Bluetooth 2.1+EDR**.
- "Pairs but no audio" is a recurring Vita complaint with modern BT audio devices (Jaybird X3,
  Sony SRS-XB10).
- **Important difference in our case:** in those reports the Vita showed *no* headphone icon at all. Here the
  BT icon *does* appear on the volume bar and A2DP start succeeds — a materially better state, meaning the
  Vita believes it has a live A2DP stream. Our failure is downstream of theirs.
- HFP/HSP interference (a confirmed Windows cause — disabling "Hands-Free Telephony" fixes audio there) does
  **not** apply here: the logged capability mask `0x39` contains bit 8 (A2DP) but not bit 4 (HSP), so the
  Vita never brought up a headset channel.

## Risk that this is genuinely impossible

Apple's stated minimum host requirement for AirPods is **Bluetooth 4.0**. The Vita is 2.1+EDR and has no BLE
at all. Related real-world evidence: on FreeBSD, an Intel controller's HCI connection to AirPods Pro "kept
failing" and required swapping to a Realtek RTL8761BU; and the stock FreeBSD pairing daemon had to be
replaced because it lacked SSP, "which modern headphones require."

Counter-evidence, which is strong: on this Vita, pairing, SSP, the encrypted ACL link, and **bidirectional**
AVRCP all work. SSP arrived in Bluetooth 2.1, so the Vita has it. The failure is isolated to the media
stream, not the link. The "4.0 minimum" is blog/marketing-level guidance, not a protocol requirement — but
it cannot be fully dismissed.

## Ranked next actions

### Tier 0 — DONE 2026-08-15, negative result

1. ~~Full de-register + reset + pair the Vita first.~~ Done. Vita showed `AirPods Pro`, no suffix.
2. ~~Report the name.~~ Clean name, still silent. **Identity hypothesis falsified.**

### Tier 1 — control test, ~10 minutes — NOW THE HIGHEST-VALUE STEP

3. Pair any cheap generic SBC headphone to the Vita. If it plays, the fault is AirPods-specific negotiation
   and the SEP hypothesis is worth chasing. If it is also silent, the AirPods are a red herring and the
   problem is the Vita's A2DP source path or another kernel plugin.

### Tier 2 — the diagnostic that settles it, zero Vita risk

4. Pair the AirPods Pro 2 to a Linux box and capture `btmon` during connect. Extract:
   - the `AVDTP DISCOVER` response — the **SEID list and its order**
   - `GET_ALL_CAPABILITIES` per SEID — **codec id per endpoint** (`0x00` SBC, `0x02` AAC)
   - the `SET_CONFIGURATION` that Linux chose

   If AAC is SEID #0 and SBC is SEID #1, the SEP-misselection hypothesis is live and the fix target is exact.

### Tier 3 — on-Vita, read-only, only after Tier 2

5. Capture the device pointer inside the existing **safe export hook** on `ksceBtStartAudio`, stash it, and
   read `dev+0x3B00..0x3B60` from the worker thread — **not** inside the hook — with a pointer range check
   before dereferencing. Compare the configured codec id against the Linux capture.
6. Enumerate all 54 entries in the SceBtForDriver nid table (file `0x1E53C`) looking for read-only
   `Get*Info` / `Get*Status` exports before resorting to any struct poking.
7. Still forbidden: offset hooks at `0xAC0C` / `0x70D8`, and `SetContentProtection(1)`.

### Fallback

8. A Bluetooth transmitter on the Vita's 3.5 mm jack works unconditionally and sidesteps all of this.

## Sources

- PS Vita User's Guide — Bluetooth devices: https://manuals.playstation.net/document/en/psvita/settings/bluetooth.html
- Microsoft Q&A — AirPods Pro pairing to Win11, no audio, shows as "Find My": https://learn.microsoft.com/en-us/answers/questions/4140423/airpod-pro-is-pairing-and-connecting-to-windows-11
- Microsoft Q&A — AirPods Pro 2 connect, no sound, pairing-order fix: https://learn.microsoft.com/en-us/answers/questions/4138170/airpods-pro-2-conncect-through-bluetooth-but-play
- Microsoft Q&A — AirPods Pro connected but not playing sound: https://learn.microsoft.com/en-us/answers/questions/3997180/airpods-pro-connected-but-isnt-playing-sound
- BlueZ issue #1093 — AirPods Pro 2 AAC no sound, SBC works: https://github.com/bluez/bluez/issues/1093
- BlueZ issue #1448 — A2DP signal visible in pavucontrol, no audible output: https://github.com/bluez/bluez/issues/1448
- BlueZ issue #13 — incomplete A2DP capabilities cached, AVDTP session version: https://github.com/bluez/bluez/issues/13
- LibrePods — AAP reverse engineering: https://github.com/librepods-org/librepods
- FreeBSD forums — AirPods Pro A2DP working with blued + virtual_oss: https://forums.freebsd.org/threads/airpods-pro-a2dp-bluetooth-audio-working-on-freebsd-15-with-blued-virtual_oss.102408/
- Nintendo Life — connecting AirPods to Switch (SBC-only host): https://www.nintendolife.com/guides/how-to-connect-bluetooth-headphones-to-nintendo-switch-use-airpods-with-switch
- Make Tech Easier — AirPods on Android/Windows, ear detection unavailable: https://maketecheasier.com/airpods-on-android-windows/
- Apple — Find My network for AirPods: https://support.apple.com/guide/airpods/turn-find-network-supported-airpods-dev0742b9ff3/web
