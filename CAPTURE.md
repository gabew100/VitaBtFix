# Capturing the AirPods AVDTP negotiation on Windows

---

# RESULTS — captured 2026-08-15

Capture saved at `dump/airpods-win-avdtp.pcapng` (896 KB). Parse with:
`tshark -r dump/airpods-win-avdtp.pcapng -Y btavdtp`

**Control test result: audio played fine on Windows.** The buds are healthy. The fault is
**definitively Vita-side.**

## What the AirPods Pro 2 actually advertise

Three Audio Sink endpoints, in this discover order (`items: 3`):

| SEID | Codec | Details |
|---|---|---|
| **1** | **SBC** (`0x00`) | 44100+48000 Hz · Mono+DualChannel+Stereo+JointStereo · block 4/8/12/16 · subbands 4/8 · allocation SNR+Loudness · **bitpool 2..53** · Delay Reporting |
| **2** | **MPEG-2,4 AAC** (`0x02`) | the one Windows chose; works |
| **3** | **non-A2DP** (`0xff`) | **Vendor ID `0x0000004C` = Apple**, 16-byte capability blob · Delay Reporting |

## Hypothesis killed: "Vita grabs SEID #0 and gets AAC"

**Wrong. SBC is SEID 1 and is listed FIRST.** A source that naively takes the first endpoint gets SBC,
correctly. The original framing in `RESEARCH.md` is dead.

## What the capture rules out

- **SBC negotiation cannot be the problem.** The SBC endpoint is maximally permissive — every sampling
  rate, every channel mode, every block length, both subband counts, both allocation methods, bitpool
  2..53. There is no standard SBC configuration the Vita could offer that this endpoint would reject.
- **Delay Reporting is not a hazard.** Windows requested `Delay Reporting` in its SET_CONFIGURATION, and
  only then did the AirPods send the unsolicited `DelayReport(150.0 ms)` command. That is spec-correct
  AVDTP 1.3 behaviour. An AVDTP 1.0/1.2 source that never asks for it should never receive it, so the
  Vita is not being ambushed by an unknown signal.

## Sharpened hypothesis: the Vita's enumeration ends on the Apple vendor endpoint

The original theory was inverted, but its failure mode survives in a better form. The Vita does not
necessarily take the *first* endpoint — if SceBt's enumeration loop overwrites its choice as it walks the
discover list, it ends on the **last** one, which is **SEID 3: Apple's proprietary vendor codec**.

Feeding SBC frames into Apple's vendor-codec endpoint produces exactly what we see: negotiation succeeds,
`StartAudio` returns success, thousands of buffers are accepted, no error is raised anywhere, and the buds
render nothing.

A second, related risk: an AVDTP-1.0-era parser from 2011 has almost certainly never seen media codec type
`0xff` with a 16-byte vendor blob. Mis-parsing that variable-length capability could desync its walk of the
response buffer, or overflow a small fixed-size SEP table. Three endpoints is itself more than many
2011-era sinks offered.

This also offers a clean explanation for why gen-1 AirPods reportedly work: W1 likely does not advertise
the Apple vendor endpoint.

## The one remaining question

**Which SEID does SceBt configure, and with what codec?** That is now the only unknown, and it is purely
Vita-side. We finally have the reference to compare against:

- Configures **SEID 1 / SBC** → the negotiation is right, and the fault is downstream in encoding or
  packetisation (media MTU, RTP/SBC payload header, encoded bitpool vs configured bitpool).
- Configures **SEID 2 or SEID 3** → confirmed root cause, and the fix is a targeted patch forcing SBC.

Get this read-only, via Tier 3 in `RESEARCH.md`. Do not add offset hooks.

---

# Procedure (for reference / re-running)

Goal: find out **which A2DP stream endpoint the AirPods Pro 2 advertise first, and what codec each endpoint
carries.** That single fact confirms or kills the SEP-misselection hypothesis in `RESEARCH.md`.

The AVDTP `DISCOVER` response order is chosen by the *sink*, so the list Windows sees is the same list the
Vita sees. Capturing on Windows is therefore valid evidence about the Vita's situation, with zero Vita risk.

## Free bonus: this doubles as the missing control test

We have no second Bluetooth headphone to test the Vita's A2DP path with. Pairing the AirPods to Windows
gives us the complementary half instead — it tests whether the *buds* can play A2DP at all in their current
state:

| Result on Windows | Meaning |
|---|---|
| AirPods play audio | Buds are healthy. The fault is definitively Vita-side. Read the SEP list and continue. |
| AirPods are silent on Windows too | The buds themselves are in a bad state, or defective. The Vita was never the problem. Fix that first. |

Either outcome is worth the twenty minutes.

**Will this disturb the Vita pairing?** Pairing to Windows does **not** re-register the buds in Find My —
only an Apple device does that. The clean `AirPods Pro` identity survives. AirPods can hold multiple link
keys, but it is not guaranteed, so budget for possibly re-pairing the Vita afterwards. Given the Vita is
already silent, there is little to lose.

## Setup

1. **Install Wireshark** if it is not already present: https://www.wireshark.org/download.html
2. **Download the Bluetooth Test Platform (BTP)**: https://www.microsoft.com/en-us/download/details.aspx?id=100872
   It installs to `C:\BTP`.
3. **Do NOT run `ConfigureMachineForBTP.bat`.** That script is for running Microsoft's full interop test
   suite and expects Secure Boot and BitLocker disabled. You only need the sniffer, which is standalone.
   If `btvs.exe` turns out not to capture without it, stop and reassess rather than weakening the machine's
   security posture for a diagnostic.

## Capture

1. **Remove the AirPods from Windows first** if they are already paired: Settings → Bluetooth & devices →
   AirPods → Remove device. Windows caches A2DP capabilities, and a cached reconnect will skip the
   `DISCOVER` / `GET_CAPABILITIES` exchange that we actually need. The capture must contain a **fresh**
   pairing.
2. Open an **elevated** PowerShell and start the sniffer. Resolved paths on this machine (BTP v1.14.0,
   checked 2026-08-15) — `btvs.exe` ships **x86 only**, which is normal and runs fine on x64:

   ```
   cd C:\BTP\v1.14.0\x86
   .\btvs.exe -Mode Wireshark
   ```

   The `.\` prefix is required — PowerShell will not run an executable from the current directory without
   it. This is what caused `CommandNotFoundException` on the first attempt.

   Wireshark is installed at `C:\Program Files\Wireshark\` but is **not on PATH**, so btvs may fail to
   launch it automatically. If no Wireshark window appears, start it manually against the default TCP pipe:

   ```
   & "C:\Program Files\Wireshark\Wireshark.exe" -k -i TCP@127.0.0.1:24352
   ```

3. In the BTVS window, click **Full Packet Logging**. Without it, large ACL packets are dropped from the log.
4. Now pair the AirPods to Windows: lid open, hold the setup button to flashing white, pair from Settings.
5. Play audio. **Note whether you actually hear it** — that is the control-test result above.
6. Stop the capture and save it as pcapng.

## Reading the result

In Wireshark, filter to the AVDTP signalling:

```
btavdtp
```

Pull these four things out:

1. **`Discover` response** — the list of SEIDs and, critically, **their order**. Note which SEID appears first.
2. **`Get Capabilities` / `Get All Capabilities`** per SEID — the **Media Codec** capability holds the codec
   type byte:
   - `0x00` = SBC
   - `0x02` = MPEG-2/4 AAC
   For any SBC endpoint, also record the sampling frequencies, channel modes, block length, subbands,
   allocation method, and **min/max bitpool**.
3. **`Set Configuration`** — which SEID Windows chose, and with what parameters.
4. Whether **Delay Reporting** appears in the capabilities, and the AVDTP version in use.

## What each outcome means

- **AAC endpoint is listed first, SBC second** → the SEP-misselection hypothesis is live. A BT 2.1-era source
  that grabs SEID #0 without checking the codec id would configure AAC and then send SBC into it, producing
  exactly the observed silence-with-no-errors. The fix target becomes specific and writable: make SceBt
  select the SBC endpoint.
- **SBC is listed first** → hypothesis dead. Move to the sibling causes in `HANDOFF.md` item 2 (media MTU,
  RTP/SBC payload header layout, bitpool outside the negotiated range) and compare the SBC capability bytes
  above against whatever the Vita configures.
- **The SBC endpoint advertises a narrow bitpool range or an unusual channel mode** → note the exact values;
  a mismatch between the configured and encoded bitpool is a known way to get frames silently discarded.

Save the pcapng and the four items above back into this repo.
