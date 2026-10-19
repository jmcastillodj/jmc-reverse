# JMC REVERSE - parameters

Generated from the plugin by `jmc_params`; every row is a host-automatable VST3 parameter with a stable ID.
IDs are a contract with saved projects and are guarded by a unit test: never rename them.

| ID | Name | Type | Range | Default | Unit | What it does |
|---|---|---|---|---|---|---|
| `mode` | Swell Mode | choice | Smooth tail / Phrase | Phrase |  | Phrase (default): classic reverse reverb of the whole source, so the swell carries the spectrum of the whole phrase. Smooth tail: one continuous swell built only from the reverb tail that rings after the source (smoothest envelope, but its colour comes from the end of the phrase). |
| `length` | Length | float | 0.25 ... 16.00 | 4.00 | s | Length of the swell in seconds (free mode). With Tempo Sync on, the LENGTH knob drives Division instead. |
| `decay` | Decay | float | 0.10 ... 20.00 | 2.50 | s | Preferred reverb decay (RT60). Automatically limited to 0.25x-3x the length so the stretch stays natural. |
| `size` | Size | float | 0.25 ... 2.00 | 1.00 | x | Room size: scales the reverb delay lines. |
| `predelay` | Pre-delay | float | 0 ... 250 | 0 | ms | Gap between the end of the swell and the source onset. |
| `damping` | Damping | float | 0.00 ... 1.00 | 0.40 |  | High-frequency damping inside the reverb loop. |
| `lowcut` | Low Cut | float | 20 ... 2000 | 20 | Hz | High-pass filter applied to the source before the reverb. |
| `highcut` | High Cut | float | 1000 ... 20000 | 20000 | Hz | Low-pass filter applied to the source before the reverb. |
| `width` | Width | float | 0.00 ... 2.00 | 1.00 |  | Stereo width (mid/side). Widening beyond 1 is limited so the mono sum never loses more than 6 dB. |
| `dry` | Dry | float | 0.00 ... 1.00 | 0.00 |  | Level of the original source appended after the swell. 0 = swell only. |
| `sync` | Tempo Sync | toggle | off / on | off |  | Lock the length to the host tempo. |
| `division` | Division | choice | 1/4 beat / 1/2 beat / 1 beat / 2 beats / 4 beats / 8 beats / 16 beats | 4 beats |  | Swell length in beats (4 beats = 1 bar in 4/4) when Tempo Sync is on. |
| `preroll` | Pre-roll | float | 0.00 ... 4.00 | 0.00 | s | Silence before the swell starts. |
| `tail` | Tail | float | 0.00 ... 8.00 | 0.00 | s | Forward reverb tail of the source, starting at the onset. |
| `sibon` | Sibilance Detection | toggle | off / on | off |  | Sibilance detection on/off: the swell is built mainly from detected S/SH regions. |
| `sibsens` | Sibilance Sensitivity | float | 0.00 ... 1.00 | 0.50 |  | Sibilance detection sensitivity. |
| `sibamt` | Sibilance Amount | float | 0.00 ... 1.00 | 0.70 |  | How strongly detected sibilants drive the swell. |
| `lowdamp` | Low Damping | float | 0.00 ... 1.00 | 0.20 |  | Extra low-frequency decay inside the reverb. |
| `tilt` | Tilt | float | -8.0 ... 8.0 | 0.0 | dB | Tonal tilt of the swell: negative darker, positive brighter. |
| `balance` | Balance | float | -1.00 ... 1.00 | 0.00 |  | Stereo balance of the swell. |
| `curve` | Fade Curve | choice | S-curve / Linear / Exponential / Logarithmic | S-curve |  | Shape of the fade-in/fade-out. |
| `pitch` | Pitch Movement | float | -12.0 ... 12.0 | 0.0 | st | Pitch movement: the swell starts this many semitones away and arrives at the original pitch at the downbeat (negative = rise, positive = fall). |
| `chorus` | Chorus | toggle | off / on | off |  | Enable the stereo chorus on the swell. |
| `moddepth` | Chorus Mix | float | 0.00 ... 1.00 | 0.50 |  | Chorus mix. |
| `rampon` | Tremolo Ramp | toggle | off / on | off |  | Enable the tempo-ramp tremolo: its speed travels through up to three note values over the length of the swell. |
| `rampmix` | Ramp Mix | float | 0.00 ... 1.00 | 1.00 |  | Ramp tremolo depth. |
| `rampa` | Ramp Start | choice | 1/1 / 1/2 / 1/4 / 1/8 / 1/16 / 1/32 / 1/64 | 1/4 |  | Ramp tremolo speed at the start of the swell (note value at the host tempo). |
| `rampb` | Ramp Mid | choice | Auto / 1/1 / 1/2 / 1/4 / 1/8 / 1/16 / 1/32 / 1/64 | Auto |  | Ramp tremolo speed in the middle of the swell; Auto = halfway between start and end. |
| `rampc` | Ramp End | choice | 1/1 / 1/2 / 1/4 / 1/8 / 1/16 / 1/32 / 1/64 | 1/16 |  | Ramp tremolo speed at the end of the swell. |
| `tremolo` | Tremolo | toggle | off / on | off |  | Enable the tempo-synced tremolo (constant speed). |
| `tremmix` | Tremolo Mix | float | 0.00 ... 1.00 | 1.00 |  | Tremolo depth. |
| `tremrate` | Tremolo Timing | choice | 1/2 / 1/4 / 1/8 / 1/16 / 1/32 | 1/8 |  | Tremolo timing (note value at the host tempo). |
| `modrate` | Mod Rate | float | 0.1 ... 8 | 0.8 | Hz | Chorus rate. |
| `gaterate` | Stutter Rate | choice | Off / 1/8 / 1/16 / 1/32 | Off |  | Rhythmic stutter rate (note value at the host tempo); Off disables it. |
| `gatedepth` | Stutter Depth | float | 0.00 ... 1.00 | 0.80 |  | Stutter depth. |
| `gateramp` | Stutter Ramp | float | 0.00 ... 1.00 | 0.00 |  | Makes the stutter accelerate towards the downbeat. |
| `drive` | Saturation | float | 0.00 ... 1.00 | 0.00 |  | Soft saturation. |
| `fadein` | Fade In | float | 0.00 ... 8.00 | 0.05 | s | Fade-in (attack) of the swell. |
| `fadeout` | Fade Out | float | 0.00 ... 2.00 | 0.00 | s | Fade-out (release) at the end of the swell. |
| `outgain` | Output Gain | float | -24.0 ... 12.0 | 0.0 | dB | Final gain. Peaks above -0.1 dBFS are scaled, never clipped or limited. |
| `trimstart` | Trim Start | float | 0.00 ... 1.00 | 0.00 |  | Start of the source region used (fraction of the file). Set by dragging on the waveform. |
| `trimend` | Trim End | float | 0.00 ... 1.00 | 1.00 |  | End of the source region used (fraction of the file). Set by dragging on the waveform. |
| `autolevel` | Auto Level | toggle | off / on | on |  | Match the swell peak to the source peak (it only scales the swell). |

43 parameters.
