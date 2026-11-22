# Proposal: Courtesy in company: removing a heir, closing the session, quick messages

Milestone: M11 (polish after the 1.0 RC).

## Why

Anyone with a six-character invite code could join, and the host could not remove them. Players
on touch or in VR could not type, so heirs had no way to say "wait" or "follow me".

## What Changes

- The host can remove a joined heir (`net.kick`): they are told why and their resume token is
  forgotten. The host can also close the session to new heirs (`net.set_open`; those away may
  still come back). `net.status().open` says which.
- Quick messages from a fixed list (follow me, wait, here, help, yes, no) go to every heir. Each
  sees them in their own language with the sender's name.
- A Company page in the menu (only in company): the quick messages; for the host, also
  "Remove <name>" after a question, and closing or opening the session.

## Platforms

All the same; in the browser through the relay as well.

## Co-op

The host is the authority for removing and closing. Quick messages travel on their own channel
to every heir.

## Impact

Capability `coop-sessions` (added "Removing a joiner and quick messages"). `src/net/session.{c,h}`,
`src/net/session_internal.h`, `src/script/script_net.c`, `coop/quick.lua`, `ui/menu.lua`,
`main.lua`, the German strings.
