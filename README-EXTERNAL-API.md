# BetterPushback external interface

Other plugins (ATC and crew-voice add-ons, copilot tools, cockpit-sharing
tools) can follow and drive a BetterPushback operation through datarefs and
commands. This file describes that interface: what is published, what each
value means, and the rules that keep it stable for the plugins that use it.

The interface is additive. Nothing here changes how BetterPushback behaves for
a pilot who uses it on its own.

## Conventions

- **Feature detection.** Read `bp/api_version` first. It is raised whenever
  the interface gains something and never lowered; a plugin that needs a
  feature checks for the version that introduced it. If the dataref does not
  exist, the installed BetterPushback predates the interface.
- **Stable numbers.** Every number published here (steps, stages, actions)
  keeps its meaning forever. New values are added at the end; none are reused
  or reordered, even if BetterPushback's internal enums change.
- **Names next to numbers.** Each number has a `*_name` byte dataref holding a
  lower-case machine name (`pushing`, `call_tug`). Names are never translated
  and are meant for logs and debugging; decide on the numbers.
- **Change detection.** `bp/state_seq` changes whenever any value of the
  operation state does. Poll it (it is cheap) and read the rest only when it
  moved.
- **Refresh rate.** The published state is refreshed about ten times a second,
  whether or not the Ground Operations panel is visible. It is the same state
  the panel draws, so the two always agree.
- **Byte datarefs** hold a zero-terminated string; read them with
  `XPLMGetDatab` into a buffer of at least 32 bytes.

## Version 1: operation state

| Dataref | Type | Meaning |
| --- | --- | --- |
| `bp/api_version` | int | Interface version (this document: 18) |
| `bp/state_seq` | int | Changes whenever a value below changes |
| `bp/started` | int | An operation is running (existing dataref) |
| `bp/emergency_tow` | int | The running operation is the Emergency Tow |
| `bp/step` | int | Controller step, see the table below |
| `bp/step_name` | byte[32] | Its machine name |
| `bp/stage` | int | Ground Operations stage: 0 tug, 1 connect, 2 comms, 3 push, 4 clear; -1 when no operation is running |
| `bp/stage_name` | byte[32] | `tug`, `connect`, `comms`, `push`, `clear` or `idle` |
| `bp/action` | int | What the pilot must do next, see below; 0 when nothing is required |
| `bp/action_name` | byte[32] | Its machine name |
| `bp/paused` | int | A pause is requested or held |

Also, outside `bp/state_seq` (they change continuously or never):

| Dataref | Type | Meaning |
| --- | --- | --- |
| `bp/push_speed` | float | The aircraft's speed during the operation, m/s; -1 when no operation is running |
| `bp/push_distance_remaining` | float | Metres of the route left while pushing or towing; -1 otherwise |
| `bp/plugin_version` | byte[32] | BetterPushback's version (its release tag, e.g. `v1.15`) |

The speed and distance are the figures the Ground Operations panel shows.
The existing datarefs `bp/connected`, `bp/plan_complete`, `bp/planner_open`,
`bp/op_complete` and `bp/tug_name` keep their meaning.

### Steps (`bp/step`)

| Value | Name | | Value | Name |
| --- | --- | --- | --- | --- |
| 0 | `off` | | 12 | `pushing` |
| 1 | `tug_load` | | 13 | `stopping` |
| 2 | `start` | | 14 | `stopped` |
| 3 | `driving_up_close` | | 15 | `lowering` |
| 4 | `waiting_for_doors` | | 16 | `ungrabbing` |
| 5 | `opening_cradle` | | 17 | `waiting_to_disconnect` |
| 6 | `waiting_for_parking_brake` | | 18 | `moving_away` |
| 7 | `driving_up_connect` | | 19 | `closing_cradle` |
| 8 | `grabbing` | | 20 | `starting_to_clear` |
| 9 | `lifting` | | 21 | `moving_to_clear` |
| 10 | `connected` | | 22 | `clear_signal` |
| 11 | `starting` | | 23 | `driving_away` |

### Pilot actions (`bp/action`)

The action the Ground Operations panel shows as required, with the command
that performs it.

| Value | Name | Command |
| --- | --- | --- |
| 0 | `none` | |
| 1 | `call_tug` | `BetterPushback/connect_first` |
| 2 | `call_emergency_tow` | `BetterPushback/call_emergency_tow` |
| 3 | `open_planner` | `BetterPushback/start` |
| 4 | `change_plan` | `BetterPushback/start` (during the connected hold) |
| 5 | `pause` | `BetterPushback/pause_resume` |
| 6 | `resume` | `BetterPushback/pause_resume` |
| 7 | `disconnect_tug` | `BetterPushback/disconnect` |
| 8 | `reconnect_tug` | `BetterPushback/reconnect` |
| 9 | `end_disconnect` | `BetterPushback/stop` (the panel asks for confirmation first) |
| 10 | `acknowledge_clear` | `BetterPushback/acknowledge_clear` |

Actions that are a brake setting rather than a button (set or release the
parking brake) are not listed: follow `bp/step` (6 `waiting_for_parking_brake`,
10 `connected`) for those.

### Example: following an operation

```c
static XPLMDataRef seq_dr, step_dr, step_name_dr;
static int last_seq = -1;

void init(void) {
    XPLMDataRef ver = XPLMFindDataRef("bp/api_version");
    if (ver == NULL || XPLMGetDatai(ver) < 1)
        return;                         /* no interface: fall back */
    seq_dr = XPLMFindDataRef("bp/state_seq");
    step_dr = XPLMFindDataRef("bp/step");
    step_name_dr = XPLMFindDataRef("bp/step_name");
}

void poll(void) {                       /* from a flight loop */
    char name[32] = "";
    int seq = XPLMGetDatai(seq_dr);
    if (seq == last_seq)
        return;
    last_seq = seq;
    XPLMGetDatab(step_name_dr, name, 0, sizeof (name) - 1);
    printf("pushback step %d (%s)\n", XPLMGetDatai(step_dr), name);
}
```

## Version 2: crew lines

Every ground-crew line BetterPushback starts is published as text, so another
plugin can show it, log it or (with a later part of the interface) speak it.

| Dataref | Type | Meaning |
| --- | --- | --- |
| `bp/msg_seq` | int | BetterPushback's line counter: goes up by one each time a line starts (a reader polling slowly may see it jump by two); 0 before the first |
| `bp/msg` | int | The line last started, see below; 0 before the first |
| `bp/msg_key` | byte[32] | Its key, e.g. `driving_up` (the voice file's name) |
| `bp/msg_text` | byte[192] | The nominal English sentence, e.g. "Ground to cockpit. Tow is driving up." |
| `bp/msg_caption` | byte[96] | The Ground Operations caption, in BetterPushback's language, e.g. "Ground crew: Tug approaching." |
| `bp/msg_voice` | byte[64] | The voice pack speaking, e.g. `en_GB` or `de`; its language is the part before `_`, `-` or `(` |
| `bp/msg_duration` | float | Length of the line's recording, seconds |
| `bp/msg_playing` | int | The line is being spoken now |
| `bp/msg_channel` | int | Who the line is said to: 0 the ground crew to the cockpit (intercom), 5 the tug driver to Ground on its frequency (version 10, lines 23 to 26) |

A line is published when it starts, even with the simulator's sound off.
`bp/msg_seq` moves once per line; read the other values when it changes. The
text is the nominal wording: the recorded voices, especially the other
languages, may say it slightly differently.

| Value | Key | Text |
| --- | --- | --- |
| 1 | `plan_start` | Ground to cockpit. Please show me where you want to go. |
| 2 | `plan_end` | Ground to cockpit. Plan acknowledged, call me through the menu when you are ready. |
| 3 | `driving_up` | Ground to cockpit. Tow is driving up. |
| 4 | `ready2conn` | Ok, all doors and hatches are closed, ready to connect. Set parking brake. |
| 5 | `ready2conn_nopark` | Ok, all doors and hatches are closed, ready to connect. (the brake is already set) |
| 6 | `winch` | Winching strap and adapter in position. Release parking brake when ready to start pushback. |
| 7 | `connected` | Tow connected and bypass pin inserted. Release parking brake. |
| 8 | `start_pb` | Starting pushback and you may start engines. |
| 9 | `start_tow` | Starting tow and you may start engines. |
| 10 | `start_pb_nostart` | Starting pushback. (engines cannot or should not be started now) |
| 11 | `start_tow_nostart` | Starting tow. (likewise) |
| 12 | `op_complete` | Operation complete, set parking brake. |
| 13 | `disco` | Disconnecting tow. Stand by. |
| 14 | `done_right` | Tow is disconnected and bypass pin has been removed, hand signal on the right, we'll see you next time and have a safe flight. |
| 15 | `done_left` | The same, hand signal on the left. |

Some messages are not recordings: BetterPushback says them through X-Plane's
speech. They are published the same way, with `bp/msg_text` and
`bp/msg_caption` holding the text as spoken (in BetterPushback's language),
`bp/msg_voice` = `xplane`, and `bp/msg_duration` an estimate (X-Plane does not
report how long it speaks).

| Value | Key | What |
| --- | --- | --- |
| 16 | `doors_gpu_open` | "Some doors are still opened or the GPU or the ASU are still connected. I'm waiting for all of them closed and disconnected then I will proceed." |
| 17 | `lights_warning` | "Hey! Quit blinding me with your landing lights! Turn them off!" (or taxi lights), during the push |
| 18 | `system` | A failure or warning about BetterPushback itself, e.g. "Pushback failure: no suitable tug for your aircraft." |
| 19 | `start_pb_chocks` | "Removing chocks, and beginning pushback. You may start engines." (version 7, instead of line 8) |
| 20 | `start_tow_chocks` | "Removing chocks, and beginning the tow. You may start engines." (instead of 9) |
| 21 | `start_pb_chocks_nostart` | "Removing chocks, and beginning pushback." (instead of 10) |
| 22 | `start_tow_chocks_nostart` | "Removing chocks, and beginning the tow." (instead of 11) |
| 23 | `tow_request` | "Ground, tug ready to tow, requesting the route." (version 10, when a tow is called; channel 5) |
| 24 | `tow_holding` | "Ground, tug holding: hold short of runway 15/33." (the hold point's text; channel 5) |
| 25 | `tow_crossing` | "Ground, tug continuing, hold short of runway 15/33 cleared." (on `continue_tow`; channel 5) |
| 26 | `tow_complete` | "Ground, tug, tow complete." (at the end of a tow; channel 5) |

## Version 3: blockers

Why a running operation is waiting, so another plugin can say so ("we can't
connect, the GPU is still on") instead of the pilot guessing. These values are
part of the operation state: `bp/state_seq` moves when they change.

| Dataref | Type | Meaning |
| --- | --- | --- |
| `bp/blocker` | int | What the operation waits for, see below; 0 when nothing |
| `bp/blocker_name` | byte[32] | Its machine name |
| `bp/blocker_item` | byte[64] | For `aircraft_not_ready`: the door-check dataref still open, from `BetterPushback_doors.cfg` (e.g. `laminar/B738/gpu_available`); empty otherwise |
| `bp/blocker_item_kind` | byte[32] | What that dataref stands for, from its name: `door`, `cargo_door`, `gpu`, `asu` or `other` |
| `bp/status_text` | byte[96] | BetterPushback's status line for the current step, in its language (e.g. "Waiting for the parking brakes release") |

| Value | Name | The operation waits until |
| --- | --- | --- |
| 0 | `none` | Nothing is blocking |
| 1 | `aircraft_not_ready` | Every configured door is closed and the GPU/ASU are disconnected |
| 2 | `set_parking_brake` | The parking brake is set (before connecting, or after the push) |
| 3 | `release_parking_brake` | The parking brake is released (connected, ready to push) |
| 4 | `plan_required` | A push route is planned |
| 5 | `external` | Another plugin clears its blocker (version 13); `bp/status_text` has its words |
| 6 | `chocks` | The chocks are removed by the plugin that owns them (version 13) |
| 7 | `held_connected` | After the push, another plugin holds the tug connected (version 16); `bp/status_text` has its words |

The item kind is a best guess from the dataref's name. Aircraft without a
`BetterPushback_doors.cfg` entry are checked with X-Plane's own doors and
GPU, so their item is `sim/flightmodel2/misc/door_open_ratio` (kind `door`)
or `sim/cockpit2/electrical/GPU_generator_volts` (kind `gpu`).

## Version 4: external voice

Another plugin can speak the ground-crew lines itself (its own voices, the
pilot's language, its own audio routing) while BetterPushback stays silent.
The operation still waits for each line to be spoken before moving on, as it
does with its own recordings, through a small handshake.

| Dataref | Type | Written by | Meaning |
| --- | --- | --- | --- |
| `bp/voice_mode` | int | the other plugin | 0: BetterPushback speaks (default); 1: the other plugin speaks |
| `bp/voice_heartbeat` | int | the other plugin | Count it up at least once a second while you are ready to speak |
| `bp/voice_done_seq` | int | the other plugin | After you finish speaking a line, write its `bp/msg_seq` here |
| `bp/voice_external` | int | BetterPushback | 1 while lines are being handed to the other plugin |

How it works:

- Lines are handed over while `bp/voice_mode` is 1 **and** the heartbeat has
  moved in the last 3 seconds (`bp/voice_external` says which). A line handed
  over is not played by BetterPushback; it is published as usual
  (`bp/msg_seq`, `bp/msg_text`, ...) for the other plugin to speak. The same
  goes for the messages BetterPushback would say through X-Plane's speech
  (lines 16 to 22): published, not spoken. The operation never waits for
  those, so reporting them in `bp/voice_done_seq` is optional.
- Where the operation waits for a line to finish (before connecting, before
  starting the push after "Release parking brake", before the tug lowers and
  before it drives clear), it waits until `bp/voice_done_seq` reaches that
  line's `bp/msg_seq`: the line lasts as long as the other plugin takes to say
  it, longer or shorter than the recording.
- Safeguards: a line is never waited for longer than its recording plus 8
  seconds. If the heartbeat stops, BetterPushback stops waiting (back to the
  recording's length), speaks the following lines itself and logs the change.
  The handshake only delays the next step of the normal sequence; Stop, Pause
  and End act at once, as before.
- `bp/msg_playing` follows the other plugin's speech for lines it speaks.
- `bp/ground_crew_audio_volume` still sets the volume of BetterPushback's own
  recordings.

```c
/* In a flight loop, while voice_mode is 1: */
XPLMSetDatai(heartbeat_dr, ++heartbeat);
int seq = XPLMGetDatai(msg_seq_dr);
if (seq != last_seq && XPLMGetDatai(voice_external_dr)) {
    last_seq = seq;
    XPLMGetDatab(msg_text_dr, text, 0, sizeof (text) - 1);
    start_speaking(text, seq);          /* your TTS or recordings */
}
/* ... and when the line for `finished_seq` has been spoken: */
XPLMSetDatai(voice_done_seq_dr, finished_seq);
```

## Version 5: external routes

In real operations the push is planned on the ground: each stand has its push
procedure, and the ground controller may name the direction ("push-back
approved, facing west"). Another plugin that knows these (an ATC add-on, a
ground-handling add-on) can supply the route, and BetterPushback does what it
is good at: it fits the legs to the aircraft and drives them.

A route is text: a header, then one line per position the aircraft should
reach, in order.

```
BPROUTE 1
# push back onto taxiway B facing west
P 47.79312345 12.99751234 180.0
P 47.79240000 12.99690000 270.0
```

- Each `P` line is `latitude longitude heading`: the aircraft's reference
  point (the position X-Plane reports for the aircraft) and its true heading
  there. Up to 64 positions (16 before version 8); lines starting with `#`
  are comments.
- BetterPushback fits each leg from the previous position (the first from
  where the aircraft stands) within the aircraft's turning limits: a position
  behind the aircraft is reached by pushing, one ahead by towing.

| Dataref / command | Type | Meaning |
| --- | --- | --- |
| `bp/route_in` | byte[4096], writable | Write the route text here, zero-terminated (byte[2048] before version 8) |
| `BetterPushback/load_route` | command | Load `bp/route_in` as the current route |
| `BetterPushback/check_route` | command | Check whether `bp/route_in` fits, changing nothing (see below) |
| `BetterPushback/clear_route` | command | Remove the current route |
| `bp/route_seq` | int | Goes up by one with every load, check or clear, accepted or not |
| `bp/route_status` | int | Answer to the last load, check or clear: 1 accepted (fits), 2 rejected (0 before any) |
| `bp/route_reason` | byte[128] | Why it was rejected, e.g. "position 2 cannot be reached from the one before it within this aircraft's turning limits" |
| `bp/route_current` | byte[4096] | The current route, in the same text form (empty when there is none) |
| `bp/route_source` | int | Where the current route came from: 0 none, 1 planner (drawn, changed or accepted there), 2 saved slot (taken unchanged), 3 external (`load_route`), 4 scenery (the scenery's WED route, version 15) |
| `bp/route_source_name` | byte[16] | `none`, `planner`, `saved`, `external` or `scenery` |

When a route is accepted:

- A route can be loaded or cleared **before the tug is called** (the classic
  plan-first flow), **while the connected tug waits for a plan**
  (`bp/blocker` = `plan_required`), and **during the connected hold** with the
  parking brake set (as *Change plan* in the panel). It is refused while the
  planner is open, during a manual push, in shared-cockpit slave mode, and
  once the tug is connecting or the push is under way.
- *Call tug* (`BetterPushback/connect_first`) starts every operation with an
  empty route, so in that workflow load the route once `bp/blocker` reads
  `plan_required`: the operation continues on its own, as when the pilot
  accepts a plan in the planner.
- A rejected route leaves the current route unchanged.
- `BetterPushback/check_route` fits the route exactly as `load_route` would,
  from where the aircraft stands now, and answers the same way (1 fits, 2 does
  not, with the reason), but changes nothing and works at any time. An ATC
  plugin can so check that a push direction suits the aircraft before it
  approves it, and before the tug is called.

`bp/route_current` lists each leg's end: the positions given to `load_route`,
or the points clicked in the planner, with `push` or `tow` after each. It can
be fed back to `load_route` unchanged. `bp/route_source` tells a plugin
whether the route is still the one it loaded: it changes to `planner` or
`saved` as soon as the pilot replaces or edits it, and to `none` when it is
cleared.

```c
XPLMSetDatab(route_in_dr, (void *)text, 0, (int)strlen(text) + 1);
int before = XPLMGetDatai(route_seq_dr);
XPLMCommandOnce(XPLMFindCommand("BetterPushback/load_route"));
/* The answer is ready when the command returns: */
if (XPLMGetDatai(route_seq_dr) != before && XPLMGetDatai(route_status_dr) == 2)
    XPLMGetDatab(route_reason_dr, reason, 0, sizeof (reason) - 1);
```

## Version 6: saved routes

The planner keeps two saved routes per published stand and aircraft profile
(see USER_GUIDE.md, "Saved routes"). Another plugin can use the same slots:
save a route it supplied so BetterPushback offers it next time, even without
that plugin, or load one the pilot saved.

| Dataref / command | Type | Meaning |
| --- | --- | --- |
| `bp/route_slot` | int, writable | The slot the commands below use: 1 or 2 |
| `BetterPushback/save_route_slot` | command | Save the current route to that slot for this stand |
| `BetterPushback/load_route_slot` | command | Make that slot's route the current route |
| `bp/route_stand` | byte[48] | The published stand the aircraft stands on, e.g. `LOWS W1`; empty when it is not on one |
| `bp/route_slot1`, `bp/route_slot2` | int | 1 when the slot holds a route for this stand and aircraft, 0 when empty, -1 when not on a published stand |

- Both commands answer through `bp/route_seq`, `bp/route_status` and
  `bp/route_reason`, as `load_route` does. A route loaded from a slot has
  `bp/route_source` = 2 (`saved`).
- The stand is recognised as the planner does: the aircraft must stand on an
  apt.dat start position. The slot information is refreshed every few seconds
  while the route may change, and right after either command.
- BetterPushback starts only when it is used, and loading a flight or an
  aircraft resets it. Until it is used again the stand is empty and the slots
  read -1, unless a plugin reads `bp/route_slot1` or `bp/route_slot2`: then
  the stand is worked out within a few seconds of the aircraft standing still
  on the ground. A plugin that wants the stand should therefore read a slot.
- Loading follows the rules for changing a route (see version 5). Saving needs
  a current route that starts at this stand. Neither works during an
  Emergency Tow, whose routes are never saved.
- Saving overwrites the slot. A plugin should save only when its user asks
  for it.

## Version 7: chocks

The pilot (or another plugin) may have chocked the wheels at the stand, and a
chocked aircraft cannot be pushed. As the push or tow starts, after the
parking brake is released, the crew now removes the chocks: X-Plane's
(`sim/flightmodel2/gear/is_chocked`, any wheel; removed with
`sim/flight_controls/remove_chocks`) and the Zibo 737's
(`laminar/B738/fms/chock_status`, which BetterPushback used to clear silently
before connecting).

When there were chocks to remove, the push-start line is replaced by the same
line chocks first, said through X-Plane's speech (there is no recording of
it) and published as lines 19 to 22 (see version 2): "Removing chocks, and
beginning pushback." with ", you may start engines" exactly where line 8 or 9
would have said it. Without chocks nothing changes. A plugin that speaks the
lines (version 4) should therefore treat lines 19 to 22 as push-start lines,
like 8 to 11.

## Version 8: routes along taxiways

Tows often follow a taxi route rather than a few chosen positions. Route
text version 2 adds two kinds of line for that, and a route may now hold up
to 64 positions (`bp/route_in` and `bp/route_current` grow to 4096 bytes).

```
BPROUTE 2
# tow to Hangar 7 across runway 15/33
P 47.79260200 12.99802200 101.6
W 47.79302000 12.99900000
H 47.79330000 13.00120000 hold short of runway 15/33
W 47.79380000 13.00450000
P 47.79398700 13.00676300 340.8
```

- `W lat lon` is a waypoint: the aircraft's reference point passes over it
  facing the next position. The last line of a route, if it is a waypoint,
  keeps the heading the aircraft arrives with. A taxi route's points convert
  directly into `W` lines.
- `H lat lon text` is a hold point: a waypoint where the tug will stop and
  wait (version 10 below); the text says why ("hold short of runway 15/33").
- `P` lines are as in version 1. Version 1 routes (`BPROUTE 1`, `P` lines
  only) are read as before.
- `bp/route_current` writes `BPROUTE 2` with its `H` lines when the route has
  a hold point, else `BPROUTE 1` as before. Waypoints are written as `P` lines
  with the heading they were given.

## Version 9: tows

A ground-handling or ATC plugin can have the tug tow the aircraft along a
route it supplies: to a hangar, a remote stand, the isolated position, back
to the stand. Unlike Emergency Tow, a tow can be called whenever the aircraft
is stopped on the ground with its gear down, not only after a push, and not
only at a stand.

| Dataref / command | Type | Meaning |
| --- | --- | --- |
| `bp/tow_kind` | int, writable | The kind of the next tow, see below (default 2) |
| `BetterPushback/call_tow` | command | Load `bp/route_in` and call the tug for a tow of that kind; answered like `load_route` (`bp/route_seq`, `bp/route_status`, `bp/route_reason`) |
| `bp/tow_state` | int | 0 idle, 1 connecting (the tug drives up and connects), 2 towing (moving, then stopping and disconnecting at the end), 3 holding (version 10), 4 done (the tug drove away), 5 failed (ended early) |
| `bp/tow_state_name` | byte[16] | `idle`, `connecting`, `towing`, `holding`, `done` or `failed` |
| `bp/tow_speed_limit` | float, writable | m/s: the tow goes no faster (0: the tug's own speeds). Read while towing, so it may change during the tow |

| `bp/tow_kind` | Name | |
| --- | --- | --- |
| 1 | `return_to_stand` | back onto a stand |
| 2 | `reposition` | to another place on the apron |
| 3 | `hangar` | into or out of a hangar |
| 4 | `isolated_position` | to the airport's isolated parking position |
| 5 | `remote_stand` | to a remote stand |
| 6 | `runup_area` | to the engine run-up area |
| 7 | `recovery` | a disabled aircraft (version 11 adds its rules) |
| 8 | `tow_in` | on arrival, into a stand that cannot be taxied into (version 16); the wing walker guides it as on a push |

- The route is fitted from where the aircraft stands, as `load_route` does: a
  position behind it is reached by pushing, one ahead by towing. A route that
  does not fit is refused with the reason, and nothing starts.
- The tow then runs as a push does: the tug drives up and connects, the pilot
  releases the parking brake, the tug tows along the route, stops at the end,
  disconnects and drives away. `BetterPushback/stop` ends it at any time.
- `call_tow` is refused, with the reason, while another push or tow runs, in
  shared-cockpit slave mode, with the preferences open, and when the kind is
  not 1 to 8. A tow route is never saved as a stand's route, and the wing
  walker stays away, as for Emergency Tow, except on a tow-in.
- `bp/tow_state` stays `done` or `failed` after a tow until the next one.

```c
XPLMSetDatab(route_in_dr, (void *)route_text, 0, (int)strlen(route_text) + 1);
XPLMSetDatai(tow_kind_dr, 3);           /* hangar */
XPLMSetDataf(tow_speed_limit_dr, 2.0f); /* walking pace inside the hangar */
int before = XPLMGetDatai(route_seq_dr);
XPLMCommandOnce(XPLMFindCommand("BetterPushback/call_tow"));
if (XPLMGetDatai(route_seq_dr) != before && XPLMGetDatai(route_status_dr) == 2)
    XPLMGetDatab(route_reason_dr, reason, 0, sizeof (reason) - 1);
```

## Version 10: hold points

A tow across a runway or a busy taxiway waits for Ground's or Tower's
clearance. An `H` line in a route (version 8) is a hold point: the tug
brings the aircraft to a stop with its reference point on the hold point,
stays connected and waits until it is told to go on.

| Dataref / command | Type | Meaning |
| --- | --- | --- |
| `bp/hold` | int | 1 while stopping or stopped at a hold point |
| `bp/hold_reason` | byte[128] | The `H` line's text ("hold short of runway 15/33") |
| `BetterPushback/continue_tow` | command | Go on from the hold point; answered like `load_route` (refused, with the reason, when the tug is not holding) |
| `bp/tow_state` | int | 3 (`holding`) while a tow is stopped at a hold point |

- The tug slows to a stop at the hold point as it does at the route's end.
  An `H` as the route's last line is simply its end.
- The tug driver calls Ground (crew lines 23 to 26 above, on channel 5,
  `bp/msg_channel`): the request when a tow is called, holding with the hold
  point's text, continuing when told to go on, complete at the end. They
  are said through X-Plane's speech, or handed to another plugin's voice as
  the other lines are (version 4), which can say them on Ground's frequency.
- Hold points work for any route loaded with `load_route` or `call_tow`.
  While held, `BetterPushback/stop` ends the operation where it stands, as
  from a pause.

## Version 11: recovery and the emergency stop

**Recovering a disabled aircraft** (`bp/tow_kind` 7, `recovery`): an
aircraft stuck on a runway with burst tyres or failed brakes.

- `call_tow` refuses a recovery, with the reason, when the nose gear is not
  down on its wheels ("the aircraft must be lifted before it can be towed")
  or the nose wheel is off the pavement. Those are jobs for other equipment.
- With failed brakes the parking brake may not hold. On a recovery the tug
  connects with only the chocks holding the aircraft
  (`sim/flightmodel2/gear/is_chocked`, any wheel). The chocks are removed as
  the tow starts, as for any push (version 7).
- A recovery goes no faster than 2 m/s (deflated tyres), or slower if
  `bp/tow_speed_limit` says so.

**The emergency stop**: a fuel spill, a fire or a vehicle in the way.

| Command | Meaning |
| --- | --- |
| `BetterPushback/emergency_stop` | Stop the push or tow at once: the tug brakes as hard as it can (about 2 m from tow speed) and stays connected. It then holds as at a hold point: `bp/hold` 1, `bp/hold_reason` "emergency stop", `bp/tow_state` 3. `BetterPushback/continue_tow` goes on; `BetterPushback/stop` ends the operation where it stands. Answered like `load_route`; refused, with the reason, when nothing is being pushed or towed |

## Version 12: the tug, estimates and milestones

What another plugin needs to show the tug among its own vehicles, to say
truthfully how long things will take, and to report the operation's times.
The tug's state and the milestones are part of the operation state:
`bp/state_seq` moves when they change.

| Dataref | Type | Meaning |
| --- | --- | --- |
| `bp/tug_state` | int | 0 none, 1 at its parking (version 14, with a tug path), 2 driving in, 3 at the stand waiting to connect (also while prepositioned, version 14), 4 connecting, 5 connected, 6 moving (pushing or towing, stopping at the end), 7 disconnecting (lowering, moving clear), 8 returning (driving away) |
| `bp/tug_state_name` | byte[16] | `none`, `at_parking`, `driving_in`, `at_stand`, `connecting`, `connected`, `moving`, `disconnecting`, `returning` |
| `bp/tug_lat`, `bp/tug_lon`, `bp/tug_hdg` | float | Where the tug is and its true heading; 0 when there is no tug |
| `bp/tug_eta_s` | float | Seconds until the tug reaches the aircraft while driving in; 0 once there; -1 without a tug |
| `bp/tug_origin` | byte[48] | Where the tug came from: the name of the parking place in `bp/tug_path_in` (version 14), "parking" when it has none; empty when the tug comes the usual way |
| `bp/tug_eta_if_called_s` | float | Before the tug is called: seconds from calling it until it would be connected (an estimate); -1 once it is called |
| `bp/connect_time_s` | float | Seconds from the tug's arrival to connected (an estimate) |
| `bp/route_time_s` | float | Seconds to drive the current route at the tug's speeds, with the stops and pauses at each change of direction; -1 without a route |
| `bp/time_off_blocks` | float | Zulu seconds when the aircraft first moved under the tug (the airline's OUT time); -1 before |
| `bp/time_push_complete` | float | Zulu seconds when it stopped at the end of the route; -1 before |
| `bp/time_tug_clear` | float | Zulu seconds when the tug, disconnected, drove away; -1 before |

The times are kept after the operation until the next one starts. The
estimates before calling the tug use today's drive-up, or from version 14
the length of the road in `bp/tug_path_in` when one is written.

## Version 13: what another plugin takes over

A ground-handling plugin runs the rest of the turnaround around the push.
These let it take over the parts both would otherwise do, and tell
BetterPushback what only it knows.

| Dataref | Type | Written by | Meaning |
| --- | --- | --- | --- |
| `bp/external_block` | int, writable | the other plugin | 1: the tug must not connect, nor the push start (a belt loader still at the aft hold, the catering truck lowering) |
| `bp/external_block_text` | byte[96], writable | the other plugin | Why, in its words; shown as "Waiting: ..." in `bp/status_text` and the panel |
| `bp/external_block_owner` | byte[32], writable | the other plugin | Its name, for the log |
| `bp/chocks_external` | int, writable | the other plugin | 1: BetterPushback leaves the chocks alone (no removal as the push starts, version 7) and a chocked aircraft blocks the push start (blocker 6 `chocks`) until the other plugin's crew removes them |
| `bp/walkers_external` | int, writable | the other plugin | 1: BetterPushback draws no wing walker; the other plugin draws its own |
| `bp/walkers_needed` | int | BetterPushback | 1 while an operation runs on which BetterPushback would have shown its wing walker |
| `bp/pin_shown` | int | BetterPushback | 1 while the tug driver shows the bypass pin (the clear signal), for the other plugin's headset crew to answer it |

- The external blocker is checked before the tug drives up to the nose wheel
  (where the doors are checked) and again before the push starts. Clearing it
  lets the operation go on by itself. While blocked, `bp/blocker` reads 5
  `external`.
- All three take effect for the next operation if set while one is running,
  except the blocker, which acts at once.
- Without another plugin nothing changes: the defaults are 0.

## Version 14: where the tug comes from, and the prepositioned tug

BetterPushback does not know an airport's service roads. A plugin that does
(a ground-handling plugin reading the airport's ground-vehicle network) can
tell it where the tug parks and which road it takes to the stand. The tug
then waits at that parking place, drives the road to the aircraft, and after
the push drives the same road back and is put away there.

| Dataref | Type | Written by | Meaning |
| --- | --- | --- | --- |
| `bp/tug_path_in` | byte[2048], writable | the other plugin | The tug's parking place and road, in the text form below; read when the tug is called |
| `bp/tug_path_status` | int | BetterPushback | For the last tug called: 0 no path written (the usual way), 1 the path was used, 2 it was not; `bp/tug_path_reason` says why |
| `bp/tug_path_reason` | byte[128] | BetterPushback | Why the path was not used ("line 3: expected \"W <latitude> <longitude>\"", "the origin is more than 3000 m from the aircraft", "the tug cannot drive this road within its turning limits") |
| `bp/prepositioned` | int | BetterPushback | 1 while a prepositioned tug waits at the nose to be called in |

The text:

```
BPTUG 1
# comments and blank lines are allowed
O 47.25981 11.34567 270 Tug parking West
W 47.26010 11.34612
W 47.26044 11.34650
```

- `O <latitude> <longitude> <heading> [name]`: the parking place, once. The
  name is shown in `bp/tug_origin`.
- `W <latitude> <longitude>`: a point on the road, in the order the tug
  drives in; up to 32. None: the tug drives from its parking straight to the
  stand.
- Every point must be within 3 km of the aircraft. From the last point the
  tug makes its usual approach, crossing in front of the nose from the side
  it comes from.
- When the path cannot be used, the tug comes the usual way, and
  `bp/tug_path_status` and the log say why. A path is never half driven.
- The way back is the road in reverse. The tug is given time to drive it
  (the usual 30 seconds plus the road at half the tug's speed) before it is
  put away, wherever it is then.
- In a shared cockpit the path is used on the master's side only.

| Command | Meaning |
| --- | --- |
| `BetterPushback/preposition` | Bring the tug to the aircraft to wait, not connected: it drives up as for `connect_first`, stops at the nose (`bp/tug_state` 3, `bp/prepositioned` 1) and waits, before the doors are checked. `BetterPushback/connect_first` then calls it in to connect, and the operation goes on as after `connect_first`; `BetterPushback/stop` sends it away. Answered like `load_route`; refused, with the reason, when an operation is already under way or the tug cannot come |

## Version 15: the scenery's push-back routes

A scenery author can draw push-back routes in WorldEditor (string placements
with the `pushback` or `pushback_tug` resource). `BetterPushback/recreate_scenery_routes`
has always converted each scenery's `earth.wed.xml` into
`BetterPushback_routes.dat` beside it, but nothing used the result since the
saved slots came. They are now a third source beside the two slots.

| Dataref / command | Type | Meaning |
| --- | --- | --- |
| `bp/scenery_route_count` | int | How many scenery routes start where the aircraft stands (within 20 m, its heading within 30 degrees); -1 until BetterPushback is initialised (reading it has it initialised, as `bp/route_slot1` does) |
| `bp/scenery_route_index` | int, writable | Which of them the command loads: 1 is the nearest |
| `BetterPushback/load_scenery_route` | command | Make that scenery route the current route; answered like `load_route` |

- The route is fitted to the aircraft as a route from `load_route` is: through
  the end of each WED node, from where the aircraft stands, within its
  turning limits, or refused with the reason. A loaded route has
  `bp/route_source` = 4 (`scenery`).
- It can be loaded when a route can be (see version 5), not during a tow.
- The count is refreshed with the slots, every few seconds while the route
  may change, and right after the command.
- The conversion still runs only on `recreate_scenery_routes`, which now
  converts every project again (files converted before marked no nodes; their
  routes are fitted through each change of direction instead). Distributed
  sceneries rarely include their `earth.wed.xml`, so this mostly serves
  sceneries made or edited locally.

## Version 16: more ways to leave a stand

Real operations have more than one way to leave a stand.

| Dataref | Type | Written by | Meaning |
| --- | --- | --- | --- |
| `bp/hold_connected` | int, writable | the other plugin | 1: after the push the tug stays connected and nothing is disconnected until it is set back to 0 (de-icing at the stand exit, a cross-bleed engine start) |
| `bp/hold_connected_text` | byte[96], writable | the other plugin | Why, in its words; shown as "Holding connected: ..." in `bp/status_text` and the panel |

- While held, `bp/blocker` reads 7 `held_connected` and the disconnect is not
  offered to the pilot; any approval given before is asked for again after.
  The automatic disconnect (the pilot's preference) follows the release.
- Set it before or during the push; it acts when the push has stopped.
- A tow into a stand on arrival is tow kind 8 `tow_in` (see version 9).

Already possible, and so described here rather than changed:

- **Engines started at the stand before the push** (an inoperative APU, a
  start with the air start unit): the engines may run when the tug connects
  on aircraft with more than one engine or with jet engines; BetterPushback's
  doors and GPU/ASU check waits only for the ASU to be disconnected, which
  is done after the start.
- **Push and pull** (push back, then tow forward to line up): a route whose
  last position is ahead of the one before it, from `load_route` or the
  planner.

## Version 17: winter and night

Pushes on a wet, snowy or icy apron are slower and gentler, and the tug's
lights are on at night and in low visibility.

| Dataref | Type | Written by | Meaning |
| --- | --- | --- | --- |
| `bp/surface` | int | BetterPushback | The apron now: 0 dry, 1 wet, 2 standing water, 3 snow, 4 ice, 5 snow on ice |
| `bp/surface_name` | byte[16] | BetterPushback | `dry`, `wet`, `standing_water`, `snow`, `ice`, `snow_on_ice` |
| `bp/surface_factor` | float | BetterPushback | What the push's top speeds are multiplied by on it (1 dry, 0.9 wet, 0.8 standing water, 0.6 snow, 0.4 ice) |
| `bp/surface_override` | int, writable | the other plugin | 0-5: the apron as that plugin knows it (cleared, de-iced, sanded); -1 (the default): X-Plane's runway condition |
| `bp/tug_lights` | int | BetterPushback | 1 when the tug's lights are on: the sun less than 5 degrees above the horizon, or visibility under 1500 m |

- The surface is X-Plane's runway condition
  (`sim/weather/region/runway_friction`) unless overridden. It is taken as
  the tug is called and kept for the operation.
- On it, the push's top speeds forward and back, and its acceleration and
  braking (1 dry, 0.8 wet, 0.7 standing water, 0.5 snow, 0.35 ice), are
  lowered; a tow speed limit (version 9) applies on top. The estimates
  (version 12) use the lowered speeds.
- The tug's lights (`bp/anim/vehicle_lights` and the cradle lights) came on
  only at dusk; now also in fog. Walkers with lit wands are the other
  plugin's (version 13).

## Version 18: availability

Small fields have no tug for every aircraft, and big ones are busy.

| Dataref | Type | Written by | Meaning |
| --- | --- | --- | --- |
| `bp/tug_available` | int | BetterPushback | 1 when a tug in BetterPushback's library suits this aircraft at this airport, 0 when none does, -1 until BetterPushback is initialised; refreshed about every half minute while no tug is called (the library is asked again only when the aircraft or the airport changes) |
| `bp/tug_available_reason` | byte[128] | BetterPushback | Why not ("no tug in the library suits this aircraft (2 t); see Log.txt"); the log lists each tug and why it does not suit |
| `bp/tug_wait_s` | float, writable | the other plugin | At a busy hub: seconds until a tug is free (0, the default: none). The tug is called as usual, but comes only after the wait |
| `bp/tug_wait_remaining_s` | float | BetterPushback | Seconds still to wait; 0 when not waiting |

- `call_tow` and `preposition` are refused at once, with the reason, when no
  tug suits, instead of failing once the tug is called. The pilot's own
  call speaks the failure as before.
- While the tug waits, `bp/tug_state` is 1 (at its parking), `bp/tug_eta_s`
  counts down the wait and the drive, and `bp/status_text` reads "All tugs
  are busy: the tug comes in about 6 minutes". Lowering `bp/tug_wait_s`
  shortens the wait at once; 0 ends it.
- Before the tug is called, `bp/tug_eta_if_called_s` includes the wait.
- Light aircraft moved by hand with a towbar need models the library does not
  have yet; until then they get "no tug suits".

## Testing

### Unit tests

All run as part of `tests/run_all_tests.sh`:

- `tests/run_ext_api_state_tests.sh` checks that every controller step and
  Ground Operations action has a unique, fixed public number and name, and
  that the published state follows the controller through idle, pushing,
  pause and the Emergency Tow, and that blockers are published with the open
  item and its kind only while an operation runs.
- `tests/run_ext_api_msgs_tests.sh` checks that every crew line has a unique,
  fixed number, key, text and caption, and that the brake-set and
  no-engine-start variants leave out the part that does not apply, that
  the X-Plane speech lines (16 to 22) follow the recordings, and that each
  chocks line matches its push-start line.
- `tests/run_ext_api_voice_tests.sh` checks the external-voice timing: an
  external line lasts until it is reported finished, never longer than its
  recording plus the grace, and falls back to the recording when the
  heartbeat stops.
- `tests/run_ext_api_msgs_tests.sh` also checks the tug driver's tow lines:
  their fixed numbers and keys, their texts, and that only they are on the
  ground channel.
- `tests/run_ext_api_handoff_tests.sh` checks that nothing changes by
  default, that an external blocker holds the connection and the start with
  the other plugin's words (a text it did not terminate is cut), and that
  chocks and walkers left to the other plugin behave as described.
- `tests/run_ext_api_tug_tests.sh` also checks the wait at a busy hub and
  its words.
- `tests/run_ext_api_surface_tests.sh` checks X-Plane's runway condition is
  read into the surfaces, that another plugin's override wins, that every
  slippery surface is slower and gentler than the one before, and when the
  lights are on.
- `tests/run_ext_api_handoff_tests.sh` also checks that the tug held
  connected after the push says why, in the other plugin's words or plainly,
  and blocks nothing before; `tests/run_tow_session_tests.sh` that a tow-in
  keeps the wing walker and other tows do not.
- `tests/run_scenery_route_tests.sh` checks that a converted route file is
  read and a broken route in it left out, that only routes starting at the
  aircraft are offered, nearest first, and that the route passes the end of
  each WED node (or each change of direction in an older file).
- `tests/run_ext_api_tugpath_tests.sh` checks that a path is read, that an
  empty one is no path and no complaint, that each mistake is named with its
  line, that a path far from the aircraft is refused, and the road's length
  and the time allowed for the drive back.
- `tests/run_ext_api_tug_tests.sh` checks the tug's state for every step
  (never going back during an operation), the route time with its stops and
  pauses at each change of direction, the arrival estimate, and that each
  milestone is set once and kept.
- `tests/run_tow_session_tests.sh` checks the tows' fixed numbers and names,
  the refusals with their reasons, the state following the controller (and
  staying done or failed after a tow), the speed limit, that tows are not
  saved and have no wing walker, and the recovery rules (refusals, the 2 m/s
  cap, chocks instead of the parking brake).
- `tests/run_ext_api_route_tests.sh` checks reading and writing route text:
  comments, line endings, every kind of bad input refused with a reason that
  names the line, the position limit (64), waypoints facing the next position,
  hold points with their text, and that a written route reads back;
  and the rule for when a route may change (before an operation, while the
  tug waits for a plan, during the connected hold; never with the planner
  open, during a manual push or in slave mode).

### In the simulator: the example client

`tools/ext_api_demo` is a small separate plugin that uses the interface as
another plugin would. It shows the published state in a window, writes every
change (state, crew lines, route answers) to X-Plane's `Log.txt` with the
prefix `BPDemo:`, and adds commands to bind to keys:

| Command | Does |
| --- | --- |
| `BPDemo/toggle_window` | Show or hide the window |
| `BPDemo/load_straight_back` | Load a 40 m straight push |
| `BPDemo/load_push_turn_left` | Load a push ending 90 degrees left (tail right) |
| `BPDemo/load_tight_turn` | Load a push too tight to fit, to see the refusal |
| `BPDemo/check_push_turn_left`, `BPDemo/check_tight_turn` | The same with `check_route` |
| `BPDemo/clear_route` | Clear the route |
| `BPDemo/save_slot1`, `BPDemo/load_slot1` | Use saved slot 1 |
| `BPDemo/voice_toggle` | Be the external voice: count the heartbeat and report each line finished after 1.5 times its recording |
| `BPDemo/voice_stall` | Stop or restart the heartbeat, to see BetterPushback take its voice back |

Build it with `tools/ext_api_demo/build_demo.sh` in the build container (see
the script), and copy `tools/ext_api_demo/BPExtDemo` to
`X-Plane/Resources/plugins`.

A test run, at a published stand:

1. Before calling the tug: the window shows interface 7, step `off`, the
   stand's name.
2. *Call tug*: the step and stage follow the panel's rail; each crew line
   appears. With a door open, the blocker is `aircraft_not_ready` and names
   it.
3. When the tug waits for a plan (blocker `plan_required`):
   `BPDemo/check_tight_turn` (rejected, with a reason; nothing changes), then
   `BPDemo/load_push_turn_left` (accepted; the operation continues; the route
   source is `external`).
4. `BPDemo/voice_toggle` before a push: the recordings stop and the
   operation holds after each line until the demo reports it finished.
   `BPDemo/voice_stall`: the recordings come back.
5. Still on the stand with a route loaded: `BPDemo/save_slot1`, then slot 1
   shows 1, and on the next visit to this stand the planner offers the route.
6. Chock the wheels (`sim/flight_controls/install_chocks`) before releasing
   the brake for the push: the line is `start_pb_chocks` (or a variant),
   "Removing chocks, and beginning pushback...", and the chocks are gone.
