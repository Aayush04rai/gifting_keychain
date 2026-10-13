#pragma once

// Find my keychain, over Bluetooth LE.
//
// The keychain advertises quietly (a connectable beacon every 1-1.5 s) so a
// paired phone can find it and ask it to announce itself. It exposes only
// standard services, so phones and generic BLE apps already understand them:
//
//   Immediate Alert (0x1802)   writing alert level 1 or 2 makes it ring - the
//                              "Find Me" profile; level 0 stops it
//   Battery (0x180F)           charge level, for the phone's find screen
//   Device Information (0x180A) model and firmware
//
// "Ring" means: the screen wakes and shows "Here I am!", the LED pulses and the
// haptic motor buzzes - nothing audible. Picking the keychain up (or 60 s) ends
// it.
//
// Security: every characteristic needs an encrypted, authenticated link. The
// first connection pairs with a 6-digit passkey that the keychain shows on its
// screen and the phone's owner types in (LE Secure Connections). The phone is
// then remembered (bonded) and reconnects silently; strangers can neither ring
// it nor read it. New pairings are accepted for the first 10 minutes after
// power-up, and any time no phone has been paired yet. The advertised address
// is a rotating private one, so the device cannot be tracked by its address,
// and the name is a generic "Keychain XXXX", never the owner's.
//
// Firmware only (the simulator has no Bluetooth; it can still show the "find"
// and "pairing" screens via CHAR_FIND_AT_MS / CHAR_PAIR_AT_MS).
void ble_find_start(void);
