# Instructions for agents

This repository holds Open-XY's own machines and firmware extensions. It is not the SDK, and it is not
where a new user's machine goes: a new machine starts from the
[machine template](https://github.com/etcetc000/open-xy-machine-template) in a
repository of its own.

- Clone the SDK beside this repository (`git clone
  https://github.com/etcetc000/open-xy ../open-xy`, default branch `staging`) and
  run its commands from here, for example
  `python ../open-xy/sdk.py test --machine machines/<id>` or
  `--extension extensions/<id>`.
- Change a machine or extension only inside its own folder. Extensions use only
  the SDK's public events and actions (`<opxy/extension.hpp>`) and carry tests;
  a new event, claim or action is an SDK pull request first. Machine code uses only the public
  `<opxy/...>` headers: no assembly, platform conditionals, OS headers or raw
  addresses, and audio callbacks never allocate, block, log or do I/O. SDK gaps
  are separate pull requests to the SDK's `staging` branch.
- Keep vendored code under `vendor/` byte-for-byte, with its licence.
- A release is a tag here, then a pull request against the SDK's `staging` that
  changes only `catalog/machines.json` (or `catalog/extensions.json`) to pin the
  new commit.
- A release's catalog entry carries a hardware `cpu` record from the SDK's
  [hardware profiling](https://github.com/etcetc000/open-xy/blob/staging/docs/profiling.md): a profiling build (synth controls are swept
  by default), one run on the user's OP-XY, then `profile report`. The sweep
  moves each control through its positions, then runs every control at its
  heaviest position together; a synth's record is the heaviest of those runs.
  Above the **recommended 3.5%** of the block budget, tell the user which setting
  is heaviest (or that the combination is) and suggest making it
  lighter before releasing; it is still accepted, with a warning.
- Device operations need the user's explicit instruction each time.
