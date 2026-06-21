**New `opts` remote command.** The JSON remote protocol (BLE/serial) now answers
`{"cmd":"opts"}` with a `{"t":"opts",...}` object listing the selectable mode,
band, step, and bandwidth labels plus the AGC range, each with the current index.
Companion apps use this to populate native pickers and jump straight to an option
by sending the existing band/mode/step/bw/agc delta. The command is read-only and
touches no radio state.
