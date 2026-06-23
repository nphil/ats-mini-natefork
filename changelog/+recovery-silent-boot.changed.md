**Recovery no longer shows a splash or countdown — the radio boots straight to firmware.**
Previously recovery drew a "KQ4TXO / press encoder to enter recovery" splash and waited 2 seconds
on every power-on before forwarding to the main app. That window is gone. Recovery now forwards to
`ota_0` silently the instant it sees the encoder isn't held, so a normal power-on looks like a
direct boot into the main firmware (brief black screen, then the radio's own boot).

To enter recovery you now **hold the encoder button while powering on** (keep holding through the
boot — recovery confirms a steady press, then drops into its menu). The automatic boot-loop guard is
unchanged: after 3 failed boots recovery stays put and shows the loop warning. Requires re-flashing
the recovery partition (USB **Recovery** in-place, or the Bootloader method) for the new behavior to
take effect.
